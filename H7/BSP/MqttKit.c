#include "MqttKit.h"

#include <stdlib.h>
#include <string.h>

#define MQTT_INVALID_PACKET_TYPE  255U
#define MQTT_FLAG_SUBSCRIBE       0x02U
#define MQTT_CONNECT_CLEAN        0x02U
#define MQTT_CONNECT_WILL_FLAG    0x04U
#define MQTT_CONNECT_WILL_QOS1    0x08U
#define MQTT_CONNECT_WILL_QOS2    0x10U
#define MQTT_CONNECT_WILL_RETAIN  0x20U
#define MQTT_CONNECT_PASSWORD     0x40U
#define MQTT_CONNECT_USERNAME     0x80U

static void MQTT_NewBuffer(MQTT_PACKET_STRUCTURE *mqttPacket, uint32_t size)
{
  uint32_t i = 0U;

  if (mqttPacket == NULL)
  {
    return;
  }

  if (mqttPacket->_data == NULL)
  {
    mqttPacket->_data = (uint8_t *)malloc(size);
    mqttPacket->_memFlag = MEM_FLAG_ALLOC;
  }
  else
  {
    mqttPacket->_memFlag = MEM_FLAG_STATIC;
  }

  if (mqttPacket->_data == NULL)
  {
    mqttPacket->_len = 0U;
    mqttPacket->_size = 0U;
    return;
  }

  mqttPacket->_len = 0U;
  mqttPacket->_size = size;
  for (i = 0U; i < size; i++)
  {
    mqttPacket->_data[i] = 0U;
  }
}

static int32_t MQTT_DumpLength(uint32_t len, uint8_t *buf)
{
  int32_t i = 0;

  for (i = 1; i <= 4; ++i)
  {
    *buf = (uint8_t)(len % 128U);
    len >>= 7;
    if (len > 0U)
    {
      *buf |= 0x80U;
      ++buf;
    }
    else
    {
      return i;
    }
  }

  return -1;
}

static int32_t MQTT_ReadLength(const uint8_t *stream, int32_t size, uint32_t *len)
{
  int32_t i = 0;
  uint32_t multiplier = 1U;

  if ((stream == NULL) || (len == NULL))
  {
    return -1;
  }

  *len = 0U;
  for (i = 0; i < size; ++i)
  {
    *len += (uint32_t)(stream[i] & 0x7FU) * multiplier;

    if ((stream[i] & 0x80U) == 0U)
    {
      return i + 1;
    }

    multiplier <<= 7;
    if (multiplier >= 2097152U)
    {
      return -2;
    }
  }

  return -1;
}

static void MQTT_WriteString(MQTT_PACKET_STRUCTURE *packet, const char *text, uint16_t length)
{
  if ((packet == NULL) || (packet->_data == NULL))
  {
    return;
  }

  packet->_data[packet->_len++] = MOSQ_MSB(length);
  packet->_data[packet->_len++] = MOSQ_LSB(length);
  if ((text != NULL) && (length > 0U))
  {
    memcpy(&packet->_data[packet->_len], text, length);
    packet->_len += length;
  }
}

void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket)
{
  if (mqttPacket == NULL)
  {
    return;
  }

  if ((mqttPacket->_memFlag == MEM_FLAG_ALLOC) && (mqttPacket->_data != NULL))
  {
    free(mqttPacket->_data);
  }

  mqttPacket->_data = NULL;
  mqttPacket->_len = 0U;
  mqttPacket->_size = 0U;
  mqttPacket->_memFlag = MEM_FLAG_NULL;
}

uint8_t MQTT_UnPacketRecv(uint8_t *dataPtr)
{
  uint8_t type = 0U;

  if (dataPtr == NULL)
  {
    return MQTT_INVALID_PACKET_TYPE;
  }

  type = (uint8_t)(dataPtr[0] >> 4);
  if ((type < MQTT_PKT_CONNECT) || (type > MQTT_PKT_DISCONNECT))
  {
    return MQTT_INVALID_PACKET_TYPE;
  }

  return type;
}

uint8_t MQTT_PacketConnect(const char *user,
                           const char *password,
                           const char *devid,
                           uint16_t keep_alive,
                           uint8_t clean_session,
                           uint8_t qos,
                           const char *will_topic,
                           const char *will_msg,
                           int32_t will_retain,
                           MQTT_PACKET_STRUCTURE *mqttPacket)
{
  uint8_t flags = 0U;
  uint16_t total_len = 10U;
  uint16_t devid_len = 0U;
  uint16_t user_len = 0U;
  uint16_t password_len = 0U;
  int32_t len_len = 0;

  if ((user == NULL) || (password == NULL) || (devid == NULL) || (mqttPacket == NULL))
  {
    return 1U;
  }

  devid_len = (uint16_t)strlen(devid);
  user_len = (uint16_t)strlen(user);
  password_len = (uint16_t)strlen(password);
  total_len = (uint16_t)(total_len + 2U + devid_len + 2U + user_len + 2U + password_len);

  if (clean_session != 0U)
  {
    flags |= MQTT_CONNECT_CLEAN;
  }

  if ((will_topic != NULL) && (will_msg != NULL))
  {
    uint16_t will_topic_len = (uint16_t)strlen(will_topic);
    uint16_t will_msg_len = (uint16_t)strlen(will_msg);

    flags |= MQTT_CONNECT_WILL_FLAG;
    if (qos == MQTT_QOS_LEVEL1)
    {
      flags |= MQTT_CONNECT_WILL_QOS1;
    }
    else if (qos == MQTT_QOS_LEVEL2)
    {
      flags |= MQTT_CONNECT_WILL_QOS2;
    }

    if (will_retain != 0)
    {
      flags |= MQTT_CONNECT_WILL_RETAIN;
    }

    total_len = (uint16_t)(total_len + 2U + will_topic_len + 2U + will_msg_len);
  }

  flags |= (MQTT_CONNECT_USERNAME | MQTT_CONNECT_PASSWORD);

  MQTT_NewBuffer(mqttPacket, (uint32_t)(total_len + 5U));
  if (mqttPacket->_data == NULL)
  {
    return 2U;
  }

  mqttPacket->_data[mqttPacket->_len++] = (uint8_t)(MQTT_PKT_CONNECT << 4);
  len_len = MQTT_DumpLength((uint32_t)total_len, &mqttPacket->_data[mqttPacket->_len]);
  if (len_len < 0)
  {
    MQTT_DeleteBuffer(mqttPacket);
    return 3U;
  }
  mqttPacket->_len += (uint32_t)len_len;

  MQTT_WriteString(mqttPacket, "MQTT", 4U);
  mqttPacket->_data[mqttPacket->_len++] = 4U;
  mqttPacket->_data[mqttPacket->_len++] = flags;
  mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(keep_alive);
  mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(keep_alive);

  MQTT_WriteString(mqttPacket, devid, devid_len);

  if ((will_topic != NULL) && (will_msg != NULL))
  {
    MQTT_WriteString(mqttPacket, will_topic, (uint16_t)strlen(will_topic));
    MQTT_WriteString(mqttPacket, will_msg, (uint16_t)strlen(will_msg));
  }

  MQTT_WriteString(mqttPacket, user, user_len);
  MQTT_WriteString(mqttPacket, password, password_len);

  return 0U;
}

uint8_t MQTT_UnPacketConnectAck(uint8_t *rev_data)
{
  uint32_t remain_len = 0U;
  int32_t len_len = 0;

  if ((rev_data == NULL) || (MQTT_UnPacketRecv(rev_data) != MQTT_PKT_CONNACK))
  {
    return 1U;
  }

  len_len = MQTT_ReadLength(rev_data + 1, 4, &remain_len);
  if ((len_len < 0) || (remain_len < 2U))
  {
    return 2U;
  }

  return rev_data[1 + len_len + 1];
}

uint8_t MQTT_PacketSubscribe(uint16_t pkt_id,
                             enum MqttQosLevel qos,
                             const char *topics[],
                             uint8_t topics_cnt,
                             MQTT_PACKET_STRUCTURE *mqttPacket)
{
  uint16_t remain_len = 2U;
  uint8_t i = 0U;
  int32_t len_len = 0;

  if ((topics == NULL) || (topics_cnt == 0U) || (mqttPacket == NULL))
  {
    return 1U;
  }

  for (i = 0U; i < topics_cnt; i++)
  {
    if (topics[i] == NULL)
    {
      return 2U;
    }
    remain_len = (uint16_t)(remain_len + 2U + strlen(topics[i]) + 1U);
  }

  MQTT_NewBuffer(mqttPacket, (uint32_t)(remain_len + 5U));
  if (mqttPacket->_data == NULL)
  {
    return 3U;
  }

  mqttPacket->_data[mqttPacket->_len++] = (uint8_t)((MQTT_PKT_SUBSCRIBE << 4) | MQTT_FLAG_SUBSCRIBE);
  len_len = MQTT_DumpLength(remain_len, &mqttPacket->_data[mqttPacket->_len]);
  if (len_len < 0)
  {
    MQTT_DeleteBuffer(mqttPacket);
    return 4U;
  }
  mqttPacket->_len += (uint32_t)len_len;

  mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
  mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);

  for (i = 0U; i < topics_cnt; i++)
  {
    MQTT_WriteString(mqttPacket, topics[i], (uint16_t)strlen(topics[i]));
    mqttPacket->_data[mqttPacket->_len++] = (uint8_t)qos;
  }

  return 0U;
}

uint8_t MQTT_UnPacketSubscribe(uint8_t *rev_data)
{
  uint32_t remain_len = 0U;
  int32_t len_len = 0;
  uint32_t offset = 0U;

  if ((rev_data == NULL) || (MQTT_UnPacketRecv(rev_data) != MQTT_PKT_SUBACK))
  {
    return 1U;
  }

  len_len = MQTT_ReadLength(rev_data + 1, 4, &remain_len);
  if ((len_len < 0) || (remain_len < 3U))
  {
    return 2U;
  }

  offset = (uint32_t)(1 + len_len + 2);
  while (offset < (uint32_t)(1 + len_len + remain_len))
  {
    if (rev_data[offset] == 0x80U)
    {
      return 3U;
    }
    offset++;
  }

  return 0U;
}

uint8_t MQTT_PacketPublish(uint16_t pkt_id,
                           const char *topic,
                           const char *payload,
                           uint32_t payload_len,
                           enum MqttQosLevel qos,
                           int32_t retain,
                           int32_t own,
                           MQTT_PACKET_STRUCTURE *mqttPacket)
{
  uint16_t topic_len = 0U;
  uint16_t remain_len = 0U;
  int32_t len_len = 0;
  uint8_t header = 0U;

  (void)own;

  if ((topic == NULL) || (payload == NULL) || (mqttPacket == NULL))
  {
    return 1U;
  }

  topic_len = (uint16_t)strlen(topic);
  remain_len = (uint16_t)(2U + topic_len + payload_len);
  if (qos != MQTT_QOS_LEVEL0)
  {
    remain_len = (uint16_t)(remain_len + 2U);
  }

  MQTT_NewBuffer(mqttPacket, (uint32_t)(remain_len + 5U));
  if (mqttPacket->_data == NULL)
  {
    return 2U;
  }

  header = (uint8_t)(MQTT_PKT_PUBLISH << 4);
  header |= (uint8_t)((uint8_t)qos << 1);
  if (retain != 0)
  {
    header |= 0x01U;
  }
  mqttPacket->_data[mqttPacket->_len++] = header;

  len_len = MQTT_DumpLength(remain_len, &mqttPacket->_data[mqttPacket->_len]);
  if (len_len < 0)
  {
    MQTT_DeleteBuffer(mqttPacket);
    return 3U;
  }
  mqttPacket->_len += (uint32_t)len_len;

  MQTT_WriteString(mqttPacket, topic, topic_len);

  if (qos != MQTT_QOS_LEVEL0)
  {
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
    mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
  }

  memcpy(&mqttPacket->_data[mqttPacket->_len], payload, payload_len);
  mqttPacket->_len += payload_len;

  return 0U;
}

uint8_t MQTT_UnPacketPublish(uint8_t *rev_data,
                             char **topic,
                             uint16_t *topic_len,
                             char **payload,
                             uint16_t *payload_len,
                             uint8_t *qos,
                             uint16_t *pkt_id)
{
  uint32_t remain_len = 0U;
  int32_t len_len = 0;
  uint32_t offset = 0U;
  uint16_t local_topic_len = 0U;
  uint8_t local_qos = 0U;

  if ((rev_data == NULL) || (MQTT_UnPacketRecv(rev_data) != MQTT_PKT_PUBLISH))
  {
    return 1U;
  }

  len_len = MQTT_ReadLength(rev_data + 1, 4, &remain_len);
  if (len_len < 0)
  {
    return 2U;
  }

  offset = (uint32_t)(1 + len_len);
  if (remain_len < 2U)
  {
    return 3U;
  }

  local_topic_len = (uint16_t)(((uint16_t)rev_data[offset] << 8) | rev_data[offset + 1U]);
  offset += 2U;
  if ((remain_len < (uint32_t)(2U + local_topic_len)) || (local_topic_len == 0U))
  {
    return 4U;
  }

  if (topic != NULL)
  {
    *topic = (char *)&rev_data[offset];
  }
  if (topic_len != NULL)
  {
    *topic_len = local_topic_len;
  }
  offset += local_topic_len;

  local_qos = (uint8_t)((rev_data[0] & 0x06U) >> 1);
  if (qos != NULL)
  {
    *qos = local_qos;
  }

  if (local_qos != MQTT_QOS_LEVEL0)
  {
    if ((uint32_t)(1 + len_len + remain_len) < (offset + 2U))
    {
      return 5U;
    }
    if (pkt_id != NULL)
    {
      *pkt_id = (uint16_t)(((uint16_t)rev_data[offset] << 8) | rev_data[offset + 1U]);
    }
    offset += 2U;
  }
  else if (pkt_id != NULL)
  {
    *pkt_id = 0U;
  }

  if (payload != NULL)
  {
    *payload = (char *)&rev_data[offset];
  }
  if (payload_len != NULL)
  {
    *payload_len = (uint16_t)((1U + (uint32_t)len_len + remain_len) - offset);
  }

  return 0U;
}

uint8_t MQTT_UnPacketPublishAck(uint8_t *rev_data)
{
  if ((rev_data == NULL) || (MQTT_UnPacketRecv(rev_data) != MQTT_PKT_PUBACK))
  {
    return 1U;
  }

  return 0U;
}

uint8_t MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket)
{
  if (mqttPacket == NULL)
  {
    return 1U;
  }

  MQTT_NewBuffer(mqttPacket, 2U);
  if (mqttPacket->_data == NULL)
  {
    return 2U;
  }

  mqttPacket->_data[mqttPacket->_len++] = (uint8_t)(MQTT_PKT_PINGREQ << 4);
  mqttPacket->_data[mqttPacket->_len++] = 0U;
  return 0U;
}
