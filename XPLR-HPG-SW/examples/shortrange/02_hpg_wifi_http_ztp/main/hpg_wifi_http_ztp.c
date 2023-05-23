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

/*
 * An example for Zero Touch Provisioning (ZTP).
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * makes an HTTPS POST request to Thingstream's ZTP credentials URL using xplr_ztp component,
 * gets different data such as key-certificate, MQTT EU-topics, decryption keys by parsing the HTTPS reply parsed by using xplr_ztp_json_parser component
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "xplr_ztp.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "xplr_wifi_starter.h"
#include "xplr_ztp_json_parser.h"

/**
 * If paths not found in VScode: 
 *      press keys --> <ctrl+shift+p> 
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define  APP_SERIAL_DEBUG_ENABLED 1U /* used to print debug messages in console. Set to 0 for disabling */
#if defined (APP_SERIAL_DEBUG_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)   esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/*
 * Simple macros used to set buffer sizes in bytes
 */
#define KIB                         (1024U)
#define APP_ZTP_PAYLOAD_BUF_SIZE    ((10U) * (KIB))
#define APP_KEYCERT_PARSE_BUF_SIZE  ((2U) * (KIB))

#define APP_TOPICS_ARRAY_MAX_SIZE   (25U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/**
 * Populate the root.crt file according to your needs.
 * If you are using Thingstream then you can find all the needed
 * certificates inside your location thing settings.
 */
extern const uint8_t server_root_crt_start[] asm("_binary_root_crt_start");
extern const uint8_t server_root_crt_end[] asm("_binary_root_crt_end");

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * Required data to make a POST request to Thingstream
 * The data are taken from KConfig or you can overwrite the 
 * values as needed here.
 */
static const char *ztpPostUrl = CONFIG_XPLR_TS_PP_ZTP_CREDENTIALS_URL;
static xplrZtpDevicePostData_t devPostData = {
    .dvcToken = CONFIG_XPLR_TS_PP_ZTP_TOKEN,
    .dvcName = CONFIG_XPLR_TS_PP_DEVICE_NAME
};

/**
 * Some buffers to experiment with
 */
static char charbuf[APP_KEYCERT_PARSE_BUF_SIZE];

/**
 * ZTP data we got from post
 */
static char payload[APP_ZTP_PAYLOAD_BUF_SIZE];
static xplrZtpData_t ztpData = {
    .payload = payload,
    .payloadLength = APP_ZTP_PAYLOAD_BUF_SIZE
};


/**
 * Topics we get from ZTP JSON parser.
 * We deliberately declare more than needed (25) topics to
 * show how the struct works.
 * populatedCount shows how many topics we really got from
 * the parser.
 * maxCount limits the number of topics.
 */
static xplrTopic topics[APP_TOPICS_ARRAY_MAX_SIZE];
static xplrZtpStyleTopics_t ztpStyleTopics = {
    .topic = topics,
    .maxCount = APP_TOPICS_ARRAY_MAX_SIZE,
    .populatedCount = 0
};

/**
 * Dynamic keys storage
 */
static xplrDynamicKeys_t dynamicKeys;

/**
 * A cJSON object to store JSON reply from ZTP POST
 */
static cJSON *json;

/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the 
 * values as needed here.
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA,
    .webserver = false
};

/**
 * Boolean flags for different functions
 */
static bool gotZtp;
static bool mqttFlag;

/**
 * Error return types
 */
static esp_err_t ret;
xplrWifiStarterError_t wifistarterErr;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitBoard(void);
static void appInitWiFi(void);
static void appZtpJsonParse(void);
static void appZtpMqttCertificateParse(void);
static void appZtpMqttClientIdParse(void);
static void appZtpDeallocateJSON(void);
static void appZtpMqttSubscriptionsParse(void);
static void appZtpMqttSupportParse(void);
static void appZtpMqttDynamicKeysParse(void);

void app_main(void)
{
    gotZtp = false;

    appInitBoard();
    appInitWiFi();

    while (1) {
        wifistarterErr = xplrWifiStarterFsm();
        if (wifistarterErr == XPLR_WIFISTARTER_ERROR) {
            APP_CONSOLE(E, "xplrWifiStarterFsm returned ERROR!");
        }

        switch (xplrWifiStarterGetCurrentFsmState()) {
            case XPLR_WIFISTARTER_STATE_CONNECT_OK:
                if (!gotZtp) {
                    APP_CONSOLE(I, "Performing HTTPS POST request.");
                    ret = xplrZtpGetPayload((const char *)server_root_crt_start, 
                                            ztpPostUrl, 
                                            &devPostData, 
                                            &ztpData);
                    if (ret != ESP_OK) {
                        APP_CONSOLE(E, "Performing HTTPS POST failed!");
                    } else {
                        if (ztpData.httpReturnCode == HttpStatus_Ok) {
                            appZtpJsonParse();
                            appZtpMqttCertificateParse();
                            appZtpMqttClientIdParse();
                            appZtpMqttSubscriptionsParse();
                            appZtpMqttSupportParse();
                            appZtpMqttDynamicKeysParse();
                            appZtpDeallocateJSON();
                            xplrWifiStarterDisconnect();
                        } else {
                            APP_CONSOLE(W, "HTTPS request returned code: %d", ztpData.httpReturnCode);
                        }
                    }
                    gotZtp = true;
                }
                break;

            case XPLR_WIFISTARTER_STATE_UNKNOWN:
            case XPLR_WIFISTARTER_STATE_TIMEOUT:
            case XPLR_WIFISTARTER_STATE_ERROR:
                APP_CONSOLE(W, "Major error encountered. Will exit!");
                abort();
                break;

            default:
                break;

        }

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_DISCONNECT_OK) {
            break;
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    APP_CONSOLE(I, "ALL DONE!!!");
}


/*
 * Initialize XPLR-HPG kit using its board file
 */
static void appInitBoard(void)
{
    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        abort();
    }
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    ret = xplrWifiStarterInitConnection(&wifiOptions);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
    }
}

/*
 * Allocation for JSON parsing.
 * From this object we can take all the data we are interested in.
 * Remember to deallocate the json object after we are done processing it.
 * In case the JSON object is invalid then the returned item will be NULL.
 */
static void appZtpJsonParse(void)
{
    json = cJSON_Parse(ztpData.payload);
    if (json == NULL) {
        APP_CONSOLE(E, "cJSON parsing failed!");
        APP_CONSOLE(E, "Seems like the JSON payload is not valid!");
        abort();
    }
}

/*
 * Getting key-certificate value
 * We will use the key-certificate just for printing
 */
static void appZtpMqttCertificateParse(void)
{
    if (xplrJsonZtpGetMqttCertificate(json, charbuf, APP_KEYCERT_PARSE_BUF_SIZE) == XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(I, "Parsed Certificate:\n%s", charbuf);
    } else {
        APP_CONSOLE(E, "Parsing Certificate failed!");
        abort();
    }
}

/*
 * Returns Client ID for MQTT
 */
static void appZtpMqttClientIdParse(void)
{
    if (xplrJsonZtpGetMqttClientId(json, charbuf, APP_KEYCERT_PARSE_BUF_SIZE) == XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(I, "Parsed MQTT client ID: %s", charbuf);
    } else {
        APP_CONSOLE(E, "Parsing MQTT client ID failed!");
        abort();
    }
}

/*
 * Fetches array with topics to subscribe
 * In this case: EU region
 */
static void appZtpMqttSubscriptionsParse(void)
{
    ztpStyleTopics.populatedCount = 0;
    if (xplrJsonZtpGetRequiredTopicsByRegion(json, &ztpStyleTopics,
                                             XPLR_ZTP_REGION_EU) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing required MQTT topics failed!");
    }
}

/*
 * Checks if MQTT is supported
 */
static void appZtpMqttSupportParse(void)
{
    if (xplrJsonZtpSupportsMqtt(json, &mqttFlag) == XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(I, "Is MQTT supported: %s", mqttFlag ? "true" : "false");
    } else {
        APP_CONSOLE(E, "Parsing MQTT support flag failed!");
        abort();
    }
}

/*
 * Parses dynamic keys
 */
static void appZtpMqttDynamicKeysParse()
{
    if (xplrJsonZtpGetDynamicKeys(json, &dynamicKeys) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing MQTT support flag failed!");
        abort();
    }
}

/*
 * Deallocate JSON after we are done
 */
static void appZtpDeallocateJSON(void)
{
    if (json != NULL) {
        APP_CONSOLE(I, "Deallocating JSON object.");
        cJSON_Delete(json);
    }
}