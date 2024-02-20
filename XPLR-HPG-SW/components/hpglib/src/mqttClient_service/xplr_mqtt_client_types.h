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

#ifndef _XPLR_MQTT_CLIENT_TYPES_H_
#define _XPLR_MQTT_CLIENT_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"
#include "./../../../../components/hpglib/src/nvs_service/xplr_nvs.h"
#include "xplr_log.h"

/** @file
 * @brief This header file defines the types used in mqtt client service API,
 * Types include status, state, config enums and structs that are exposed to the user
 * providing an easy to use and configurable mqtt client library.
 * The API builds on top of ubxlib, implementing some high level logic that can be used
 * in common IoT scenarios.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to hpgMqtt module. */
typedef enum {
    XPLR_CELL_MQTT_ERROR = -1,    /**< process returned with errors. */
    XPLR_CELL_MQTT_OK,            /**< indicates success of returning process. */
    XPLR_CELL_MQTT_BUSY           /**< returning process currently busy. */
} xplrCell_mqtt_error_t;

/** Certification methods for logging in the MQTT broker. */
typedef enum {
    XPLR_CELL_MQTT_CERT_METHOD_NONE = 0,    /**< register to an open broker. */
    XPLR_CELL_MQTT_CERT_METHOD_TLS,         /**< register to a broker using tls certificates. */
    XPLR_CELL_MQTT_CERT_METHOD_PWD          /**< register to a broker using username and password. */
} xplrCell_mqtt_cert_method_t;

/** Region selection for subscribing to related PointPerfect MQTT topics. */
typedef enum {
    XPLR_CELL_MQTT_PP_REGION_NONE = -1,     /**< invalid region for PointPerfect service. */
    XPLR_CELL_MQTT_PP_REGION_EU,            /**< Europe region for PointPerfect service. */
    XPLR_CELL_MQTT_PP_REGION_US,            /**< USA region for PointPerfect service. */
    XPLR_CELL_MQTT_PP_REGION_KR             /**< south Korea region for PointPerfect service. */
} xplrCell_mqtt_pp_region_t;

/** MQTT configuration struct for setting up deviceSettings.
 * Struct to be provided by the user via xplrCellMqttInit().
*/
typedef struct xplrCell_mqtt_config_type {
    const char          *brokerAddress;     /**< MQTT Broker Address. */
    uCellMqttQos_t      qos;                /**< MQTT QoS. */
    bool                useFlexService;     /**< weather flex service to be used. */
    bool                retainMsg;          /**< Whether or not message is retained on disconnect. */
    uint16_t            keepAliveTime;      /**< Configure "keep-alive" functionality with the broker.
                                                 If enabled (>0) then value should be less than inactivityTimeout.
                                                 Value in seconds. */
    uint16_t            inactivityTimeout;  /**< Configure inactivity timeout.
                                                 Module to be disconnected when inactive for
                                                 inactivityTimeout value in seconds. */
} xplrCell_mqtt_config_t;

/** Broker credentials configuration struct.
 * Struct to be provided by the user via xplrCellMqttInit().
*/
typedef struct xplrCell_mqtt_credentials_type {
    const char          *name;      /**< Broker name. */
    const char          *user;      /**< User name to use when connecting to broker. */
    const char          *password;  /**< Password to use when connecting to broker. */
    const char          *token;     /**< Device ID / Token to use. */
    const char          *rootCa;    /**< Root Certificate to use. Stored in cell's flash. */
    const char          *rootCaName; /**< Root Certificate name to use. */
    const char          *rootCaHash;
    const char          *cert;      /**< Certificate to use. Stored in cell's flash. */
    const char          *certName;  /**< Certificate name to use. */
    const char          *certHash;
    const char          *key;       /**< Key to use. Stored in cell's flash. */
    const char          *keyName;   /**< Key name to use. */
    const char          *keyHash;
    xplrCell_mqtt_cert_method_t    registerMethod; /**< registration method to use. */
} xplrCell_mqtt_credentials_t;

/** States describing the cellular MQTT client process. */
typedef enum {
    XPLR_CELL_MQTT_CLIENT_FSM_TIMEOUT = -3,
    XPLR_CELL_MQTT_CLIENT_FSM_ERROR,
    XPLR_CELL_MQTT_CLIENT_FSM_BUSY,
    XPLR_CELL_MQTT_CLIENT_FSM_CHECK_MODULE_CREDENTIALS = 0,
    XPLR_CELL_MQTT_CLIENT_FSM_WRITE_MODULE_CREDENTIALS,
    XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE,
    XPLR_CELL_MQTT_CLIENT_FSM_READY
} xplrCell_mqtt_client_fsm_t;

// *INDENT-OFF*
typedef struct xplrCell_mqtt_topic_type {
    uint16_t        index;
    const char      *name;
    char            *rxBuffer;
    uint32_t        rxBufferSize;
    uint32_t        msgSize;
    bool            msgAvailable;   /**< indicates if a message is available to read. */
} xplrCell_mqtt_topic_t;
// *INDENT-ON*

/** MQTT NVS struct.
 * contains data to be stored in NVS under namespace <id>
*/
typedef struct xplrCell_mqtt_nvs_type {
    xplrNvs_t   nvs;            /**< nvs module to handle operations */
    char        id[15];         /**< nvs namespace */
    char        md5RootCa[33];  /**< md5 hash of rootCa stored */
    char        md5PpCert[33];  /**< md5 hash of client cert stored */
    char        md5PpKey[33];   /**< md5 hash of client key stored */
} xplrCell_mqtt_nvs_t;

/** MQTT client struct. */
typedef struct xplrCell_mqtt_client_type {
    int8_t                         id;
    xplrCell_mqtt_nvs_t            storage;        /**< storage module for provisioning settings */
    xplrCell_mqtt_config_t         settings;
    xplrCell_mqtt_credentials_t    credentials;
    uint8_t                        numOfTopics;    /**< Number of topics in list. */
    xplrCell_mqtt_topic_t          *topicList;     /**< List of topics to subscribe to. */
    xplrCell_mqtt_client_fsm_t     fsm[2];         /**< MQTT fsm array.
                                                         element 0 holds most current state.
                                                         d element 1 holds previous state */
    void (*msgReceived)                             /**< function pointer to msg received callback. */
    (int32_t numUnread, void *received);
    void (*disconnected)                            /**< function pointer to client disconnect callback. */
    (int32_t status, void *param);
    int64_t                         lastActionTime; /**< Last action time. Used to trigger watchdog */
    bool                            enableWdg;      /**< Option to enable module's watchdog timer */
} xplrCell_mqtt_client_t;

#ifdef __cplusplus
}
#endif
#endif // _XPLR_MQTT_CLIENT_TYPES_H_

// End of file