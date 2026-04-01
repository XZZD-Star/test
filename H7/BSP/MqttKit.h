#ifndef __MQTTKIT_H__
#define __MQTTKIT_H__

#include <stddef.h>
#include <stdint.h>

#define MEM_FLAG_NULL    0U
#define MEM_FLAG_ALLOC   1U
#define MEM_FLAG_STATIC  2U

#define MOSQ_MSB(A)      ((uint8_t)(((A) & 0xFF00U) >> 8))
#define MOSQ_LSB(A)      ((uint8_t)((A) & 0x00FFU))

#define MQTT_PUBLISH_ID    10U
#define MQTT_SUBSCRIBE_ID  20U

typedef struct
{
  uint8_t *_data;
  uint32_t _len;
  uint32_t _size;
  uint8_t _memFlag;
} MQTT_PACKET_STRUCTURE;

enum MqttPacketType
{
  MQTT_PKT_CONNECT = 1,
  MQTT_PKT_CONNACK,
  MQTT_PKT_PUBLISH,
  MQTT_PKT_PUBACK,
  MQTT_PKT_PUBREC,
  MQTT_PKT_PUBREL,
  MQTT_PKT_PUBCOMP,
  MQTT_PKT_SUBSCRIBE,
  MQTT_PKT_SUBACK,
  MQTT_PKT_UNSUBSCRIBE,
  MQTT_PKT_UNSUBACK,
  MQTT_PKT_PINGREQ,
  MQTT_PKT_PINGRESP,
  MQTT_PKT_DISCONNECT
};

enum MqttQosLevel
{
  MQTT_QOS_LEVEL0 = 0,
  MQTT_QOS_LEVEL1 = 1,
  MQTT_QOS_LEVEL2 = 2
};

void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket);

uint8_t MQTT_UnPacketRecv(uint8_t *dataPtr);
uint8_t MQTT_PacketConnect(const char *user,
                           const char *password,
                           const char *devid,
                           uint16_t keep_alive,
                           uint8_t clean_session,
                           uint8_t qos,
                           const char *will_topic,
                           const char *will_msg,
                           int32_t will_retain,
                           MQTT_PACKET_STRUCTURE *mqttPacket);
uint8_t MQTT_UnPacketConnectAck(uint8_t *rev_data);
uint8_t MQTT_PacketSubscribe(uint16_t pkt_id,
                             enum MqttQosLevel qos,
                             const char *topics[],
                             uint8_t topics_cnt,
                             MQTT_PACKET_STRUCTURE *mqttPacket);
uint8_t MQTT_UnPacketSubscribe(uint8_t *rev_data);
uint8_t MQTT_PacketPublish(uint16_t pkt_id,
                           const char *topic,
                           const char *payload,
                           uint32_t payload_len,
                           enum MqttQosLevel qos,
                           int32_t retain,
                           int32_t own,
                           MQTT_PACKET_STRUCTURE *mqttPacket);
uint8_t MQTT_UnPacketPublish(uint8_t *rev_data,
                             char **topic,
                             uint16_t *topic_len,
                             char **payload,
                             uint16_t *payload_len,
                             uint8_t *qos,
                             uint16_t *pkt_id);
uint8_t MQTT_UnPacketPublishAck(uint8_t *rev_data);
uint8_t MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket);

#endif /* __MQTTKIT_H__ */
