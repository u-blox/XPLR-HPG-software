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

#ifndef _XPLR_MQTT_H_
#define _XPLR_MQTT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../hpglib/xplr_hpglib_cfg.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/ringbuf.h"
#include "xplr_ztp_json_parser.h"

/** @file
 * @brief This header file defines the wifi mqtt service API,
 * including broker settings, subscribing to topics, receiving messages from topics,
 * publishing messages to topics.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN  (128U)  /**< maximum size for topic name/address. */
#define XPLR_MQTTWIFI_PAYLOAD_DATA_LEN   (1024U) /**< maximum data length for both xplrMqttWifiRingBuffItem buffer and MQTT config buffer size .*/

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/**
 * Error codes specific to hpgCom module.
 */
typedef enum {
    XPLR_MQTTWIFI_ERROR = -1,   /**< process returned with errors. */
    XPLR_MQTTWIFI_OK,           /**< indicates success of returning process. */
    XPLR_MQTTWIFI_BUSY,         /**< indicates process is busy. */
} xplrMqttWifiError_t;

/**
 * Return value while getting item from MQTT
 */
typedef enum {
    XPLR_MQTTWIFI_ITEM_ERROR = -2,  /**< there was an error populating the item to return. */
    XPLR_MQTTWIFI_ITEM_NOITEM,      /**< no item could be retrieved. */
    XPLR_MQTTWIFI_ITEM_OK = 0,      /**< item returned successfully. */
    XPLR_MQTTWIFI_ITEM_FETCHING,    /**< item is still in the process of fetching. */
} xplrMqttWifiGetItemError_t;

/**
 * States describing the MQTT Client.
 */
typedef enum {
    XPLR_MQTTWIFI_STATE_TIMEOUT = -2,        /**< timeout ocurred. */
    XPLR_MQTTWIFI_STATE_ERROR,               /**< error state. */
    XPLR_MQTTWIFI_STATE_CONNECT_OK = 0,      /**< ok state. */
    XPLR_MQTTWIFI_STATE_CONFIG,              /**< running configuration. */
    XPLR_MQTTWIFI_STATE_REGISTER_EVENT,      /**< register event. */
    XPLR_MQTTWIFI_STATE_START,               /**< starting MQTT client. */
    XPLR_MQTTWIFI_STATE_RECONNECT,           /**< requested a reconnection. */
    XPLR_MQTTWIFI_STATE_CONNECT_WAIT,        /**< waiting for connection. */
    XPLR_MQTTWIFI_STATE_CONNECTED,           /**< connected to the broker. */
    XPLR_MQTTWIFI_STATE_SUBSCRIBED,          /**< subscribed to a topic. */
    XPLR_MQTTWIFI_STATE_WAIT,                /**< waiting for a step to finish. */
    XPLR_MQTTWIFI_STATE_DISCONNECT_REQUESTED,       /**< client requested a disconnect. */
    XPLR_MQTTWIFI_STATE_HARD_DISCONNECT_REQUESTED,  /**< client requested a hard disconnect. */
    XPLR_MQTTWIFI_STATE_DISCONNECTED_OK,     /**< MQTT client disconnected successfully. */
    XPLR_MQTTWIFI_STATE_UNINIT               /**< MQTT uninitialized phase, beginning of program. */
} xplrMqttWifiClientStates_t;

/**
 * QoS levels
 */
typedef enum {
    XPLR_MQTTWIFI_QOS_LVL_0,        /**< override QoS for all topics to 0. */
    XPLR_MQTTWIFI_QOS_LVL_1,        /**< override QoS for all topics to 1. */
    XPLR_MQTTWIFI_QOS_LVL_2         /**< override QoS for all topics to 2. */
} xplrMqttWifiQosLvl_t;

/**
 * Ring Buffer data item
 */
typedef struct xplrMqttWifiRingBuffItem_type {
    uint16_t dataLength;       /**< data length for current item. Items arrive in parts and
                                    data length show the current length of the item in the
                                    ringbuffer. It is used as a calculation/double checking as a
                                    sum. The sums of the parts lengths must match the totalDataLength
                                    to make sure we got all data from MQTT.*/
    uint16_t totalDataLength;  /**< total data length received from MQTT. It must match
                                    the sum of dataLength (see above) after all parts are processed.*/
    uint16_t part_no;          /**< number of this specific part. */
    uint16_t parts_total_no;   /**< total number of parts for this MQTT message. */
    char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN]; /**< topic we received data from. */
    char data[XPLR_MQTTWIFI_PAYLOAD_DATA_LEN];   /**< data buffer as chars/bytes. */
} xplrMqttWifiRingBuffItem_t;

/**
 * A complete MQTT message payload.
 * In case of multiple parts it will try to return
 * a complete payload.
 */
typedef struct xplrMqttWifiPayload_type {
    uint16_t dataLength;  /**< data length that the buffer contain, populated number of data. */
    uint16_t maxDataLength;  /**< max data length the buffer can accept. This is checked in
                                  the parsing function to make sure memcpy will not try
                                  to write beyond the buffer capabilities. */
    char *topic;     /**< topic we received from, account for the string NULL terminator. */
    char *data;      /**< the data itself. */
} xplrMqttWifiPayload_t;

/**
 * MQTT Final State Machine User Context Data
 * This is a data pack in which we can encapsulate and
 * pass to the MQTT callback and other helper functions.
 */
typedef struct xplrMqttWifiFsmUCD_type {
    xplrMqttWifiClientStates_t mqttFsm[2];  /**< current and previous FSM state. */
    bool isConnected;                       /**< shows if we are connected or disconnected. */
    RingbufHandle_t xRingbuffer;            /**< ring buffer handler. */
    uint16_t ringBufferSlotsNumber;         /**< how many items of type xplrMqttWifiRingBuffItem_t the
                                                 the ring buffer should hold. */
    xplrMqttWifiRingBuffItem_t prevItem;    /**< previous item parsed from ring buffer. */
    uint64_t lastActionTime;                /**< last time stamp when the FSM executed a step, useful
                                                 for timeout detections. */
} xplrMqttWifiFsmUCD_t;

/**
 * Contains FSM client items and their UCD
 */
typedef struct xplrMqttWifiClient_type {
    esp_mqtt_client_handle_t handler;  /**< client handler. */
    xplrMqttWifiFsmUCD_t ucd;          /**< user context data pack. */
} xplrMqttWifiClient_t;


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Init state machine to a known state
 * 
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiInitState(xplrMqttWifiClient_t *client);

/**
 * @brief Initializes a client inside the FSM client item data pack.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param cfg     MQTT client configuration.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiInitClient(xplrMqttWifiClient_t *client, esp_mqtt_client_config_t *cfg);

/**
 * @brief Sets MQTT client to first state, COnfig state, from which it can start
 * executing it's FSM. 
 * 
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiStart(xplrMqttWifiClient_t *client);

/**
 * @brief Sets ring buffer slots count
 * 
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param count   count of ringbuffer slots.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiSetRingbuffSlotsCount(xplrMqttWifiClient_t *client, uint8_t count);

/**
 * @brief Checks if MQTT client is connected or not.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        true if connected otherwise false
 */
bool xplrMqttWifiIsConnected(xplrMqttWifiClient_t *client);

/**
 * @brief Runs the MQTT FSM through states.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        zero on success or negative error code on
 *                failure.
 */
xplrMqttWifiError_t xplrMqttWifiFsm(xplrMqttWifiClient_t *client);

/**
 * @brief Subscribe to an array of ZTP Topics.
 * Useful when getting the whole ZTP reply with populated topics.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topics  a topics array to subscribe to.
 * @param qos     quality of service flag.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiSubscribeToTopicArrayZtp(xplrMqttWifiClient_t *client,
                                               xplrZtpStyleTopics_t *topics,
                                               xplrMqttWifiQosLvl_t qos);

/**
 * @brief Subscribe to an array of topics in string format.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topics  a topics array to subscribe to.
 * @param cnt     count of topics.
 * @param qos     quality of service flag.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiSubscribeToTopicArray(xplrMqttWifiClient_t *client, 
                                            char *topics[], 
                                            uint16_t cnt,
                                            xplrMqttWifiQosLvl_t qos);

/**
 * @brief Subscribe to single ZTP Topic.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topic   a ZTP topic to subscribe to.
 * @param qos     quality of service flag.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiSubscribeToTopicZtp(xplrMqttWifiClient_t *client, 
                                          xplrTopic *topic,
                                          xplrMqttWifiQosLvl_t qos);

/**
 * @brief Subscribe to a single MQTT topic using string.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topic   a topic string to subscribe to.
 * @param qos     quality of service flag.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiSubscribeToTopic(xplrMqttWifiClient_t *client, 
                                       char *topic,
                                       xplrMqttWifiQosLvl_t qos);

/**
 * @brief Unsubscribe from a ZTP Topics array.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topics  a ZTP topics array to unsubscribe from.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiUnsubscribeFromTopicArrayZtp(xplrMqttWifiClient_t *client,
                                                   xplrZtpStyleTopics_t *topics);

/**
 * @brief Unsubscribe from a string Topics array.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topics  a topics array to unsubscribe from.
 * @param cnt     zero on success or negative error code on
 *                failure.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiUnsubscribeFromTopicArray(xplrMqttWifiClient_t *client, 
                                                char *topics[],
                                                uint16_t cnt);

/**
 * @brief Unsubscribe from a single ZTP Topic.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topic   a single ZTP topic to unsubscribe from.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiUnsubscribeFromTopicZtp(xplrMqttWifiClient_t *client, xplrTopic *topic);

/**
 * @brief Unsubscribe from a single topic.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param topic   a single topic string to unsubscribe from.
 * @return        zero on success or negative error code on
 *                failure.
 */
esp_err_t xplrMqttWifiUnsubscribeFromTopic(xplrMqttWifiClient_t *client, char *topic);

/**
 * @brief Used to completely destroy the conenction.
 * After this function has been called you must call xplrMqttStarterInitClient
 * to re-initialize all aspects of the client (handlers, topics, ring buffers).
 *
 * @param client  a client item containing MQTT client handler UCD pack
 */
void xplrMqttWifiHardDisconnect(xplrMqttWifiClient_t *client);

/**
 * @brief Tries to reconnect to an MQTT broker.
 * If using xplrMqttWifiReconnect() you don't need to reconfigure the client.
 * This requires that the client has not been destroyed by calling xplrMqttWifiHardDisconnect().
 * In the latter case you will have to re configure and restart the client, see
 *
 * @param client  a client item containing MQTT client handler UCD pack
 */
void xplrMqttWifiReconnect(xplrMqttWifiClient_t *client);

/**
 * @brief Returns an MQTT payload to the user. The function
 * will try to return a complete message since it might be received in
 * segments from the MQTT client.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @param reply   contains the payload, data length and topic.
 * @return        zero on success, negative error code on failure or
 *                positive values on states.
 */
xplrMqttWifiGetItemError_t xplrMqttWifiReceiveItem(xplrMqttWifiClient_t *client,
                                                   xplrMqttWifiPayload_t *reply);

/**
 * @brief Returns current client's FSM state.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        zero on success, negative error code on failure or.
 *                positive values on states.
 */
xplrMqttWifiClientStates_t xplrMqttWifiGetCurrentState(xplrMqttWifiClient_t *client);

/**
 * @brief Returns previous client's FSM state.
 *
 * @param client  a client item containing MQTT client handler UCD pack.
 * @return        zero on success, negative error code on failure or
 *                positive values on states.
 */
xplrMqttWifiClientStates_t xplrMqttWifiGetPreviousState(xplrMqttWifiClient_t *client);

/**
 * @brief Tries to publish a message to the desired topic using
 * a ZTP style topic
 * 
 * @param client    a client item containing MQTT client handler UCD pack.
 * @param topic     a ZTP topic to publish to.
 * @param data      data to publish.
 * @param dataLength  data length.
 * @param qos       quality of service flag.
 * @param retain    retain flag.
 * @return          zero on success or negative error code on
 *                  failure.
 */
esp_err_t xplrMqttWifiPublishMsgZtp(xplrMqttWifiClient_t *client, 
                                    xplrTopic *topic, 
                                    char *data,
                                    uint64_t dataLength, 
                                    xplrMqttWifiQosLvl_t qos,
                                    int retain);

/**
 * @brief Tries to publish a message to the desired topic.
 *
 * @param client    a client item containing MQTT client handler UCD pack.
 * @param topic     topic name to publish to.
 * @param data      data to publish.
 * @param dataLength  data length.
 * @param qos       quality of service flag.
 * @param retain    retain flag.
 * @return          zero on success or negative error code on
 *                  failure.
 */
esp_err_t xplrMqttWifiPublishMsg(xplrMqttWifiClient_t *client, 
                                 char *topic, 
                                 char *data,
                                 uint64_t dataLength, 
                                 xplrMqttWifiQosLvl_t qos, 
                                 int retain);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_MQTT_H_ */

// End of file