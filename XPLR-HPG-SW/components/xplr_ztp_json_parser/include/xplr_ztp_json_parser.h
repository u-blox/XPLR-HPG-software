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

#ifndef _XPLR_ZTP_JSON_PARSER_H_
#define _XPLR_ZTP_JSON_PARSER_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../hpglib/xplr_hpglib_cfg.h"
#include "cJSON.h"

/** @file
 * @brief This header file defines the ztp json parser API,
 * includes functions to parse all the necessary settings needed to 
 * establish a connection to the Thingstream PointPerfect broker.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * These lengths can be changed as needed. For now these lengths
 * are more than adequate to store the received data.
 * In the future these length might change.
 * According to the MQTT protocol specs
 * max XPLR_ZTP_JP_DESCRIPTION_LENGTH = approx 260MB
 * max XPLR_ZTP_JP_PATH_LENGTH = 65k chars
 * The values can be overwritten.
 */
#define XPLR_ZTP_JP_DESCRIPTION_LENGTH  (256U)
#define XPLR_ZTP_JP_PATH_LENGTH         (128U)

/**
 * Geographic region where Point Perfect is offered
 * eu = Europe
 * us = United States
 * kr = Korea
 */
#define XPLR_ZTP_REGION_EU "eu"
#define XPLR_ZTP_REGION_US "us"
#define XPLR_ZTP_REGION_KR "kr"

/**
 * Key words to parse specific topic
 * You can read more about each of them in the Thingstream documentation
 *
 * NOTE: XPLR_ZTP_TOPIC_CORRECTION_DATA_ID uses key "corr_topic" which does
 * not really appear on any of the JSON topics but is rather used to get the
 * topic for correction data. Due to the way the topics are constructed
 * i.e. /pp/ip/eu and e.g. /pp/ip/eu/hpac it is not possible to parse the topic
 * since the substring is not unique (in comparison with "hpac").
 * Instead we parse the string and check that if *region* is present in the string
 * then this substring must be at the end.
 */
#define XPLR_ZTP_TOPIC_KEY_DISTRIBUTION_ID       "0236"
#define XPLR_ZTP_TOPIC_ASSIST_NOW_ID             "mga"
#define XPLR_ZTP_TOPIC_CORRECTION_DATA_ID        "corr_topic"
#define XPLR_ZTP_TOPIC_GEOGRAPHIC_AREA_ID        "gad"
#define XPLR_ZTP_TOPIC_ATMOSPHERE_CORRECTION_ID  "hpac"
#define XPLR_ZTP_TOPIC_ORBIT_CLOCK_BIAS_ID       "ocb"
#define XPLR_ZTP_TOPIC_CLOCK_ID                  "clk"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/**
 * Struct to parse subscription topics.
 * Follows the same naming convention as in the
 * Thingstream web API.
 */
typedef struct xplrTopic_type {
    char description[XPLR_ZTP_JP_DESCRIPTION_LENGTH];  /**< topic description string. */
    char path[XPLR_ZTP_JP_PATH_LENGTH];                /**< topic to subscribe to, called path in received json. */
} xplrTopic;

/**
 * Contains all the topics from ZTP.
 */
typedef struct xplrSubscriptionTopics_type {
    xplrTopic *topic;         /**< pointer to an array of topics. */
    uint16_t maxCount;        /**< topic array count. */
    uint16_t populatedCount;  /**< count of how many topics we populated. */
} xplrZtpStyleTopics_t;

/**
 * Error codes to return when using JSON parsing functions.
 * XPLR_JSON_PARSER_OK should terminate on 0.
 */
typedef enum {
    XPLR_JSON_PARSER_NO_ITEM = -5,   /**< could not find an item with that key. */
    XPLR_JSON_PARSER_WRONG_TYPE,     /**< the item requested is not the expected type. */
    XPLR_JSON_PARSER_OVERFLOW,       /**< the provided buffer is not big enough. */
    XPLR_JSON_PARSER_NULL_PTR_ERROR, /**< the pointer passed is null. */
    XPLR_JSON_PARSER_ERROR,          /**< cJSON parsing error. */
    XPLR_JSON_PARSER_OK = 0          /**< parsed item OK. */
} xplrJsonParserStatus;

/**
 * Struct to parse a single dynamic key.
 * Follows the same naming convention as in the
 * Thingstream web API.
 * Key length is standard.
 */
typedef struct xplrDynamicKeyUnit_type {
    uint64_t duration;   /**< duration of key. */
    uint64_t start;      /**< starting time of key. */
    char     value[33];  /**< key value, length is 32 chars + 1 for termination. */
} xplrDynamicKeyUnit_t;

/**
 * Struct to parse dynamic keys.
 * Follows the same naming convention as in the
 * Thingstream web API.
 */
typedef struct xplrDynamicKeys_type {
    xplrDynamicKeyUnit_t next;      /**< next dynamic key. */
    xplrDynamicKeyUnit_t current;   /**< current dynamic key. */
} xplrDynamicKeys_t;

/**
 * Used to get the desired topic from the ZTP topics array
 * returned by function xplrJsonZtpGetRequiredTopicsByRegion
 */
typedef enum {
    XPLR_JSON_PARSER_REQTOPIC_KEYDISTRIB = 0,
    XPLR_JSON_PARSER_REQTOPIC_CORRECDATA
} xplrJsonParserReqTopicsId;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Returns the MQTT Certificate
 *
 * @param json        JSON object.
 * @param res         the certificate value.
 * @param buffLength  maximum buffer length.
 * @return            zero on success or negative error
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetMqttCertificate(const cJSON *const json, 
                                                   char *res,
                                                   uint16_t buffLength);

/**
 * @brief Returns the private key
 *
 * @param json        JSON object.
 * @param res         the private key value.
 * @param buffLength  maximum buffer length.
 * @return            zero on success or negative error
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetPrivateKey(const cJSON *const json, 
                                              char *res,
                                              uint16_t buffLength);

/**
 * @brief Returns the unique MQTT client ID
 *
 * @param json        JSON object.
 * @param res         the client ID value.
 * @param buffLength  maximum buffer length.
 * @return            zero on success or negative error
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetMqttClientId(const cJSON *const json, 
                                                char *res,
                                                uint16_t buffLength);

/**
 * @brief Returns rotating key title
 *
 * @param json        JSON object.
 * @param res         rotating key value.
 * @param buffLength  maximum buffer length.
 * @return            zero on success or negative error
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetRotatingKeyTitle(const cJSON *const json, 
                                                    char *res,
                                                    uint16_t buffLength);

/**
 * @brief Returns subscriptions title
 *
 * @param json        JSON object.
 * @param res         subscription key value.
 * @param buffLength  maximum buffer length.
 * @return            zero on success or negative error
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetSubscriptionsTitle(const cJSON *const json, 
                                                      char *res,
                                                      uint16_t buffLength);

/**
 * @brief Returns topic for SPARTN key distribution
 *
 * @param json  JSON object.
 * @param res   subscription key value.
 * @return      zero on success or negative error
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetKeyDistributionTopic(const cJSON *const json, xplrTopic *res);

/**
 * @brief Returns Assist now topic
 *
 * @param json  JSON object.
 * @param res   subscription key value.
 * @return      zero on success or negative error
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetAssistNowTopic(const cJSON *const json, xplrTopic *res);

/**
 * @brief Returns correction topic
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetCorrectionTopic(const cJSON *const json, 
                                                   xplrTopic *res,
                                                   char *region);

/**
 * @brief Returns Geographic Area Definition topic
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetGeographicAreaDefinitionTopic(const cJSON *const json,
                                                                 xplrTopic *res,
                                                                 char *region);

/**
 * @brief Returns High Precision Atmosphere Correction topic
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetAtmosphereCorrectionTopic(const cJSON *const json, 
                                                             xplrTopic *res,
                                                             char *region);

/**
 * @brief Returns Orbits Clock Bias topic
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetOrbitsClockBiasTopic(const cJSON *const json, 
                                                        xplrTopic *res,
                                                        char *region);

/**
 * @brief Returns Clock topic
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetClockTopic(const cJSON *const json, xplrTopic *res, char *region);

/**
 * @brief Adds a topic to the array topic.
 * The array will be returned to the user to pass directly to the
 * MQTT client.
 *
 * @param array  array of topics.
 * @param topic  topic to add to the array.
 * @return       zero on success or negative error
 *               code on failure.
 */
xplrJsonParserStatus xplrJsonZtpAddTopicToArray(xplrZtpStyleTopics_t *array, xplrTopic *topic);

/**
 * @brief Will return all topics that belong to specific region
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetAllTopicsByRegion(const cJSON *const json,
                                                     xplrZtpStyleTopics_t *res,
                                                     char *region);

/**
 * @brief Returns specific topic that belongs to a region
 *
 * @param json    JSON object.
 * @param res     subscription key value.
 * @param region  region from which to get topic.
 * @return        zero on success or negative error
 *                code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetRequiredTopicsByRegion(const cJSON *const json,
                                                          xplrZtpStyleTopics_t *res, char *region);

/**
 * @brief Returns the subscription topics to use
 * with MQTT. Items stored are SubscriptionsTopics_t.
 *
 * @param json  JSON object
 * @param res   subscription topics in an array.
 *
 * @return      zero on success or negative error
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetAllTopics(const cJSON *const json,
                                             xplrZtpStyleTopics_t *res);

/**
 * @brief Checks if LBAND is supported.
 *
 * @param json  JSON object.
 * @param res   true or false result for LBAND support.
 * @return      zero on success or negative error.
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpSupportsLband(const cJSON *const json, bool *res);

/**
 * @brief Returns broker host address.
 *
 * @param json        JSON object
 * @param res         broker host address.
 * @param buffLength  maximum buffer length
 * @return            zero on success or negative error.
 *                    code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetBrokerHost(const cJSON *const json, 
                                              char *res,
                                              uint16_t buffLength);

/**
 * @brief Checks if MQTT is supported.
 *
 * @param json  JSON object.
 * @param res   true or false result for MQTT support.
 * @return      zero on success or negative error
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpSupportsMqtt(const cJSON *const json, bool *res);

/* ----------------------------------------------------------------
 * DEPRECATED
 * -------------------------------------------------------------- */

/**
 * @brief Returns dynamic keys values, current and next.
 *
 * @param json  JSON object
 * @param res   A struct containing the keys.
 * @return      zero on success or negative error
 *              code on failure.
 */
xplrJsonParserStatus xplrJsonZtpGetDynamicKeys(const cJSON *const json, xplrDynamicKeys_t *res);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_ZTP_JSON_PARSER_H_ */

// End of file