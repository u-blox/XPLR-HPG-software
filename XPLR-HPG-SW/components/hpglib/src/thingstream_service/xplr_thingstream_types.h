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

#ifndef XPLR_THINGSTREAM_TYPES_H_
#define XPLR_THINGSTREAM_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>

/** @file
 * @brief This header file defines the types used in thingstream service API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLR_THINGSTREAM_URL_SIZE_MAX           (128U)
#define XPLR_THINGSTREAM_DEVICEUID_SIZE         (11U)
#define XPLR_THINGSTREAM_CERT_SIZE_MAX          (2 * 1024)
#define XPLR_THINGSTREAM_PP_TOKEN_SIZE          (37U)
#define XPLR_THINGSTREAM_PP_DEVICEID_SIZE       (37U)
#define XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX    (15U)
#define XPLR_THINGSTREAM_PP_TOPIC_NAME_SIZE_MAX (256U)
#define XPLR_THINGSTREAM_PP_TOPIC_PATH_SIZE_MAX (128U)
#define XPLR_THINGSTREAM_PP_DKEY_SIZE           (64U)

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to xplrThingstream component. */
typedef enum {
    XPLR_THINGSTREAM_ERROR = -1,    /**< process returned with errors. */
    XPLR_THINGSTREAM_OK,            /**< indicates success of returning process. */
} xplr_thingstream_error_t;

/** Thingstream API message types. */
typedef enum {
    XPLR_THINGSTREAM_API_INVALID = -1,    /**< invalid or not supported API command. */
    XPLR_THINGSTREAM_API_LOCATION_ZTP,    /**< location service ztp post. */
} xplr_thingstream_api_t;

/** Thingstream Location Service region types. */
typedef enum {
    XPLR_THINGSTREAM_PP_REGION_INVALID = -1,    /**< invalid or not supported supported. */
    XPLR_THINGSTREAM_PP_REGION_EU,              /**< Europe region. */
    XPLR_THINGSTREAM_PP_REGION_US,              /**< USA region. */
    XPLR_THINGSTREAM_PP_REGION_KR,              /**< Korea region. */
    XPLR_THINGSTREAM_PP_REGION_ALL              /**< IP and LBAND topics of any region available. */
} xplr_thingstream_pp_region_t;

/** Thingstream Subscription Plan types */
typedef enum {
    XPLR_THINGSTREAM_PP_PLAN_INVALID = -1,       /**< invalid or not supported supported. */
    XPLR_THINGSTREAM_PP_PLAN_IP,                 /**< Point Perfect IP data stream*/
    XPLR_THINGSTREAM_PP_PLAN_LBAND,              /**< Point Perfect L-band data stream*/
    XPLR_THINGSTREAM_PP_PLAN_IPLBAND             /**< Point Perfect L-band + IP data streams*/
} xplr_thingstream_pp_plan_t;

/** Thingstream Location Service region types. */
typedef enum {
    XPLR_THINGSTREAM_PP_SERVER_INVALID = -1,    /**< invalid or not supported supported. */
    XPLR_THINGSTREAM_PP_SERVER_ADDRESS,         /**< point perfect broker address. */
    XPLR_THINGSTREAM_PP_SERVER_CERT,            /**< point perfect client certificate. */
    XPLR_THINGSTREAM_PP_SERVER_KEY,             /**< point perfect client private key. */
    XPLR_THINGSTREAM_PP_SERVER_ID               /**< point perfect client id. */
} xplr_thingstream_pp_serverInfo_type_t;

/** Thingstream Location Service region types. */
typedef enum {
    XPLR_THINGSTREAM_PP_TOPIC_INVALID = -1,    /**< invalid or not supported supported. */
    XPLR_THINGSTREAM_PP_TOPIC_KEYS_DIST,       /**< keys distribution topic. */
    XPLR_THINGSTREAM_PP_TOPIC_ASSIST_NOW,      /**< assist now topic. */
    XPLR_THINGSTREAM_PP_TOPIC_CORRECTION_DATA, /**< correction data topic. */
    XPLR_THINGSTREAM_PP_TOPIC_GAD,             /**< geographic area definition topic. */
    XPLR_THINGSTREAM_PP_TOPIC_HPAC,            /**< atmospheric correction topic. */
    XPLR_THINGSTREAM_PP_TOPIC_OCB,             /**< orbital clock bias topic. */
    XPLR_THINGSTREAM_PP_TOPIC_CLK,             /**< clock topic. */
    XPLR_THINGSTREAM_PP_TOPIC_FREQ,            /**< frequencies topic. */
    XPLR_THINGSTREAM_PP_TOPIC_ALL_EU,          /**< all eu related topics. */
    XPLR_THINGSTREAM_PP_TOPIC_ALL_US,          /**< all us related topics. */
    XPLR_THINGSTREAM_PP_TOPIC_ALL              /**< all us related topics. */
} xplr_thingstream_pp_topic_type_t;

typedef struct __attribute__((packed)) xplr_thingstream_server_settings_type {
    char serverUrl[XPLR_THINGSTREAM_URL_SIZE_MAX];
    char deviceId[XPLR_THINGSTREAM_DEVICEUID_SIZE];
    char ppToken[XPLR_THINGSTREAM_PP_TOKEN_SIZE];
    char rootCa[XPLR_THINGSTREAM_CERT_SIZE_MAX];
} xplr_thingstream_server_settings_t;

/**
 * Struct to describe a thingstream topic.
 * Follows the same naming convention as in the
 * Thingstream web API.
 */
typedef struct __attribute__((packed)) xplr_thingstream_pp_topic_type {
    char description[XPLR_THINGSTREAM_PP_TOPIC_NAME_SIZE_MAX];  /**< topic description string. */
    char path[XPLR_THINGSTREAM_PP_TOPIC_PATH_SIZE_MAX];         /**< topic to subscribe to, called path in received json. */
    uint8_t qos;                                                /**< quality of service level. */
} xplr_thingstream_pp_topic_t;

/**
 * Struct to describe a dynamic key.
 * Follows the same naming convention as in the
 * Thingstream web API.
 */
typedef struct __attribute__((packed)) xplr_thingstream_pp_dKeyUnit_type {
    uint64_t duration;                              /**< duration of key. */
    uint64_t start;                                 /**< starting time of key. */
    char     value[XPLR_THINGSTREAM_PP_DKEY_SIZE];  /**< key value. */
} xplr_thingstream_pp_dKeyUnit_t;

/**
 * Struct to hold dynamic keys.
 * Follows the same naming convention as in the
 * Thingstream web API.
 */
typedef struct xplr_thingstream_pp_dKeys_type {
    xplr_thingstream_pp_dKeyUnit_t next;      /**< next dynamic key. */
    xplr_thingstream_pp_dKeyUnit_t current;   /**< current dynamic key. */
} xplr_thingstream_pp_dKeys_t;

/**
 * Struct to hold the User's subscription plan and region
 * for MQTT topic provisioning
*/
typedef struct xplr_thingstream_pp_sub_type {
    xplr_thingstream_pp_region_t region;      /**< thingstream location region type */
    xplr_thingstream_pp_plan_t plan;          /**< thingstream subscription plan type*/
} xplr_thingstream_pp_sub_t;

typedef struct __attribute__((packed)) xplr_thingstream_pp_settings_type {
    char urlPath[XPLR_THINGSTREAM_URL_SIZE_MAX];        /**< url used to get the root CA. */
    char brokerAddress[XPLR_THINGSTREAM_URL_SIZE_MAX];  /**< MQTT broker address. */
    uint16_t brokerPort;                                /**< MQTT broker port. */
    char deviceId[XPLR_THINGSTREAM_PP_DEVICEID_SIZE];   /**< device ID to use in MQTT broker. */
    char clientKey[XPLR_THINGSTREAM_CERT_SIZE_MAX];     /**< client private key used in pp service. */
    char clientCert[XPLR_THINGSTREAM_CERT_SIZE_MAX];    /**< client certificate used in pp service. */
    bool mqttSupported;
    bool lbandSupported;
    xplr_thingstream_pp_dKeys_t dynamicKeys;
    xplr_thingstream_pp_topic_t topicList[XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX];
    size_t numOfTopics;
} xplr_thingstream_pp_settings_t;

typedef struct __attribute__((packed)) xplr_thingstream_type {
    xplr_thingstream_server_settings_t server;
    xplr_thingstream_pp_settings_t pointPerfect;
} xplr_thingstream_t;

#ifdef __cplusplus
}
#endif
#endif // XPLR_THINGSTREAM_TYPES_H_

// End of file