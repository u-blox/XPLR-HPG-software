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
 * An example for LBAND data correction with Thingstream (U-blox broker) MQTT assistance for decryption keys and GNSS for location.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream, using the Zero Touch Provisioning (referred as ZTP from now on) to achieve a connection to the MQTT broker
 * subscribes to PointPerfect decryption keys topic, using hpg_mqtt component,
 * sets up LBAND and GNSS using location_Service component,
 * and finally feed correction data from LBAND to GNSS
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
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
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
#define APP_MQTT_PAYLOAD_BUF_SIZE (512U)

/**
 * Interval to print location (in seconds)
 */
#define APP_LOCATION_PRINT_PERIOD  5

/**
 * GNSS and LBAND I2C address in hex
 */
#define XPLR_GNSS_I2C_ADDR  0x42
#define XPLR_LBAND_I2C_ADDR 0x43

/**
 * Decryption keys distribution topic
 */
#define APP_KEYS_TOPIC  "/pp/ubx/0236/Lb"
#define APP_FREQ_TOPIC  "/pp/frequencies/Lb"

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
 * You can use KConfig to set up these values.
 * CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME are taken from Thingstream.
 * In any case you can chose to overwrite those settings
 * by replacing CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME 
 * with the strings of your choice.
 */
static const char mqttClientId[] = CONFIG_XPLR_MQTTWIFI_CLIENT_ID;
static const char mqttHost[] = CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME;


/**
 * Use frequency according your region
 * Valid Options:
 * XPLR_LBAND_FREQUENCY_EU
 * XPLR_LBAND_FREQUENCY_US
 */
static xplrLbandRegion lbandRegion = XPLR_LBAND_FREQUENCY_EU;

/**
 * Frequency read from LBAND module
 */
static uint32_t frequency;

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
static char *topicArray[] = {APP_KEYS_TOPIC,
                             APP_FREQ_TOPIC};

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
static bool keysSent;

static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */


static void appInitBoard(void);
static void appInitLocationDevices(void);
static void appPrintDeviceInfos(void);
static void appInitWiFi(void);
static void appMqttInit(void);
static void appPrintLocation(uint8_t periodSecs);
static void appHaltExecution(void);

void app_main(void)
{
    appInitBoard();
    appInitWiFi();
    xplrMqttWifiInitState(&mqttClient);
    appInitLocationDevices();
    appPrintDeviceInfos();

    
    timeNow = 0;

    keysSent = false;

    while (1) {
        wifistarterErr = xplrWifiStarterFsm();

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_UNINIT || 
                xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_DISCONNECTED_OK) {
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
             * If we are subscribed to a topic we can start printing some messages
             * We only need the decryption keys since correction data will be fed from
             * the LBAND module to the GNSS module
            */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                /**
                 * The following function will digest messages store in the internal buffer.
                 * If the user does not use it it will be discarded.
                 */
                if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                    if (strcmp(mqttMessage.topic, APP_KEYS_TOPIC) == 0) {
                        espRet = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                           APP_CONSOLE(E, "Failed to send decryption keys!");
                           appHaltExecution();
                        } else {
                            APP_CONSOLE(I, "Decryption keys sent successfully!");
                            keysSent = true;
                        }
                    }

                    if (strcmp(mqttMessage.topic, APP_FREQ_TOPIC) == 0) {
                        espRet = xplrLbandSetFrequencyFromMqtt(0, mqttMessage.data, lbandRegion);
                        if (espRet != ESP_OK) {
                           APP_CONSOLE(E, "Failed to set frequency!");
                           appHaltExecution();
                        } else {
                            frequency = xplrLbandGetFrequency(0);
                            if (frequency == 0) {
                                APP_CONSOLE(I, "No LBAND frequency is set");
                            }
                            APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
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
 * Makes all required initializations for location
 * modules/devices
 */
static void appInitLocationDevices(void)
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
    espRet = xplrGnssStartDeviceDefaultSettings(0, XPLR_GNSS_I2C_ADDR);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }
    
    /**
     * Setting desired correction data source
     */
    espRet = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_LBAND);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to set correction data source!");
        appHaltExecution();
    }

    APP_CONSOLE(D, "Waiting for LBAND device to come online!");
    espRet = xplrLbandStartDeviceDefaultSettings(0,
                                                 XPLR_LBAND_I2C_ADDR,
                                                 xplrGnssGetHandler(0));
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Lband device config failed!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Prints some info for the initialized devices
 */
static void appPrintDeviceInfos(void)
{
    espRet = xplrGnssPrintDeviceInfo(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to print GNSS device info!");
        appHaltExecution();
    }

    espRet = xplrLbandPrintDeviceInfo(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to print LBAND device info!");
        appHaltExecution();
    }
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
     * Settings for MQTT client
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
 * Declare a period in Period in seconds to print location.
 */
static void appPrintLocation(uint8_t periodSecs)
{
    if ((MICROTOSEC(esp_timer_get_time()) - timeNow >= periodSecs) && xplrGnssHasMessage(0)) {
        espRet = xplrGnssPrintLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print location!");
            appHaltExecution();
        }

        espRet = xplrGnssPrintGmapsLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print GMaps location!");
            appHaltExecution();
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

