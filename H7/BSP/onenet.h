#ifndef __ONENET_H__
#define __ONENET_H__

#include <stdint.h>

#define ONENET_PRODUCT_ID               "S9U9FY9ZdS"
#define ONENET_DEVICE_NAME              "Bracelet"
#define ONENET_CLIENT_ID                ONENET_DEVICE_NAME
#define ONENET_USERNAME                 ONENET_PRODUCT_ID
#define ONENET_PASSWORD                 "version=2018-10-31&res=products%2FS9U9FY9ZdS%2Fdevices%2FBracelet&et=1805863774&method=md5&sign=h%2F8qRCOICNVzmUC0cTq5Bg%3D%3D"

#define ONENET_KEEP_ALIVE_SECONDS       60U
#define ONENET_PUBLISH_INTERVAL_MS    5000U

#define ONENET_DP_HEART_RATE_KEY        "HeartRate"
#define ONENET_DP_BLOOD_OXYGEN_KEY      "BloodOxygen"
#define ONENET_DP_TEST_KEY              "test"

#define ONENET_TOPIC_PROP_POST          "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/post"
#define ONENET_TOPIC_PROP_SET           "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set"
#define ONENET_TOPIC_PROP_SET_REPLY     "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set_reply"

#define ONENET_STATUS_OK                    0U
#define ONENET_STATUS_FAIL_CONNECT_PACKET   1U
#define ONENET_STATUS_FAIL_CONNECT_SEND     2U
#define ONENET_STATUS_FAIL_CONNECT_WAIT     3U
#define ONENET_STATUS_FAIL_CONNECT_TYPE     4U
#define ONENET_STATUS_FAIL_CONNECT_ACK      5U
#define ONENET_STATUS_FAIL_SUB_PACKET       6U
#define ONENET_STATUS_FAIL_SUB_SEND         7U
#define ONENET_STATUS_FAIL_SUB_WAIT         8U
#define ONENET_STATUS_FAIL_SUB_ACK          9U
#define ONENET_STATUS_FAIL_PUBLISH_PACKET  10U
#define ONENET_STATUS_FAIL_PUBLISH_SEND    11U
#define ONENET_STATUS_FAIL_RX_PARSE        12U
#define ONENET_STATUS_FAIL_REPLY_PACKET    13U
#define ONENET_STATUS_FAIL_REPLY_SEND      14U

uint8_t OneNet_DevLink(void);
uint8_t OneNet_Subscribe(void);
uint8_t OneNet_Publish(void);
void OneNet_RevPro(const uint8_t *packet);
void OneNet_ResetSubscribeState(void);
uint8_t OneNet_IsSubscribeReady(void);

uint8_t OneNet_HasSessionError(void);
void OneNet_ClearSessionError(void);
uint8_t OneNet_GetLastStatus(void);
uint8_t OneNet_GetLastConnAckCode(void);
const char *OneNet_GetLastSubscribeTopic(void);

#endif /* __ONENET_H__ */
