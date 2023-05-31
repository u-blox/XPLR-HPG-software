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
 * An example for MQTT connection to Thingstream (U-blox broker) using certificates and correction data for the GNSS module.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * uses certificated downloaded from Thingstream to achieve a connection to the Thingstream MQTT broker
 * subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * sets up GNSS module using location_service component,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

/**
 * If paths not found in VScode: 
 *      press keys --> <ctrl+shift+p> 
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "xplr_wifi_starter.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
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
#define KIB (1024U)
#define APP_MQTT_PAYLOAD_BUF_SIZE ((10U) * (KIB))

/**
 * Seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD  5

/**
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42

/**
 * Valid values
 * EU
 * US
 */
#define APP_ORIGIN_COUNTRY "EU"

/**
 * Valid values
 * IP
 * IPLBAND
 */
#define APP_CORRECTION_TYPE "sdfs"

#define APP_MAX_TOPICLEN 64

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/**
 * Populate files:
 * - client.crt
 * - client.key
 * - root.crt
 * according to your needs.
 * If you are using Thingstream then you can find all the needed
 * certificates inside your location thing settings.
 */
extern const uint8_t client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t client_crt_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");
extern const uint8_t server_root_crt_start[] asm("_binary_root_crt_start");
extern const uint8_t server_root_crt_end[] asm("_binary_root_crt_end");

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * GNSS configuration.
 * This is an example for a GNSS ZED-F9 module.
 * The same structure can be used with an LBAND NEO-D9S module.
 * Depending on your device and board you might have to change these values.
 */
static xplrGnssDeviceCfg_t gnssCfg = {
    .dvcSettings =  {
        .deviceType = U_DEVICE_TYPE_GNSS,
        .deviceCfg = {
            .cfgGnss = {
                .moduleType = 1,        /**< Module type, 1 for M9 devices */
                .pinEnablePower = -1,   /**< -1 if unused */
                .pinDataReady = -1,     /**< -1 if unused */
                .i2cAddress = APP_GNSS_I2C_ADDR /**< I2C address value */
            },
        },
        .transportType = U_DEVICE_TRANSPORT_TYPE_I2C,
        .transportCfg = {
            .cfgI2c = {
                .i2c = 0,       /**< ESP-IDF port number for I2C */
                .pinSda = BOARD_IO_I2C_PERIPHERALS_SDA,   /**< SDA pin value */
                .pinScl = BOARD_IO_I2C_PERIPHERALS_SCL,   /**< SCL pin value */
                .clockHertz = 400000    /**< Clock value in Hertz */
            },
        },
    },

    .dvcNetwork = {
        .type = U_NETWORK_TYPE_GNSS,            /**< Network type */
        .moduleType = U_GNSS_MODULE_TYPE_M9,    /**< Module type family */
        .devicePinPwr = -1,                     /**< -1 if power pin is not used */
        .devicePinDataReady = -1                /**< -1 if data ready pin is not used */
    }
};

/**
 * You can use KConfig to set up these values.
 * CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME are taken from Thingstream.
 * In any case you can chose to overwrite those settings
 * by replacing CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME 
 * with the strings of your choice.
 */
static const char mqttClientId[] = CONFIG_XPLR_MQTTWIFI_CLIENT_ID;
static const char mqttHost[] = CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME;

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
static const char  wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char  wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA,
    .webserver = false
};


/**
 * MQTT client configuration
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;
static char appKeysTopic[APP_MAX_TOPICLEN];
static char appCorrectionDataTopic[APP_MAX_TOPICLEN];
static char *topicArray[] = {appKeysTopic, appCorrectionDataTopic};

/**
 * A struct where we can store our received MQTT message
 */
static char data[APP_MQTT_PAYLOAD_BUF_SIZE];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
static xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = ELEMENTCNT(data)
};

/**
 * Boolean flags for different functions
 */
static bool requestDc;

/**
 * Error return types
 */
static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;
typedef struct subTopics_type {
    char appKeysTopic[APP_MAX_TOPICLEN];
    char appCorrDataTopic[APP_MAX_TOPICLEN];
} subTopics_t;


/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitBoard(void);
static void appInitWiFi(void);
static void appInitGnssDevice(void);
static void appMqttInit(void);
static void appPrintLocation(uint8_t periodSecs);
static void appHaltExecution(void);
static esp_err_t appConfigTopics(char **subTopics,
                                      const char *region, 
                                      const char *corrType);

void app_main(void)
{
    appInitBoard();
    appInitWiFi();
    appInitGnssDevice();
    xplrMqttWifiInitState(&mqttClient);
    xplrGnssPrintDeviceInfo(0);
    
    timeNow = 0;

    while (1) {
        wifistarterErr = xplrWifiStarterFsm();

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_UNINIT || 
                xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_DISCONNECTED_OK) {
                if (appConfigTopics(topicArray, APP_ORIGIN_COUNTRY, APP_CORRECTION_TYPE) != ESP_OK) {
                    APP_CONSOLE(E, "appConfigTopics failed!");
                    appHaltExecution();
                }
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                requestDc = false;
            }
        }

        mqttErr = xplrMqttWifiFsm(&mqttClient);

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                espRet = xplrMqttWifiSubscribeToTopicArray(&mqttClient, topicArray, ELEMENTCNT(topicArray), XPLR_MQTTWIFI_QOS_LVL_0);
                if (espRet != ESP_OK) {
                    APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArray failed!");
                    appHaltExecution();
                }
                break;

            /**
             * If we are subscribed to a topic we can start sending messages to our
             * GNSS module: decryption keys and correction data
             */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                /**
                 * The following function will digest messages stored in the internal buffer.
                 * If the user does not use it it will be discarded.
                 */
                if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                    if (strcmp(mqttMessage.topic, topicArray[0]) == 0) {
                        espRet = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                           APP_CONSOLE(E, "Failed to send decryption keys!");
                           appHaltExecution();
                        }
                    }
                    if (strcmp(mqttMessage.topic, topicArray[1]) == 0) {
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
    /**
     * Init ubxlib
     */
    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    APP_CONSOLE(D, "Waiting for GNSS device to come online!");
    espRet = xplrGnssStartDevice(0, &gnssCfg);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }
    
    /**
     * Setting desired correction data source
     */
    espRet = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_IP);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to set correction data source!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Populate MQTT Wi-Fi client settings
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
     * Settings for mqtt client
     */
    mqttClientConfig.uri = mqttHost;
    mqttClientConfig.client_id = mqttClientId;
    mqttClientConfig.client_cert_pem = (const char *)client_crt_start;
    mqttClientConfig.client_key_pem = (const char *)client_key_start;
    mqttClientConfig.cert_pem = (const char *)server_root_crt_start;

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Start MQTT Wi-Fi Client
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/**
 * Prints location data over a period (seconds).
 * Declare a period in seconds to print location.
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

static esp_err_t appConfigTopics(char **subTopics,
                                      const char *region, 
                                      const char *corrType)
{
    esp_err_t ret = ESP_OK;

    memset(subTopics[0], 0x00, APP_MAX_TOPICLEN);
    memset(subTopics[1], 0x00, APP_MAX_TOPICLEN);

    if (strcmp(corrType, "IP") == 0) {
        strcpy(subTopics[0], "/pp/ubx/0236/ip");
        strcpy(subTopics[1], "/pp/ip/");
    } else if ((strcmp(corrType, "IPLBAND") == 0)) {
        strcpy(subTopics[0], "/pp/ubx/0236/Lb");
        strcpy(subTopics[1], "/pp/Lb/");
    } else if((strcmp(corrType, "LBAND") == 0)) {
        APP_CONSOLE(E, "LBAND not supported by example");
        ret = ESP_FAIL;
    } else {
        APP_CONSOLE(E, "Invalid Thingstream plan!");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        if (strcmp(region, "EU") == 0) {
            strcat(subTopics[1], "eu");
        } else if (strcmp(region, "US") == 0) {
            strcat(subTopics[1], "us");
        } else {
            APP_CONSOLE(E, "Invalid region!");
            ret = ESP_FAIL;
        }
    }
    
    return ret;
}