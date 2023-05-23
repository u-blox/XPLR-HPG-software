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
 * An example for MQTT connection to Thingstream (U-blox broker) using ZTP and correction data for the GNSS module.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit, 
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream, using the Zero Touch Provisioning (referred as ZTP from now on) to achieve a connection to the MQTT broker
 * subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * sets up GNSS module using location_service component,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "xplr_wifi_starter.h"
#include "xplr_ztp_json_parser.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
#include "xplr_ztp.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "u_cfg_app_platform_specific.h"
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
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

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
 * Simple macros used to set buffer size in bytes
 */
#define KIB                         (1024U)
#define APP_ZTP_PAYLOAD_BUF_SIZE    ((10U) * (KIB))
#define APP_KEYCERT_PARSE_BUF_SIZE  ((2U) * (KIB))
#define APP_MQTT_PAYLOAD_BUF_SIZE   ((10U) * (KIB))
#define APP_MQTT_CLIENT_ID_BUF_SIZE (128U)
#define APP_MQTT_HOST_BUF_SIZE      (128U)

/**
 * Period in seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD  5

/**
 * Max topic count
 */
#define APP_MAX_TOPIC_CNT   2

/**
 * GNSS I2C address in hex
 */
#define XPLR_GNSS_I2C_ADDR  0x42

/**
 * MQTT topics for correction data and decryption keys
 */
#define APP_KEYS_TOPIC              "/pp/ubx/0236/Lb"
#define APP_CORRECTION_DATA_TOPIC   "/pp/Lb/eu"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/**
 * Populate the following files according to your needs.
 * If you are using Thingstream then you can find all the needed
 * certificates inside your location thing settings.
 */
extern const uint8_t server_root_crt_start[] asm("_binary_root_crt_start");
extern const uint8_t server_root_crt_end[] asm("_binary_root_crt_end");

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the 
 * values as needed here.
 */
static const char *ztpPostUrl = CONFIG_XPLR_TS_PP_ZTP_CREDENTIALS_URL;
static xplrZtpDevicePostData_t devPostData = {
    .dvcToken = CONFIG_XPLR_TS_PP_ZTP_TOKEN,
    .dvcName = CONFIG_XPLR_TS_PP_DEVICE_NAME
};

/**
 * Buffers used to store data
 */
static char cert[APP_KEYCERT_PARSE_BUF_SIZE];
static char privateKey[APP_KEYCERT_PARSE_BUF_SIZE];
static char mqttClientId[APP_MQTT_CLIENT_ID_BUF_SIZE];
static char mqttHost[APP_MQTT_HOST_BUF_SIZE];

/**
 * ZTP payload from POST
 */
static char ztpPostPayload[APP_ZTP_PAYLOAD_BUF_SIZE];
xplrZtpData_t ztpData = {
    .payload = ztpPostPayload,
    .payloadLength = APP_ZTP_PAYLOAD_BUF_SIZE
};

/**
 * ZTP style topics to subscribe to
 */
static xplrTopic topics[APP_MAX_TOPIC_CNT];
static xplrZtpStyleTopics_t ztpStyleTopics = {
    .topic = topics,
    .maxCount = APP_MAX_TOPIC_CNT,
    .populatedCount = 0
};

/**
 * A cJSON object to store JSON reply from ZTP POST
 */
static cJSON *json;

/**
 * Keeps time at "this" point of code execution.
 * Used to calculate elapse time.
 */
static uint64_t timeNow;

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
 * Providing MQTT client configuration
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;

/**
 * A struct where we can store our received MQTT message
 */
static char data[APP_MQTT_PAYLOAD_BUF_SIZE];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = ELEMENTCNT(data)
};

/**
 * Boolean flags for different functions
 */
static bool requestDc;
static bool gotZtp;
static bool mqttFlag;

/**
 * Error return types
 */
static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitBoard(void);
static void appInitWiFi(void);
static void appInitGnssDevice(void);
static esp_err_t appDoZtp(void);
static void appMqttInit(void);
static void appMqttExecParsers(void);
static void appJsonParse(void);
static void appMqttCertificateParse(void);
static void appMqttClientIdParse(void);
static void appDeallocateJSON(void);
static void appMqttSubscriptionsParse(void);
static void appMqttSupportParse(void);
static void appMqttHostParse(void);
static void appMqttPrivateKeyParse(void);
static void appPrintLocation(uint8_t periodSecs);
static void appHaltExecution(void);

void app_main(void)
{
    gotZtp = false;

    appInitBoard();
    appInitWiFi();
    appInitGnssDevice();
    xplrGnssPrintDeviceInfo(0);
    xplrMqttWifiInitState(&mqttClient);

    while (1) {
        wifistarterErr = xplrWifiStarterFsm();

        /**
         * If we connect to WiFi we can continue with ZTP and
         * then with MQTT
         */
        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (!gotZtp) {
                if (appDoZtp() != ESP_OK) {
                    APP_CONSOLE(E, "ZTP failed");
                    appHaltExecution();
                }

                /**
                 * Since MQTT is supported we can initialize the MQTT broker and try to connect
                 */
                if (mqttFlag) {
                    appMqttInit();
                    xplrMqttWifiStart(&mqttClient);
                    requestDc = false;
                }
            }
        }

        /**
         * This sample uses ZTP to function which gets all required setting to connect
         * to Thingstream services such as PointPerfect.
         */
        mqttErr = xplrMqttWifiFsm(&mqttClient);

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                /**
                 * Lets try to use ZTP format topics to subscribe
                 */
                espRet = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClient, &ztpStyleTopics, 0);
                if (espRet != ESP_OK) {
                    APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArrayZtp failed");
                    appHaltExecution();
                }
                break;

            /**
             * If we are subscribed to a topic we can start sending messages to our
             * GNSS module: decryption keys and correction data
             */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                /**
                 * The following function will digest messages and store them in the internal buffer.
                 * If the user does not use it it will be discarded.
                 */
                if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                    if (strcmp(mqttMessage.topic, APP_KEYS_TOPIC) == 0) {
                        espRet = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to send decryption keys!");
                            appHaltExecution();
                        }
                    }

                    if (strcmp(mqttMessage.topic, APP_CORRECTION_DATA_TOPIC) == 0) {
                        espRet = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to send correction data!");
                        }
                    }
                }
                break;

            default:
                break;
        }

        /**
         * Print location every APP_LOCATION_PRINT_PERIOD secs
         */
        appPrintLocation(APP_LOCATION_PRINT_PERIOD);

        /**
         * We lost WiFi connection.
         * When we reconnect everything will be executed from the beginning:
         * - ZTP gets the settings again
         * - Try to re-connect to MQTT
         * We are using xplrMqttWifiHardDisconnect() because autoreconnect is enabled
         * by default in the client's settings. This is a default behaviour
         * of the component provided by ESP-IDF.
         * You can change this by setting
         *      disable_auto_reconnect = false
         * in esp_mqtt_client_config_t
         * When autoreconnect is enabled then the client will always try
         * to reconnect, even if the user requests a disconnect.
         * If you request a hard disconnect then the client handler
         * and callback are destroyed and so is autoreconnect.
         */
        if ((requestDc == false) && 
            ((xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_DISCONNECT_OK) ||
             (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_SCHEDULE_RECONNECT))) {
            
            if (mqttClient.handler != NULL) {
                xplrMqttWifiHardDisconnect(&mqttClient);
            }

            requestDc = true;
            gotZtp = false;
        }

        /**
         * A window so other tasks can run
         */
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/*
 * Initialize XPLR-HPG kit using its board file
 */
static void appInitBoard(void)
{
    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    }
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    }
}

/**
 * Makes all required initializations
 */
static void appInitGnssDevice(void)
{
    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }


    APP_CONSOLE(D, "Waiting for GNSS device to come online!");
    espRet = xplrGnssStartDeviceDefaultSettings(0, XPLR_GNSS_I2C_ADDR);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    espRet = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_IP);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to set correction data source!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Execute ZTP and fetch settings to start MQTT
 */
static esp_err_t appDoZtp(void)
{
    APP_CONSOLE(I, "Performing HTTPS POST request.");
    espRet = xplrZtpGetPayload((const char *)server_root_crt_start, 
                               ztpPostUrl, 
                               &devPostData, 
                               &ztpData);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Performing HTTPS POST failed!");
        return ESP_FAIL;
    } else {
        if (ztpData.httpReturnCode == HttpStatus_Ok) {
            appMqttExecParsers();
        } else {
            APP_CONSOLE(E, "ZTP MQTT is not supported!");
        }
    }

    gotZtp = true;

    return ESP_OK;
}

/**
 * Executes all parsing function in a batch
 */
static void appMqttExecParsers(void)
{
    appJsonParse();
    appMqttCertificateParse();
    appMqttPrivateKeyParse();
    appMqttClientIdParse();
    appMqttSubscriptionsParse();
    appMqttSupportParse();
    appMqttHostParse();
    appDeallocateJSON();
}

/**
 * Populate some example settings
 */
static void appMqttInit(void)
{
    /**
     * We declare how many slots a ring buffer should have.
     * You can chose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    mqttClient.ucd.ringBufferSlotsNumber = 3;

    /**
     * Settings for client
     * If ZTP is successful then all the following settings
     * will be populated
     */
    mqttClientConfig.uri = mqttHost;
    mqttClientConfig.client_id = mqttClientId;
    mqttClientConfig.client_cert_pem = (const char *)cert;
    mqttClientConfig.client_key_pem = (const char *)privateKey;
    mqttClientConfig.cert_pem = (const char *)server_root_crt_start;
    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Let's start our client. 
     * In case of multiple clients an array of clients can be used. 
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/*
 * Allocation for JSON parsing.
 * From this object we can take all the data we are interested in.
 * Remember to deallocate the json file after we are done precessing it.
 * In case the JSON object is invalid then the returned item will be NULL.
 */
static void appJsonParse(void)
{
    json = cJSON_Parse(ztpData.payload);
    if (json == NULL) {
        APP_CONSOLE(E, "cJSON parsing failed!");
        APP_CONSOLE(E, "Seems like the JSON payload is not valid!");
        appHaltExecution();
    }
}

/*
 * Getting certificate value
 * Please refer to Readme and the Thingstream documentation
 * for more fields and their uses
 */
static void appMqttCertificateParse(void)
{
    if (xplrJsonZtpGetMqttCertificate(json, cert, APP_KEYCERT_PARSE_BUF_SIZE) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing Certificate failed!");
        appHaltExecution();
    }
}

/**
 * Parse private key
 */
static void appMqttPrivateKeyParse(void)
{
    if (xplrJsonZtpGetPrivateKey(json, privateKey, APP_KEYCERT_PARSE_BUF_SIZE) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing Private Key failed!");
        appHaltExecution();
    }
}

/*
 * Returns Client ID for MQTT
 */
static void appMqttClientIdParse(void)
{
    if (xplrJsonZtpGetMqttClientId(json, mqttClientId, APP_MQTT_CLIENT_ID_BUF_SIZE) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing MQTT client ID failed!");
        appHaltExecution();
    }
}

/*
 * Fetches array with topics to subscribe
 */
static void appMqttSubscriptionsParse(void)
{
    ztpStyleTopics.populatedCount = 0;
    if (xplrJsonZtpGetRequiredTopicsByRegion(json, &ztpStyleTopics,
                                             XPLR_ZTP_REGION_EU) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing required MQTT topics failed!");
        appHaltExecution();
    }
}

/*
 * Fetches host
 */
static void appMqttHostParse(void)
{
    if (xplrJsonZtpGetBrokerHost(json, mqttHost, APP_MQTT_HOST_BUF_SIZE) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing host failed!");
        appHaltExecution();
    }
}

/*
 * Checks if MQTT is supported
 */
static void appMqttSupportParse(void)
{
    if (xplrJsonZtpSupportsMqtt(json, &mqttFlag) != XPLR_JSON_PARSER_OK) {
        APP_CONSOLE(E, "Parsing MQTT support flag failed!");
        appHaltExecution();
    }
}

/*
 * Deallocate JSON after we are done
 */
static void appDeallocateJSON(void)
{
    if (json != NULL) {
        APP_CONSOLE(I, "Deallocating JSON object.");
        cJSON_Delete(json);
    }
}

/**
 * Declare a period in seconds to print location
 */
static void appPrintLocation(uint8_t periodSecs)
{
    if ((MICROTOSEC(esp_timer_get_time()) - timeNow >= periodSecs) && xplrGnssHasMessage(0)) {        
        espRet = xplrGnssPrintLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print gnss location!");
        }

        espRet = xplrGnssPrintGmapsLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }

        timeNow = MICROTOSEC(esp_timer_get_time());
    }
}

/**
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}