#include "onenet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ESP8266.h"
#include "MqttKit.h"
#include "debug_uart7.h"

#define ONENET_PACKET_TIMEOUT_MS       3000U
#define ONENET_DOWNLINK_TOPIC_SIZE      128U
#define ONENET_DOWNLINK_PAYLOAD_SIZE    256U
#define ONENET_REQUEST_ID_SIZE           48U
#define ONENET_REPLY_PAYLOAD_SIZE       128U
#define ONENET_TEST_HEART_RATE           66
#define ONENET_TEST_SPO2                 99
#define ONENET_SUBSCRIBE_GROUP_NAME      ONENET_TOPIC_PROP_SET
#define ONENET_REPLY_CODE_OK            200U
#define ONENET_REPLY_CODE_BAD_REQUEST   400U

static volatile uint8_t g_onenet_session_error = 0U;
static volatile uint8_t g_onenet_last_status = ONENET_STATUS_OK;
static volatile uint8_t g_onenet_last_connack_code = 0U;
static volatile uint8_t g_onenet_subscribe_ready = 0U;
static volatile uint32_t g_onenet_downlink_count = 0U;
static volatile uint32_t g_onenet_publish_seq = 1U;
static char g_onenet_last_topic[ONENET_DOWNLINK_TOPIC_SIZE];
static char g_onenet_last_payload[ONENET_DOWNLINK_PAYLOAD_SIZE];
static const char *g_onenet_last_subscribe_topic = "";

static uint8_t OneNet_TopicMatches(const char *topic, uint16_t topic_len, const char *expected)
{
  size_t expected_len = 0U;

  if ((topic == NULL) || (expected == NULL))
  {
    return 0U;
  }

  expected_len = strlen(expected);
  if (topic_len != expected_len)
  {
    return 0U;
  }

  return (memcmp(topic, expected, expected_len) == 0) ? 1U : 0U;
}

static void OneNet_CopyTextForLog(const char *src,
                                  uint16_t src_len,
                                  char *dst,
                                  size_t dst_size)
{
  size_t copy_len = 0U;

  if ((dst == NULL) || (dst_size == 0U))
  {
    return;
  }

  memset(dst, 0, dst_size);
  if ((src == NULL) || (src_len == 0U))
  {
    return;
  }

  copy_len = src_len;
  if (copy_len >= dst_size)
  {
    copy_len = dst_size - 1U;
  }

  memcpy(dst, src, copy_len);
  dst[copy_len] = '\0';
}

static void OneNet_CopyDownlink(const char *topic,
                                uint16_t topic_len,
                                const char *payload,
                                uint16_t payload_len)
{
  uint16_t copy_topic_len = topic_len;
  uint16_t copy_payload_len = payload_len;

  if (copy_topic_len >= ONENET_DOWNLINK_TOPIC_SIZE)
  {
    copy_topic_len = (uint16_t)(ONENET_DOWNLINK_TOPIC_SIZE - 1U);
  }
  if (copy_payload_len >= ONENET_DOWNLINK_PAYLOAD_SIZE)
  {
    copy_payload_len = (uint16_t)(ONENET_DOWNLINK_PAYLOAD_SIZE - 1U);
  }

  memset(g_onenet_last_topic, 0, sizeof(g_onenet_last_topic));
  memset(g_onenet_last_payload, 0, sizeof(g_onenet_last_payload));

  if ((topic != NULL) && (copy_topic_len > 0U))
  {
    memcpy(g_onenet_last_topic, topic, copy_topic_len);
  }
  if ((payload != NULL) && (copy_payload_len > 0U))
  {
    memcpy(g_onenet_last_payload, payload, copy_payload_len);
  }

  g_onenet_last_topic[copy_topic_len] = '\0';
  g_onenet_last_payload[copy_payload_len] = '\0';
  g_onenet_downlink_count++;
}

static const char *OneNet_SkipJsonSpaces(const char *text)
{
  const char *cursor = text;

  while ((cursor != NULL) &&
         ((*cursor == ' ') || (*cursor == '\t') ||
          (*cursor == '\r') || (*cursor == '\n')))
  {
    cursor++;
  }

  return cursor;
}

static const char *OneNet_FindJsonValueStart(const char *json, const char *key)
{
  char pattern[24];
  const char *match = NULL;
  const char *colon = NULL;
  int pattern_len = 0;

  if ((json == NULL) || (key == NULL))
  {
    return NULL;
  }

  pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if ((pattern_len <= 0) || ((size_t)pattern_len >= sizeof(pattern)))
  {
    return NULL;
  }

  match = strstr(json, pattern);
  if (match == NULL)
  {
    return NULL;
  }

  colon = strchr(match + pattern_len, ':');
  if (colon == NULL)
  {
    return NULL;
  }

  return OneNet_SkipJsonSpaces(colon + 1);
}

static uint8_t OneNet_ParseJsonTextValue(const char *value_start,
                                         char *output,
                                         size_t output_size)
{
  const char *begin = NULL;
  const char *end = NULL;
  size_t copy_len = 0U;

  if ((value_start == NULL) || (output == NULL) || (output_size == 0U))
  {
    return 0U;
  }

  if (*value_start == '\"')
  {
    begin = value_start + 1;
    end = begin;
    while ((*end != '\0') && (*end != '\"'))
    {
      end++;
    }
    if (*end != '\"')
    {
      return 0U;
    }
  }
  else
  {
    begin = value_start;
    end = begin;
    while ((*end != '\0') &&
           (*end != ',') &&
           (*end != '}') &&
           (*end != ' ') &&
           (*end != '\t') &&
           (*end != '\r') &&
           (*end != '\n'))
    {
      end++;
    }
  }

  copy_len = (size_t)(end - begin);
  if ((copy_len == 0U) || (copy_len >= output_size))
  {
    return 0U;
  }

  memcpy(output, begin, copy_len);
  output[copy_len] = '\0';
  return 1U;
}

static uint8_t OneNet_ParseJsonIntValue(const char *value_start, int32_t *value)
{
  char number_text[24];
  char *end_ptr = NULL;
  long parsed = 0L;

  if ((value_start == NULL) || (value == NULL))
  {
    return 0U;
  }

  if (OneNet_ParseJsonTextValue(value_start, number_text, sizeof(number_text)) == 0U)
  {
    return 0U;
  }

  parsed = strtol(number_text, &end_ptr, 10);
  if ((end_ptr == NULL) || (*end_ptr != '\0'))
  {
    return 0U;
  }

  *value = (int32_t)parsed;
  return 1U;
}

static uint8_t OneNet_ParseRequestId(const char *json,
                                     char *request_id,
                                     size_t request_id_size)
{
  const char *value_start = OneNet_FindJsonValueStart(json, "id");
  return OneNet_ParseJsonTextValue(value_start, request_id, request_id_size);
}

static uint8_t OneNet_ParseTestValue(const char *json, int32_t *test_value)
{
  const char *params_start = NULL;
  const char *test_value_start = NULL;
  const char *nested_value_start = NULL;

  if ((json == NULL) || (test_value == NULL))
  {
    return 0U;
  }

  params_start = OneNet_FindJsonValueStart(json, "params");
  if (params_start != NULL)
  {
    test_value_start = OneNet_FindJsonValueStart(params_start, ONENET_DP_TEST_KEY);
  }

  if (test_value_start == NULL)
  {
    test_value_start = OneNet_FindJsonValueStart(json, ONENET_DP_TEST_KEY);
  }

  if (test_value_start == NULL)
  {
    return 0U;
  }

  if (*test_value_start == '{')
  {
    nested_value_start = OneNet_FindJsonValueStart(test_value_start, "value");
    return OneNet_ParseJsonIntValue(nested_value_start, test_value);
  }

  return OneNet_ParseJsonIntValue(test_value_start, test_value);
}

static uint8_t OneNet_PublishRaw(const char *topic,
                                 const char *payload,
                                 uint32_t payload_len,
                                 uint8_t packet_fail_status,
                                 uint8_t send_fail_status)
{
  MQTT_PACKET_STRUCTURE packet = {0};

  if (MQTT_PacketPublish(MQTT_PUBLISH_ID,
                         topic,
                         payload,
                         payload_len,
                         MQTT_QOS_LEVEL0,
                         0,
                         1,
                         &packet) != 0U)
  {
    g_onenet_last_status = packet_fail_status;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = send_fail_status;
    return 0U;
  }

  MQTT_DeleteBuffer(&packet);
  g_onenet_last_status = ONENET_STATUS_OK;
  return 1U;
}

static uint8_t OneNet_SendPropertySetReply(const char *request_id,
                                           uint16_t code,
                                           const char *message)
{
  char payload[ONENET_REPLY_PAYLOAD_SIZE];
  int payload_len = 0;

  if ((request_id == NULL) || (message == NULL))
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY invalid args\r\n");
    g_onenet_last_status = ONENET_STATUS_FAIL_REPLY_PACKET;
    return 0U;
  }

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%s\",\"code\":%u,\"msg\":\"%s\"}",
                         request_id,
                         (unsigned int)code,
                         message);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY payload build fail id=%s\r\n", request_id);
    g_onenet_last_status = ONENET_STATUS_FAIL_REPLY_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] SET_REPLY TX topic=%s payload=%s\r\n",
               ONENET_TOPIC_PROP_SET_REPLY,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_SET_REPLY,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_REPLY_PACKET,
                        ONENET_STATUS_FAIL_REPLY_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] SET_REPLY send fail id=%s code=%u\r\n",
                 request_id,
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  Debug_Printf("[MQTT] SET_REPLY OK id=%s\r\n", request_id);
  return 1U;
}

static uint8_t OneNet_PostTestValue(int32_t test_value)
{
  char payload[ONENET_REPLY_PAYLOAD_SIZE + 32U];
  int payload_len = 0;

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%ld}}}",
                         (unsigned long)g_onenet_publish_seq,
                         ONENET_DP_TEST_KEY,
                         (long)test_value);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    Debug_Printf("[MQTT][ERR] PROP POST test payload build fail value=%ld\r\n",
                 (long)test_value);
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  Debug_Printf("[MQTT] PROP POST test TX topic=%s payload=%s\r\n",
               ONENET_TOPIC_PROP_POST,
               payload);
  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    Debug_Printf("[MQTT][ERR] PROP POST test send fail value=%ld code=%u\r\n",
                 (long)test_value,
                 (unsigned int)OneNet_GetLastStatus());
    return 0U;
  }

  g_onenet_publish_seq++;
  Debug_Printf("[MQTT] PROP POST test OK value=%ld\r\n", (long)test_value);
  return 1U;
}

uint8_t OneNet_DevLink(void)
{
  MQTT_PACKET_STRUCTURE packet = {0};
  uint8_t *data_ptr = NULL;
  uint8_t connack = 0U;

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_last_connack_code = 0U;
  if (MQTT_PacketConnect(ONENET_USERNAME,
                         ONENET_PASSWORD,
                         ONENET_CLIENT_ID,
                         ONENET_KEEP_ALIVE_SECONDS,
                         1U,
                         MQTT_QOS_LEVEL0,
                         NULL,
                         NULL,
                         0,
                         &packet) != 0U)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_PACKET;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_SEND;
    return 0U;
  }
  MQTT_DeleteBuffer(&packet);

  data_ptr = ESP8266_GetIPD(ONENET_PACKET_TIMEOUT_MS);
  if (data_ptr == NULL)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_WAIT;
    return 0U;
  }

  if (MQTT_UnPacketRecv(data_ptr) != MQTT_PKT_CONNACK)
  {
    ESP8266_Clear();
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_TYPE;
    return 0U;
  }

  connack = MQTT_UnPacketConnectAck(data_ptr);
  ESP8266_Clear();
  if (connack != 0U)
  {
    g_onenet_last_connack_code = connack;
    g_onenet_session_error = 1U;
    g_onenet_last_status = ONENET_STATUS_FAIL_CONNECT_ACK;
    return 0U;
  }

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_last_connack_code = 0U;
  return 1U;
}

uint8_t OneNet_Subscribe(void)
{
  MQTT_PACKET_STRUCTURE packet = {0};
  static const char *topics[] =
  {
    ONENET_TOPIC_PROP_SET
  };

  g_onenet_last_status = ONENET_STATUS_OK;
  g_onenet_subscribe_ready = 0U;
  g_onenet_last_subscribe_topic = ONENET_SUBSCRIBE_GROUP_NAME;

  if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID,
                           MQTT_QOS_LEVEL0,
                           topics,
                           (uint8_t)(sizeof(topics) / sizeof(topics[0])),
                           &packet) != 0U)
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_SUB_PACKET;
    return 0U;
  }

  if (ESP8266_SendData(packet._data, (uint16_t)packet._len) == 0U)
  {
    MQTT_DeleteBuffer(&packet);
    g_onenet_last_status = ONENET_STATUS_FAIL_SUB_SEND;
    return 0U;
  }

  MQTT_DeleteBuffer(&packet);
  return 1U;
}

uint8_t OneNet_Publish(void)
{
  char payload[160];
  int payload_len = 0;

  payload_len = snprintf(payload,
                         sizeof(payload),
                         "{\"id\":\"%lu\",\"params\":{\"%s\":{\"value\":%d},\"%s\":{\"value\":%d}}}",
                         (unsigned long)g_onenet_publish_seq,
                         ONENET_DP_HEART_RATE_KEY,
                         ONENET_TEST_HEART_RATE,
                         ONENET_DP_BLOOD_OXYGEN_KEY,
                         ONENET_TEST_SPO2);
  if ((payload_len <= 0) || ((size_t)payload_len >= sizeof(payload)))
  {
    g_onenet_last_status = ONENET_STATUS_FAIL_PUBLISH_PACKET;
    return 0U;
  }

  if (OneNet_PublishRaw(ONENET_TOPIC_PROP_POST,
                        payload,
                        (uint32_t)payload_len,
                        ONENET_STATUS_FAIL_PUBLISH_PACKET,
                        ONENET_STATUS_FAIL_PUBLISH_SEND) == 0U)
  {
    return 0U;
  }

  g_onenet_publish_seq++;
  return 1U;
}

void OneNet_RevPro(const uint8_t *packet)
{
  uint8_t type = 0U;

  if (packet == NULL)
  {
    return;
  }

  type = MQTT_UnPacketRecv((uint8_t *)packet);
  if ((type < MQTT_PKT_CONNECT) || (type > MQTT_PKT_DISCONNECT))
  {
    Debug_Printf("[MQTT] RX INVALID first=0x%02X\r\n",
                 (unsigned int)packet[0]);
    ESP8266_DebugDumpCurrentRx("MQTT_INVALID", 48U);
    return;
  }

  Debug_Printf("[MQTT] RX TYPE=%u\r\n", (unsigned int)type);
  switch (type)
  {
    case MQTT_PKT_CONNACK:
      Debug_Printf("[MQTT] RX CONNACK\r\n");
      if (MQTT_UnPacketConnectAck((uint8_t *)packet) != 0U)
      {
        g_onenet_session_error = 1U;
      }
      break;

    case MQTT_PKT_SUBACK:
      Debug_Printf("[MQTT] RX SUBACK\r\n");
      if (MQTT_UnPacketSubscribe((uint8_t *)packet) != 0U)
      {
        g_onenet_session_error = 1U;
        g_onenet_last_status = ONENET_STATUS_FAIL_SUB_ACK;
        g_onenet_subscribe_ready = 0U;
      }
      else
      {
        g_onenet_last_status = ONENET_STATUS_OK;
        g_onenet_subscribe_ready = 1U;
      }
      break;

    case MQTT_PKT_PUBACK:
      Debug_Printf("[MQTT] RX PUBACK\r\n");
      (void)MQTT_UnPacketPublishAck((uint8_t *)packet);
      break;

    case MQTT_PKT_PINGRESP:
      Debug_Printf("[MQTT] RX PINGRESP\r\n");
      break;

    case MQTT_PKT_PUBLISH:
    {
      char *topic = NULL;
      char *payload = NULL;
      char topic_text[ONENET_DOWNLINK_TOPIC_SIZE];
      char payload_text[ONENET_DOWNLINK_PAYLOAD_SIZE];
      char request_id[ONENET_REQUEST_ID_SIZE];
      uint16_t topic_len = 0U;
      uint16_t payload_len = 0U;
      uint8_t qos = 0U;
      uint16_t pkt_id = 0U;
      int32_t test_value = 0;
      uint8_t has_request_id = 0U;
      uint8_t has_test_value = 0U;

      {
        uint8_t parse_status = MQTT_UnPacketPublish((uint8_t *)packet,
                                                    &topic,
                                                    &topic_len,
                                                    &payload,
                                                    &payload_len,
                                                    &qos,
                                                    &pkt_id);
        if (parse_status != 0U)
        {
          Debug_Printf("[MQTT] PUBLISH parse fail code=%u\r\n",
                       (unsigned int)parse_status);
          g_onenet_session_error = 1U;
          g_onenet_last_status = ONENET_STATUS_FAIL_RX_PARSE;
          break;
        }
      }

      OneNet_CopyTextForLog(topic, topic_len, topic_text, sizeof(topic_text));
      OneNet_CopyTextForLog(payload, payload_len, payload_text, sizeof(payload_text));

      Debug_Printf("[MQTT] RX PUBLISH topic=%s qos=%u pkt_id=%u payload_len=%u\r\n",
                   topic_text,
                   (unsigned int)qos,
                   (unsigned int)pkt_id,
                   (unsigned int)payload_len);
      Debug_Printf("[MQTT] RX PUBLISH payload=%s\r\n", payload_text);

      if (OneNet_TopicMatches(topic, topic_len, ONENET_TOPIC_PROP_SET) != 0U)
      {
        Debug_Printf("[MQTT] PROP SET matched\r\n");
        OneNet_CopyDownlink(topic, topic_len, payload, payload_len);

        memset(request_id, 0, sizeof(request_id));
        has_request_id = OneNet_ParseRequestId(g_onenet_last_payload,
                                               request_id,
                                               sizeof(request_id));
        has_test_value = OneNet_ParseTestValue(g_onenet_last_payload, &test_value);

        Debug_Printf("[MQTT] PROP SET payload=%s\r\n", g_onenet_last_payload);

        if (has_test_value != 0U)
        {
          Debug_Printf("[MQTT] PROP SET test=%ld\r\n", (long)test_value);
        }
        else
        {
          Debug_Printf("[MQTT][WARN] PROP SET test parse failed\r\n");
        }

        if (has_request_id != 0U)
        {
          Debug_Printf("[MQTT] PROP SET reply id=%s\r\n", request_id);
          if (OneNet_SendPropertySetReply(request_id,
                                          ONENET_REPLY_CODE_OK,
                                          "success") == 0U)
          {
            g_onenet_session_error = 1U;
          }
          else if (has_test_value != 0U)
          {
            if (OneNet_PostTestValue(test_value) == 0U)
            {
              g_onenet_session_error = 1U;
            }
          }
        }
        else
        {
          Debug_Printf("[MQTT][ERR] PROP SET missing id\r\n");
          g_onenet_last_status = ONENET_STATUS_FAIL_RX_PARSE;
        }
      }
      else
      {
        Debug_Printf("[MQTT] PUBLISH topic mismatch expect=%s got=%s\r\n",
                     ONENET_TOPIC_PROP_SET,
                     topic_text);
      }
      break;
    }

    default:
      Debug_Printf("[MQTT] RX UNHANDLED TYPE=%u\r\n", (unsigned int)type);
      break;
  }
}

uint8_t OneNet_HasSessionError(void)
{
  return g_onenet_session_error;
}

void OneNet_ClearSessionError(void)
{
  g_onenet_session_error = 0U;
}

void OneNet_ResetSubscribeState(void)
{
  g_onenet_subscribe_ready = 0U;
}

uint8_t OneNet_IsSubscribeReady(void)
{
  return g_onenet_subscribe_ready;
}

uint8_t OneNet_GetLastStatus(void)
{
  return g_onenet_last_status;
}

uint8_t OneNet_GetLastConnAckCode(void)
{
  return g_onenet_last_connack_code;
}

const char *OneNet_GetLastSubscribeTopic(void)
{
  return g_onenet_last_subscribe_topic;
}
