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
 * sets up LBAND module (in our case the NEO-D9S module), if the Thingstream plan supports it,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_crt_bundle.h"
#include "xplr_wifi_starter.h"
#include "xplr_thingstream.h"
#include "xplr_ztp.h"
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

#define APP_PRINT_IMU_DATA         0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
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
 * Time in seconds to trigger an inactivity timeout and cause a restart
 */
#define APP_INACTIVITY_TIMEOUT          30

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
 * Max topic count
 */
#define APP_MAX_TOPIC_CNT   2

/**
 * GNSS and LBAND I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42
#define APP_LBAND_I2C_ADDR 0x43

/**
 * Option to enable the correction message watchdog mechanism.
 * When set to 1, if no correction data are forwarded to the GNSS
 * module (either via IP or SPARTN) for a defined amount of time
 * (MQTT_MESSAGE_TIMEOUT macro defined in hpglib/xplr_mqtt/include/xplr_mqtt.h)
 * an error event will be triggered
*/
#define APP_ENABLE_CORR_MSG_WDG (1U)

/**
 * Thingstream subscription plan region
 * for correction data
*/
#define APP_THINGSTREAM_REGION  XPLR_THINGSTREAM_PP_REGION_EU

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
        uint8_t appLog          : 1;
        uint8_t nvsLog          : 1;
        uint8_t ztpLog          : 1;
        uint8_t mqttLog         : 1;
        uint8_t gnssLog         : 1;
        uint8_t gnssAsyncLog    : 1;
        uint8_t lbandLog        : 1;
        uint8_t locHelperLog    : 1;
        uint8_t thingstreamLog  : 1;
        uint8_t wifistarterLog  : 1;
    } singleLogOpts;
    uint16_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          ztpLogIndex;
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

/* thingstream platform vars */
static xplr_thingstream_t thingstreamSettings;
static xplr_thingstream_pp_region_t ppRegion = APP_THINGSTREAM_REGION;
/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the
 * values as needed here.
 */
static const char *urlAwsRootCa = CONFIG_XPLR_AWS_ROOTCA_URL;
static const char *tsPpZtpToken = CONFIG_XPLR_TS_PP_ZTP_TOKEN;

/**
 * ZTP payload from POST
 */
static char ztpPostPayload[APP_ZTP_PAYLOAD_BUF_SIZE];
static xplrZtpData_t ztpData = {
    .payload = ztpPostPayload,
    .payloadLength = APP_ZTP_PAYLOAD_BUF_SIZE
};

/**
 * Keeps time at "this" point of code execution.
 * Used to calculate elapse time.
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

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
static bool isNeededTopic;
static bool deviceOffRequested = false;
static bool isPlanLband = false;

/**
 * Stack pointer used in HTTP response callback
*/
static uint32_t bufferStackPointer = 0;

/**
 * Error return types
 */
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;
static xplrMqttWifiGetItemError_t mqttGetItemErr;
static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0, // All modules selected to log
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .ztpLogIndex = -1,
    .mqttLogIndex = -1,
    .gnssLogIndex = -1,
    .gnssAsyncLogIndex = -1,
    .lbandLogIndex = -1,
    .locHelperLogIndex = -1,
    .thingstreamLogIndex = -1,
    .wifiStarterLogIndex = -1
};

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif

static char *configData = ztpPostPayload;
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
static esp_err_t appGetRootCa(void);
static esp_err_t appApplyThingstreamCreds(void);
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

/* ---------------------------------------------------------------
 * CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
static esp_err_t httpClientEventCB(esp_http_client_event_handle_t evt);

void app_main(void)
{
    esp_err_t espRet;
    xplr_thingstream_error_t tsErr;
    xplrGnssError_t gnssErr;
    uint64_t gnssLastAction = esp_timer_get_time();
    static bool fetchedCorrectionDataInitial = true;
    static bool sentCorrectionDataInitial = true;
    static bool mqttConnectedInitial = true;
    gotZtp = false;

    appInitBoard();
    espRet = appFetchConfigFromFile();
    if (espRet == ESP_OK) {
        appApplyConfigFromFile();
    } else {
        APP_CONSOLE(D, "No configuration file found, running on Kconfig configuration");
    }

#if (APP_SD_LOGGING_ENABLED == 1)
    espRet = appInitLogging();
    if (espRet != ESP_OK) {
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
                        espRet = xplrLbandSetDestGnssHandler(lbandDvcPrfId, dvcLbandConfig.destHandler);
                        if (espRet == ESP_OK) {
                            espRet = xplrLbandSendCorrectionDataAsyncStart(lbandDvcPrfId);
                            if (espRet != ESP_OK) {
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
                    espRet = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    if (espRet != ESP_OK) {
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
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
                }
                break;
                break;
        }

        wifistarterErr = xplrWifiStarterFsm();

        /**
         * If we connect to WiFi we can continue with ZTP and
         * then with MQTT
         */
        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (!gotZtp) {
                thingstreamSettings.connType = XPLR_THINGSTREAM_PP_CONN_WIFI;
                tsErr = xplrThingstreamInit(tsPpZtpToken, &thingstreamSettings);
                if (tsErr != XPLR_THINGSTREAM_OK) {
                    APP_CONSOLE(E, "Error in Thingstream configuration");
                    XPLR_CI_CONSOLE(405, "ERROR");
                    appHaltExecution();
                } else {
                    XPLR_CI_CONSOLE(405, "OK");
                    espRet = appGetRootCa();
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "Could not get Root CA certificate from Amazon. Halting execution...");
                        XPLR_CI_CONSOLE(406, "ERROR");
                        appHaltExecution();
                    } else {
                        XPLR_CI_CONSOLE(406, "OK");
                        espRet = xplrZtpGetPayloadWifi(&thingstreamSettings,
                                                       &ztpData);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Error in ZTP");
                            XPLR_CI_CONSOLE(407, "ERROR");
                            appHaltExecution();
                        } else {
                            XPLR_CI_CONSOLE(407, "OK");
                            espRet = appApplyThingstreamCreds();
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Error in applying Thingstream Credentials");
                                appHaltExecution();
                            } else {
                                gotZtp = true;
                                APP_CONSOLE(I, "ZTP Successful!");
                            }
                        }
                    }
                }
                /**
                 * Since MQTT is supported we can initialize the MQTT broker and try to connect
                 */
                if (thingstreamSettings.pointPerfect.mqttSupported) {
                    appMqttInit();
                    espRet = xplrMqttWifiStart(&mqttClient);
                    requestDc = false;
                }

                if (thingstreamSettings.pointPerfect.lbandSupported) {
                    isPlanLband = (bool)gnssCorrSrc;
                    if (isPlanLband) {
                        /* We have Lband support, so we need to initialize the module */
                        appInitLbandDevice();
                    }
                    if (!thingstreamSettings.pointPerfect.mqttSupported) {
                        appMqttInit();
                        xplrMqttWifiStart(&mqttClient);
                        requestDc = false;
                    }
                }
            }
        }

        /**
         * This sample uses ZTP to function which gets all required setting to connect
         * to Thingstream services such as PointPerfect.
         */
        mqttErr = xplrMqttWifiFsm(&mqttClient);
        if (mqttErr == XPLR_MQTTWIFI_ERROR) {
            XPLR_CI_CONSOLE(409, "ERROR");
        }

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                if (mqttConnectedInitial) {
                    XPLR_CI_CONSOLE(409, "OK");
                    mqttConnectedInitial = false;
                }
                /**
                 * Lets try to use ZTP format topics to subscribe
                 * We subscribe after the GNSS device is ready. This way we do not
                 * lose the first message which contains the decryption keys
                 */
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    gnssLastAction = esp_timer_get_time();
                    espRet = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArrayZtp failed");
                        XPLR_CI_CONSOLE(410, "ERROR");
                        appHaltExecution();
                    } else {
                        XPLR_CI_CONSOLE(410, "OK");
                    }
                } else if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
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
                mqttGetItemErr = xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage);
                if (mqttGetItemErr == XPLR_MQTTWIFI_ITEM_OK) {
                    if (fetchedCorrectionDataInitial) {
                        XPLR_CI_CONSOLE(411, "OK");
                        fetchedCorrectionDataInitial = false;
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
                            espRet = xplrGnssSendDecryptionKeys(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                XPLR_CI_CONSOLE(412, "ERROR");
                                appHaltExecution();
                            } else {
                                XPLR_CI_CONSOLE(412, "OK");
                            }
                        }
                        /*INDENT-OFF*/
                        isNeededTopic = xplrThingstreamPpMsgIsCorrectionData(mqttMessage.topic, &thingstreamSettings);
                        /*INDENT-ON*/
                        if (isNeededTopic && (!isPlanLband)) {
                            espRet = xplrGnssSendCorrectionData(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send correction data!");
                                XPLR_CI_CONSOLE(11, "ERROR");
                            } else if (sentCorrectionDataInitial) {
                                XPLR_CI_CONSOLE(11, "OK");
                                sentCorrectionDataInitial = false;
                            }
                        }
                        isNeededTopic = xplrThingstreamPpMsgIsFrequency(mqttMessage.topic, &thingstreamSettings);
                        if (isNeededTopic && isPlanLband) {
                            espRet = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                                   mqttMessage.data,
                                                                   dvcLbandConfig.corrDataConf.region);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to set frequency!");
                                XPLR_CI_CONSOLE(413, "ERROR");
                                appHaltExecution();
                            } else {
                                frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                                if (frequency == 0) {
                                    APP_CONSOLE(I, "No LBAND frequency is set");
                                    XPLR_CI_CONSOLE(413, "ERROR");
                                }
                                APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
                            }
                        }
                    } else if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                        appTerminate();
                    }
                } else if (mqttGetItemErr == XPLR_MQTTWIFI_ITEM_ERROR) {
                    XPLR_CI_CONSOLE(411, "ERROR");
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

        if (deviceOffRequested) {
            xplrMqttWifiUnsubscribeFromTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
            xplrMqttWifiHardDisconnect(&mqttClient);
            if ((dvcLbandConfig.destHandler != NULL) && isPlanLband) {
                espRet = xplrLbandPowerOffDevice(lbandDvcPrfId);
                if (espRet != ESP_OK) {
                    APP_CONSOLE(E, "Failed to stop Lband device!");
                } else {
                    dvcLbandConfig.destHandler = NULL;
                }
            }
            xplrGnssStopAllAsyncs(gnssDvcPrfId);
            espRet = xplrGnssPowerOffDevice(gnssDvcPrfId);
            timePrevLoc = esp_timer_get_time();
            do {
                gnssErr = xplrGnssFsm(gnssDvcPrfId);
                vTaskDelay(pdMS_TO_TICKS(10));
                if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
                    gnssErr == XPLR_GNSS_ERROR &&
                    espRet != ESP_OK) {
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
        if (appLogCfg.logOptions.singleLogOpts.ztpLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.ztpLogIndex];
                appLogCfg.ztpLogIndex = xplrZtpInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.ztpLogIndex = xplrZtpInitLogModule(NULL);
            }

            if (appLogCfg.ztpLogIndex >= 0) {
                APP_CONSOLE(D, "ZTP logging instance initialized");
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
        if (appLogCfg.logOptions.singleLogOpts.wifistarterLog == 1) {
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
            memset(configData, 0, APP_ZTP_PAYLOAD_BUF_SIZE);
            sdErr = xplrSdReadFileString(configFilename, configData, APP_ZTP_PAYLOAD_BUF_SIZE);
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
    /* Empty the array for the next functions */
    memset(configData, 0, APP_ZTP_PAYLOAD_BUF_SIZE);

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
    tsPpZtpToken = appOptions.tsCfg.ztpToken;
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
                appLogCfg.logOptions.singleLogOpts.wifistarterLog = 1;
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
        } else if (strstr(instance->description, "ZTP") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.ztpLog = 1;
                appLogCfg.ztpLogIndex = i;
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
        XPLR_CI_CONSOLE(404, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(404, "OK");
    }
}

/**
 * Populates gnss settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
{
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

/**
 * Function that initializes the GNSS device
 */
static void appInitGnssDevice(void)
{
    esp_err_t espRet;

    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        XPLR_CI_CONSOLE(401, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(401, "OK");
    }

    appConfigGnssSettings(&dvcGnssConfig);
    espRet = xplrGnssStartDevice(0, &dvcGnssConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        XPLR_CI_CONSOLE(402, "ERROR");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
    XPLR_CI_CONSOLE(402, "OK");
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
        XPLR_CI_CONSOLE(403, "ERROR");
        appHaltExecution();
    } else {
        espRet = xplrLbandPrintDeviceInfo(lbandDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print LBAND device info!");
            XPLR_CI_CONSOLE(403, "ERROR");
            appHaltExecution();
        }
    }
}

/**
 * Function that handles the HTTP GET request to fetch the Root CA certificate
*/
static esp_err_t appGetRootCa(void)
{
    esp_err_t ret;
    esp_http_client_handle_t client = NULL;
    char rootCa[APP_KEYCERT_PARSE_BUF_SIZE];
    xplrZtpData_t userData = {
        .payload = rootCa,
        .payloadLength = APP_KEYCERT_PARSE_BUF_SIZE
    };

    //Configure http client
    esp_http_client_config_t clientConfig = {
        .url = urlAwsRootCa,
        .method = HTTP_METHOD_GET,
        .event_handler = httpClientEventCB,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = &userData
    };

    client = esp_http_client_init(&clientConfig);
    if (client != NULL) {
        ret = esp_http_client_set_header(client, "Accept", "text/html");
        if (ret == ESP_OK) {
            ret = esp_http_client_perform(client); //Blocking no need for retries or while loop
            if (ret == ESP_OK) {
                userData.httpReturnCode = esp_http_client_get_status_code(client);
                if (userData.httpReturnCode == 200) {
                    int len = esp_http_client_get_content_length(client);
                    APP_CONSOLE(I, "HTTPS GET request OK: code [%d] - payload size [%d].", userData.httpReturnCode,
                                len);
                } else {
                    APP_CONSOLE(E, "HTTPS GET request failed with code [%d]", userData.httpReturnCode);
                }
            } else {
                APP_CONSOLE(E, "Error in GET request");
            }
        } else {
            APP_CONSOLE(E, "Failed to set HTTP headers");
        }
        esp_http_client_cleanup(client);
    } else {
        APP_CONSOLE(E, "Could not initiate HTTP client");
        ret = ESP_FAIL;
    }

    if (rootCa != NULL) {
        memcpy(thingstreamSettings.server.rootCa, rootCa, APP_KEYCERT_PARSE_BUF_SIZE);
    }
    return ret;
}

/**
 * Function that applies the Thingstream credentials fetched from the ZTP
*/
static esp_err_t appApplyThingstreamCreds(void)
{
    esp_err_t espRet;
    xplr_thingstream_error_t tsErr;

    tsErr = xplrThingstreamPpConfig(ztpData.payload, ppRegion, (bool)gnssCorrSrc, &thingstreamSettings);
    if (tsErr != XPLR_THINGSTREAM_OK) {
        espRet = ESP_FAIL;
        APP_CONSOLE(E, "Error in Thingstream credential payload");
        XPLR_CI_CONSOLE(408, "ERROR");
    } else {
        XPLR_CI_CONSOLE(408, "OK");
        espRet = ESP_OK;
    }
    return espRet;
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
     * If ZTP is successful then all the following settings
     * will be populated
     */
    mqttClientConfig.uri = thingstreamSettings.pointPerfect.brokerAddress;
    mqttClientConfig.client_id = thingstreamSettings.pointPerfect.deviceId;
    mqttClientConfig.client_cert_pem = (const char *)thingstreamSettings.pointPerfect.clientCert;
    mqttClientConfig.client_key_pem = (const char *)thingstreamSettings.pointPerfect.clientKey;
    mqttClientConfig.cert_pem = (const char *)thingstreamSettings.server.rootCa;

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Let's start our client.
     * In case of multiple clients an array of clients can be used.
     */
    ret = xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to initialize Mqtt client!");
        appHaltExecution();
    }
}

/**
 * Prints locations according to period
 */
static void appPrintLocation(uint8_t periodSecs)
{
    esp_err_t ret;
    static bool locRTKFirstTime = true;
    static bool allowedPrint = false;
    static double initialTime = 0;

    // postpone printing for ~10 seconds to avoid CI time out
    if (allowedPrint == false) {
        if (initialTime == 0) {
            initialTime = MICROTOSEC(esp_timer_get_time());
        } else {
            if ((MICROTOSEC(esp_timer_get_time()) - initialTime) > 12) {
                allowedPrint = true;
            }
        }
    } else {
        if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) && xplrGnssHasMessage(0)) {
            ret = xplrGnssGetLocationData(0, &locData);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get gnss location data!");
                XPLR_CI_CONSOLE(415, "ERROR");
            } else {
                if (locRTKFirstTime) {
                    if ((locData.locFixType == XPLR_GNSS_LOCFIX_FLOAT_RTK) ||
                        (locData.locFixType == XPLR_GNSS_LOCFIX_FIXED_RTK)) {
                        locRTKFirstTime = false;
                        XPLR_CI_CONSOLE(10, "OK");
                    }
                }
                ret = xplrGnssPrintLocationData(&locData);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not print gnss location data!");
                    XPLR_CI_CONSOLE(415, "ERROR");
                } else {
                    XPLR_CI_CONSOLE(415, "OK");
                }
            }

            ret = xplrGnssPrintGmapsLocation(0);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Gmaps location!");
                XPLR_CI_CONSOLE(415, "ERROR");
            }

            timePrevLoc = MICROTOSEC(esp_timer_get_time());
        }
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
        }

        ret = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment data!");
        }

        ret = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        }
        ret = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment status!");
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            ret = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            }

            ret = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
            }
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
                if (xplrLogDisableAll() == XPLR_LOG_OK && xplrGnssAsyncLogStop() == ESP_OK) {
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

/**
 * CALLBACK FUNCTION
*/
static esp_err_t httpClientEventCB(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            APP_CONSOLE(D, "HTTP_EVENT_ON_CONNECTED!");
            break;

        case HTTP_EVENT_HEADERS_SENT:
            break;

        case HTTP_EVENT_ON_HEADER:
            break;

        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                xplrZtpData_t *tempData = (xplrZtpData_t *)evt->user_data;
                if (bufferStackPointer < tempData->payloadLength) {
                    memcpy(tempData->payload + bufferStackPointer, evt->data, evt->data_len);
                    bufferStackPointer += evt->data_len;
                    tempData->payload[bufferStackPointer] = 0;
                } else {
                    APP_CONSOLE(E, "Payload buffer not big enough. Could not copy all data from HTTP!");
                }
            }
            break;

        case HTTP_EVENT_ERROR:
            /*
             * How does the following ESP_LOG work?
             * We are currently receiving data from the internet.
             * We are not sure if the payload is null terminated.
             * We need to define the length of the string to print.
             * The following "%.*s" --> expands to: print string s as many chars as dataLength.
             * It's the equivalent of writing
             *      ESP_LOGE("HTTP_EVENT_ERROR: %.6f", float);
             * which is more intuitive to understand; we define a constant number of decimal places.
             * In our case the number of characters is defined on runtime by ".*"
             * You can see how it works on:
             * https://embeddedartistry.com/blog/2017/07/05/printf-a-limited-number-of-characters-from-a-string/
             */
            APP_CONSOLE(E, "HTTP_EVENT_ERROR: %.*s", evt->data_len, (char *)evt->data);
            break;

        case HTTP_EVENT_ON_FINISH:
            APP_CONSOLE(D, "HTTP_EVENT_ON_FINISH");
            break;

        default:
            break;
    }

    return ESP_OK;
}