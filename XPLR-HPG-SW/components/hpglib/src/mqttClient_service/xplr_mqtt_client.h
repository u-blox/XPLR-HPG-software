/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _XPLR_MQTT_CLIENT_H_
#define _XPLR_MQTT_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "xplr_mqtt_client_types.h"

/** @file
 * @brief This header file defines the general communication service API,
 * including com profile configuration, initialization and deinitialization
 * of corresponding modules and high level functions to be used by the application.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize MQTT API using user settings.
 *        Must be called before running xplrCellMqttFsmRun().
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @param  client       pointer to MQTT client struct. Provided by the user.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttInit(int8_t dvcProfile,
                                       int8_t clientId,
                                       xplrCell_mqtt_client_t *client);

/**
 * @brief De-initialize MQTT API.
 *        Must be called after running xplrCellMqttInit().
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to de-initialize.
 */
void xplrCellMqttDeInit(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Disconnect MQTT client from current broker.
 *        Checks if client is currently connected to a broker and disconnects.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to disconnect from broker.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttDisconnect(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Subscribe to topic.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to make a topic subscription.
 * @param  topic        pointer to topic instance to unsubscribe from.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttSubscribeToTopic(int8_t dvcProfile,
                                                   int8_t clientId,
                                                   xplrCell_mqtt_topic_t *topic);

/**
 * @brief Subscribe to topic list.
 *        List provided by the user during config.
 *        When xplrCellMqttInit() os called a subscription to this list performed.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttSubscribeToTopicList(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Unsubscribe from topic list.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopicList(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Unsubscribe from a registered topic.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @param  topic        pointer to topic instance to unsubscribe from.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopic(int8_t dvcProfile,
                                                       int8_t clientId,
                                                       xplrCell_mqtt_topic_t *topic);

/**
 * @brief Unsubscribe from topic list.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopicList(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Unsubscribe from a registered topic.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to config.
 * @param  topic        pointer to topic topic instance to unsubscribe from.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopic(int8_t dvcProfile,
                                                       int8_t clientId,
                                                       xplrCell_mqtt_topic_t *topic);

/**
 * @brief Get number of available messages.
 *        Corresponds to the total number of unread messages from a given client.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to check for messages.
 * @return      the number of unread messages or negative error code.
 */
int32_t xplrCellMqttGetNumofMsgAvailable(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Get number of available messages.
 *        Corresponds to the total number of unread messages from a given client.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to update.
 * @return      the number of unread messages or negative error code.
 */
int32_t xplrCellMqttUpdateTopicList(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Remove certificates stored in modules memory and delete user space.
 *        Can be used only if xplrCellMqttInit() has been called.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientIndex  MQTT client index to reset.
 * @return      XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellFactoryReset(int8_t dvcProfile, int8_t clientId);

/**
 * @brief FSM handling the MQTT client over cellular interface.
 *        xplrCellMqttInit() has to be called before running the FSM.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @return XPLR_CELL_MQTT_OK on success, XPLR_CELL_MQTT_ERROR otherwise.
 */
xplrCell_mqtt_error_t xplrCellMqttFsmRun(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrCellMqttInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops the logging of the http cell module
 *
 * @return  XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
*/
esp_err_t xplrCellMqttStopLogModule(void);

/**
 * @brief Function that allows the user to feed the MQTT watchdog externally
 *
 * @param dvcProfile   device profile id. Stored in xplrCom_cell_config_t
 * @param clientId     MQTT client index
*/
void xplrCellMqttFeedWatchdog(int8_t dvcProfile, int8_t clientId);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_MQTT_CLIENT_H_

// End of file