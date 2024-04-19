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
#include "xplr_log.h"
#include "cJSON.h"

#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
/**
 * Debugging print macro
 */
#if (1 == XPLRTHINGSTREAM_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLR_THINGSTREAM_LOG_ACTIVE))
#define XPLR_THINGSTREAM_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgThingstream", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRTHINGSTREAM_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLR_THINGSTREAM_LOG_ACTIVE)
#define XPLR_THINGSTREAM_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgThingstream", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRTHINGSTREAM_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLR_THINGSTREAM_LOG_ACTIVE)
#define XPLR_THINGSTREAM_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgThingstream", __FUNCTION__, __LINE__, ##__VA_ARGS__)
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
const char thingstreamApiUrlCell[] = "api.thingstream.io:443";
const char thingstreamApiUrlWifi[] = "https://api.thingstream.io";
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
const char thingstreamPpFilterRegionKR[] =             "KR";
const char thingstreamPpFilterRegionKRAll[] =          "kr";
const char thingstreamPpFilterRegionAU[] =             "AU";
const char thingstreamPpFilterRegionAUAll[] =          "au";
const char thingstreamPpFilterRegionJP[] =             "Japan";
const char thingstreamPpFilterRegionJPAll[] =          "jp";
const char thingstreamPpFilterKeyDist[] =              "key distribution";
const char thingstreamPpFilterAssistNow[] =            "AssistNow";
const char thingstreamPpFilterCorrectionDataIpLb[] =   "L-band + IP correction";
const char thingstreamPpFilterCorrectionDataIp[] =     "IP correction";
const char thingstreamPpFilterCorrectionDataLb[] =     "L-band correction";
const char thingstreamPpFilterGAD[] =                  "geographic area definition";
const char thingstreamPpFilterHPAC[] =                 "high-precision atmosphere correction";
const char thingstreamPpFilterOCB[] =                  "GNSS orbit, clocks and bias";
const char thingstreamPpFilterClock[] =                "GNSS clock";
const char thingstreamPpFilterFreq[] =                 "frequencies";
const char thingstreamPpFilterAll[] =                  "/pp/";

const char thingstreamPpDescAllEu[] = "L-band + IP EU topics";
const char thingstreamPpDescAllUs[] = "L-band + IP US topics";
const char thingstreamPpDescAll[] = "L-band + IP EU + US topics";

static const char *thingstreamPpFilterCorrectionData;

const char tsCommThingServerUrlStart[] =    "<ServerURI>";
const char tsCommThingServerUrlEnd[] =      "</ServerURI>";
const char tsCommThingClientIdStart[] =      "<ClientID>";
const char tsCommThingClientIdEnd[] =       "</ClientID>";
const char tsCommThingUsernameStart[] =     "<Username>";
const char tsCommThingUsernameEnd[] =       "</Username>";
const char tsCommThingPasswordStart[] =     "<Password>";
const char tsCommThingPasswordEnd[] =       "</Password>";

static int8_t logIndex = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplr_thingstream_error_t tsCreateDeviceUid(char *uid);
static xplr_thingstream_error_t tsApiMsgCreatePpZtp(char *msg,
                                                    size_t *size,
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

static xplr_thingstream_pp_plan_t tsPpGetPlanType(bool lbandSupported, bool mqttSupported);

static xplr_thingstream_error_t tsPpGetKeysTopic(xplr_thingstream_pp_sub_t *tsplan,
                                                 char *keysTopic);
static xplr_thingstream_error_t tsPpGetKeysDesc(xplr_thingstream_pp_sub_t *tsplan, char *keysDesc);
static xplr_thingstream_error_t tsPpGetCorrTopic(xplr_thingstream_pp_sub_t *tsplan,
                                                 char *corrTopic);
static xplr_thingstream_error_t tsPpGetCorrDesc(xplr_thingstream_pp_sub_t *tsplan, char *corrDesc);
static xplr_thingstream_error_t tsPpGetFreqTopic(xplr_thingstream_pp_sub_t *tsplan,
                                                 char *freqTopic);
static xplr_thingstream_error_t tsPpGetFreqDesc(xplr_thingstream_pp_sub_t *tsplan, char *freqDesc);
static void tsPpModifyBroker(char *brokerAddress);
static xplr_thingstream_error_t tsPpConfigFileGetBroker(char *payload, char *brokerAddress);
static xplr_thingstream_error_t tsPpConfigFileGetDeviceId(char *payload, char *deviceId);
static xplr_thingstream_error_t tsPpConfigFileGetClientKey(char *payload, char *clientKey);
static xplr_thingstream_error_t tsPpConfigFileGetClientCert(char *payload, char *clientCert);
static xplr_thingstream_error_t tsPpConfigFileGetRootCa(char *payload, char *rootCa);
static xplr_thingstream_error_t tsPpConfigFileGetDynamicKeys(char *payload,
                                                             xplr_thingstream_pp_dKeys_t *dynamicKeys);
static xplr_thingstream_error_t tsPpConfigFileParseTopicsInfoByRegionAll(char *payload,
                                                                         xplr_thingstream_pp_region_t region,
                                                                         bool lbandOverIpPreference,
                                                                         xplr_thingstream_pp_settings_t *settings);
static xplr_thingstream_error_t tsPpConfigFileFormatCert(char *cert,
                                                         xplr_thingstream_pp_serverInfo_type_t type);
static void tsPpSetDescFilter(xplr_thingstream_pp_settings_t *settings);
static xplr_thingstream_error_t tsCommThingParserCheckSize(const char *start,
                                                           const char *end,
                                                           size_t size);
static xplr_thingstream_error_t tsCommThingGetCredential(char *payload,
                                                         char *credential,
                                                         xplr_thingstream_comm_cred_type_t
                                                         credType);/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplr_thingstream_error_t xplrThingstreamInit(const char *ppToken,
                                             xplr_thingstream_t *thingstream)
{
    size_t tokenLength;
    size_t serverUrlLength;
    size_t ppZtpUrlLength = strlen(thingstreamApiPpCredPath);
    xplr_thingstream_error_t ret;

    if (ppToken != NULL) {
        /* check token length */
        tokenLength = strlen(ppToken);
        if (tokenLength == XPLR_THINGSTREAM_PP_TOKEN_SIZE - 1) {
            memcpy(&thingstream->server.ppToken, ppToken, XPLR_THINGSTREAM_PP_TOKEN_SIZE);
            switch (thingstream->connType) {
                case XPLR_THINGSTREAM_PP_CONN_WIFI:
                    serverUrlLength = strlen(thingstreamApiUrlWifi);
                    memcpy(&thingstream->server.serverUrl, thingstreamApiUrlWifi, serverUrlLength);
                    ret = XPLR_THINGSTREAM_OK;
                    break;
                case XPLR_THINGSTREAM_PP_CONN_CELL:
                    serverUrlLength = strlen(thingstreamApiUrlCell);
                    memcpy(&thingstream->server.serverUrl, thingstreamApiUrlCell, serverUrlLength);
                    ret = XPLR_THINGSTREAM_OK;
                    break;
                case XPLR_THINGSTREAM_PP_CONN_INVALID:
                default:
                    XPLR_THINGSTREAM_CONSOLE(E, "Error in selection of connection type");
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
            }
            if (ret != XPLR_THINGSTREAM_OK) {
                XPLR_THINGSTREAM_CONSOLE(E, "Thingstream init failed.");
            } else {
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
                                                 bool lbandOverIpPreference,
                                                 xplr_thingstream_t *settings)
{
    xplr_thingstream_error_t err[8];
    xplr_thingstream_error_t ret;
    xplr_thingstream_pp_plan_t subType;

    /* get broker configuration settings */
    err[0] = xplrThingstreamPpParseServerInfo(data,
                                              settings->pointPerfect.brokerAddress,
                                              XPLR_THINGSTREAM_URL_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_ADDRESS);

    err[1] = xplrThingstreamPpParseServerInfo(data,
                                              settings->pointPerfect.deviceId,
                                              XPLR_THINGSTREAM_PP_DEVICEID_SIZE,
                                              XPLR_THINGSTREAM_PP_SERVER_ID);

    err[2] = xplrThingstreamPpParseServerInfo(data,
                                              settings->pointPerfect.clientCert,
                                              XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_CERT);

    err[3] = xplrThingstreamPpParseServerInfo(data,
                                              settings->pointPerfect.clientKey,
                                              XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                              XPLR_THINGSTREAM_PP_SERVER_KEY);

    err[4] = xplrThingstreamPpParseLbandSupport(data,
                                                &settings->pointPerfect.lbandSupported);

    err[5] = xplrThingstreamPpParseMqttSupport(data,
                                               &settings->pointPerfect.mqttSupported);

    subType = tsPpGetPlanType(settings->pointPerfect.lbandSupported,
                              settings->pointPerfect.mqttSupported);

    err[6] = xplrThingstreamPpParseDynamicKeys(data,
                                               &settings->pointPerfect.dynamicKeys);

    // IPLBAND plan and LBAND correction source, parse topics as LBAND plan
    if (subType == XPLR_THINGSTREAM_PP_PLAN_IPLBAND && lbandOverIpPreference) {
        subType = XPLR_THINGSTREAM_PP_PLAN_LBAND;
    } else {
        // do nothing;
    }
    /* get broker topics (region filtered) */
    err[7] = xplrThingstreamPpParseTopicsInfoByRegionAll(data,
                                                         region,
                                                         subType,
                                                         settings->pointPerfect.topicList);

    tsPpSetDescFilter(&settings->pointPerfect);
    if ((settings->connType == XPLR_THINGSTREAM_PP_CONN_WIFI) && (err[0] == XPLR_THINGSTREAM_OK)) {
        tsPpModifyBroker(settings->pointPerfect.brokerAddress);
    } else if ((settings->connType == XPLR_THINGSTREAM_PP_CONN_CELL) &&
               (err[0] == XPLR_THINGSTREAM_OK)) {
        //Modify certs and broker
        xplrAddPortInfo(settings->pointPerfect.brokerAddress, settings->pointPerfect.brokerPort);
        if ((err[2] == XPLR_THINGSTREAM_OK) && (err[3] == XPLR_THINGSTREAM_OK)) {
            /* remove LFs from certificates */
            xplrRemoveChar(settings->pointPerfect.clientCert, '\n');
            xplrRemoveChar(settings->pointPerfect.clientKey, '\n');
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Certificates are parsed incorrectly!");
            err[0] = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        //Error
        XPLR_THINGSTREAM_CONSOLE(E, "Connection type not configured correctly!");
        err[0] = XPLR_THINGSTREAM_ERROR;
    }
    for (int8_t i = 0; i < 8; i++) {
        if (err[i] != XPLR_THINGSTREAM_OK) {
            ret = XPLR_THINGSTREAM_ERROR;
            break;
        } else {
            ret = XPLR_THINGSTREAM_OK;
        }
    }
    if (region == XPLR_THINGSTREAM_PP_REGION_AU) {
        // AU region has 2 MQTT topics
        settings->pointPerfect.numOfTopics = 2;
        settings->pointPerfect.lbandSupported = false;
    } else if (subType == XPLR_THINGSTREAM_PP_PLAN_LBAND) {
        if ((region == XPLR_THINGSTREAM_PP_REGION_EU) || (region == XPLR_THINGSTREAM_PP_REGION_US)) {
            // LBAND plan has 2 MQTT topics
            settings->pointPerfect.numOfTopics = 2;
        } else {
            settings->pointPerfect.lbandSupported = false;
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else if (subType == XPLR_THINGSTREAM_PP_PLAN_IP) {
        // IP plan has 6 MQTT topics
        settings->pointPerfect.numOfTopics = 6;
    } else if (subType == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
        if ((region == XPLR_THINGSTREAM_PP_REGION_EU) || (region == XPLR_THINGSTREAM_PP_REGION_US)) {
            // IPLBAND plan has 7 MQTT topics
            settings->pointPerfect.numOfTopics = 7;
        } else {
            settings->pointPerfect.numOfTopics = 6;
            settings->pointPerfect.lbandSupported = false;
        }
    } else {
        // We shouldn't reach this point. If we did something went wrong
        XPLR_THINGSTREAM_CONSOLE(E, "Could not set number of topics");
        ret = XPLR_THINGSTREAM_ERROR;
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
                                                         xplr_thingstream_pp_plan_t planType,
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
        case XPLR_THINGSTREAM_PP_REGION_KR:
            regionFilter = thingstreamPpFilterRegionKR;
            ret = XPLR_THINGSTREAM_OK;
            break;
        case XPLR_THINGSTREAM_PP_REGION_AU:
            regionFilter = thingstreamPpFilterRegionAU;
            ret = XPLR_THINGSTREAM_OK;
            break;
        case XPLR_THINGSTREAM_PP_REGION_JP:
            regionFilter = thingstreamPpFilterRegionJP;
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
            case XPLR_THINGSTREAM_PP_TOPIC_CORRECTION_DATA:
                switch (planType) {
                    case XPLR_THINGSTREAM_PP_PLAN_IP:
                        topicFilter = thingstreamPpFilterCorrectionDataIp;
                        ret = XPLR_THINGSTREAM_OK;
                        break;
                    case XPLR_THINGSTREAM_PP_PLAN_LBAND:
                        topicFilter = thingstreamPpFilterCorrectionDataLb;
                        ret = XPLR_THINGSTREAM_OK;
                        break;
                    case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
                        topicFilter = thingstreamPpFilterCorrectionDataIpLb;
                        ret = XPLR_THINGSTREAM_OK;
                        break;
                    case XPLR_THINGSTREAM_PP_PLAN_INVALID:
                    default:
                        ret = XPLR_THINGSTREAM_ERROR;
                        break;
                }
                if (ret == XPLR_THINGSTREAM_OK) {
                    thingstreamPpFilterCorrectionData = topicFilter;
                }
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
            case XPLR_THINGSTREAM_PP_TOPIC_ALL_KR:
                topicFilter = thingstreamPpFilterRegionKRAll;
                ret = XPLR_THINGSTREAM_OK;
                break;
            case XPLR_THINGSTREAM_PP_TOPIC_ALL_JP:
                topicFilter = thingstreamPpFilterRegionJPAll;
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
                                                                  xplr_thingstream_pp_plan_t planType,
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
                                                 planType,
                                                 (xplr_thingstream_pp_topic_type_t)i,
                                                 &topics[i]);
    }

    err[3] = xplrThingstreamPpParseTopicInfo(data,
                                             region,
                                             planType,
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
                                                                     xplr_thingstream_pp_plan_t planType,
                                                                     xplr_thingstream_pp_topic_t *topics)
{
    xplr_thingstream_error_t err[7];
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_ERROR;
    uint8_t numOfTopics;
    bool isRegionValid = (region == XPLR_THINGSTREAM_PP_REGION_EU) |
                         (region == XPLR_THINGSTREAM_PP_REGION_US);

    /* iterate through the topics type list and grab the info */
    if (planType == XPLR_THINGSTREAM_PP_PLAN_LBAND) {
        if (isRegionValid) {
            /**
            * For LBAND plan we need to parse 2 topics:
            *  - Key distribution
            *  - Frequencies
            */
            err[0] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     planType,
                                                     XPLR_THINGSTREAM_PP_TOPIC_KEYS_DIST,
                                                     &topics[0]);
            err[1] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     planType,
                                                     XPLR_THINGSTREAM_PP_TOPIC_FREQ,
                                                     &topics[1]);
            for (int8_t i = 0; i < 2; i++) {
                ret = err[i];
                if (err[i] != XPLR_THINGSTREAM_OK) {
                    break;
                }
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "LBAND plan is not supported in your region");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else if (region == XPLR_THINGSTREAM_PP_REGION_AU) {
        // AU region has 2 MQTT topics
        for (int8_t i = 0; i < 2; i++) {
            err[i] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     planType,
                                                     (xplr_thingstream_pp_topic_type_t)i,
                                                     &topics[i]);
        }
        for (int8_t i = 0; i < 2; i++) {
            ret = err[i];
            if (err[i] != XPLR_THINGSTREAM_OK) {
                break;
            }
        }
    } else if (region == XPLR_THINGSTREAM_PP_REGION_KR &&
               planType == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
        XPLR_THINGSTREAM_CONSOLE(E, "IP+LBAND plan is not supported in Korea region");
        ret = XPLR_THINGSTREAM_ERROR;
    } else {
        /* IP plan has 6 topics, IPLBAND plan has 7 topics (6 + frequencies) */
        if (planType == XPLR_THINGSTREAM_PP_PLAN_IP) {
            numOfTopics = 6;
        } else if (planType == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
            if (isRegionValid) {
                numOfTopics = 7;
            } else {
                numOfTopics = 6;
            }
        } else {
            numOfTopics = 0;
            ret = XPLR_THINGSTREAM_ERROR;
        }

        for (int8_t i = 0; i < numOfTopics; i++) {
            err[i] = xplrThingstreamPpParseTopicInfo(data,
                                                     region,
                                                     planType,
                                                     (xplr_thingstream_pp_topic_type_t)i,
                                                     &topics[i]);
        }
        for (int8_t i = 0; i < numOfTopics; i++) {
            ret = err[i];
            if (err[i] != XPLR_THINGSTREAM_OK) {
                break;
            }
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

    if (descriptionFilter == NULL) {
        /*INDENT-OFF*/
        XPLR_THINGSTREAM_CONSOLE(E, "Subscription plan to Thingstream has not been specified... Please call xplrThingstreamPpSetSubType first!");
        /*INDENT-ON*/
        return false;
    }

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

xplr_thingstream_error_t xplrThingstreamPpConfigTopics(xplr_thingstream_pp_region_t region,
                                                       xplr_thingstream_pp_plan_t plan,
                                                       bool lbandOverIpPreference,
                                                       xplr_thingstream_t *instance)
{
    xplr_thingstream_error_t ret[6];

    xplr_thingstream_pp_sub_t sub =  {
        .region = region,
        .plan = plan
    };

    /** Init error array*/
    for (int i = 0; i < 6; i++) {
        ret[i] = XPLR_THINGSTREAM_OK;
    }

    /** Get topics and descriptions*/
    ret[0] = tsPpGetKeysTopic(&sub, instance->pointPerfect.topicList[0].path);
    ret[1] = tsPpGetKeysDesc(&sub, instance->pointPerfect.topicList[0].description);
    if (sub.plan == XPLR_THINGSTREAM_PP_PLAN_LBAND) {
        if (sub.region == XPLR_THINGSTREAM_PP_REGION_US || sub.region == XPLR_THINGSTREAM_PP_REGION_EU) {
            ret[2] = tsPpGetFreqTopic(&sub, instance->pointPerfect.topicList[1].path);
            ret[3] = tsPpGetFreqDesc(&sub, instance->pointPerfect.topicList[1].description);
            ret[4] = XPLR_THINGSTREAM_OK;
            ret[5] = XPLR_THINGSTREAM_OK;
            instance->pointPerfect.numOfTopics = 2;
            instance->pointPerfect.mqttSupported = false;
            instance->pointPerfect.lbandSupported = true;
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "LBAND plan available only for EU and US regions");
            instance->pointPerfect.mqttSupported = false;
            instance->pointPerfect.lbandSupported = false;
            instance->pointPerfect.numOfTopics = 1;
            ret[2] = XPLR_THINGSTREAM_ERROR;
        }
    } else if (sub.plan == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
        if (sub.region == XPLR_THINGSTREAM_PP_REGION_KR) {
            XPLR_THINGSTREAM_CONSOLE(E, "IPLBAND plan not available for Korea region");
            instance->pointPerfect.numOfTopics = 1;
            instance->pointPerfect.mqttSupported = false;
            instance->pointPerfect.lbandSupported = false;
            ret[2] = XPLR_THINGSTREAM_ERROR;
        } else {
            if (lbandOverIpPreference) {
                if (sub.region == XPLR_THINGSTREAM_PP_REGION_US || sub.region == XPLR_THINGSTREAM_PP_REGION_EU) {
                    ret[2] = tsPpGetFreqTopic(&sub, instance->pointPerfect.topicList[1].path);
                    ret[3] = tsPpGetFreqDesc(&sub, instance->pointPerfect.topicList[1].description);
                    instance->pointPerfect.numOfTopics = 2;
                    instance->pointPerfect.mqttSupported = false;
                    instance->pointPerfect.lbandSupported = true;
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "LBAND plan available only for EU and US regions");
                    instance->pointPerfect.mqttSupported = false;
                    instance->pointPerfect.lbandSupported = false;
                    instance->pointPerfect.numOfTopics = 1;
                    ret[2] = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                ret[4] = tsPpGetCorrTopic(&sub, instance->pointPerfect.topicList[1].path);
                ret[5] = tsPpGetCorrDesc(&sub, instance->pointPerfect.topicList[1].description);
                instance->pointPerfect.numOfTopics = 2;
                instance->pointPerfect.mqttSupported = true;
                instance->pointPerfect.lbandSupported = false;
            }
        }
    } else if (sub.plan == XPLR_THINGSTREAM_PP_PLAN_IP) {
        instance->pointPerfect.mqttSupported = true;
        instance->pointPerfect.lbandSupported = false;
        ret[2] = tsPpGetCorrTopic(&sub, instance->pointPerfect.topicList[1].path);
        ret[3] = tsPpGetCorrDesc(&sub, instance->pointPerfect.topicList[1].description);
        ret[4] = XPLR_THINGSTREAM_OK;
        ret[5] = XPLR_THINGSTREAM_OK;
        instance->pointPerfect.numOfTopics = 2;
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "Invalid plan");
        instance->pointPerfect.mqttSupported = false;
        instance->pointPerfect.lbandSupported = false;
        instance->pointPerfect.numOfTopics = 1;
        ret[2] = XPLR_THINGSTREAM_ERROR;
    }

    tsPpSetDescFilter(&instance->pointPerfect);

    /** Check for errors*/
    for (int i = 0; i < 6; i++) {
        if (ret[i] != XPLR_THINGSTREAM_OK) {
            return XPLR_THINGSTREAM_ERROR;
        }
    }

    return XPLR_THINGSTREAM_OK;
}

xplr_thingstream_error_t xplrThingstreamPpConfigFromFile(char *data,
                                                         xplr_thingstream_pp_region_t region,
                                                         bool lbandOverIpPreference,
                                                         xplr_thingstream_t *instance)
{
    xplr_thingstream_error_t ret;
    xplr_thingstream_error_t err[7];
    uint8_t index;

    // Null pointer check
    if (data == NULL) {
        XPLR_THINGSTREAM_CONSOLE(E, "Payload to parse is NULL!");
        ret = XPLR_THINGSTREAM_ERROR;
    } else {
        // Get Broker address and port
        err[0] = tsPpConfigFileGetBroker(data, instance->pointPerfect.brokerAddress);
        instance->pointPerfect.brokerPort = brokerPort;
        // Get device ID
        err[1] = tsPpConfigFileGetDeviceId(data, instance->pointPerfect.deviceId);
        // Get MQTT Key
        err[2] = tsPpConfigFileGetClientKey(data, instance->pointPerfect.clientKey);
        // Get MQTT Cert
        err[3] = tsPpConfigFileGetClientCert(data, instance->pointPerfect.clientCert);
        // Get Root CA
        err[4] = tsPpConfigFileGetRootCa(data, instance->server.rootCa);
        // Get Dynamic Keys
        err[5] = tsPpConfigFileGetDynamicKeys(data, &instance->pointPerfect.dynamicKeys);
        // Get topics by region and num of topics and set mqtt and lband support flags
        err[6] = tsPpConfigFileParseTopicsInfoByRegionAll(data,
                                                          region,
                                                          lbandOverIpPreference,
                                                          &instance->pointPerfect);
        if ((instance->connType == XPLR_THINGSTREAM_PP_CONN_WIFI) && (err[0] == XPLR_THINGSTREAM_OK)) {
            tsPpModifyBroker(instance->pointPerfect.brokerAddress);
        } else if ((instance->connType == XPLR_THINGSTREAM_PP_CONN_CELL) &&
                   (err[0] == XPLR_THINGSTREAM_OK)) {
            //Modify certs and broker
            xplrAddPortInfo(instance->pointPerfect.brokerAddress, instance->pointPerfect.brokerPort);
            if ((err[2] == XPLR_THINGSTREAM_OK) &&
                (err[3] == XPLR_THINGSTREAM_OK) &&
                (err[4] == XPLR_THINGSTREAM_OK)) {
                /* remove LFs from certificates */
                xplrRemoveChar(instance->pointPerfect.clientCert, '\n');
                xplrRemoveChar(instance->pointPerfect.clientKey, '\n');
                xplrRemoveChar(instance->server.rootCa, '\n');
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Certificates are parsed incorrectly!");
                err[0] = XPLR_THINGSTREAM_ERROR;
            }
        } else {
            //Error
            XPLR_THINGSTREAM_CONSOLE(E, "Connection type not configured correctly!");
            err[0] = XPLR_THINGSTREAM_ERROR;
        }
        // Check for errors and return
        for (index = 0; index < 7; index++) {
            if (err[index] != XPLR_THINGSTREAM_OK) {
                ret = XPLR_THINGSTREAM_ERROR;
                break;
            } else {
                ret = XPLR_THINGSTREAM_OK;
            }
        }
    }

    return ret;
}

xplr_thingstream_error_t xplrThingstreamCommConfigFromFile(char *data,
                                                           xplr_thingstream_comm_thing_t *instance)
{
    xplr_thingstream_error_t ret;
    xplr_thingstream_error_t err[4];

    // Null Pointer check
    if (data == NULL) {
        ret = XPLR_THINGSTREAM_ERROR;
    } else {
        // Get broker url and port
        err[0] = tsCommThingGetCredential(data,
                                          instance->brokerAddress,
                                          XPLR_THINGSTREAM_COMM_CRED_SERVER_URL);
        // Get device ID
        err[1] = tsCommThingGetCredential(data,
                                          instance->deviceId,
                                          XPLR_THINGSTREAM_COMM_CRED_DEVICE_ID);
        // Get username
        err[2] = tsCommThingGetCredential(data,
                                          instance->username,
                                          XPLR_THINGSTREAM_COMM_CRED_USERNAME);
        // Get password
        err[3] = tsCommThingGetCredential(data,
                                          instance->password,
                                          XPLR_THINGSTREAM_COMM_CRED_PASSWORD);
        // Check for errors and return
        for (int i = 0; i < 4; i++) {
            if (err[i] != XPLR_THINGSTREAM_OK) {
                ret = XPLR_THINGSTREAM_ERROR;
                break;
            } else {
                ret = XPLR_THINGSTREAM_OK;
            }
        }
    }
    return ret;
}

int8_t xplrThingstreamInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_THINGSTREAM_DEFAULT_FILENAME,
                                   XPLRLOG_FILE_SIZE_INTERVAL,
                                   XPLRLOG_NEW_FILE_ON_BOOT);
        } else {
            /* logCfg contains the instance settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   logCfg->filename,
                                   logCfg->sizeInterval,
                                   logCfg->erasePrev);
        }
        ret = logIndex;
    } else {
        /* logIndex is positive so logging has been initialized before */
        logErr = xplrLogEnable(logIndex);
        if (logErr != XPLR_LOG_OK) {
            ret = -1;
        } else {
            ret = logIndex;
        }
    }

    return ret;
}

esp_err_t xplrThingstreamStopLogModule(void)
{
    esp_err_t ret;
    xplrLog_error_t logErr;

    logErr = xplrLogDisable(logIndex);
    if (logErr != XPLR_LOG_OK) {
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
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
                             "hpg-%02x%02x%02x",
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

static xplr_thingstream_pp_plan_t tsPpGetPlanType(bool lbandSupported, bool mqttSupported)
{
    xplr_thingstream_pp_plan_t ret;

    if (lbandSupported && mqttSupported) {
        ret = XPLR_THINGSTREAM_PP_PLAN_IPLBAND;
        XPLR_THINGSTREAM_CONSOLE(I,
                                 "Your current Thingstream plan is : PointPerfect L-band and IP, thus, valid to receive correction data via MQTT");
    } else if (mqttSupported) {
        ret = XPLR_THINGSTREAM_PP_PLAN_IP;
        XPLR_THINGSTREAM_CONSOLE(I,
                                 "Your current Thingstream plan is : PointPerfect IP, thus, valid to receive correction data via MQTT");
    } else if (lbandSupported) {
        ret = XPLR_THINGSTREAM_PP_PLAN_LBAND;
        XPLR_THINGSTREAM_CONSOLE(I, "Your current Thingstream plan is : PointPerfect L-band");
    } else {
        ret = XPLR_THINGSTREAM_PP_PLAN_INVALID;
        XPLR_THINGSTREAM_CONSOLE(E, "Invalid Thingstream plan.");
    }
    return ret;
}

static xplr_thingstream_error_t tsPpGetKeysTopic(xplr_thingstream_pp_sub_t *tsplan, char *keysTopic)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;
    strcpy(keysTopic, "/pp/ubx/0236/");
    switch (tsplan->plan) {
        case XPLR_THINGSTREAM_PP_PLAN_IP:
            strcat(keysTopic, "ip");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
        case XPLR_THINGSTREAM_PP_PLAN_LBAND:
            strcat(keysTopic, "Lb");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_INVALID:
        default:
            XPLR_THINGSTREAM_CONSOLE(E, "Invalid Subscription Plan Type... Cannot get key distribution topic");
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }
    return ret;
}

static xplr_thingstream_error_t tsPpGetKeysDesc(xplr_thingstream_pp_sub_t *tsplan, char *keysDesc)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;

    switch (tsplan->plan) {
        case XPLR_THINGSTREAM_PP_PLAN_IP:
            strcpy(keysDesc, "IP ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
            strcpy(keysDesc, "L-band + IP ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_LBAND:
            strcpy(keysDesc, "L-band ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_INVALID:
        default:
            XPLR_THINGSTREAM_CONSOLE(E,
                                     "Invalid Subscription Plan Type... Cannot get key distribution topic description");
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    if (ret == XPLR_THINGSTREAM_OK) {
        strcat(keysDesc, "key distribution topic");
    }

    return ret;
}

static xplr_thingstream_error_t tsPpGetCorrTopic(xplr_thingstream_pp_sub_t *tsplan, char *corrTopic)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;

    strcpy(corrTopic, "/pp/");
    switch (tsplan->plan) {
        case XPLR_THINGSTREAM_PP_PLAN_IP:
            strcat(corrTopic, "ip/");
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataIp;
            break;
        case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
            strcat(corrTopic, "Lb/");
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataIpLb;
            break;
        case XPLR_THINGSTREAM_PP_PLAN_LBAND:
            strcat(corrTopic, "Lb/");
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataLb;
            break;
        case XPLR_THINGSTREAM_PP_PLAN_INVALID:
        default:
            XPLR_THINGSTREAM_CONSOLE(E, "Invalid Subscription Plan Type... Cannot get correction topic");
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    if (ret == XPLR_THINGSTREAM_OK) {
        switch (tsplan->region) {
            case XPLR_THINGSTREAM_PP_REGION_EU:
                strcat(corrTopic, "eu");
                break;
            case XPLR_THINGSTREAM_PP_REGION_US:
                strcat(corrTopic, "us");
                break;
            case XPLR_THINGSTREAM_PP_REGION_KR:
                strcat(corrTopic, "kr");
                break;
            case XPLR_THINGSTREAM_PP_REGION_AU:
                strcat(corrTopic, "au");
                break;
            case XPLR_THINGSTREAM_PP_REGION_JP:
                strcat(corrTopic, "jp");
                break;
            case XPLR_THINGSTREAM_PP_REGION_INVALID:
            default:
                XPLR_THINGSTREAM_CONSOLE(E, "Invalid region type... Only EU and US are supported");
                ret = XPLR_THINGSTREAM_ERROR;
                break;
        }
    }

    return ret;
}

static xplr_thingstream_error_t tsPpGetCorrDesc(xplr_thingstream_pp_sub_t *tsplan, char *corrDesc)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;

    switch (tsplan->plan) {
        case XPLR_THINGSTREAM_PP_PLAN_IP:
            strcpy(corrDesc, "IP ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
            strcpy(corrDesc, "L-band + IP ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_LBAND:
            strcpy(corrDesc, "L-band ");
            break;
        case XPLR_THINGSTREAM_PP_PLAN_INVALID:
        default:
            XPLR_THINGSTREAM_CONSOLE(E,
                                     "Invalid Subscription Plan Type... Cannot get key correction topic description");
            ret = XPLR_THINGSTREAM_ERROR;
            break;
    }

    if (ret == XPLR_THINGSTREAM_OK) {
        strcat(corrDesc, "correction topic for ");
        switch (tsplan->region) {
            case XPLR_THINGSTREAM_PP_REGION_EU:
                strcat(corrDesc, "EU region");
                break;
            case XPLR_THINGSTREAM_PP_REGION_US:
                strcat(corrDesc, "US region");
                break;
            case XPLR_THINGSTREAM_PP_REGION_KR:
                strcat(corrDesc, "KR region");
                break;
            case XPLR_THINGSTREAM_PP_REGION_AU:
                strcat(corrDesc, "AU region");
                break;
            case XPLR_THINGSTREAM_PP_REGION_JP:
                strcat(corrDesc, "JP region");
                break;
            case XPLR_THINGSTREAM_PP_REGION_INVALID:
            default:
                XPLR_THINGSTREAM_CONSOLE(E, "Invalid region type... Only EU and US are supported");
                ret = XPLR_THINGSTREAM_ERROR;
                break;
        }
    }

    return ret;
}

static xplr_thingstream_error_t tsPpGetFreqTopic(xplr_thingstream_pp_sub_t *tsplan, char *freqTopic)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;
    if (tsplan->plan == XPLR_THINGSTREAM_PP_PLAN_LBAND ||
        tsplan->plan == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
        strcpy(freqTopic, "/pp/frequencies/Lb");
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "Non Lband plan does not have access to frequencies topic");
        ret = XPLR_THINGSTREAM_ERROR;
    }

    return ret;
}
static xplr_thingstream_error_t tsPpGetFreqDesc(xplr_thingstream_pp_sub_t *tsplan, char *freqDesc)
{
    xplr_thingstream_error_t ret = XPLR_THINGSTREAM_OK;
    if (tsplan->plan == XPLR_THINGSTREAM_PP_PLAN_LBAND) {
        strcpy(freqDesc, "L-band frequencies topic");
    } else if (tsplan->plan == XPLR_THINGSTREAM_PP_PLAN_IPLBAND) {
        strcpy(freqDesc, "L-band + IP frequencies topic");
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "Non Lband plan does not have access to frequencies topic");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

static void tsPpModifyBroker(char *brokerAddress)
{
    char modifiedBroker[XPLR_THINGSTREAM_URL_SIZE_MAX];
    snprintf(modifiedBroker, XPLR_THINGSTREAM_URL_SIZE_MAX, "mqtts://%s", brokerAddress);
    strcpy(brokerAddress, modifiedBroker);
}

static xplr_thingstream_error_t tsPpConfigFileGetBroker(char *payload, char *brokerAddress)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element;
    cJSON_bool jBool;
    char *token;
    char *keys[] = {"MQTT", "Connectivity", "ServerURI"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = root;
            for (int i = 0; i < 3; i++) {
                jBool = cJSON_HasObjectItem(element, keys[i]);
                if (!jBool) {
                    XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keys[i]);
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
                } else {
                    element = cJSON_GetObjectItem(element, keys[i]);
                    ret = XPLR_THINGSTREAM_OK;
                }
            }
            if (ret == XPLR_THINGSTREAM_OK) {
                jBool = cJSON_IsString(element);
                if (jBool) {
                    XPLR_THINGSTREAM_CONSOLE(D, "Parsed Server URI from configuration payload");
                    token = strrchr(element->valuestring, '/');
                    if ((strlen(token) - 1) <= XPLR_THINGSTREAM_URL_SIZE_MAX) {
                        xplrRemovePortInfo(token + 1, brokerAddress, XPLR_THINGSTREAM_URL_SIZE_MAX);
                        // tsPpModifyBroker(brokerAddress);
                        ret = XPLR_THINGSTREAM_OK;
                    } else {
                        XPLR_THINGSTREAM_CONSOLE(E, "Parsed Server URI greater than URL max size");
                        ret = XPLR_THINGSTREAM_ERROR;
                    }
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Invalid value for Server URI in configuration file!");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Parsing Failed!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

static xplr_thingstream_error_t tsPpConfigFileGetDeviceId(char *payload, char *deviceId)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element;
    cJSON_bool jBool;
    char *keys[] = {"MQTT", "Connectivity", "ClientID"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = root;
            for (int i = 0; i < 3; i++) {
                jBool = cJSON_HasObjectItem(element, keys[i]);
                if (!jBool) {
                    XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keys[i]);
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
                } else {
                    element = cJSON_GetObjectItem(element, keys[i]);
                    ret = XPLR_THINGSTREAM_OK;
                }
            }
            if (ret == XPLR_THINGSTREAM_OK) {
                jBool = cJSON_IsString(element);
                if (jBool) {
                    XPLR_THINGSTREAM_CONSOLE(D, "Parsed Client ID from configuration payload");
                    if (strlen(element->valuestring) <= XPLR_THINGSTREAM_CLIENTID_MAX) {
                        strcpy(deviceId, element->valuestring);
                        ret = XPLR_THINGSTREAM_OK;
                    } else {
                        XPLR_THINGSTREAM_CONSOLE(E, "Parsed Client ID larger than CLIENTID_MAX_SIZE!");
                        ret = XPLR_THINGSTREAM_ERROR;
                    }
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Invalid value for Client ID in configuration file!");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Parsing Failed!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}
static xplr_thingstream_error_t tsPpConfigFileGetClientKey(char *payload, char *clientKey)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element;
    cJSON_bool jBool;
    char *keys[] = {"MQTT", "Connectivity", "ClientCredentials", "Key"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = root;
            for (int i = 0; i < 4; i++) {
                jBool = cJSON_HasObjectItem(element, keys[i]);
                if (!jBool) {
                    XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keys[i]);
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
                } else {
                    element = cJSON_GetObjectItem(element, keys[i]);
                    ret = XPLR_THINGSTREAM_OK;
                }
            }
            if (ret == XPLR_THINGSTREAM_OK) {
                jBool = cJSON_IsString(element);
                if (jBool) {
                    XPLR_THINGSTREAM_CONSOLE(D, "Parsed Client Key from configuration payload");
                    if (strlen(element->valuestring) <= XPLR_THINGSTREAM_CERT_SIZE_MAX) {
                        strcpy(clientKey, element->valuestring);
                        ret = tsPpConfigFileFormatCert(clientKey, XPLR_THINGSTREAM_PP_SERVER_KEY);
                    } else {
                        XPLR_THINGSTREAM_CONSOLE(E, "Parsed Client Key larger tha CERT_SIZE_MAX!");
                        ret = XPLR_THINGSTREAM_ERROR;
                    }
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Invalid value for Client Key in configuration file!");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Parsing Failed!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

static xplr_thingstream_error_t tsPpConfigFileGetClientCert(char *payload, char *clientCert)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element;
    cJSON_bool jBool;
    char *keys[] = {"MQTT", "Connectivity", "ClientCredentials", "Cert"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = root;
            for (int i = 0; i < 4; i++) {
                jBool = cJSON_HasObjectItem(element, keys[i]);
                if (!jBool) {
                    XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keys[i]);
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
                } else {
                    element = cJSON_GetObjectItem(element, keys[i]);
                    ret = XPLR_THINGSTREAM_OK;
                }
            }
            if (ret == XPLR_THINGSTREAM_OK) {
                jBool = cJSON_IsString(element);
                if (jBool) {
                    XPLR_THINGSTREAM_CONSOLE(D, "Parsed Client Cert from configuration payload");
                    if (strlen(element->valuestring) <= XPLR_THINGSTREAM_CERT_SIZE_MAX) {
                        strcpy(clientCert, element->valuestring);
                        ret = tsPpConfigFileFormatCert(clientCert, XPLR_THINGSTREAM_PP_SERVER_CERT);
                    } else {
                        XPLR_THINGSTREAM_CONSOLE(E, "Parsed Client Cert larger than CERT_SIZE_MAX!");
                    }
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Invalid value for Client Cert in configuration file!");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Parsing Failed!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

static xplr_thingstream_error_t tsPpConfigFileGetRootCa(char *payload, char *rootCa)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element;
    cJSON_bool jBool;
    char *keys[] = {"MQTT", "Connectivity", "ClientCredentials", "RootCA"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = root;
            for (int i = 0; i < 4; i++) {
                jBool = cJSON_HasObjectItem(element, keys[i]);
                if (!jBool) {
                    XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keys[i]);
                    ret = XPLR_THINGSTREAM_ERROR;
                    break;
                } else {
                    element = cJSON_GetObjectItem(element, keys[i]);
                    ret = XPLR_THINGSTREAM_OK;
                }
            }
            if (ret == XPLR_THINGSTREAM_OK) {
                jBool = cJSON_IsString(element);
                if (jBool) {
                    XPLR_THINGSTREAM_CONSOLE(D, "Parsed Root CA from configuration payload");
                    if (strlen(element->valuestring) <= XPLR_THINGSTREAM_CERT_SIZE_MAX) {
                        strcpy(rootCa, element->valuestring);
                        ret = tsPpConfigFileFormatCert(rootCa, XPLR_THINGSTREAM_PP_SERVER_ROOTCA);
                    } else {
                        XPLR_THINGSTREAM_CONSOLE(E, "Parsed Root CA larger than CERT_MAX_SIZE");
                        ret = XPLR_THINGSTREAM_ERROR;
                    }
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Invalid value for Server URI in configuration file!");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Parsing Failed!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}
static xplr_thingstream_error_t tsPpConfigFileGetDynamicKeys(char *payload,
                                                             xplr_thingstream_pp_dKeys_t *dynamicKeys)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element, *currKey, *nextKey;
    cJSON_bool jBool, hasCurrKey, hasNextKey;
    char *value;
    char *keysFilter[] = {"MQTT", "dynamicKeys", "current", "next"};
    char *keysTags[] = {"duration", "start", "value"};

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, keysFilter[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = cJSON_GetObjectItem(root, keysFilter[0]);
            jBool = cJSON_HasObjectItem(element, keysFilter[1]);
            if (jBool) {
                element = cJSON_GetObjectItem(element, keysFilter[1]);
                hasCurrKey = cJSON_HasObjectItem(element, keysFilter[2]);
                hasNextKey = cJSON_HasObjectItem(element, keysFilter[3]);
                if (hasCurrKey && hasNextKey) {
                    currKey = cJSON_GetObjectItem(element, keysFilter[2]);
                    dynamicKeys->current.duration = cJSON_GetObjectItem(currKey, keysTags[0])->valuedouble;
                    dynamicKeys->current.start = cJSON_GetObjectItem(currKey, keysTags[1])->valuedouble;
                    value = cJSON_GetObjectItem(currKey, keysTags[2])->valuestring;
                    strcpy(dynamicKeys->current.value, value);
                    nextKey = cJSON_GetObjectItem(element, keysFilter[3]);
                    dynamicKeys->next.duration = cJSON_GetObjectItem(nextKey, keysTags[0])->valuedouble;
                    dynamicKeys->next.start = cJSON_GetObjectItem(nextKey, keysTags[1])->valuedouble;
                    value = cJSON_GetObjectItem(nextKey, keysTags[2])->valuestring;
                    strcpy(dynamicKeys->next.value, value);
                    ret = XPLR_THINGSTREAM_OK;
                } else {
                    XPLR_THINGSTREAM_CONSOLE(E, "Cannot find dynamic keys values in configuration file payload");
                    ret = XPLR_THINGSTREAM_ERROR;
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Could not find tag <%s> in configuration file", keysFilter[1]);
                ret = XPLR_THINGSTREAM_ERROR;
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }

    if (ret == XPLR_THINGSTREAM_OK) {
        XPLR_THINGSTREAM_CONSOLE(D,
                                 "\nDynamic keys parsed:\
                                  \nCurrent key:\n\t start (UTC):%llu\n\t duration (UTC):%llu\n\t value:%s\
                                  \nNext key:\n\t start (UTC):%llu\n\t duration (UTC):%llu\n\t value:%s\n",
                                 dynamicKeys->current.start,
                                 dynamicKeys->current.duration,
                                 dynamicKeys->current.value,
                                 dynamicKeys->next.start,
                                 dynamicKeys->next.duration,
                                 dynamicKeys->next.value);
    }
    return ret;
}

static xplr_thingstream_error_t tsPpConfigFileParseTopicsInfoByRegionAll(char *payload,
                                                                         xplr_thingstream_pp_region_t region,
                                                                         bool lbandOverIpPreference,
                                                                         xplr_thingstream_pp_settings_t *settings)
{
    xplr_thingstream_error_t ret;
    cJSON *root, *element, *keyDist, *corrData;
    cJSON_bool jBool, hasCorrData, hasKeyDist;
    char *subKeys[] = {"MQTT", "Subscriptions", "Key", "Data"};
    const char *secTopicsDesc[] = {thingstreamPpFilterGAD, thingstreamPpFilterHPAC, thingstreamPpFilterOCB, thingstreamPpFilterClock, thingstreamPpFilterFreq};
    char *token, *corrTopic, *secondaryTopics;
    int arraySize, index;

    if (payload != NULL) {
        ret = tsApiMsgParsePpZtpCheckTag(payload, subKeys[0]);
        if (ret == XPLR_THINGSTREAM_OK) {
            root = cJSON_Parse(payload);
            element = cJSON_GetObjectItem(root, subKeys[0]);
            jBool = cJSON_HasObjectItem(element, subKeys[1]);
            if (jBool) {
                element = cJSON_GetObjectItem(element, subKeys[1]);
                hasKeyDist = cJSON_HasObjectItem(element, subKeys[2]);
                hasCorrData = cJSON_HasObjectItem(element, subKeys[3]);
                if (hasKeyDist && hasCorrData) {
                    // Parse Key Distribution Topic path and QoS
                    keyDist = cJSON_GetObjectItem(element, subKeys[2]);
                    settings->topicList[0].qos = cJSON_GetObjectItem(keyDist, "QoS")->valueint;
                    keyDist = cJSON_GetObjectItem(keyDist, "KeyTopics");
                    keyDist = cJSON_GetArrayItem(keyDist, 0);
                    strcpy(settings->topicList[0].path, keyDist->valuestring);
                    strcpy(settings->topicList[0].description, thingstreamPpFilterKeyDist);
                    settings->numOfTopics = 1;
                    // Check IP or LBAND support
                    if (strstr(settings->topicList[0].path, "Lb") != NULL) {
                        settings->lbandSupported = true;
                    } else if (strstr(settings->topicList[0].path, "ip") != NULL) {
                        settings->mqttSupported = true;
                        settings->lbandSupported = false;
                    } else {
                        settings->mqttSupported = false;
                        settings->lbandSupported = false;
                    }
                    if (!settings->mqttSupported && !settings->lbandSupported) {
                        XPLR_THINGSTREAM_CONSOLE(E, "Error regarding subscription type to Thingstream!");
                        ret = XPLR_THINGSTREAM_ERROR;
                    } else {
                        // Parse Correction Data Topics
                        corrData = cJSON_GetObjectItem(element, subKeys[3]);
                        settings->topicList[1].qos = cJSON_GetObjectItem(corrData, "QoS")->valueint;
                        corrData = cJSON_GetObjectItem(corrData, "DataTopics");
                        arraySize = cJSON_GetArraySize(corrData);
                        if (arraySize == 1) {
                            // No correction data topics so plan is LBAND
                            settings->mqttSupported = false;
                            // LBAND plan is supported only in EU and US regions currently
                            if (region == XPLR_THINGSTREAM_PP_REGION_EU || region == XPLR_THINGSTREAM_PP_REGION_US) {
                                strcpy(settings->topicList[1].path, "/pp/frequencies/Lb");
                                strcpy(settings->topicList[1].description, thingstreamPpFilterFreq);
                                settings->numOfTopics++;
                                tsPpSetDescFilter(settings);
                                // We are done! All topics are parsed
                                ret = XPLR_THINGSTREAM_OK;
                            } else {
                                XPLR_THINGSTREAM_CONSOLE(E, "Correction via LBAND is not supported in your region");
                                ret = XPLR_THINGSTREAM_ERROR;
                            }
                        } else if (lbandOverIpPreference) {
                            // IPLBAND plan with LBAND correction source preference, configure only LBAND topics
                            settings->mqttSupported = false;
                            // LBAND plan is supported only in EU and US regions currently
                            if (region == XPLR_THINGSTREAM_PP_REGION_EU || region == XPLR_THINGSTREAM_PP_REGION_US) {
                                strcpy(settings->topicList[1].path, "/pp/frequencies/Lb");
                                strcpy(settings->topicList[1].description, thingstreamPpFilterFreq);
                                settings->numOfTopics++;
                                tsPpSetDescFilter(settings);
                                // We are done! All topics are parsed
                                ret = XPLR_THINGSTREAM_OK;
                            } else {
                                XPLR_THINGSTREAM_CONSOLE(E, "Correction via LBAND is not supported in your region");
                                ret = XPLR_THINGSTREAM_ERROR;
                            }
                        } else {
                            // Select topics based on region
                            switch (region) {
                                case XPLR_THINGSTREAM_PP_REGION_EU:
                                    corrTopic = cJSON_GetArrayItem(corrData, 0)->valuestring;
                                    secondaryTopics = cJSON_GetArrayItem(corrData, 1)->valuestring;
                                    settings->mqttSupported = true;
                                    break;
                                case XPLR_THINGSTREAM_PP_REGION_US:
                                    corrTopic = cJSON_GetArrayItem(corrData, 2)->valuestring;
                                    secondaryTopics = cJSON_GetArrayItem(corrData, 3)->valuestring;
                                    settings->mqttSupported = true;
                                    break;
                                case XPLR_THINGSTREAM_PP_REGION_KR:
                                    if (settings->lbandSupported == true) {
                                        // Plan is IPLBAND
                                        XPLR_THINGSTREAM_CONSOLE(E, "IPLBAND plan is not supported in Korea");
                                        corrTopic = NULL;
                                        secondaryTopics = NULL;
                                        settings->mqttSupported = false;
                                        /* No LBAND support */
                                        settings->lbandSupported = false;
                                    } else {
                                        // Plan is IP
                                        corrTopic = cJSON_GetArrayItem(corrData, 4)->valuestring;
                                        secondaryTopics = cJSON_GetArrayItem(corrData, 5)->valuestring;
                                        settings->mqttSupported = true;
                                    }
                                    break;
                                case XPLR_THINGSTREAM_PP_REGION_AU:
                                    if (settings->lbandSupported == true) {
                                        // Plan is IPLBAND
                                        corrTopic = cJSON_GetArrayItem(corrData, 4)->valuestring;
                                        secondaryTopics = NULL;
                                        settings->mqttSupported = true;
                                        /* No LBAND support */
                                        settings->lbandSupported = false;
                                    } else {
                                        // Plan is IP
                                        corrTopic = cJSON_GetArrayItem(corrData, 6)->valuestring;
                                        secondaryTopics = NULL;
                                        settings->mqttSupported = true;
                                    }
                                    break;
                                case XPLR_THINGSTREAM_PP_REGION_JP:
                                    if (settings->lbandSupported == true) {
                                        // Plan is IPLBAND
                                        corrTopic = cJSON_GetArrayItem(corrData, 6)->valuestring;
                                        secondaryTopics = cJSON_GetArrayItem(corrData, 7)->valuestring;
                                        settings->mqttSupported = true;
                                        /* No LBAND support */
                                        settings->lbandSupported = false;
                                    } else {
                                        // Plan is IP
                                        corrTopic = cJSON_GetArrayItem(corrData, 8)->valuestring;
                                        secondaryTopics = cJSON_GetArrayItem(corrData, 9)->valuestring;
                                        settings->mqttSupported = true;
                                    }
                                    break;
                                case XPLR_THINGSTREAM_PP_REGION_INVALID:
                                default:
                                    XPLR_THINGSTREAM_CONSOLE(E, "Region not supported!");
                                    corrTopic = NULL;
                                    secondaryTopics = NULL;
                                    settings->mqttSupported = false;
                            }
                            if (settings->mqttSupported) {
                                strcpy(settings->topicList[1].path, corrTopic);
                                if (settings->lbandSupported) {
                                    strcpy(settings->topicList[1].description, thingstreamPpFilterCorrectionDataIpLb);
                                } else {
                                    strcpy(settings->topicList[1].description, thingstreamPpFilterCorrectionDataIp);
                                }
                                settings->numOfTopics++;
                                // Let's separate the secondary topics
                                // Get first token
                                token = strtok(secondaryTopics, ";");
                                index = 0;
                                // Get the rest of the tokens
                                while (token != NULL &&
                                       index < 5 &&
                                       settings->numOfTopics < XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX) {
                                    strcpy(settings->topicList[settings->numOfTopics].path, token);
                                    strcpy(settings->topicList[settings->numOfTopics].description, secTopicsDesc[index]);
                                    // All data topics share the same QoS
                                    settings->topicList[settings->numOfTopics].qos = settings->topicList[1].qos;
                                    settings->numOfTopics++;
                                    index++;
                                    token = strtok(NULL, ";");
                                }
                                tsPpSetDescFilter(settings);
                                // In IPLBAND plan and EU region the frequencies topic is missing, so we will add it manually...
                                if (region == XPLR_THINGSTREAM_PP_REGION_EU &&
                                    settings->mqttSupported &&
                                    settings->lbandSupported) {
                                    strcpy(settings->topicList[settings->numOfTopics].path, "/pp/frequencies/Lb");
                                    strcpy(settings->topicList[settings->numOfTopics].description, secTopicsDesc[index]);
                                    settings->topicList[settings->numOfTopics].qos = settings->topicList[1].qos;
                                    settings->numOfTopics++;
                                    index++;
                                }
                                // We are done! All topics are parsed
                                ret = XPLR_THINGSTREAM_OK;
                            } else {
                                ret = XPLR_THINGSTREAM_ERROR;
                            }
                        }
                    }
                }
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Subscription tag not found in configuration file payload!");
                ret = XPLR_THINGSTREAM_ERROR;
            }
            cJSON_Delete(root);
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Configuration file invalid tags");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "NULL pointer to file payload!");
        ret = XPLR_THINGSTREAM_ERROR;
    }
    return ret;
}

static xplr_thingstream_error_t tsPpConfigFileFormatCert(char *cert,
                                                         xplr_thingstream_pp_serverInfo_type_t type)
{
    esp_err_t espRet;
    xplr_thingstream_error_t ret;
    xplr_common_cert_type_t commonCertType;

    switch (type) {
        case XPLR_THINGSTREAM_PP_SERVER_INVALID:
            commonCertType = XPLR_COMMON_CERT_INVALID;
            break;
        case XPLR_THINGSTREAM_PP_SERVER_ADDRESS:
            commonCertType = XPLR_COMMON_CERT_INVALID;
            break;
        case XPLR_THINGSTREAM_PP_SERVER_CERT:
            commonCertType = XPLR_COMMON_CERT;
            break;
        case XPLR_THINGSTREAM_PP_SERVER_KEY:
            commonCertType = XPLR_COMMON_CERT_KEY;
            break;
        case XPLR_THINGSTREAM_PP_SERVER_ID:
            commonCertType = XPLR_COMMON_CERT_INVALID;
            break;
        case XPLR_THINGSTREAM_PP_SERVER_ROOTCA:
            commonCertType = XPLR_COMMON_CERT_ROOTCA;
            break;
        default:
            commonCertType = XPLR_COMMON_CERT_INVALID;
            break;
    }

    espRet = xplrPpConfigFileFormatCert(cert, commonCertType, true);
    if (espRet == ESP_OK) {
        ret = XPLR_THINGSTREAM_OK;
    } else {
        ret = XPLR_THINGSTREAM_ERROR;
    }

    return ret;
}

static xplr_thingstream_error_t tsCommThingParserCheckSize(const char *start,
                                                           const char *end,
                                                           size_t size)
{
    xplr_thingstream_error_t ret;

    if (end > start && start != NULL && end != NULL && size != 0) {
        if (sizeof(end - start) <= size) {
            ret = XPLR_THINGSTREAM_OK;
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Check size failed for size");
            ret = XPLR_THINGSTREAM_ERROR;
        }
    } else {
        XPLR_THINGSTREAM_CONSOLE(E, "Check size was given empty pointer or wrong size");
        ret = XPLR_THINGSTREAM_ERROR;
    }

    return ret;
}

static xplr_thingstream_error_t tsCommThingGetCredential(char *payload,
                                                         char *credential,
                                                         xplr_thingstream_comm_cred_type_t credType)
{
    xplr_thingstream_error_t ret;
    const char *startStr, *endStr;
    char *start, *end;
    size_t size;

    switch (credType) {
        case XPLR_THINGSTREAM_COMM_CRED_SERVER_URL:
            startStr = tsCommThingServerUrlStart;
            endStr = tsCommThingServerUrlEnd;
            size = XPLR_THINGSTREAM_URL_SIZE_MAX;
            break;
        case XPLR_THINGSTREAM_COMM_CRED_DEVICE_ID:
            startStr = tsCommThingClientIdStart;
            endStr = tsCommThingClientIdEnd;
            size = XPLR_THINGSTREAM_CLIENTID_MAX;
            break;
        case XPLR_THINGSTREAM_COMM_CRED_USERNAME:
            startStr = tsCommThingUsernameStart;
            endStr = tsCommThingUsernameEnd;
            size = XPLR_THINGSTREAM_USERNAME_MAX;
            break;
        case XPLR_THINGSTREAM_COMM_CRED_PASSWORD:
            startStr = tsCommThingPasswordStart;
            endStr = tsCommThingPasswordEnd;
            size = XPLR_THINGSTREAM_PASSWORD_MAX;
            break;
        case XPLR_THINGSTREAM_COMM_CRED_CERT:
        case XPLR_THINGSTREAM_COMM_CRED_KEY:
        case XPLR_THINGSTREAM_COMM_CRED_INVALID:
        default:
            XPLR_THINGSTREAM_CONSOLE(E, "Error in credential type!");
            startStr = NULL;
            endStr = NULL;
            break;
    }

    // Null pointer check
    if (payload == NULL || startStr == NULL || endStr == NULL) {
        XPLR_THINGSTREAM_CONSOLE(E, "Error in parsing credentials due to NULL pointer!");
        ret = XPLR_THINGSTREAM_ERROR;
    } else {
        start = strstr(payload, startStr);
        end = strstr(payload, endStr);
        // Check size
        ret = tsCommThingParserCheckSize(start, end, size);
        if (ret == XPLR_THINGSTREAM_OK) {
            size = end - start - strlen(startStr);
            strncpy(credential, start + strlen(startStr), size);
            if (credential != NULL && credential[0] != 0) {
                ret = XPLR_THINGSTREAM_OK;
            } else {
                XPLR_THINGSTREAM_CONSOLE(E, "Tags : <%s><%s> contain no credential...", startStr, endStr);
                ret = XPLR_THINGSTREAM_ERROR;
            }
        } else {
            XPLR_THINGSTREAM_CONSOLE(E,
                                     "Tags : <%s><%s> contain credential larger than <%u> bytes",
                                     startStr,
                                     endStr,
                                     size);
            ret = XPLR_THINGSTREAM_ERROR;
        }
    }

    return ret;
}

static void tsPpSetDescFilter(xplr_thingstream_pp_settings_t *settings)
{
    if (settings->mqttSupported) {
        if (settings->lbandSupported) {
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataIpLb;
            XPLR_THINGSTREAM_CONSOLE(D, "IP + L-Band plan does support correction data via MQTT!");
        } else {
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataIp;
            XPLR_THINGSTREAM_CONSOLE(D, "IP plan does support correction data via MQTT!");
        }
    } else {
        if (settings->lbandSupported) {
            thingstreamPpFilterCorrectionData = thingstreamPpFilterCorrectionDataLb;
            XPLR_THINGSTREAM_CONSOLE(D, "L-Band plan. Frequency and decryption keys will be fetched via MQTT!");
        } else {
            XPLR_THINGSTREAM_CONSOLE(E, "Invalid plan.");
        }
    }
}
// End of file

xplr_thingstream_pp_region_t xplrThingstreamRegionFromStr(const char *regionStr)
{
    xplr_thingstream_pp_region_t regionTs;

    if (strncmp(regionStr, "EU", 2) == 0) {
        regionTs = XPLR_THINGSTREAM_PP_REGION_EU;
    } else if (strncmp(regionStr, "US", 2) == 0) {
        regionTs = XPLR_THINGSTREAM_PP_REGION_US;
    } else if (strncmp(regionStr, "KR", 2) == 0) {
        regionTs = XPLR_THINGSTREAM_PP_REGION_KR;
    } else if (strncmp(regionStr, "AU", 2) == 0) {
        regionTs = XPLR_THINGSTREAM_PP_REGION_AU;
    } else if (strncmp(regionStr, "JP", 2) == 0) {
        regionTs = XPLR_THINGSTREAM_PP_REGION_JP;
    } else {
        regionTs = XPLR_THINGSTREAM_PP_REGION_INVALID;
    }

    return regionTs;
}

xplr_thingstream_pp_plan_t xplrThingstreamPlanFromStr(const char *planStr)
{
    xplr_thingstream_pp_plan_t planTs;

    if (strncmp(planStr, "IP+LBAND", strlen("IP+LBAND")) == 0) {
        planTs = XPLR_THINGSTREAM_PP_PLAN_IPLBAND;
    } else if (strncmp(planStr, "IP", strlen("IP")) == 0) {
        planTs = XPLR_THINGSTREAM_PP_PLAN_IP;
    } else if (strncmp(planStr, "LBAND", strlen("LBAND")) == 0) {
        planTs = XPLR_THINGSTREAM_PP_PLAN_LBAND;
    } else {
        planTs = XPLR_THINGSTREAM_PP_PLAN_INVALID;
    }

    return planTs;
}