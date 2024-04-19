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
 * An example for MQTT connection to Thingstream (U-blox broker) using a configuration file from the SD card.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream, using the credentials of the configuration file to achieve a connection to the MQTT broker
 * subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * sets up GNSS module using location_service component,
 * sets up LBAND module (in our case the NEO-D9S module), if the Thingstream plan supports it,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "xplr_wifi_starter.h"
#include "xplr_thingstream.h"
#include "xplr_gnss.h"
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

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA          0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED    1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED      0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_PRINT_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/*
 * Simple macros used to set buffer size in bytes
 */
#define KIB                         (1024U)
#define APP_JSON_PAYLOAD_BUF_SIZE   ((6U) * (KIB))

/**
 * Period in seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD  5

/*
 * Button for shutting down device
 */
#define APP_DEVICE_OFF_MODE_BTN        (BOARD_IO_BTN1)

/*
 * Device off press duration in sec
 */
#define APP_DEVICE_OFF_MODE_TRIGGER    (3U)

#if 1 == APP_PRINT_IMU_DATA
/**
 * Period in seconds to print Dead Reckoning data
 */
#define APP_DEAD_RECKONING_PRINT_PERIOD 5
#endif

/**
 * Time in seconds to trigger an inactivity timeout and cause a restart
 */
#define APP_INACTIVITY_TIMEOUT 30

/**
 * GNSS and LBAND I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42
#define APP_LBAND_I2C_ADDR 0x43

/**
 * Thingstream subscription plan region
 * for correction data
*/
#define APP_THINGSTREAM_REGION  XPLR_THINGSTREAM_PP_REGION_EU

/**
 * Option to enable the correction message watchdog mechanism.
 * When set to 1, if no correction data are forwarded to the GNSS
 * module (either via IP or SPARTN) for a defined amount of time
 * (MQTT_MESSAGE_TIMEOUT macro defined in hpglib/xplr_mqtt/include/xplr_mqtt.h)
 * an error event will be triggered
*/
#define APP_ENABLE_CORR_MSG_WDG         (1U)

/**
 * Trigger soft reset if device in error state
 */
#define APP_RESTART_ON_ERROR            (1U)

/*
 * Option to enable/disable the hot plug functionality for the SD card
 */
#define APP_SD_HOT_PLUG_FUNCTIONALITY   (1U) & APP_SD_LOGGING_ENABLED

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog           : 1;
        uint8_t nvsLog           : 1;
        uint8_t mqttLog          : 1;
        uint8_t gnssLog          : 1;
        uint8_t gnssAsyncLog     : 1;
        uint8_t lbandLog         : 1;
        uint8_t locHelperLog     : 1;
        uint8_t thingstreamLog   : 1;
        uint8_t wifiStarterLog   : 1;
    } singleLogOpts;
    uint16_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          mqttLogIndex;
    int8_t          gnssLogIndex;
    int8_t          gnssAsyncLogIndex;
    int8_t          lbandLogIndex;
    int8_t          locHelperLogIndex;
    int8_t          thingstreamLogIndex;
    int8_t          wifiStarterLogIndex;
} appLog_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* Application settings */
static uint64_t appRunTime = ~0;
static uint32_t locPrintInterval = APP_LOCATION_PRINT_PERIOD;
#if (APP_PRINT_IMU_DATA == 1)
static uint32_t imuPrintInterval = APP_DEAD_RECKONING_PRINT_PERIOD;
#endif

/**
 * Location modules configs
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static xplrLbandDeviceCfg_t dvcLbandConfig;
static xplrLocDeviceType_t gnssDvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
static xplrGnssCorrDataSrc_t gnssCorrSrc = (xplrGnssCorrDataSrc_t)
                                           CONFIG_XPLR_CORRECTION_DATA_SOURCE;
static bool gnssDrEnable = CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE;

/**
 * Frequency read from LBAND module
 */
static uint32_t frequency;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/**
 * Gnss and lband device profiles id
 */
const uint8_t gnssDvcPrfId = 0;
const uint8_t lbandDvcPrfId = 0;

/**
 * Location data struct
 */
static xplrGnssLocation_t locData;

#if 1 == APP_PRINT_IMU_DATA
/**
 * Dead Reckoning data structs
 */
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif

static xplr_thingstream_t thingstreamSettings;
static xplr_thingstream_pp_region_t ppRegion = APP_THINGSTREAM_REGION;

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
 * Name of the config file in the SD card
*/
static char *uCenterConfigFilename = CONFIG_XPLR_UCENTER_CONFIG_FILENAME;

/**
 * Providing MQTT client configuration
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;

/**
 * A struct where we can store our received MQTT message
 */
static char data[XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_PAYLOAD];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = ELEMENTCNT(data)
};

/**
 * Keeps time at "this" point of code execution.
 * Used to calculate elapse time.
 */
static uint64_t timePrevLoc;
static uint64_t gnssLastAction;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

/**
 * Static log configuration struct and variables
 */

static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0, // All modules selected to log
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .mqttLogIndex = -1,
    .gnssLogIndex = -1,
    .gnssAsyncLogIndex = -1,
    .lbandLogIndex = -1,
    .locHelperLogIndex = -1,
    .thingstreamLogIndex = -1,
    .wifiStarterLogIndex = -1,
};

/**
 * Static variables and flags
*/
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;
static xplrMqttWifiGetItemError_t wifiGetItemErr;
static bool requestDc;
static bool deviceOffRequested = false;
static bool isPlanLband = false;

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif

static char configData[APP_JSON_PAYLOAD_BUF_SIZE];
/* The name of the configuration file */
static char configFilename[] = "xplr_config.json";
/* Application configuration options */
xplr_cfg_t appOptions;
/* Flag indicating the board is setup by the SD config file */
static bool isConfiguredFromFile = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
static void appDeInitLogging(void);
#endif
static esp_err_t appInitBoard(void);
static esp_err_t appFetchConfigFromFile(void);
static void appApplyConfigFromFile(void);
static esp_err_t appInitSd(void);
static void appInitWiFi(void);
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
static void appInitGnssDevice(void);
static void appInitLbandDevice(void);
static esp_err_t appGetSdCredentials(void);
static void appMqttInit(void);
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appHaltExecution(void);
static void appTerminate(void);
static void appDeviceOffTask(void *arg);
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appInitHotPlugTask(void);
static void appCardDetectTask(void *arg);
#endif

void app_main(void)
{
    bool isNeededTopic, gotJson = false;
    esp_err_t espErr;
    xplrGnssError_t gnssErr;
    static bool mqttWifiReceivedInitial = true;
    static bool SentCorrectionDataInitial = true;

    appInitBoard();
    espErr = appFetchConfigFromFile();
    if (espErr == ESP_OK) {
        appApplyConfigFromFile();
    } else {
        APP_CONSOLE(D, "No configuration file found, running on Kconfig configuration");
    }

#if (APP_SD_LOGGING_ENABLED == 1)
    espErr = appInitLogging();
    if (espErr != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    appInitHotPlugTask();
#endif
    appInitWiFi();
    appInitGnssDevice();
    xplrMqttWifiInitState(&mqttClient);

    timePrevLoc = MICROTOSEC(esp_timer_get_time());
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = MICROTOSEC(esp_timer_get_time());
#endif

    while (1) {
        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                gnssLastAction = esp_timer_get_time();
                if ((dvcLbandConfig.destHandler == NULL) && isPlanLband) {
                    dvcLbandConfig.destHandler = xplrGnssGetHandler(gnssDvcPrfId);
                    if (dvcLbandConfig.destHandler != NULL) {
                        espErr = xplrLbandSetDestGnssHandler(lbandDvcPrfId, dvcLbandConfig.destHandler);
                        if (espErr == ESP_OK) {
                            espErr = xplrLbandSendCorrectionDataAsyncStart(lbandDvcPrfId);
                            if (espErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to get start Lband Async sender!");
                                appHaltExecution();
                            } else {
                                APP_CONSOLE(D, "Successfully started Lband Async sender!");
                            }
                        }
                    } else {
                        APP_CONSOLE(E, "Failed to get GNSS handler!");
                        appHaltExecution();
                    }
                }
                appPrintLocation(locPrintInterval);
#if 1 == APP_PRINT_IMU_DATA
                if (appOptions.drCfg.printImuData) {
                    appPrintDeadReckoning(imuPrintInterval);
                }
#endif
                break;
            case XPLR_GNSS_STATE_DEVICE_RESTART:
                if ((dvcLbandConfig.destHandler != NULL) && isPlanLband) {
                    espErr = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    if (espErr != ESP_OK) {
                        APP_CONSOLE(E, "Failed to get stop Lband Async sender!");
                        appHaltExecution();
                    } else {
                        APP_CONSOLE(D, "Successfully stoped Lband Async sender!");
                        dvcLbandConfig.destHandler = NULL;
                    }
                }
                break;
            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                if (isPlanLband) {
                    xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    dvcLbandConfig.destHandler = NULL;
                }
                appTerminate();
                break;
            default:
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
                }
                break;
        }

        wifistarterErr = xplrWifiStarterFsm();
        /**
         * If we connect to WiFi we can continue with JSON parsing from the SD and
         * then with MQTT
         */
        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (!gotJson) {
                espErr = appGetSdCredentials();

                if (espErr != ESP_OK) {
                    APP_CONSOLE(E, "Credential Fetch from SD card failed");
                    XPLR_CI_CONSOLE(705, "ERROR");
                    requestDc = true;
                    appHaltExecution();
                } else {
                    /**
                     * Since MQTT is supported we can initialize the MQTT broker and try to connect
                     */
                    XPLR_CI_CONSOLE(705, "OK");
                    gotJson = true;
                    appMqttInit();
                    espErr = xplrMqttWifiStart(&mqttClient);
                    if (espErr != ESP_OK) {
                        XPLR_CI_CONSOLE(706, "ERROR");
                    } else {
                        XPLR_CI_CONSOLE(706, "OK");
                    }
                    requestDc = false;
                }
            }
        }

        /**
         * This example uses the credentials
         * stored in the config file in the SD card
         * to get all required settings to connect
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
                 * We subscribe after the GNSS device is ready. This way we do not
                 * lose the first message which contains the decryption keys
                 */

                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    /**
                     * Lets try to use ZTP format topics to subscribe
                     */
                    gnssLastAction = esp_timer_get_time();
                    espErr = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
                    if (espErr != ESP_OK) {
                        APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArrayZtp failed");
                        XPLR_CI_CONSOLE(707, "ERROR");
                        appHaltExecution();
                    } else {
                        XPLR_CI_CONSOLE(707, "OK");
                    }
                } else {
                    if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                        appTerminate();
                    }
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
                wifiGetItemErr = xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage);
                if (wifiGetItemErr == XPLR_MQTTWIFI_ITEM_OK) {
                    if (mqttWifiReceivedInitial) {
                        XPLR_CI_CONSOLE(708, "OK");
                        mqttWifiReceivedInitial = false;
                    }
                    /**
                     * Do not send data if GNSS is not in ready state.
                     * The device might not be initialized and the device handler
                     * would be NULL
                     */
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        gnssLastAction = esp_timer_get_time();
                        isNeededTopic = xplrThingstreamPpMsgIsKeyDist(mqttMessage.topic, &thingstreamSettings);
                        if (isNeededTopic) {
                            espErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                            if (espErr != ESP_OK) {
                                XPLR_CI_CONSOLE(709, "ERROR");
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                appHaltExecution();
                            } else {
                                XPLR_CI_CONSOLE(709, "OK");
                            }
                        }
                        /*INDENT-OFF*/
                        isNeededTopic = xplrThingstreamPpMsgIsCorrectionData(mqttMessage.topic, &thingstreamSettings);
                        /*INDENT-ON*/
                        if (isNeededTopic && (!isPlanLband)) {
                            espErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                            if (espErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send correction data!");
                                XPLR_CI_CONSOLE(11, "ERROR");
                            } else if (SentCorrectionDataInitial) {
                                XPLR_CI_CONSOLE(11, "OK");
                                SentCorrectionDataInitial = false;
                            }
                        }
                        isNeededTopic = xplrThingstreamPpMsgIsFrequency(mqttMessage.topic, &thingstreamSettings);
                        if (isNeededTopic && isPlanLband) {
                            espErr = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                                   mqttMessage.data,
                                                                   dvcLbandConfig.corrDataConf.region);
                            if (espErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to set frequency!");
                                XPLR_CI_CONSOLE(710, "ERROR");
                                appHaltExecution();
                            } else {
                                frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                                if (frequency == 0) {
                                    APP_CONSOLE(I, "No LBAND frequency is set");
                                    XPLR_CI_CONSOLE(710, "ERROR");
                                }
                                APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
                            }
                        }
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                            appTerminate();
                        }
                    }
                } else if (wifiGetItemErr == XPLR_MQTTWIFI_ITEM_ERROR) {
                    XPLR_CI_CONSOLE(708, "ERROR");
                }
                break;
            case XPLR_MQTTWIFI_STATE_DISCONNECTED_OK:
                // We have a disconnect event (probably from the watchdog). Let's reconnect
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                break;
            default:
                break;
        }

        /**
         * Check if application has reached the max runtime.
         * If yes, raise the device off flag
        */
        if (MICROTOSEC(esp_timer_get_time()) >= appRunTime) {
            APP_CONSOLE(W, "Reached maximum runtime. Terminating...");
            deviceOffRequested = true;
        }
        /**
         * Check if any LBAND messages have been forwarded to the GNSS module
         * and if there are feed the MQTT module's watchdog.
        */
        if (xplrLbandHasFrwdMessage()) {
            xplrMqttWifiFeedWatchdog(&mqttClient);
        }

        /**
         * We lost WiFi connection.
         * When we reconnect everything will be executed from the beginning:
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

        if (deviceOffRequested) {
            xplrMqttWifiUnsubscribeFromTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
            xplrMqttWifiHardDisconnect(&mqttClient);
            if ((dvcLbandConfig.destHandler != NULL) && isPlanLband) {
                espErr = xplrLbandPowerOffDevice(lbandDvcPrfId);
                if (espErr != ESP_OK) {
                    APP_CONSOLE(E, "Failed to stop Lband device!");
                } else {
                    dvcLbandConfig.destHandler = NULL;
                }
            }
            xplrGnssStopAllAsyncs(gnssDvcPrfId);
            espErr = xplrGnssPowerOffDevice(gnssDvcPrfId);
            timePrevLoc = esp_timer_get_time();
            do {
                gnssErr = xplrGnssFsm(gnssDvcPrfId);
                vTaskDelay(pdMS_TO_TICKS(10));
                if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
                    gnssErr == XPLR_GNSS_ERROR &&
                    espErr != ESP_OK) {
                    break;
                }
            } while (gnssErr != XPLR_GNSS_STOPPED);
#if (APP_SD_LOGGING_ENABLED == 1)
            appDeInitLogging();
#endif
            appHaltExecution();
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

/**
 * Initialize logging to the SD card
*/
#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplr_cfg_logInstance_t *instance = NULL;

    /* Initialize the SD card */
    if (!xplrSdIsCardInit()) {
        ret = appInitSd();
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.appLogIndex];
                appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                    instance->filename,
                                                    instance->sizeInterval,
                                                    instance->erasePrev);
                instance = NULL;
            } else {
                appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                    "main_app.log",
                                                    XPLRLOG_FILE_SIZE_INTERVAL,
                                                    XPLRLOG_NEW_FILE_ON_BOOT);
            }

            if (appLogCfg.appLogIndex >= 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.nvsLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.nvsLogIndex];
                appLogCfg.nvsLogIndex = xplrNvsInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.nvsLogIndex = xplrNvsInitLogModule(NULL);
            }

            if (appLogCfg.nvsLogIndex > 0) {
                APP_CONSOLE(D, "NVS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.mqttLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.mqttLogIndex];
                appLogCfg.mqttLogIndex = xplrMqttWifiInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.mqttLogIndex = xplrMqttWifiInitLogModule(NULL);
            }

            if (appLogCfg.mqttLogIndex > 0) {
                APP_CONSOLE(D, "MQTT logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.gnssLogIndex];
                appLogCfg.gnssLogIndex = xplrGnssInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.gnssLogIndex = xplrGnssInitLogModule(NULL);
            }

            if (appLogCfg.gnssLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssAsyncLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.gnssAsyncLogIndex];
                appLogCfg.gnssAsyncLogIndex = xplrGnssAsyncLogInit(instance);
                instance = NULL;
            } else {
                appLogCfg.gnssAsyncLogIndex = xplrGnssAsyncLogInit(NULL);
            }

            if (appLogCfg.gnssAsyncLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS Async logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.lbandLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.lbandLogIndex];
                appLogCfg.lbandLogIndex = xplrLbandInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.lbandLogIndex = xplrLbandInitLogModule(NULL);
            }

            if (appLogCfg.lbandLogIndex >= 0) {
                APP_CONSOLE(D, "LBAND service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.locHelperLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.locHelperLogIndex];
                appLogCfg.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            }

            if (appLogCfg.locHelperLogIndex >= 0) {
                APP_CONSOLE(D, "Location Helper Service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.thingstreamLog == 1) {
            appLogCfg.thingstreamLogIndex = xplrThingstreamInitLogModule(NULL);
            if (appLogCfg.thingstreamLogIndex >= 0) {
                APP_CONSOLE(D, "Thingstream logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.wifiStarterLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.wifiStarterLogIndex];
                appLogCfg.wifiStarterLogIndex = xplrWifiStarterInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.wifiStarterLogIndex = xplrWifiStarterInitLogModule(NULL);
            }

            if (appLogCfg.wifiStarterLogIndex >= 0) {
                APP_CONSOLE(D, "WiFi Starter logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.thingstreamLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.thingstreamLogIndex];
                appLogCfg.thingstreamLogIndex = xplrThingstreamInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.thingstreamLogIndex = xplrThingstreamInitLogModule(NULL);
            }

            if (appLogCfg.thingstreamLogIndex >= 0) {
                APP_CONSOLE(D, "Thingstream module logging instance initialized");
            }
        }
    }

    return ret;
}

static void appDeInitLogging(void)
{
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr = XPLR_SD_ERROR;
    esp_err_t espErr;

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    vTaskDelete(cardDetectTaskHandler);
#endif
    logErr = xplrLogDisableAll();
    if (logErr != XPLR_LOG_OK) {
        APP_CONSOLE(E, "Error disabling logging");
    } else {
        logErr = xplrLogDeInitAll();
        if (logErr != XPLR_LOG_OK) {
            APP_CONSOLE(E, "Error de-initializing logging");
        } else {
            espErr = xplrGnssAsyncLogDeInit();
            if (espErr != XPLR_LOG_OK) {
                APP_CONSOLE(E, "Error de-initializing async logging");
                logErr = XPLR_LOG_ERROR;
            } else {
                //Do nothing
            }
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdStopCardDetectTask();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error stopping the the SD card detect task");
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdDeInit();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error de-initializing the SD card");
        }
    } else {
        //Do nothing
    }

    if (logErr == XPLR_LOG_OK && sdErr == XPLR_SD_OK) {
        APP_CONSOLE(I, "Logging service de-initialized successfully");
    }
}
#endif

/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t espRet;

    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_DEVICE_OFF_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        espRet = gpio_config(&io_conf);
    }

    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        if (xTaskCreate(appDeviceOffTask, "deviceOffTask", 2 * 2048, NULL, 10, NULL) == pdPASS) {
            APP_CONSOLE(D, "Boot0 pin configured as button OK");
            APP_CONSOLE(D, "Board Initialized");
        } else {
            APP_CONSOLE(D, "Failed to start deviceOffTask task");
            APP_CONSOLE(E, "Board initialization failed!");
            espRet = ESP_FAIL;
        }
    }

    return espRet;
}

/**
 * Fetch configuration options from SD card (if existent),
 * otherwise, keep Kconfig values
*/
static esp_err_t appFetchConfigFromFile(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;
    xplr_board_error_t boardErr = xplrBoardDetectSd();

    if (boardErr == XPLR_BOARD_ERROR_OK) {
        ret = appInitSd();
        if (ret == ESP_OK) {
            memset(configData, 0, APP_JSON_PAYLOAD_BUF_SIZE);
            sdErr = xplrSdReadFileString(configFilename, configData, APP_JSON_PAYLOAD_BUF_SIZE);
            if (sdErr == XPLR_SD_OK) {
                ret = xplrParseConfigSettings(configData, &appOptions);
                if (ret == ESP_OK) {
                    APP_CONSOLE(I, "Successfully parsed application and module configuration");
                } else {
                    APP_CONSOLE(E, "Failed to parse application and module configuration from <%s>", configFilename);
                }
            } else {
                APP_CONSOLE(E, "Unable to get configuration from the SD card");
                ret = ESP_FAIL;
            }
        } else {
            // Do nothing
        }
    } else {
        APP_CONSOLE(D, "SD is not mounted. Keeping Kconfig configuration");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * Apply configuration from file
*/
static void appApplyConfigFromFile(void)
{
    xplr_cfg_logInstance_t *instance = NULL;
    /* Applying the options that are relevant to the example */
    /* Application settings */
    appRunTime = (uint64_t)appOptions.appCfg.runTime;
    locPrintInterval = appOptions.appCfg.locInterval;
#if (1 == APP_PRINT_IMU_DATA)
    imuPrintInterval = appOptions.drCfg.printInterval;
#endif
    /* Wi-Fi Settings */
    wifiOptions.ssid = appOptions.wifiCfg.ssid;
    wifiOptions.password = appOptions.wifiCfg.pwd;
    /* Thingstream Settings */
    uCenterConfigFilename = appOptions.tsCfg.uCenterConfigFilename;
    if (strcmp(appOptions.tsCfg.region, "EU") == 0) {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_EU;
    } else if (strcmp(appOptions.tsCfg.region, "US") == 0) {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_US;
    } else if (strcmp(appOptions.tsCfg.region, "KR") == 0) {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_KR;
    } else if (strcmp(appOptions.tsCfg.region, "AU") == 0) {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_AU;
    } else if (strcmp(appOptions.tsCfg.region, "JP") == 0) {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_JP;
    } else {
        ppRegion = XPLR_THINGSTREAM_PP_REGION_INVALID;
    }
    /* Logging Settings */
    appLogCfg.logOptions.allLogOpts = 0;
    for (uint8_t i = 0; i < appOptions.logCfg.numOfInstances; i++) {
        instance = &appOptions.logCfg.instance[i];
        if (strstr(instance->description, "Application") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.appLog = 1;
                appLogCfg.appLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "NVS") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.nvsLog = 1;
                appLogCfg.nvsLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "Wifi Starter") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.wifiStarterLog = 1;
                appLogCfg.wifiStarterLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "MQTT Wifi") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.mqttLog = 1;
                appLogCfg.mqttLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "GNSS Info") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.gnssLog = 1;
                appLogCfg.gnssLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "GNSS Async") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.gnssAsyncLog = 1;
                appLogCfg.gnssAsyncLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "Lband") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.lbandLog = 1;
                appLogCfg.lbandLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "Location") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.locHelperLog = 1;
                appLogCfg.locHelperLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "Thingstream") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.thingstreamLog = 1;
                appLogCfg.thingstreamLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else {
            // Do nothing module not used in example
        }
    }
    /* GNSS and DR settings */
    gnssDvcType = (xplrLocDeviceType_t)appOptions.gnssCfg.module;
    gnssCorrSrc = (xplrGnssCorrDataSrc_t)appOptions.gnssCfg.corrDataSrc;
    gnssDrEnable = appOptions.drCfg.enable;
    /* Options from SD config file applied */
    isConfiguredFromFile = true;
}

/**
 * Initialize SD card
*/
static esp_err_t appInitSd(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;

    sdErr = xplrSdConfigDefaults();
    if (sdErr != XPLR_SD_OK) {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = ESP_FAIL;
    } else {
        /* Create the card detect task */
        sdErr = xplrSdStartCardDetectTask();
        /* A time window so that the card gets detected*/
        vTaskDelay(pdMS_TO_TICKS(50));
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Failed to start the card detect task");
            ret = ESP_FAIL;
        } else {
            /* Initialize the SD card */
            sdErr = xplrSdInit();
            if (sdErr != XPLR_SD_OK) {
                APP_CONSOLE(E, "Failed to initialize the SD card");
                ret = ESP_FAIL;
            } else {
                APP_CONSOLE(D, "SD card initialized");
                ret = ESP_OK;
            }
        }
    }

    return ret;
}

/**
 * Populates gnss settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
{
    /**
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case a gnss
    * module connected via I2C
    */
    gnssCfg->hw.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    gnssCfg->hw.dvcType = gnssDvcType;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_GNSS_I2C_ADDR;
    gnssCfg->hw.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    gnssCfg->hw.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    gnssCfg->hw.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    gnssCfg->hw.dvcNetwork.devicePinPwr = -1;
    gnssCfg->hw.dvcNetwork.devicePinDataReady = -1;

    gnssCfg->dr.enable = gnssDrEnable;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;
    gnssCfg->corrData.source = gnssCorrSrc;
}

/**
 * Populates lband settings
 */
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg)
{
    /**
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case an lband
    * module connected via I2C
    */
    lbandCfg->hwConf.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_LBAND_I2C_ADDR;
    lbandCfg->hwConf.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    lbandCfg->hwConf.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    lbandCfg->hwConf.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    lbandCfg->hwConf.dvcNetwork.devicePinPwr = -1;
    lbandCfg->hwConf.dvcNetwork.devicePinDataReady = -1;

    lbandCfg->destHandler = NULL;
    lbandCfg->corrDataConf.freq = 0;

    /* Set frequency region */
    switch (ppRegion) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_US;
            break;
        default:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
            break;
    }
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    esp_err_t espRet;
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        XPLR_CI_CONSOLE(704, "ERROR");
        appHaltExecution();
    } else {
        APP_CONSOLE(D, "Wifi station mode Initialized");
        XPLR_CI_CONSOLE(704, "OK");
    }
}

/**
 * Makes all required initializations
 */
static void appInitGnssDevice(void)
{
    esp_err_t espRet;

    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        XPLR_CI_CONSOLE(701, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(701, "OK");
    }

    appConfigGnssSettings(&dvcGnssConfig);

    espRet = xplrGnssStartDevice(0, &dvcGnssConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        XPLR_CI_CONSOLE(702, "ERROR");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
    XPLR_CI_CONSOLE(702, "OK");
}

/**
 * Function that initializes the LBAND device
*/
static void appInitLbandDevice(void)
{
    esp_err_t espRet;

    APP_CONSOLE(D, "Waiting for LBAND device to come online!");
    appConfigLbandSettings(&dvcLbandConfig);
    espRet = xplrLbandStartDevice(lbandDvcPrfId, &dvcLbandConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Lband device config failed!");
        XPLR_CI_CONSOLE(703, "ERROR");
        appHaltExecution();
    } else {
        espRet = xplrLbandPrintDeviceInfo(lbandDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print LBAND device info!");
            XPLR_CI_CONSOLE(703, "ERROR");
            appHaltExecution();
        }
    }
}

/**
 * Gets the credentials located in the config file of the SD card
*/
static esp_err_t appGetSdCredentials(void)
{
    esp_err_t ret;
    xplrSd_error_t err;
    xplr_thingstream_error_t tsErr;

    if (!xplrSdIsCardInit()) {
        ret = appInitSd();
    } else {
        ret = ESP_OK;
    }
    if (ret == ESP_OK) {
        err = xplrSdReadFileString(uCenterConfigFilename, configData, APP_JSON_PAYLOAD_BUF_SIZE);
        if (err == XPLR_SD_OK) {
            tsErr = xplrThingstreamPpConfigFromFile(configData,
                                                    ppRegion,
                                                    (bool)gnssCorrSrc,
                                                    &thingstreamSettings);
            if (tsErr == XPLR_THINGSTREAM_OK) {
                APP_CONSOLE(I, "Successfully parsed configuration file");
                /* We check the lbandSupported flag to see if the LBAND module needs to be started */
                if (thingstreamSettings.pointPerfect.lbandSupported) {
                    if (isConfiguredFromFile) {
                        isPlanLband = (bool)gnssCorrSrc;
                    } else {
                        isPlanLband = (bool)CONFIG_XPLR_CORRECTION_DATA_SOURCE;
                    }
                    if (isPlanLband) {
                        appInitLbandDevice();
                    } else {
                        //Do nothing
                    }
                }
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
                APP_CONSOLE(E, "Error in parsing");
            }
        } else {
            APP_CONSOLE(E, "Error fetching payload from SD");
            ret = ESP_FAIL;
        }
    } else {
        APP_CONSOLE(E, "Error initializing SD");
    }

    return ret;
}

/**
 * Populate some example settings
 */
static void appMqttInit(void)
{
    esp_err_t ret;

    if (isConfiguredFromFile) {
        mqttClient.ucd.enableWatchdog = appOptions.appCfg.mqttWdgEnable;
    } else {
        mqttClient.ucd.enableWatchdog = (bool) APP_ENABLE_CORR_MSG_WDG;
    }

    /**
     * We declare how many slots a ring buffer should have.
     * You can chose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    ret = xplrMqttWifiSetRingbuffSlotsCount(&mqttClient, 6);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set MQTT ringbuffer slots!");
        appHaltExecution();
    }

    /**
     * Settings for client
     * If Json parse is successful then all the following settings
     * will be populated
     */
    mqttClientConfig.uri = thingstreamSettings.pointPerfect.brokerAddress;
    mqttClientConfig.client_id = thingstreamSettings.pointPerfect.deviceId;
    mqttClientConfig.client_cert_pem = thingstreamSettings.pointPerfect.clientCert;
    mqttClientConfig.client_key_pem = thingstreamSettings.pointPerfect.clientKey;
    mqttClientConfig.cert_pem = thingstreamSettings.server.rootCa;

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Let's start our client.
     * In case of multiple clients an array of clients can be used.
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/**
 * Declare a period in seconds to print location
 */
static void appPrintLocation(uint8_t periodSecs)
{
    esp_err_t espRet;
    static bool locRTKFirstTime = true;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) && xplrGnssHasMessage(0)) {
        espRet = xplrGnssGetLocationData(0, &locData);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
            XPLR_CI_CONSOLE(712, "ERROR");
        } else {
            if (locRTKFirstTime) {
                if ((locData.locFixType == XPLR_GNSS_LOCFIX_FLOAT_RTK) ||
                    (locData.locFixType == XPLR_GNSS_LOCFIX_FIXED_RTK)) {
                    locRTKFirstTime = false;
                    XPLR_CI_CONSOLE(10, "OK");
                }
            }
            espRet = xplrGnssPrintLocationData(&locData);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
                XPLR_CI_CONSOLE(712, "ERROR");
            } else {
                XPLR_CI_CONSOLE(712, "OK");
            }
        }

        espRet = xplrGnssPrintGmapsLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
            XPLR_CI_CONSOLE(712, "ERROR");
        } else {
            //Do nothing
        }

        timePrevLoc = MICROTOSEC(esp_timer_get_time());
    }
}

#if 1 == APP_PRINT_IMU_DATA
/**
 * Prints Dead Reckoning data over a period (seconds).
 * Declare a period in seconds to print location.
 */
static void appPrintDeadReckoning(uint8_t periodSecs)
{
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevDr >= periodSecs) &&
        xplrGnssIsDrEnabled(gnssDvcPrfId)) {
        ret = xplrGnssGetImuAlignmentInfo(gnssDvcPrfId, &imuAlignmentInfo);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment info!");
        } else {
            ret = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment data!");
            } else {
                //Do nothing
            }
        }

        ret = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        } else {
            ret = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment status!");
            } else {
                //Do nothing
            }
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            ret = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            } else {
                ret = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
                } else {
                    //Do nothing
                }
            }
        } else {
            //Do nothing
        }

        timePrevDr = MICROTOSEC(esp_timer_get_time());
    }
}
#endif

/*
 * Function to halt app execution
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appTerminate(void)
{
    xplrGnssError_t gnssErr;
    esp_err_t espErr;

    APP_CONSOLE(E, "Unrecoverable error in application. Terminating and restarting...");

    xplrMqttWifiUnsubscribeFromTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
    xplrMqttWifiHardDisconnect(&mqttClient);
    if ((dvcLbandConfig.destHandler != NULL) && isPlanLband) {
        espErr = xplrLbandStopDevice(lbandDvcPrfId);
        if (espErr != ESP_OK) {
            APP_CONSOLE(E, "Failed to stop Lband device!");
        } else {
            dvcLbandConfig.destHandler = NULL;
        }
    }
    espErr = xplrGnssStopDevice(gnssDvcPrfId);
    timePrevLoc = esp_timer_get_time();
    do {
        gnssErr = xplrGnssFsm(gnssDvcPrfId);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
            gnssErr == XPLR_GNSS_ERROR &&
            espErr != ESP_OK) {
            break;
        }
    } while (gnssErr != XPLR_GNSS_STOPPED);

#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif

#if (APP_RESTART_ON_ERROR == 1)
    esp_restart();
#else
    appHaltExecution();
#endif
}

static void appDeviceOffTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;

            if (btnPressDuration >= APP_DEVICE_OFF_MODE_TRIGGER) {
                if (!deviceOffRequested) {
                    APP_CONSOLE(W, "Device OFF triggered");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    btnPressDuration = 0;
                    deviceOffRequested = true;
                } else {
                    APP_CONSOLE(D, "Device is powered down, nothing to do...");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appInitHotPlugTask(void)
{
    if (!isConfiguredFromFile || appOptions.logCfg.hotPlugEnable) {
        if (xTaskCreate(appCardDetectTask,
                        "hotPlugTask",
                        4 * 1024,
                        NULL,
                        20,
                        &cardDetectTaskHandler) == pdPASS) {
            APP_CONSOLE(D, "Hot plug for SD card OK");
        } else {
            APP_CONSOLE(W, "Hot plug for SD card failed");
        }
    } else {
        // Do nothing Hot plug task disabled
    }
}

static void appCardDetectTask(void *arg)
{
    bool prvState = xplrSdIsCardOn();
    bool currState;
    esp_err_t espErr;

    for (;;) {
        currState = xplrSdIsCardOn();

        /* Check if state has changed */
        if (currState ^ prvState) {
            if (currState) {
                if (!xplrSdIsCardInit()) {
                    espErr = appInitLogging();
                    if (espErr == ESP_OK) {
                        APP_CONSOLE(I, "Logging is enabled!");
                    } else {
                        APP_CONSOLE(E, "Failed to enable logging");
                    }
                }
                /* Enable all log instances (the ones enabled during configuration) */
                if (xplrLogEnableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is re-enabled!");
                } else {
                    APP_CONSOLE(E, "Failed to re-enable logging");
                }
            } else {
                if (xplrSdIsCardInit()) {
                    xplrSdDeInit();
                }
                if (xplrLogDisableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is disabled!");
                } else {
                    APP_CONSOLE(E, "Failed to disable logging");
                }
            }
        } else {
            // Do nothing
        }
        prvState = currState;
        /* Window for other tasks to run */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif