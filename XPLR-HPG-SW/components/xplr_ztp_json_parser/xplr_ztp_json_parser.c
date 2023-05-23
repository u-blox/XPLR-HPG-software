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

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "xplr_ztp_json_parser.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRZTPJSONPARSER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRZTPJSONPARSER_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrZtpJsonParser", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRZTPJSONPARSER_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplrJsonParserStatus xplrJsonZtpGetString(const cJSON *const json, const char *name,
                                                 char *res,
                                                 uint16_t length);
static xplrJsonParserStatus xplrJsonZtpGetBool(const cJSON *const json, const char *name,
                                               bool *res);
static xplrJsonParserStatus xplrJsonZtpGetSpecificTopic(const cJSON *const json, xplrTopic *res,
                                                        char *region,
                                                        char *topicName);

static xplrJsonParserStatus xplrJsonCheckJsonPtrForNull(const cJSON *const json);
static xplrJsonParserStatus xplrJsonCheckSubTopicsPtrForNull(xplrZtpStyleTopics_t *res);
static xplrJsonParserStatus xplrJsonCheckSubTopicPtrForNull(xplrTopic *topic);
static xplrJsonParserStatus xplrJsonCheckCharPtrForNull(char *ptr, char *varName);
static xplrJsonParserStatus xplrJsonCheckDynamicKeysPtrForNull(xplrDynamicKeys_t *dynKeys);
static xplrJsonParserStatus xplrJsonCheckDynamicKeyPtrForNull(xplrDynamicKeyUnit_t *dynKey);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

xplrJsonParserStatus xplrJsonZtpGetMqttCertificate(const cJSON *const json, char *res,
                                                   uint16_t buffLength)
{
    return xplrJsonZtpGetString(json, "certificate", res, buffLength);
}

xplrJsonParserStatus xplrJsonZtpGetPrivateKey(const cJSON *const json, char *res,
                                              uint16_t buffLength)
{
    return xplrJsonZtpGetString(json, "privatekey", res, buffLength);
}

xplrJsonParserStatus xplrJsonZtpGetMqttClientId(const cJSON *const json, char *res,
                                                uint16_t buffLength)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetString(json, "clientid", res, buffLength);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Rotating Key Title\"!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetRotatingKeyTitle(const cJSON *const json, char *res,
                                                    uint16_t buffLength)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetString(json, "keytitle", res, buffLength);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Rotating Key Title\"!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetSubscriptionsTitle(const cJSON *const json, char *res,
                                                      uint16_t buffLength)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetString(json, "subscriptionsTitle", res, buffLength);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Subscriptions Title\"!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetKeyDistributionTopic(const cJSON *const json, xplrTopic *res)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, NULL,
                                                           XPLR_ZTP_TOPIC_KEY_DISTRIBUTION_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Key Distribution\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetAssistNowTopic(const cJSON *const json, xplrTopic *res)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, NULL,
                                                           XPLR_ZTP_TOPIC_ASSIST_NOW_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Assist Now\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetCorrectionTopic(const cJSON *const json, xplrTopic *res,
                                                   char *region)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, region,
                                                           XPLR_ZTP_TOPIC_CORRECTION_DATA_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Correction\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetGeographicAreaDefinitionTopic(const cJSON *const json,
                                                                 xplrTopic *res,
                                                                 char *region)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, region,
                                                           XPLR_ZTP_TOPIC_GEOGRAPHIC_AREA_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Geographic Area Definition\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetAtmosphereCorrectionTopic(const cJSON *const json, xplrTopic *res,
                                                             char *region)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, region,
                                                           XPLR_ZTP_TOPIC_ATMOSPHERE_CORRECTION_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"High Precision Atmospheric Correction\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetOrbitsClockBiasTopic(const cJSON *const json, xplrTopic *res,
                                                        char *region)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, region,
                                                           XPLR_ZTP_TOPIC_ORBIT_CLOCK_BIAS_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Orbits Clock Bias\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetClockTopic(const cJSON *const json, xplrTopic *res, char *region)
{
    xplrJsonParserStatus ret = xplrJsonZtpGetSpecificTopic(json, res, region, XPLR_ZTP_TOPIC_CLOCK_ID);

    if (ret != XPLR_JSON_PARSER_OK) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not get \"Clock\" topic!");
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpGetRequiredTopicsByRegion(const cJSON *const json,
                                                          xplrZtpStyleTopics_t *res, char *region)
{
    xplrJsonParserStatus ret;
    xplrTopic tmpTopic;

    if (json == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "JSON pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (res == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Result topic pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (region == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Region pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    ret = xplrJsonZtpGetKeyDistributionTopic(json, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret  = xplrJsonZtpGetCorrectionTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    return XPLR_JSON_PARSER_OK;
}

xplrJsonParserStatus xplrJsonZtpGetAllTopicsByRegion(const cJSON *const json,
                                                     xplrZtpStyleTopics_t *res,
                                                     char *region)
{
    xplrJsonParserStatus ret;
    xplrTopic tmpTopic;

    if (json == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "JSON pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (res == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Result topic pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (region == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Region pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    ret = xplrJsonZtpGetKeyDistributionTopic(json, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonZtpGetAssistNowTopic(json, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonZtpGetCorrectionTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonZtpGetGeographicAreaDefinitionTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret  = xplrJsonZtpGetAtmosphereCorrectionTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret  = xplrJsonZtpGetOrbitsClockBiasTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret  = xplrJsonZtpGetClockTopic(json, &tmpTopic, region);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }
    ret = xplrJsonZtpAddTopicToArray(res, &tmpTopic);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    return XPLR_JSON_PARSER_OK;
}

xplrJsonParserStatus xplrJsonZtpAddTopicToArray(xplrZtpStyleTopics_t *array, xplrTopic *topic)
{
    if (array == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Array pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (topic == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Topic pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (array->populatedCount < array->maxCount) {
        memcpy(&array->topic[array->populatedCount], topic, sizeof(xplrTopic));
        array->populatedCount += 1;
        return XPLR_JSON_PARSER_OK;
    }

    XPLRZTPJSONPARSER_CONSOLE(E, "Failed adding topic to array! Buffer overflow!");
    return XPLR_JSON_PARSER_OVERFLOW;
}

xplrJsonParserStatus xplrJsonZtpGetAllTopics(const cJSON *const json, xplrZtpStyleTopics_t *res)
{
    xplrJsonParserStatus ret;

    cJSON *array     = NULL;
    cJSON *array_obj = NULL;
    uint16_t array_size;
    uint8_t i;

    ret = xplrJsonCheckJsonPtrForNull(json);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckSubTopicsPtrForNull(res);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if (cJSON_HasObjectItem(json, "subscriptions")) {
        array = cJSON_GetObjectItem(json, "subscriptions");
        array_size = cJSON_GetArraySize(array);

        if (array_size > res->maxCount) {
            return XPLR_JSON_PARSER_OVERFLOW;
        }

        res->populatedCount = 0;

        for (i = 0; i < array_size; i++) {
            array_obj = cJSON_GetArrayItem(array, i);

            if (cJSON_HasObjectItem(array_obj, "description")) {
                ret = xplrJsonZtpGetString(array_obj, "description", res->topic[i].description,
                                           ELEMENTCNT(res->topic[i].description));
                if (ret != XPLR_JSON_PARSER_OK) {
                    return ret;
                }

                ret = xplrJsonZtpGetString(array_obj, "path", res->topic[i].path, ELEMENTCNT(res->topic[i].path));
                if (ret != XPLR_JSON_PARSER_OK) {
                    return ret;
                }
            } else {
                ret = xplrJsonZtpGetString(array_obj, "path", res->topic[i].path, ELEMENTCNT(res->topic[i].path));
                if (ret != XPLR_JSON_PARSER_OK) {
                    return ret;
                }

                if (strstr(res->topic[i].path, XPLR_ZTP_REGION_US) != NULL) {
                    strcpy(res->topic[i].description, "US concat");
                } else if (strstr(res->topic[i].path, XPLR_ZTP_REGION_EU) != NULL) {
                    strcpy(res->topic[i].description, "EU concat");
                } else if (strstr(res->topic[i].path, XPLR_ZTP_REGION_KR) != NULL) {
                    strcpy(res->topic[i].description, "KR concat");
                }
            }

            res->populatedCount += 1;
        }

        return XPLR_JSON_PARSER_OK;
    } else {
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    return XPLR_JSON_PARSER_ERROR;
}

xplrJsonParserStatus xplrJsonZtpSupportsLband(const cJSON *const json, bool *res)
{
    return xplrJsonZtpGetBool(json, "supportsLband", res);
}

xplrJsonParserStatus xplrJsonZtpGetBrokerHost(const cJSON *const json, char *res,
                                              uint16_t buffLength)
{
    xplrJsonParserStatus ret;

    strncpy(res, "mqtts://", strlen("mqtts://") + 1);
    ret = xplrJsonZtpGetString(json, "brokerHost", res + strlen("mqtts://"), buffLength);
    if (ret != XPLR_JSON_PARSER_OK && ret != XPLR_JSON_PARSER_NULL_PTR_ERROR) {
        res[0] = 0;
    }

    return ret;
}

xplrJsonParserStatus xplrJsonZtpSupportsMqtt(const cJSON *const json, bool *res)
{
    return xplrJsonZtpGetBool(json, "supportsMqtt", res);
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/*
 * Parses strings with specific key if available
 */
static xplrJsonParserStatus xplrJsonZtpGetString(const cJSON *const json, const char *name,
                                                 char *res,
                                                 uint16_t buffLength)
{
    cJSON *current_element = NULL;
    xplrJsonParserStatus ret;

    ret = xplrJsonCheckJsonPtrForNull(json);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckCharPtrForNull((char *)name, "Name");
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckCharPtrForNull(res, "Result");
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if (cJSON_HasObjectItem(json, name)) {
        current_element = cJSON_GetObjectItem(json, name);
        if (cJSON_IsString(current_element)) {
            if (strlen(current_element->valuestring) < buffLength) {
                strcpy(res, current_element->valuestring);
                return XPLR_JSON_PARSER_OK;
            } else {
                XPLRZTPJSONPARSER_CONSOLE(E, "Result buffer not big enough");
                return XPLR_JSON_PARSER_OVERFLOW;
            }
        } else {
            XPLRZTPJSONPARSER_CONSOLE(E, "Requested item \"%s\" is not a of type \"String\"", name);
            return XPLR_JSON_PARSER_WRONG_TYPE;
        }
    } else {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not find item: \"%s\" in JSON!", name);
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    XPLRZTPJSONPARSER_CONSOLE(E, "Get string failed!");
    return XPLR_JSON_PARSER_ERROR;
}

/*
 * Parses and returns a boolean from JSON
 */
static xplrJsonParserStatus xplrJsonZtpGetBool(const cJSON *const json, const char *name, bool *res)
{
    cJSON *current_element = NULL;

    if (json == NULL || name == NULL || res == NULL) {
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }

    if (cJSON_HasObjectItem(json, name)) {
        current_element = cJSON_GetObjectItem(json, name);
        if (cJSON_IsBool(current_element)) {
            if (cJSON_IsTrue(current_element)) {
                *res = 1;
            } else {
                *res = 0;
            }
            return XPLR_JSON_PARSER_OK;
        } else {
            XPLRZTPJSONPARSER_CONSOLE(E, "Requested item \"%s\" is not a of type \"Boolean\"", name);
            return XPLR_JSON_PARSER_WRONG_TYPE;
        }
    } else {
        XPLRZTPJSONPARSER_CONSOLE(E, "Could not find item: \"%s\" in JSON!", name);
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    XPLRZTPJSONPARSER_CONSOLE(E, "Get boolean failed!");
    return XPLR_JSON_PARSER_ERROR;
}

static xplrJsonParserStatus xplrJsonZtpGetSpecificTopic(const cJSON *const json, xplrTopic *res,
                                                        char *region,
                                                        char *topicName)
{
    xplrJsonParserStatus ret;

    cJSON *array     = NULL;
    cJSON *array_obj = NULL;
    char  *ptr;
    uint16_t array_size;
    uint8_t i;

    ret = xplrJsonCheckJsonPtrForNull(json);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckSubTopicPtrForNull(res);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if ((strcmp(topicName, XPLR_ZTP_TOPIC_KEY_DISTRIBUTION_ID) != 0) &&
        (strcmp(topicName, XPLR_ZTP_TOPIC_ASSIST_NOW_ID) != 0)) {

        ret = xplrJsonCheckCharPtrForNull(region, "Region");
        if (ret != XPLR_JSON_PARSER_OK) {
            return ret;
        }
    }

    ret = xplrJsonCheckCharPtrForNull(topicName, "Topic Name");
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if (cJSON_HasObjectItem(json, "subscriptions")) {
        array = cJSON_GetObjectItem(json, "subscriptions");
        array_size = cJSON_GetArraySize(array);

        for (i = 0; i < array_size; i++) {
            array_obj = cJSON_GetArrayItem(array, i);

            if (cJSON_HasObjectItem(array_obj, "description")) {
                ret = xplrJsonZtpGetString(array_obj, "description", res->description,
                                           ELEMENTCNT(res->description));
                if (ret != XPLR_JSON_PARSER_OK) {
                    return ret;
                }

                ret = xplrJsonZtpGetString(array_obj, "path", res->path, ELEMENTCNT(res->path));
                if (ret != XPLR_JSON_PARSER_OK) {
                    return ret;
                }

                if (region == NULL) {
                    if (strstr(res->path, topicName) != NULL) {
                        return XPLR_JSON_PARSER_OK;
                    }
                } else {
                    if (strcmp(topicName, XPLR_ZTP_TOPIC_CORRECTION_DATA_ID) == 0) {
                        /**
                         * current correction topics /pp/ip/region
                         */
                        ptr = strstr(res->path, region);

                        if (ptr != NULL && ptr[2] == 0) {
                            return XPLR_JSON_PARSER_OK;
                        }
                    } else {
                        if (strstr(res->path, topicName) != NULL && strstr(res->path, region) != NULL) {
                            return XPLR_JSON_PARSER_OK;
                        }
                    }
                }
            }
        }

        return XPLR_JSON_PARSER_NO_ITEM;
    } else {
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    return XPLR_JSON_PARSER_ERROR;
}

/* ----------------------------------------------------------------
 * DEPRECATED
 * -------------------------------------------------------------- */

static xplrJsonParserStatus xplrJsonZtpGetDynamicKey(const cJSON *const json, const char *name,
                                                     xplrDynamicKeyUnit_t *res);

xplrJsonParserStatus xplrJsonZtpGetDynamicKeys(const cJSON *const json, xplrDynamicKeys_t *res)
{
    xplrJsonParserStatus ret;

    cJSON *dynamicKeys = NULL;

    ret = xplrJsonCheckJsonPtrForNull(json);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckDynamicKeysPtrForNull(res);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if (cJSON_HasObjectItem(json, "dynamickeys")) {
        dynamicKeys = cJSON_GetObjectItem(json, "dynamickeys");

        if (cJSON_HasObjectItem(dynamicKeys, "next") && cJSON_HasObjectItem(dynamicKeys, "current")) {
            ret = xplrJsonZtpGetDynamicKey(dynamicKeys, "next", &res->next);
            if (ret != XPLR_JSON_PARSER_OK) {
                return ret;
            }

            ret = xplrJsonZtpGetDynamicKey(dynamicKeys, "current", &res->current);
            if (ret != XPLR_JSON_PARSER_OK) {
                return ret;
            }
        } else {
            return XPLR_JSON_PARSER_NO_ITEM;
        }

        return XPLR_JSON_PARSER_OK;
    } else {
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    return XPLR_JSON_PARSER_ERROR;
}

/**
 * Parses and returns one key
 */
static xplrJsonParserStatus xplrJsonZtpGetDynamicKey(const cJSON *const json, const char *name,
                                                     xplrDynamicKeyUnit_t *res)
{
    cJSON *current_element = NULL;
    xplrJsonParserStatus ret;

    ret = xplrJsonCheckJsonPtrForNull(json);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckCharPtrForNull((char *)name, "Name");
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    ret = xplrJsonCheckDynamicKeyPtrForNull(res);
    if (ret != XPLR_JSON_PARSER_OK) {
        return ret;
    }

    if (cJSON_HasObjectItem(json, name)) {
        current_element = cJSON_GetObjectItem(json, name);
        res->duration = cJSON_GetObjectItem(current_element, "duration")->valuedouble;
        res->start = cJSON_GetObjectItem(current_element, "start")->valuedouble;

        ret = xplrJsonZtpGetString(current_element, "value", res->value, ELEMENTCNT(res->value));
        if (ret != XPLR_JSON_PARSER_OK) {
            return ret;
        }

        return XPLR_JSON_PARSER_OK;
    } else {
        return XPLR_JSON_PARSER_NO_ITEM;
    }

    return XPLR_JSON_PARSER_ERROR;
}

static xplrJsonParserStatus xplrJsonCheckJsonPtrForNull(const cJSON *const json)
{
    if (json == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "JSON pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}

static xplrJsonParserStatus xplrJsonCheckSubTopicsPtrForNull(xplrZtpStyleTopics_t *topics)
{
    if (topics == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Result topic pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}

static xplrJsonParserStatus xplrJsonCheckSubTopicPtrForNull(xplrTopic *topic)
{
    if (topic == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Result topic pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}

static xplrJsonParserStatus xplrJsonCheckCharPtrForNull(char *ptr, char *varName)
{
    if (ptr == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "%s pointer is NULL!", varName);
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}

static xplrJsonParserStatus xplrJsonCheckDynamicKeysPtrForNull(xplrDynamicKeys_t *dynKeys)
{
    if (dynKeys == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Dynamic Keys pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}

static xplrJsonParserStatus xplrJsonCheckDynamicKeyPtrForNull(xplrDynamicKeyUnit_t *dynKey)
{
    if (dynKey == NULL) {
        XPLRZTPJSONPARSER_CONSOLE(E, "Dynamic Key pointer is NULL!");
        return XPLR_JSON_PARSER_NULL_PTR_ERROR;
    }
    return XPLR_JSON_PARSER_OK;
}