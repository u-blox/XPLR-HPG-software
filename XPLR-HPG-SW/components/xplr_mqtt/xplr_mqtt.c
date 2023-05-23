/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "xplr_mqtt.h"
#include <stdlib.h>
#include <math.h>
#include "xplr_ztp_json_parser.h"
#include "esp_check.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRMQTTWIFI_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRMQTTWIFI_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrMqttWifi", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRMQTTWIFI_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Type of ringbuffer: doesn't allow splitting of packets"
 */
#define MQTT_RING_BUFFER_TYPE   RINGBUF_TYPE_NOSPLIT

/**
 * Timeout for command execution and state change.
 * If nothing happens in 30 seconds then timeout.
 */
#define MQTT_ACTION_TIMEOUT     30

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static xplrMqttWifiError_t ret;
static esp_err_t esp_ret;
static char prevTopic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t xplrMqttWifiEventHandlerCb(esp_mqtt_event_handle_t event);

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void xplrMqttWifiEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                     void *event_data);
static esp_err_t xplrMqttWifiEvtToMqttPayload(esp_mqtt_event_handle_t event,
                                              xplrMqttWifiRingBuffItem_t *ringBuffCell);
static esp_err_t xplrMqttWifiAddItemToRingBuff(xplrMqttWifiFsmUCD_t *ucd,
                                               esp_mqtt_event_handle_t event);
static void xplrMqttWifiUpdateNextState(xplrMqttWifiFsmUCD_t *ucd,
                                        xplrMqttWifiClientStates_t nextState);
static void xplrMqttWifiUpdateNextStateToError(xplrMqttWifiFsmUCD_t *ucd);
static void xplrMqttWifiToNextState(xplrMqttWifiFsmUCD_t *ucd,
                                    xplrMqttWifiClientStates_t nextState);
static esp_err_t xplrMqttWifiDestroyConnection(xplrMqttWifiClient_t *client);
static xplrMqttWifiClientStates_t xplrMqttWifiGetCurrentStatePrivate(xplrMqttWifiFsmUCD_t *ucd);
static xplrMqttWifiClientStates_t xplrMqttWifiGetPreviousStatePrivate(xplrMqttWifiFsmUCD_t *ucd);
static esp_err_t xplrMqttWifiCheckQosLvl(xplrMqttWifiQosLvl_t qosLvl);

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/** 
 * MQTT event handler
 */
static void xplrMqttWifiEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                     void *event_data)
{
    xplrMqttWifiEventHandlerCb(event_data);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

esp_err_t xplrMqttWifiInitState(xplrMqttWifiClient_t *client)
{
    client->ucd.mqttFsm[0] = XPLR_MQTTWIFI_STATE_UNINIT;
    
    return ESP_OK;
}

esp_err_t xplrMqttWifiInitClient(xplrMqttWifiClient_t *client,
                                 esp_mqtt_client_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_FAIL;
    }

    /**
     * This is not the most efficient way to do it but for now
     * since ringbuff item has a data capacity of XPLR_MQTTWIFI_PAYLOAD_DATA_LEN
     * Can be changed inside KConfig
     */

    cfg->buffer_size = XPLR_MQTTWIFI_PAYLOAD_DATA_LEN;

    client->handler = esp_mqtt_client_init(cfg);
    if (client->handler == NULL) {
        return ESP_FAIL;
    }

    client->ucd.isConnected = false;
    client->ucd.xRingbuffer = xRingbufferCreate(client->ucd.ringBufferSlotsNumber * sizeof(xplrMqttWifiRingBuffItem_t), 
                                                RINGBUF_TYPE_NOSPLIT);
    if (client->ucd.xRingbuffer == NULL) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrMqttWifiStart(xplrMqttWifiClient_t *client)
{
    xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_CONFIG);

    return ESP_OK;
}

esp_err_t xplrMqttWifiSetRingbuffSlotsCount(xplrMqttWifiClient_t *client, uint8_t count)

{
    if (count <= 1) {
        XPLRMQTTWIFI_CONSOLE(W, "Ring buffer count is a non valid value: [%d]! Will assign default value 1!", count);
        client->ucd.ringBufferSlotsNumber = 1;
    } else {
        client->ucd.ringBufferSlotsNumber = count;
    }
    
    return ESP_OK;
}

bool xplrMqttWifiIsConnected(xplrMqttWifiClient_t *client)
{
    return client->ucd.isConnected;
}

xplrMqttWifiError_t xplrMqttWifiFsm(xplrMqttWifiClient_t *client)
{
    switch (xplrMqttWifiGetCurrentState(client)) {
        case XPLR_MQTTWIFI_STATE_UNINIT:
            break;

        case XPLR_MQTTWIFI_STATE_CONFIG:
            xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_REGISTER_EVENT);
            XPLRMQTTWIFI_CONSOLE(D, "MQTT config successful!");
            break;

        case XPLR_MQTTWIFI_STATE_REGISTER_EVENT:
            esp_ret = esp_mqtt_client_register_event(client->handler, ESP_EVENT_ANY_ID,
                                                     xplrMqttWifiEventHandler,
                                                     client->handler);
            if (esp_ret == ESP_OK) {
                xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_START);
                XPLRMQTTWIFI_CONSOLE(D, "MQTT event register successful!");
            } else {
                xplrMqttWifiUpdateNextStateToError(&client->ucd);
                XPLRMQTTWIFI_CONSOLE(E, "MQTT event register failed!");
            }
            break;

        case XPLR_MQTTWIFI_STATE_START:
            /**
             * Starting the client automatically connect to URI
             */
            esp_ret = esp_mqtt_client_start(client->handler);
            if (esp_ret == ESP_OK) {
                client->ucd.lastActionTime = MICROTOSEC(esp_timer_get_time());
                xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_WAIT);
                XPLRMQTTWIFI_CONSOLE(D, "MQTT client start successful!");
            } else {
                xplrMqttWifiUpdateNextStateToError(&client->ucd);
                XPLRMQTTWIFI_CONSOLE(E, "MQTT client start failed!");
            }
            break;

        case XPLR_MQTTWIFI_STATE_RECONNECT:
            esp_ret = esp_mqtt_client_reconnect(client->handler);
            if (esp_ret == ESP_OK) {
                client->ucd.lastActionTime = MICROTOSEC(esp_timer_get_time());
                xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_WAIT);
            } else {
                xplrMqttWifiUpdateNextStateToError(&client->ucd);
            }
            break;

        case XPLR_MQTTWIFI_STATE_WAIT:
            if (MICROTOSEC(esp_timer_get_time()) - client->ucd.lastActionTime >= MQTT_ACTION_TIMEOUT) {
                xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_TIMEOUT);
            }
            break;

        case XPLR_MQTTWIFI_STATE_CONNECTED:
            break;

        case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
            break;

        case XPLR_MQTTWIFI_STATE_DISCONNECT_REQUESTED:
            esp_ret = esp_mqtt_client_disconnect(client->handler);
            if (esp_ret == ESP_OK) {
                client->ucd.lastActionTime = MICROTOSEC(esp_timer_get_time());
                xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_WAIT);
                XPLRMQTTWIFI_CONSOLE(D, "MQTT request disconnect!");
            } else {
                xplrMqttWifiUpdateNextStateToError(&client->ucd);
            }
            break;

        case XPLR_MQTTWIFI_STATE_DISCONNECTED_OK:
            break;

        case XPLR_MQTTWIFI_STATE_TIMEOUT:
        case XPLR_MQTTWIFI_STATE_ERROR:
            break;

        default:
            xplrMqttWifiUpdateNextStateToError(&client->ucd);
            XPLRMQTTWIFI_CONSOLE(E, "MQTT Unknown state [%d]", xplrMqttWifiGetCurrentState(client));
            break;
    }

    return ret;
}

xplrMqttWifiClientStates_t xplrMqttWifiGetCurrentState(xplrMqttWifiClient_t *client)
{
    return xplrMqttWifiGetCurrentStatePrivate(&client->ucd);
}

xplrMqttWifiClientStates_t xplrMqttWifiGetPreviousState(xplrMqttWifiClient_t *client)
{
    return xplrMqttWifiGetPreviousStatePrivate(&client->ucd);
}

esp_err_t xplrMqttWifiSubscribeToTopicArrayZtp(xplrMqttWifiClient_t *client, xplrZtpStyleTopics_t *topics, xplrMqttWifiQosLvl_t qos)
{
    uint16_t topicCnt;

    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    if (topics->populatedCount == 0) {
        XPLRMQTTWIFI_CONSOLE(E, "Topics count is 0. There are no topics to subscribe!");
        return ESP_FAIL;
    }

    for (topicCnt = 0; topicCnt < topics->populatedCount; topicCnt++) {
        esp_ret = xplrMqttWifiSubscribeToTopicZtp(client, &topics->topic[topicCnt], qos);
        if (esp_ret != ESP_OK) {
            return esp_ret;
        }
    }

    return ESP_OK;
}

esp_err_t xplrMqttWifiSubscribeToTopicArray(xplrMqttWifiClient_t *client, char *topics[],
                                            uint16_t cnt,
                                            xplrMqttWifiQosLvl_t qos)
{
    uint16_t topicCnt;

    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    if (topics == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Topic pointer is NULL!");
        return ESP_FAIL;
    }

    if (cnt == 0) {
        XPLRMQTTWIFI_CONSOLE(E, "Topics count is 0. There are no topics to subscribe!");
        return ESP_FAIL;
    }

    esp_ret = xplrMqttWifiCheckQosLvl(qos);
    if (esp_ret != ESP_OK) {
        XPLRMQTTWIFI_CONSOLE(E, "QoS level [%d] is out of bounds!", qos);
        return esp_ret;
    }

    for (topicCnt = 0; topicCnt < cnt; topicCnt++) {
        esp_ret = xplrMqttWifiSubscribeToTopic(client, topics[topicCnt], qos);
        if (esp_ret != ESP_OK) {
            return esp_ret;
        }
    }

    return ESP_OK;
}

esp_err_t xplrMqttWifiSubscribeToTopicZtp(xplrMqttWifiClient_t *client, xplrTopic *topic,
                                          xplrMqttWifiQosLvl_t qos)
{
    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    esp_ret = xplrMqttWifiCheckQosLvl(qos);
    if (esp_ret != ESP_OK) {
        XPLRMQTTWIFI_CONSOLE(E, "QoS level [%d] is out of bounds!", qos);
        return esp_ret;
    }

    return xplrMqttWifiSubscribeToTopic(client, topic->path, qos);
}

esp_err_t xplrMqttWifiSubscribeToTopic(xplrMqttWifiClient_t *client, char *topic,
                                       xplrMqttWifiQosLvl_t qos)
{
    int return_id;

    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    if (topic == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Topic pointer is NULL!");
        return ESP_FAIL;
    }

    esp_ret = xplrMqttWifiCheckQosLvl(qos);
    if (esp_ret != ESP_OK) {
        XPLRMQTTWIFI_CONSOLE(E, "QoS level [%d] is out of bounds!", qos);
        return ESP_FAIL;
    }

    return_id = esp_mqtt_client_subscribe(client->handler, topic, qos);

    if (return_id < 0) {
        XPLRMQTTWIFI_CONSOLE(W, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }

    XPLRMQTTWIFI_CONSOLE(D, "Successfully subscribed to topic: %s with id: %d", topic, return_id);
    xplrMqttWifiToNextState(&client->ucd, XPLR_MQTTWIFI_STATE_SUBSCRIBED);

    return ESP_OK;
}

esp_err_t xplrMqttWifiPublishMsgZtp(xplrMqttWifiClient_t *client, 
                                    xplrTopic *topic, 
                                    char *data,
                                    uint64_t dataLength, 
                                    xplrMqttWifiQosLvl_t qos,
                                    int retain)
{
    int retPublish;

    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    if (data == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Data pointer is NULL!");
        return ESP_FAIL;
    }

    if (dataLength == 0) {
        XPLRMQTTWIFI_CONSOLE(E, "Data length is 0!");
        return ESP_FAIL;
    }

    esp_ret = xplrMqttWifiCheckQosLvl(qos);
    if (esp_ret != ESP_OK) {
        XPLRMQTTWIFI_CONSOLE(E, "QoS level [%d] is not valid!", qos);
        return esp_ret;
    }

    /**
     * We are using forced enqueued flag since we are using the function
     * to also publish messages with QoS level of 0.
     */
    retPublish = esp_mqtt_client_enqueue(client->handler, 
                                         topic->path, 
                                         data, 
                                         dataLength, 
                                         qos, 
                                         retain,
                                         true);

    if (retPublish == -1) {
        XPLRMQTTWIFI_CONSOLE(E, 
                             "Failed to publish data to topic: %s with QoS: %d and retaind flag: %d",
                             topic->path,
                             qos, 
                             retain);
        return ESP_FAIL;
    }

    XPLRMQTTWIFI_CONSOLE(I, 
                         "Published data to topic: %s with QoS: %d and retaind flag: %d", 
                         topic->path,
                         qos, 
                         retain);
    return ESP_OK;

}

esp_err_t xplrMqttWifiPublishMsg(xplrMqttWifiClient_t *client, char *topic, char *data,
                                 uint64_t dataLength, xplrMqttWifiQosLvl_t qos, int retain)
{
    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    if (topic == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Topic pointer is NULL!");
        return ESP_FAIL;
    }

    if (data == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Data pointer is NULL!");
        return ESP_FAIL;
    }

    if (dataLength == 0) {
        XPLRMQTTWIFI_CONSOLE(E, "Data length is 0!");
        return ESP_FAIL;
    }

    esp_ret = xplrMqttWifiCheckQosLvl(qos);
    if (esp_ret != ESP_OK) {
        XPLRMQTTWIFI_CONSOLE(E, "QoS level [%d] is out of bounds!", qos);
        return esp_ret;
    }

    int retPublish = esp_mqtt_client_enqueue(client->handler, topic, data, dataLength, qos, false, true);

    if (retPublish == -1) {
        return ESP_FAIL;
    }

    XPLRMQTTWIFI_CONSOLE(E, 
                         "Published data to topic: %s with QoS: %d and retaind flag: %d", 
                         topic, 
                         qos,
                         retain);
    return ESP_OK;
}

esp_err_t xplrMqttWifiUnsubscribeFromTopicArrayZtp(xplrMqttWifiClient_t *client,
                                                   xplrZtpStyleTopics_t *topics)
{
    uint16_t topicCnt;

    if (client == NULL) {
        XPLRMQTTWIFI_CONSOLE(E, "Client pointer is NULL!");
        return ESP_FAIL;
    }

    for (topicCnt = 0; topicCnt < topics->populatedCount; topicCnt++) {
        esp_ret = xplrMqttWifiUnsubscribeFromTopicZtp(client, &topics->topic[topicCnt]);
        if (esp_ret != ESP_OK) {
            return esp_ret;
        }
    }

    return ESP_OK;
}

esp_err_t xplrMqttWifiUnsubscribeFromTopicArray(xplrMqttWifiClient_t *client, char *topics[],
                                                uint16_t cnt)
{
    uint16_t topicCnt;

    for (topicCnt = 0; topicCnt < cnt; topicCnt++) {
        esp_ret = xplrMqttWifiUnsubscribeFromTopic(client, topics[topicCnt]);
    }

    return ESP_OK;
}

esp_err_t xplrMqttWifiUnsubscribeFromTopicZtp(xplrMqttWifiClient_t *client, xplrTopic *topic)
{
    return xplrMqttWifiUnsubscribeFromTopic(client, topic->path);
}

esp_err_t xplrMqttWifiUnsubscribeFromTopic(xplrMqttWifiClient_t *client, char *topic)
{
    int return_id = esp_mqtt_client_unsubscribe(client->handler, topic);

    if (return_id < 0) {
        XPLRMQTTWIFI_CONSOLE(W, "Failed to unsubscribe from topic: %s", topic);
        return ESP_FAIL;
    }

    XPLRMQTTWIFI_CONSOLE(D, "Successfully unsubscribed from topic: %s with id: %d", topic, return_id);
    return ESP_FAIL;
}

void xplrMqttWifiDisconnect(xplrMqttWifiClient_t *client)
{
    xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_DISCONNECT_REQUESTED);
}

void xplrMqttWifiHardDisconnect(xplrMqttWifiClient_t *client)
{
    esp_ret = xplrMqttWifiDestroyConnection(client);
    if (esp_ret == ESP_OK) {
        xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_DISCONNECTED_OK);
    }
}

void xplrMqttWifiReconnect(xplrMqttWifiClient_t *client)
{
    xplrMqttWifiUpdateNextState(&client->ucd, XPLR_MQTTWIFI_STATE_RECONNECT);
}

xplrMqttWifiGetItemError_t xplrMqttWifiReceiveItem(xplrMqttWifiClient_t *client,
                                                   xplrMqttWifiPayload_t *reply)
{
    uint32_t cntWaiting;
    xplrMqttWifiRingBuffItem_t *item;

    vRingbufferGetInfo(client->ucd.xRingbuffer, NULL, NULL, NULL, NULL, &cntWaiting);

    if (cntWaiting > 0) {
        /**
         * Execute immediately in ISR mode
         */
        item = (xplrMqttWifiRingBuffItem_t *)xRingbufferReceiveFromISR(client->ucd.xRingbuffer, NULL);

        if (item != NULL) {
            strcpy(reply->topic, item->topic);

            if (item->parts_total_no == 1) {
                if (reply->maxDataLength >= item->dataLength) {
                    memcpy(reply->data, item->data, item->dataLength);
                    memcpy(&client->ucd.prevItem, item, sizeof(xplrMqttWifiRingBuffItem_t));
                    reply->dataLength = item->dataLength;
                    vRingbufferReturnItem(client->ucd.xRingbuffer, (void *)item);
                    return XPLR_MQTTWIFI_ITEM_OK;
                } else {
                    XPLRMQTTWIFI_CONSOLE(E, "MQTT get buffer is not big enough. Cannot copy item.");
                    return XPLR_MQTTWIFI_ITEM_ERROR;
                }
                
            } else {
                /**
                 * Managing part 1 of N
                 */
                if (item->part_no == 1) {
                    if (reply->maxDataLength >= item->dataLength) {
                        memcpy(reply->data, item->data, item->dataLength);
                        memcpy(&client->ucd.prevItem, item, sizeof(xplrMqttWifiRingBuffItem_t));
                        reply->dataLength = item->dataLength;
                        vRingbufferReturnItem(client->ucd.xRingbuffer, (void *)item);
                        return XPLR_MQTTWIFI_ITEM_FETCHING;
                    } else {
                        XPLRMQTTWIFI_CONSOLE(E, "MQTT get buffer is not big enough. Cannot copy item.");
                        return XPLR_MQTTWIFI_ITEM_ERROR;
                    }
                }

                /**
                 * If it is the final part of the same topic as we started
                 * then return the final part and declare item as finished (whole).
                 */
                if ((item->part_no == item->parts_total_no) &&
                    (strcmp(item->topic, client->ucd.prevItem.topic) == 0)) {
                    if (reply->maxDataLength >= (reply->dataLength + item->dataLength)) { 
                        memcpy(reply->data + reply->dataLength, item->data, item->dataLength);
                        memcpy(&client->ucd.prevItem, item, sizeof(xplrMqttWifiRingBuffItem_t));
                        reply->dataLength += item->dataLength;
                        vRingbufferReturnItem(client->ucd.xRingbuffer, (void *)item);
                        return XPLR_MQTTWIFI_ITEM_OK;
                    } else {
                        XPLRMQTTWIFI_CONSOLE(E, "MQTT get buffer is not big enough. Cannot copy item.");
                        return XPLR_MQTTWIFI_ITEM_ERROR;
                    }
                }

                /**
                 * We continue parsing the rest of the parts to concat to the main message.
                 * We have certain
                 */
                if (((item->part_no - client->ucd.prevItem.part_no) == 1) &&
                    (strcmp(item->topic, client->ucd.prevItem.topic) == 0)) {
                    if (reply->maxDataLength >= (reply->dataLength + item->dataLength)) { 
                        memcpy(reply->data + reply->dataLength, item->data, item->dataLength);
                        memcpy(&client->ucd.prevItem, item, sizeof(xplrMqttWifiRingBuffItem_t));
                        reply->dataLength += item->dataLength;
                        vRingbufferReturnItem(client->ucd.xRingbuffer, (void *)item);
                        return XPLR_MQTTWIFI_ITEM_FETCHING;
                    } else {
                        XPLRMQTTWIFI_CONSOLE(E, "MQTT get buffer is not big enough. Cannot copy item.");
                        return XPLR_MQTTWIFI_ITEM_ERROR;
                    }
                }

                /**
                 * If all previous cases fail then it means that the chain is broken.
                 * Most probably a part was not added into the ringbuffer in the correct order
                 * which should be the normal case.
                 */
                vRingbufferReturnItem(client->ucd.xRingbuffer, (void *)item);
                reply->dataLength = 0;
                return XPLR_MQTTWIFI_ITEM_ERROR;
            }
        } else {
            XPLRMQTTWIFI_CONSOLE(W, "NULL item from RingBuff");
            return XPLR_MQTTWIFI_ITEM_ERROR;
        }
    }

    return XPLR_MQTTWIFI_ITEM_NOITEM;
}

/* ----------------------------------------------------------------
 * CALLBACK FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/**
 * Callback event handler for MQTT.
 * We are able to pass user context data through client config.
 */
static esp_err_t xplrMqttWifiEventHandlerCb(esp_mqtt_event_handle_t event)
{
    xplrMqttWifiFsmUCD_t *ucd = (xplrMqttWifiFsmUCD_t *)event->user_context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            xplrMqttWifiUpdateNextState(ucd, XPLR_MQTTWIFI_STATE_CONNECTED);
            ucd->isConnected = true;
            XPLRMQTTWIFI_CONSOLE(D, "MQTT event connected!");
            break;

        case MQTT_EVENT_DISCONNECTED:
            if ((xplrMqttWifiGetCurrentStatePrivate(ucd) == XPLR_MQTTWIFI_STATE_WAIT) &&
                (xplrMqttWifiGetPreviousStatePrivate(ucd) == XPLR_MQTTWIFI_STATE_DISCONNECT_REQUESTED)) {
                xplrMqttWifiUpdateNextState(ucd, XPLR_MQTTWIFI_STATE_DISCONNECTED_OK);
            }
            ucd->isConnected = false;
            XPLRMQTTWIFI_CONSOLE(D, "MQTT event disconnected!");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            XPLRMQTTWIFI_CONSOLE(D, "MQTT event subscribed!");
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            XPLRMQTTWIFI_CONSOLE(D, "MQTT event unsubscribed!");
            break;

        case MQTT_EVENT_PUBLISHED:
            break;

        case MQTT_EVENT_DATA:
            /**
             * Data will be segmented if received payload is larger than the client's
             * configured inbound buffer
             */
            xplrMqttWifiAddItemToRingBuff((xplrMqttWifiFsmUCD_t *)event->user_context, event);
            break;

        case MQTT_EVENT_ERROR:
            XPLRMQTTWIFI_CONSOLE(E, "MQTT event error!");
            xplrMqttWifiUpdateNextStateToError(ucd);
            break;

        default:
            break;
    }
    return ESP_OK;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

static esp_err_t xplrMqttWifiAddItemToRingBuff(xplrMqttWifiFsmUCD_t *ucd,
                                               esp_mqtt_event_handle_t event)
{
    UBaseType_t res;
    xplrMqttWifiRingBuffItem_t tmpRingBuffCell;

    xplrMqttWifiEvtToMqttPayload(event, &tmpRingBuffCell);

    res = xRingbufferSend(ucd->xRingbuffer, (uint8_t *)&tmpRingBuffCell,
                          sizeof(xplrMqttWifiRingBuffItem_t), pdMS_TO_TICKS(10000));
    if (res != pdTRUE) {
        XPLRMQTTWIFI_CONSOLE(W, "RingBuff add timeout! The buffer might be full!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * Helper function to "cast" esp_mqtt_event_handle_t INTO xplrMqttWifiRingBuffItem_t
 */
static esp_err_t xplrMqttWifiEvtToMqttPayload(esp_mqtt_event_handle_t event,
                                              xplrMqttWifiRingBuffItem_t *ringBuffCell)
{
    ringBuffCell->dataLength      = event->data_len;
    ringBuffCell->totalDataLength = event->total_data_len;
    ringBuffCell->parts_total_no  = ceil((float)event->total_data_len / ((float)(ELEMENTCNT(ringBuffCell->data))));

    if (ringBuffCell->parts_total_no > 1) {
        if (event->current_data_offset == 0) {
            ringBuffCell->part_no = 1;
        } else {
            ringBuffCell->part_no++;
        }
    } else {
        ringBuffCell->part_no = 1;
    }

    if (event->topic_len > ELEMENTCNT(ringBuffCell->topic) - 1) {
        return ESP_FAIL;
    } else {
        if (event->topic_len == 0) {
            strcpy(ringBuffCell->topic, prevTopic);
        } else {
            memcpy(ringBuffCell->topic, event->topic, event->topic_len);
            memcpy(prevTopic, event->topic, event->topic_len);
            ringBuffCell->topic[event->topic_len] = 0;
            prevTopic[event->topic_len] = 0;
        }
    }

    if (event->data_len > ELEMENTCNT(ringBuffCell->data)) {
        return ESP_FAIL;
    } else {
        memcpy(ringBuffCell->data, event->data, event->data_len);
    }

    return ESP_OK;
}

/**
 * Will try to completely disconnect MQTT conenction together with it's handler
 * and client.
 * The destroyed handlers will need to be restarted
 */
static esp_err_t xplrMqttWifiDestroyConnection(xplrMqttWifiClient_t *client)
{
    if (client->handler == NULL) {
        XPLRMQTTWIFI_CONSOLE(W, "Client handler seems to be NULL! Maybe client has not been initialized.");
        return ESP_FAIL;
    }

    esp_ret = esp_mqtt_client_stop(client->handler);
    if (esp_ret != ESP_OK || esp_ret != ESP_ERR_INVALID_ARG) {
        return esp_ret;
    }

    esp_ret = esp_mqtt_client_destroy(client->handler);
    if (esp_ret != ESP_OK || esp_ret != ESP_ERR_INVALID_ARG) {
        return esp_ret;
    }

    vRingbufferDelete(client->ucd.xRingbuffer);

    return ESP_OK;
}

static void xplrMqttWifiUpdateNextState(xplrMqttWifiFsmUCD_t *ucd,
                                        xplrMqttWifiClientStates_t nextState)
{
    xplrMqttWifiToNextState(ucd, nextState);
    ret = XPLR_MQTTWIFI_OK;
}

static void xplrMqttWifiUpdateNextStateToError(xplrMqttWifiFsmUCD_t *ucd)
{
    xplrMqttWifiToNextState(ucd, XPLR_MQTTWIFI_STATE_ERROR);
    ret = XPLR_MQTTWIFI_ERROR;
}

static void xplrMqttWifiToNextState(xplrMqttWifiFsmUCD_t *ucd, xplrMqttWifiClientStates_t nextState)
{
    ucd->mqttFsm[1] = ucd->mqttFsm[0];
    ucd->mqttFsm[0] = nextState;
}

static xplrMqttWifiClientStates_t xplrMqttWifiGetCurrentStatePrivate(xplrMqttWifiFsmUCD_t *ucd)
{
    return ucd->mqttFsm[0];
}

static xplrMqttWifiClientStates_t xplrMqttWifiGetPreviousStatePrivate(xplrMqttWifiFsmUCD_t *ucd)
{
    return ucd->mqttFsm[1];
}

static esp_err_t xplrMqttWifiCheckQosLvl(xplrMqttWifiQosLvl_t qosLvl)
{
    if ((qosLvl != XPLR_MQTTWIFI_QOS_LVL_0) &&
        (qosLvl != XPLR_MQTTWIFI_QOS_LVL_1) && 
        (qosLvl != XPLR_MQTTWIFI_QOS_LVL_2)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}