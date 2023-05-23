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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include <stdio.h>
#include "string.h"
#include "xplr_thingstream.h"
#include "cJSON.h"

#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRTHINGSTREAM_DEBUG_ACTIVE) && (1 == U_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLR_THINGSTREAM_CONSOLE(tag, message, ...)   esp_rom_printf(U_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrThingstream", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLR_THINGSTREAM_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/**
 * Error codes to return when using JSON parsing functions.
 * TS_PARSER_ERROR_OK should terminate on 0.
 */
typedef enum {
    TS_PARSER_ERROR_NO_ITEM = -5,   /**< could not find an item with that key. */
    TS_PARSER_ERROR_WRONG_TYPE,     /**< the item requested is not the expected type. */
    TS_PARSER_ERROR_OVERFLOW,       /**< the provided buffer is not big enough. */
    TS_PARSER_ERROR_NULL_PTR,       /**< the pointer passed is null. */
    TS_PARSER_ERROR,                /**< cJSON parsing error. */
    TS_PARSER_ERROR_OK = 0          /**< parsed item OK. */
} ts_parser_status_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

const uint16_t brokerPort = 8883;
const char thingstreamApiUrl[] = "api.thingstream.io:443";
const char thingstreamApiPpCredPath[] = "/ztp/pointperfect/credentials";

const char tsPpClientCertTag[] =         "certificate";     /* string */
const char tsPpClientIdTag[] =           "clientId";        /* string */
const char tsPpClientKeyTag[] =          "privateKey";      /* string */
const char tsPpBrokerTag[] =             "brokerHost";      /* string */
const char tsPpLbandSupportTag[] =       "supportsLband";   /* bool */
const char tsPpMqttSupportTag[] =        "supportsMqtt";    /* bool */

const char tsPpDkeysTag[] =              "dynamickeys";     /* object */
const char tsPpDkeyCurrentTag[] =        "current";         /* object */
const char tsPpDkeyNextTag[] =           "next";            /* object */
const char tsPpDkeyAttributeDuration[] = "duration";        /* int */
const char tsPpDkeyAttributeStart[] =    "start";           /* int */
const char tsPpDkeyAttributeValue[] =    "value";           /* string */

const char tsPpTopicsTag[] =             "subscriptions";   /* array */
const char tsPpTopicDescriptionTag[] =   "description";     /* string */
const char tsPpTopicPathTag[] =          "path";            /* string */

const char thingstreamPpFilterRegionEU[] =             "EU";
const char thingstreamPpFilterRegionEUAll[] =          "eu";
const char thingstreamPpFilterRegionUS[] =             "US";
const char thingstreamPpFilterRegionUSAll[] =          "us";
const char thingstreamPpFilterKeyDist[] =              "key distribution";
const char thingstreamPpFilterAssistNow[] =            "AssistNow";
const char thingstreamPpFilterCorrectionData[] =       "L-band + IP correction";
const char thingstreamPpFilterGAD[] =                  "geographic area definition";
const char thingstreamPpFilterHPAC[] =                 "high-precision atmosphere correction";
const char thingstreamPpFilterOCB[] =                  "GNSS orbit, clocks and bias";
const char thingstreamPpFilterClock[] =                "GNSS clock";
const char thingstreamPpFilterFreq[] =                 "frequencies";
const char thingstreamPpFilterAll[] =                  "/pp/";

const char thingstreamPpDescAllEu[] = "L-band + IP EU topics";
const char thingstreamPpDescAllUs[] = "L-band + IP US topics";
const char thingstreamPpDescAll[] = "L-band + IP EU + US topics";

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplr_thingstream_error_t tsCreateDeviceUid(char *uid);
static xplr_thingstream_error_t tsApiMsgCreatePpZtp(char *msg, size_t *size,
                                                    xplr_thingstream_t *settings);
static xplr_thingstream_error_t tsApiMsgParsePpZtpBrokerAddress(const char *msg,
                                                                char *info,
                                                                size_t infoSize);
static xplr_thingstream_error_t tsApiMsgParsePpZtpClientCert(const char *msg,
                                                             char *info,
                                                             size_t infoSize);
static xplr_thingstream_error_t tsApiMsgParsePpZtpClientKey(const char *msg,
                                                            char *info,
                                                            size_t infoSize);
static xplr_thingstream_error_t tsApiMsgParsePpZtpClientId(const char *msg,
                                                           char *info,
                                                           size_t infoSize);
static xplr_thingstream_error_t tsApiMsgParsePpZtpLbandSupport(const char *msg,
                                                               bool *supported);
static xplr_thingstream_error_t tsApiMsgParsePpZtpMqttSupport(const char *msg,
                                                              bool *supported);
static xplr_thingstream_error_t tsApiMsgParsePpZtpDkeys(const char *msg,
                                                        xplr_thingstream_pp_dKeys_t *dKeys);
static xplr_thingstream_error_t tsApiMsgParsePpZtpTopic(const char *msg,
                                                        const char *regionFilter,
                                                        const char *typeFilter,
                                                        xplr_thingstream_pp_topic_t *topic);
static xplr_thingstream_error_t tsApiMsgParsePpZtpTopicList(const char *msg,
                                                            xplr_thingstream_pp_topic_t *topic);

static xplr_thingstream_error_t tsApiMsgParsePpZtpCheckTag(const char *msg, const char *tag);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplr_thingstream_error_t xplrThingstreamInit(const char *ppToken,
                                             xplr_thingstream_t *thingstream)
{
    size_t tokenLength;
    size_t serverUrlLength = strlen(thingstreamApiUrl);
    size_t ppZtpUrlLength = strlen(thingstreamApiPpCredPath);
    xplr_thingstream_error_t ret;

    if (ppToken != NULL) {
        /* check token length */
        tokenLength = strlen(ppToken);
        if (tokenLength == XPLR_THINGSTREAM_PP_TOKEN_SIZE - 1) {
            memcpy(&thingstream->server.ppToken, ppToken, XPLR_THINGSTREAM_PP_TOKEN_SIZE);
            memcpy(&thingstream->server.serverUrl, thingstreamApiUrl, serverUrlLength);
            memcpy(&thingstream->pointPerfect.urlPath, thingstreamApiPpCredPath, ppZtpUrlLength);
            thingstream->pointPerfect.brokerPort = brokerPort;
            ret = tsCreateDeviceUid(thingstream->server.deviceId);
            if (ret != XPLR_THINGSTREAM_OK) {
                XPLR_THINGSTREAM_CONSOLE(E, "Thingstream init failed.");
            } else {
                XPLR_THINGSTREAM_CONSOLE(D, "Thingstream settings config ok.");
                XPLR_THINGSTREAM_CONSOLE(D, "Server url:%s.", thingstream->server.serverUrl);
                XPLR_THINGSTREAM_CONSOLE(D, "PP credentials path:%s.", thingstream->pointPerfect.urlPath);
                XPLR_THINGSTREAM_CONSOLE(D, "Device UID:%s.", thingstream->server.deviceId);
                XPLR_THINGSTREAM_CONSOLE(D, "Location service token:%s.", thingstream->server.ppToken);
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Provided token is invalid.");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "Provided token is NULL.");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

xplr_thingstream_error_t xplrThingstreamApiMsgCreate(xplr_thingstream_api_t cmd,
                                                     char *msg,
                                                     size_t *size,
                                                     xplr_thingstream_t *instance)
{
    xplr_thingstream_error_t ret;

    switch (cmd) {
        case XPLR_THINGSTREAM_API_INVALID:
            ret = XPLR_THINGSTREAM_ERROR;
            break;
        case XPLR_THINGSTREAM_API_LOCATION_ZTP:
            ret = tsApiMsgCreatePpZtp(msg, size, instance);
            break;

        default:
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpConfig(const char *data,
                                                 xplr_thingstream_pp_region_t region,
                                                 xplr_thingstream_pp_settings_t *settings)
{
    xplr_thingstream_error_t err[8];
    xplr_thingstream_error_t ret;

    /* get broker configuration settings */
    err[0] = xplrThingstreamPpParseServerInfo(data,
                                              settings->brokerAddress,
                                              XPLR_THINGSTREAM_URL_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_ADDRESS);

    err[1] = xplrThingstreamPpParseServerInfo(data,
                                              settings->deviceId,
                                              XPLR_THINGSTREAM_PP_DEVICEID_SIZE,
                                              XPLR_THINGSTREAM_PP_SERVER_ID);

    err[2] = xplrThingstreamPpParseServerInfo(data,
                                              settings->clientCert,
                                              XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_CERT);

    err[3] = xplrThingstreamPpParseServerInfo(data,
                                              settings->clientKey,
                                              XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_KEY);

    err[4] = xplrThingstreamPpParseLbandSupport(data,
                                                &settings->lbandSupported);

    err[5] = xplrThingstreamPpParseMqttSupport(data,
                                               &settings->mqttSupported);

    err[6] = xplrThingstreamPpParseDynamicKeys(data,
                                               &settings->dynamicKeys);
    /* get broker topics (region filtered) */
    err[7] = xplrThingstreamPpParseTopicsInfoByRegionAll(data,
                                                         region,
                                                         settings->topicList);

    for (int8_t i = 0; i < 8; i++) {
        if (err[i] != XPLR_THINGSTREAM_OK) {
            ret = XPLR_THINGSTREAM_ERROR;
            break;
        } else {
            ret = XPLR_THINGSTREAM_OK;
        }
        settings->numOfTopics = i + 1;
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpParseServerInfo(const char *data,
                                                          char *value,
                                                          size_t size,
                                                          xplr_thingstream_pp_serverInfo_type_t info)
{
    xplr_thingstream_error_t ret;

    switch (info) {
        case XPLR_THINGSTREAM_PP_SERVER_ADDRESS:
            ret = tsApiMsgParsePpZtpBrokerAddress(data, value, size);
            break;
        case XPLR_THINGSTREAM_PP_SERVER_CERT:
            ret = tsApiMsgParsePpZtpClientCert(data, value, size);
            break;
        case XPLR_THINGSTREAM_PP_SERVER_KEY:
            ret = tsApiMsgParsePpZtpClientKey(data, value, size);
            break;
        case XPLR_THINGSTREAM_PP_SERVER_ID:
            ret = tsApiMsgParsePpZtpClientId(data, value, size);
            break;

        default:
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpParseLbandSupport(const char *data,
                                                            bool *lband)
{
    return tsApiMsgParsePpZtpLbandSupport(data, lband);
}

xplr_thingstream_error_t xplrThingstreamPpParseMqttSupport(const char *data,
                                                           bool *mqtt)
{
    return tsApiMsgParsePpZtpMqttSupport(data, mqtt);
}

xplr_thingstream_error_t xplrThingstreamPpParseTopicInfo(const char *data,
                                                         xplr_thingstream_pp_region_t region,
                                                         xplr_thingstream_pp_topic_type_t type,
                                                         xplr_thingstream_pp_topic_t *topic)
{
    const char *regionFilter, *topicFilter;
    xplr_thingstream_error_t ret;

    /* set region filter */
    switch (region) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            regionFilter = thingstreamPpFilterRegionEU;
            ret = XPLR_THINGSTREAM_OK;
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            regionFilter = thingstreamPpFilterRegionUS;
            ret = XPLR_THINGSTREAM_OK;
            break;
        case XPLR_THINGSTREAM_PP_REGION_ALL:
            regionFilter = " ";
            ret = XPLR_THINGSTREAM_OK;
            break;

        default:
            XPLR_THINGSTREAM_CONSOLE(E, "Region not supported.");
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    /* set topic filter */
    if (ret != XPLR_THINGSTREAM_ERROR) {
        switch (type) {
            case XPLR_THINGSTREAM_PP_TOPIC_KEYS_DIST:
                topicFilter = thingstreamPpFilterKeyDist;
                /* key distribution topic does not contain region id. mask filter */
                regionFilter = " ";
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_ASSIST_NOW:
                topicFilter = thingstreamPpFilterAssistNow;
                /* assistnow topic does not contain region id. mask filter */
                regionFilter = " ";
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_CORRECTION_DATA:
                topicFilter = thingstreamPpFilterCorrectionData;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_GAD:
                topicFilter = thingstreamPpFilterGAD;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_HPAC:
                topicFilter = thingstreamPpFilterHPAC;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_OCB:
                topicFilter = thingstreamPpFilterOCB;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_CLK:
                topicFilter = thingstreamPpFilterClock;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_FREQ:
                topicFilter = thingstreamPpFilterFreq;
                /* frequencies topic does not contain region id. mask filter */
                regionFilter = " ";
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_ALL_EU:
                topicFilter = thingstreamPpFilterRegionEUAll;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_ALL_US:
                topicFilter = thingstreamPpFilterRegionUSAll;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_ALL:
                topicFilter = thingstreamPpFilterAll;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_INVALID:
                ret = XPLR_THINGSTREAM_ERROR;
                break;

            default:
                ret = XPLR_THINGSTREAM_ERROR;
                break;
        }
    }

    /* search for topic */
    if (ret != XPLR_THINGSTREAM_ERROR) {
        ret = tsApiMsgParsePpZtpTopic(data, regionFilter, topicFilter, topic);
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoByRegion(const char *data,
                                                                  xplr_thingstream_pp_region_t region,
                                                                  xplr_thingstream_pp_topic_t *topics)
{
    xplr_thingstream_error_t err[4];
    xplr_thingstream_error_t ret;

    /* minimum required topics are the first 3 (non-negative) elements
       in xplr_thingstream_pp_topic_type_t enum.
       Iterate xplrThingstreamPpParseTopicsInfoByRegion() and grab them.
       Before exiting, add the frequencies topic to the list as well.
    */

    for (int8_t i = 0; i < 3; i++) {
        err[i] = xplrThingstreamPpParseTopicInfo(data,
                                                 region,
                                                 (xplr_thingstream_pp_topic_type_t)i,
                                                 &topics[i]);
    }

    err[3] = xplrThingstreamPpParseTopicInfo(data,
                                             region,
                                             XPLR_THINGSTREAM_PP_TOPIC_FREQ,
                                             &topics[3]);

    for (int8_t i = 0; i < 4; i++) {
        ret = err[i];
        if (err[i] != XPLR_THINGSTREAM_OK) {
            break;
        }
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoByRegionAll(const char *data,
                                                                     xplr_thingstream_pp_region_t region,
                                                                     xplr_thingstream_pp_topic_t *topics)
{
    xplr_thingstream_error_t err[8];
    xplr_thingstream_error_t ret;

    /* iterate through the topics type list and grab the info */
    for (int8_t i = 0; i < 7; i++) {
        err[i] = xplrThingstreamPpParseTopicInfo(data,
                                                 region,
                                                 (xplr_thingstream_pp_topic_type_t)i,
                                                 &topics[i]);
    }

    /* filter concatenated topic path based on region property */
    switch (region) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            err[7] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     XPLR_THINGSTREAM_PP_TOPIC_ALL_EU,
                                                     &topics[7]);
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            err[7] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     XPLR_THINGSTREAM_PP_TOPIC_ALL_US,
                                                     &topics[7]);
            break;
        case XPLR_THINGSTREAM_PP_REGION_ALL:
            err[7] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     XPLR_THINGSTREAM_PP_TOPIC_ALL,
                                                     &topics[7]);
            break;

        default:
            err[7] = XPLR_THINGSTREAM_ERROR;
            break;
    }

    for (int8_t i = 0; i < 8; i++) {
        ret = err[i];
        if (err[i] != XPLR_THINGSTREAM_OK) {
            break;
        }
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoAll(const char *data,
                                                             xplr_thingstream_pp_topic_t *topics)
{
    return tsApiMsgParsePpZtpTopicList(data, topics);
}

xplr_thingstream_error_t xplrThingstreamPpParseDynamicKeys(const char *data,
                                                           xplr_thingstream_pp_dKeys_t *keys)
{
    return tsApiMsgParsePpZtpDkeys(data, keys);
}

bool xplrThingstreamPpMsgIsKeyDist(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterKeyDist;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsAssistNow(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterAssistNow;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsCorrectionData(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterCorrectionData;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsGAD(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterGAD;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsHPAC(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterHPAC;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsOCB(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterOCB;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsClock(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterClock;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

bool xplrThingstreamPpMsgIsFrequency(const char *name, const xplr_thingstream_t *instance)
{
    bool ret;
    const char *path = NULL;
    const char *description = NULL;
    int32_t pathFoundInData;
    const char *descriptionFilter = thingstreamPpFilterFreq;

    /* find key distribution path from thingstream instance topic list */
    for (int i = 0; i < instance->pointPerfect.numOfTopics; i++) {
        description = strstr(instance->pointPerfect.topicList[i].description, descriptionFilter);
        if (description != NULL) {
            path = instance->pointPerfect.topicList[i].path;
            break; /* found it, no need to continue searching the list */
        }
    }

    /* check if found path is contained in data */
    if (path != NULL) {
        pathFoundInData = strcmp(path, name);
        if (pathFoundInData != 0) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = false;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static xplr_thingstream_error_t tsCreateDeviceUid(char *uid)
{
    uint8_t mac[6];
    int32_t err;
    int32_t uidLength;
    xplr_thingstream_error_t ret;

    err = xplrGetDeviceMac(mac);

    if (err != 0) {
        ret = XPLR_THINGSTREAM_ERROR;
    } else {
        uidLength = snprintf(uid,
                             XPLR_THINGSTREAM_DEVICEUID_SIZE,
                             "xplr%02x%02x%02x",
                             mac[3],
                             mac[4],
                             mac[5]);

        if (uidLength < 0 || uidLength < XPLR_THINGSTREAM_DEVICEUID_SIZE - 1) {
            ret = XPLR_THINGSTREAM_ERROR;
        } else {
            ret = XPLR_THINGSTREAM_OK;
        }
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgCreatePpZtp(char *msg, size_t *size,
                                                    xplr_thingstream_t *settings)
{
    cJSON *root, *array, *element;
    char *jMsg;
    size_t jMsgSize;
    xplr_thingstream_error_t ret;
    ret = XPLR_THINGSTREAM_ERROR;

    if ((strlen(settings->server.ppToken) != XPLR_THINGSTREAM_PP_TOKEN_SIZE - 1) ||
        (strlen(settings->server.deviceId) != XPLR_THINGSTREAM_DEVICEUID_SIZE - 1)) {
        ret = XPLR_THINGSTREAM_ERROR;
        XPLR_THINGSTREAM_CONSOLE(E, "Token size or device uid invalid.");
    } else {
        root = cJSON_CreateObject();
        array = cJSON_AddArrayToObject(root, "tags");
        element = cJSON_CreateString("ztp");
        cJSON_AddItemToArray(array, element);
        cJSON_AddStringToObject(root, "token", settings->server.ppToken);
        cJSON_AddStringToObject(root, "hardwareId", settings->server.deviceId);
        cJSON_AddStringToObject(root, "givenName", "xplrHpg");
        jMsg = cJSON_Print(root);
        jMsgSize = strlen(jMsg);
        if (*size >= jMsgSize) {
            memcpy(msg, jMsg, jMsgSize);
            *size = jMsgSize;
            ret = XPLR_THINGSTREAM_OK;
            XPLR_THINGSTREAM_CONSOLE(D, "Thingstream API PP ZTP POST of %d bytes created:\n%s", *size, msg);
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json msg of %d bytes could not fit buffer of %d bytes.", jMsgSize,
                                     *size);
        }
        cJSON_Delete(root);
        cJSON_free(jMsg);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpBrokerAddress(const char *msg,
                                                                char *info,
                                                                size_t infoSize)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    size_t jMsgSize;
    const char *key = tsPpBrokerTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsString(element);
        if (jBool) {
            jMsgSize = strlen(element->valuestring);
            if (jMsgSize < infoSize) {
                strcpy(info, element->valuestring);
                XPLR_THINGSTREAM_CONSOLE(D, "Broker address (%d bytes): %s.", jMsgSize, info);
                ret = XPLR_THINGSTREAM_OK;
            } else {
                ret = XPLR_THINGSTREAM_ERROR;
                XPLR_THINGSTREAM_CONSOLE(E, "Broker address of %d bytes could not fit buffer of %d bytes.",
                                         jMsgSize, infoSize);
            }
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <String>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpClientCert(const char *msg,
                                                             char *info,
                                                             size_t infoSize)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    size_t jMsgSize;
    const char *key = tsPpClientCertTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsString(element);
        if (jBool) {
            jMsgSize = strlen(element->valuestring);
            if (jMsgSize < infoSize) {
                strcpy(info, element->valuestring);
                //XPLR_THINGSTREAM_CONSOLE(D, "PP client cert (%d bytes): \n%s", jMsgSize, info);
                XPLR_THINGSTREAM_CONSOLE(D, "PP client cert parsed ok (%d bytes)", jMsgSize);
                ret = XPLR_THINGSTREAM_OK;
            } else {
                ret = XPLR_THINGSTREAM_ERROR;
                XPLR_THINGSTREAM_CONSOLE(E, "PP client cert of %d bytes could not fit buffer of %d bytes.",
                                         jMsgSize, infoSize);
            }
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <String>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpClientKey(const char *msg,
                                                            char *info,
                                                            size_t infoSize)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    size_t jMsgSize;
    const char *key = tsPpClientKeyTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsString(element);
        if (jBool) {
            jMsgSize = strlen(element->valuestring);
            if (jMsgSize < infoSize) {
                strcpy(info, element->valuestring);
                //XPLR_THINGSTREAM_CONSOLE(D, "PP client key (%d bytes): \n%s", jMsgSize, info);
                XPLR_THINGSTREAM_CONSOLE(D, "PP client key parsed ok (%d bytes)", jMsgSize);
                ret = XPLR_THINGSTREAM_OK;
            } else {
                ret = XPLR_THINGSTREAM_ERROR;
                XPLR_THINGSTREAM_CONSOLE(E, "PP client key of %d bytes could not fit buffer of %d bytes.",
                                         jMsgSize, infoSize);
            }
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <String>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpClientId(const char *msg,
                                                           char *info,
                                                           size_t infoSize)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    size_t jMsgSize;
    const char *key = tsPpClientIdTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsString(element);
        if (jBool) {
            jMsgSize = strlen(element->valuestring);
            if (jMsgSize < infoSize) {
                strcpy(info, element->valuestring);
                XPLR_THINGSTREAM_CONSOLE(D, "PP client key (%d bytes): %s.", jMsgSize, info);
                ret = XPLR_THINGSTREAM_OK;
            } else {
                ret = XPLR_THINGSTREAM_ERROR;
                XPLR_THINGSTREAM_CONSOLE(E, "PP client key of %d bytes could not fit buffer of %d bytes.",
                                         jMsgSize, infoSize);
            }
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <String>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpLbandSupport(const char *msg,
                                                               bool *supported)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    const char *key = tsPpLbandSupportTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsBool(element);
        if (jBool) {
            *supported = (bool)element->valueint;
            XPLR_THINGSTREAM_CONSOLE(D, "PP LBand service: (%d).", (int)*supported);
            ret = XPLR_THINGSTREAM_OK;
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <Bool>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpMqttSupport(const char *msg,
                                                              bool *supported)
{
    cJSON *root, *element;
    cJSON_bool jBool;
    const char *key = tsPpMqttSupportTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        element = cJSON_GetObjectItem(root, key);
        jBool = cJSON_IsBool(element);
        if (jBool) {
            *supported = (bool)element->valueint;
            XPLR_THINGSTREAM_CONSOLE(D, "PP MQTT service: (%d).", (int)*supported);
            ret = XPLR_THINGSTREAM_OK;
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "json element not of type <Bool>.");
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpDkeys(const char *msg,
                                                        xplr_thingstream_pp_dKeys_t *dKeys)
{
    cJSON *root, *jArrDkeys, *jObjDkeyCurrent, *jObjDkeyNext;
    bool curKeyPresent, nextKeyPresent;
    char *jKeyVal;
    const char *key = tsPpDkeysTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        jArrDkeys = cJSON_GetObjectItem(root, key);
        curKeyPresent = cJSON_HasObjectItem(jArrDkeys, tsPpDkeyCurrentTag);
        nextKeyPresent = cJSON_HasObjectItem(jArrDkeys, tsPpDkeyNextTag);
        if ((curKeyPresent) && (nextKeyPresent)) {
            jObjDkeyCurrent = cJSON_GetObjectItem(jArrDkeys, tsPpDkeyCurrentTag);
            jObjDkeyNext = cJSON_GetObjectItem(jArrDkeys, tsPpDkeyNextTag);

            dKeys->current.duration = cJSON_GetObjectItem(jObjDkeyCurrent,
                                                          tsPpDkeyAttributeDuration)->valuedouble;
            dKeys->current.start = cJSON_GetObjectItem(jObjDkeyCurrent,
                                                       tsPpDkeyAttributeStart)->valuedouble;
            jKeyVal = cJSON_GetObjectItem(jObjDkeyCurrent, tsPpDkeyAttributeValue)->valuestring;

            if (jKeyVal != NULL) {
                if (strlen(jKeyVal) <= XPLR_THINGSTREAM_PP_DKEY_SIZE) {
                    strcpy(dKeys->current.value, jKeyVal);
                    ret = XPLR_THINGSTREAM_OK;
                } else {
                    ret = XPLR_THINGSTREAM_ERROR;
                    XPLR_THINGSTREAM_CONSOLE(E, "dKey value of %d bytes could not fit allocated buffer.",
                                             strlen(jKeyVal));
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "current dKey value error.");
                ret = XPLR_THINGSTREAM_ERROR;
            }

            dKeys->next.duration = cJSON_GetObjectItem(jObjDkeyNext,
                                                       tsPpDkeyAttributeDuration)->valuedouble;
            dKeys->next.start = cJSON_GetObjectItem(jObjDkeyNext,
                                                    tsPpDkeyAttributeStart)->valuedouble;
            jKeyVal = cJSON_GetObjectItem(jObjDkeyNext,
                                          tsPpDkeyAttributeValue)->valuestring;

            if (jKeyVal != NULL) {
                if (strlen(jKeyVal) <= XPLR_THINGSTREAM_PP_DKEY_SIZE) {
                    strcpy(dKeys->next.value, jKeyVal);
                    ret = XPLR_THINGSTREAM_OK;
                } else {
                    ret = XPLR_THINGSTREAM_ERROR;
                    XPLR_THINGSTREAM_CONSOLE(E, "dKey value of %d bytes could not fit allocated buffer.",
                                             strlen(jKeyVal));
                }
            } else {
                ret = XPLR_THINGSTREAM_ERROR;
                XPLR_THINGSTREAM_CONSOLE(E, "next dKey value error.");
            }

            if (ret != XPLR_THINGSTREAM_ERROR) {
                XPLR_THINGSTREAM_CONSOLE(D, "\nDynamic keys parsed:\
                                             \nCurrent key:\n\t start (UTC):%llu\n\t duration (UTC):%llu\n\t value:%s\
                                             \nNext key:\n\t start (UTC):%llu\n\t duration (UTC):%llu\n\t value:%s\n",
                                         dKeys->current.start,
                                         dKeys->current.duration,
                                         dKeys->current.value,
                                         dKeys->next.start,
                                         dKeys->next.duration,
                                         dKeys->next.value);
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Tag <%s> or <%s> not found.", tsPpDkeyCurrentTag, tsPpDkeyNextTag);
            ret = XPLR_THINGSTREAM_ERROR;
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpTopic(const char *msg,
                                                        const char *regionFilter,
                                                        const char *typeFilter,
                                                        xplr_thingstream_pp_topic_t *topic)
{
    cJSON *root, *jArrSubs, *jObjTopic, *jElDesc, *jElPath;
    size_t topicListSize;
    char *description, *path;
    char *regionFound, *typeFound;
    size_t descSize, pathSize, curPathSize;
    const char *key = tsPpTopicsTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        jArrSubs = cJSON_GetObjectItem(root, key);
        topicListSize = cJSON_GetArraySize(jArrSubs);
        for (int i = 0; i < topicListSize; i++) {
            jObjTopic = cJSON_GetArrayItem(jArrSubs, i);
            jElDesc = cJSON_GetObjectItem(jObjTopic, tsPpTopicDescriptionTag);
            jElPath = cJSON_GetObjectItem(jObjTopic, tsPpTopicPathTag);

            if ((cJSON_IsString(jElDesc)) && (cJSON_IsString(jElPath))) {
                /* we should have a normal description/path obj */
                description = jElDesc->valuestring;
                path = jElPath->valuestring;
                /* now check if object attributes have correct properties */
                regionFound = strstr(description, regionFilter);
                typeFound = strstr(description, typeFilter);
                if ((regionFound != NULL) && (typeFound != NULL)) {
                    /* we have a match. copy and break */
                    descSize = strlen(description);
                    pathSize = strlen(path);
                    memcpy(topic->description, description, descSize);
                    memcpy(topic->path, path, pathSize);
                    ret = XPLR_THINGSTREAM_OK;
                    break;
                } else {
                    ret = XPLR_THINGSTREAM_ERROR; /* topic does not match filters */
                }
            } else {
                /* check if we have a region "all" object */
                if (cJSON_IsString(jElPath)) {
                    path = jElPath->valuestring;
                    typeFound = strstr(path, typeFilter);
                    if (typeFound != NULL) {
                        /* check region filters and complete topic description accordingly */
                        if (strstr(typeFilter, thingstreamPpFilterRegionEUAll) != NULL) {
                            descSize = strlen(thingstreamPpDescAllEu);
                            memcpy(topic->description, thingstreamPpDescAllEu, descSize);
                            ret = XPLR_THINGSTREAM_OK;
                        } else if (strstr(typeFilter, thingstreamPpFilterRegionUSAll) != NULL) {
                            descSize = strlen(thingstreamPpDescAllUs);
                            memcpy(topic->description, thingstreamPpDescAllUs, descSize);
                            ret = XPLR_THINGSTREAM_OK;
                        } else if (strstr(typeFilter, thingstreamPpFilterAll) != NULL) {
                            descSize = strlen(thingstreamPpDescAll);
                            memcpy(topic->description, thingstreamPpDescAll, descSize);
                            ret = XPLR_THINGSTREAM_OK;
                        } else {
                            XPLR_THINGSTREAM_CONSOLE(D, "Failed to find region attribute <%s>\
                                                         in topic path <%s>. ",
                                                     typeFilter,
                                                     path);
                            ret = XPLR_THINGSTREAM_ERROR;
                        }

                        /* description has been copied to topic instance.
                           in case of "filterAll" we need to continue parsing
                           through the subscription list and concatenate all objects of type
                           {"path":"<mqtt path>"}
                        */
                        if (ret != XPLR_THINGSTREAM_ERROR) {
                            if (strstr(typeFilter, thingstreamPpFilterAll) != NULL) {
                                /* "filterAll" - check current length of topic path instance.
                                    if topic already present, add ";" at the end concatenate with
                                    next found path.
                                 */
                                pathSize = strlen(path);
                                curPathSize = strlen(topic->path);
                                if (curPathSize > 0) {
                                    topic->path[curPathSize] = ';';
                                    curPathSize++;
                                }
                                memcpy(&topic->path[curPathSize], path, pathSize);
                                ret = XPLR_THINGSTREAM_OK;
                            } else {
                                pathSize = strlen(path);
                                memcpy(topic->path, path, pathSize);
                                ret = XPLR_THINGSTREAM_OK;
                                break;
                            }
                        }
                    } else {
                        ret = XPLR_THINGSTREAM_ERROR; /* topic does not match filters */
                    }
                } else {
                    ret = XPLR_THINGSTREAM_ERROR;
                    XPLR_THINGSTREAM_CONSOLE(E, "Unknown topic.");
                }
            }
            /* clean up for next iteration */
            jElDesc = NULL;
            jElPath = NULL;
            description = NULL;
            path = NULL;
        }

        if (ret != XPLR_THINGSTREAM_ERROR) {
            XPLR_THINGSTREAM_CONSOLE(D, "Parsed %s @ %s.", topic->description, topic->path);
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpTopicList(const char *msg,
                                                            xplr_thingstream_pp_topic_t *topic)
{
    cJSON *root, *jArrSubs, *jObjTopic, *jElDesc, *jElPath;
    size_t topicListSize;
    char *description, *path;
    size_t descSize, pathSize;
    const char *key = tsPpTopicsTag;
    xplr_thingstream_error_t ret;

    ret = tsApiMsgParsePpZtpCheckTag(msg, key);

    if (ret != XPLR_THINGSTREAM_ERROR) {
        root = cJSON_Parse(msg);
        jArrSubs = cJSON_GetObjectItem(root, key);
        topicListSize = cJSON_GetArraySize(jArrSubs);
        if (topicListSize <= XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX) {
            for (int i = 0; i < topicListSize; i++) {
                jObjTopic = cJSON_GetArrayItem(jArrSubs, i);
                jElDesc = cJSON_GetObjectItem(jObjTopic, tsPpTopicDescriptionTag);
                jElPath = cJSON_GetObjectItem(jObjTopic, tsPpTopicPathTag);

                if ((cJSON_IsString(jElDesc)) && (cJSON_IsString(jElPath))) {
                    /* we should have a normal description/path obj */
                    description = jElDesc->valuestring;
                    path = jElPath->valuestring;
                    descSize = strlen(description);
                    pathSize = strlen(path);
                    if ((descSize <= XPLR_THINGSTREAM_PP_TOPIC_NAME_SIZE_MAX) &&
                        (pathSize <= XPLR_THINGSTREAM_PP_TOPIC_PATH_SIZE_MAX)) {
                        memcpy(topic[i].description, description, descSize);
                        memcpy(topic[i].path, path, pathSize);
                        ret = XPLR_THINGSTREAM_OK;
                        XPLR_THINGSTREAM_CONSOLE(D, "Parsed %s @ %s.", topic[i].description, topic[i].path);
                    } else {
                        ret = XPLR_THINGSTREAM_ERROR;
                        XPLR_THINGSTREAM_CONSOLE(E, "Description or path contents cannot fit buffers");
                        break;
                    }

                } else if (cJSON_IsString(jElPath)) {
                    path = jElPath->valuestring;
                    pathSize = strlen(path);
                    if ((pathSize <= XPLR_THINGSTREAM_PP_TOPIC_PATH_SIZE_MAX)) {
                        memcpy(topic[i].path, path, pathSize);
                        ret = XPLR_THINGSTREAM_OK;
                        XPLR_THINGSTREAM_CONSOLE(D, "Parsed %s @ %s.", topic[i].description, topic[i].path);
                    } else {
                        ret = XPLR_THINGSTREAM_ERROR;
                        XPLR_THINGSTREAM_CONSOLE(E, "Path contents cannot fit buffers");
                    }
                } else {
                    ret = XPLR_THINGSTREAM_ERROR;
                    XPLR_THINGSTREAM_CONSOLE(E, "Unknown topic.");
                }
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Subscription list contains more (%d) objects\
                                             than topic list can handle (%d).",
                                     topicListSize,
                                     XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX);
            ret = XPLR_THINGSTREAM_ERROR;
        }
        cJSON_Delete(root);
    }

    return ret;
}

static xplr_thingstream_error_t tsApiMsgParsePpZtpCheckTag(const char *msg, const char *tag)
{
    cJSON *root;
    cJSON_bool jBool;
    xplr_thingstream_error_t ret;

    if (msg != NULL) {
        root = cJSON_Parse(msg);
        jBool = cJSON_HasObjectItem(root, tag);
        if (jBool) {
            ret = XPLR_THINGSTREAM_OK;
            XPLR_THINGSTREAM_CONSOLE(D, "Tag <%s> found.", tag);
        } else {
            ret = XPLR_THINGSTREAM_ERROR;
            XPLR_THINGSTREAM_CONSOLE(E, "Tag <%s> not found.", tag);
        }
        cJSON_Delete(root);
    } else {
        ret = XPLR_THINGSTREAM_ERROR;
        XPLR_THINGSTREAM_CONSOLE(E, "input msg is <NULL>.");
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

// End of file
