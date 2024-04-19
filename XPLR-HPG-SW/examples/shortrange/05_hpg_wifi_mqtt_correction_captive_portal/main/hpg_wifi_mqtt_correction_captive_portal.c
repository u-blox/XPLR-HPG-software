/*
 * Copyright 2022 u-blox Ltd
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

/*Captive portal with live map tracking implementation.
 *
 * Current example demonstrates a captive portal implementation to configure U-blox XPLR-HPG-1/2 kits
 * with Wi-Fi and Thingstream credentials in order to acquire correction data from Thingstream's PointPerfect
 * location service, over Wi-Fi.
 *
 * The web interface also provides a map tracking application to visualize kit's position in real time.
 * To configure required credentials, the device boots in access point mode providing to the user an easy
 * configuration interface. As soon as the device is configured, credentials are stored in MCU's non-volatile memory
 * and switches to station mode for connecting to provisioned Wi-Fi.
 * If for any reason the kit fails to connect to a Wi-Fi network using the provisioned credentials then it switches back to
 * access point mode thus, allowing the user to reconfigure it. The device tries for at least 5 minutes to connect to a Wi-Fi network
 * before switching to access point again.
 *
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "xplr_wifi_starter.h"
#include "xplr_wifi_webserver.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
#include "xplr_common.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "u_cfg_app_platform_specific.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA         0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     1U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
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


#define MAJOR_APP_VER 1
#define MINOR_APP_VER 0
#define INTERNAL_APP_VER 0

#define KIB                         (1024U)                         /* Macros used to set buffer size in bytes */
#define APP_MQTT_PAYLOAD_BUF_SIZE   ((10U) * (KIB))                 /* Macros used to set buffer size in bytes */
#define APP_LOCATION_PRINT_PERIOD   (5U)                            /* Seconds to print location */
#define APP_LOCATION_UPDATE_PERIOD  (1U)                            /* Seconds to update location in webserver */
#define APP_MAX_TOPIC_CNT           (2U)                            /* Max MQTT topics to register */
#define APP_GNSS_I2C_ADDR           (0x42)                          /* GNSS I2C address in hex */
#define APP_LBAND_I2C_ADDR          (0x43)                          /* LBAND I2C address in hex */
#define APP_FACTORY_MODE_BTN        (BOARD_IO_BTN1)                 /* Button for factory reset */
#define APP_FACTORY_MODE_TRIGGER    (5U)                            /* Factory reset button press duration in sec */
#define APP_DEVICE_OFF_MODE_TRIGGER (APP_FACTORY_MODE_TRIGGER - 2U) /* Device off press duration in sec */
#define APP_INACTIVITY_TIMEOUT      (30)                            /* Time in seconds to trigger an inactivity timeout and cause a restart */
#define APP_RESTART_ON_ERROR        (1U)                            /* Trigger soft reset if device in error state */
#define APP_SD_DETECT_UPDATE_PERIOD (1U)                            /* Seconds to update the SD detect pin value */
#if 1 == APP_PRINT_IMU_DATA
#define APP_DEADRECK_PRINT_PERIOD   (10U)                           /* Seconds to print dead reckoning */
#endif

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
        uint8_t wifiWebserverLog : 1;
    } singleLogOpts;
    uint16_t value;
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
    int8_t          wifiWebserverLogIndex;
} appLog_t;

/* HPG related data vars */
typedef struct appHpg_type {
    xplrGnssDeviceCfg_t gnssConfig;     /**< GNSS config struct */
    uint64_t gnssLastAction;
    xplrLbandDeviceCfg_t lbandConfig;   /**< LBAND config struct */
    xplrLbandRegion_t lbandRegion;
    uint32_t lbandFrequency;
    bool isLbandInit;
    xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
    xplrGnssImuFusionStatus_t imuFusionStatus;
    xplrGnssImuVehDynMeas_t imuVehicleDynamics;
    xplrGnssStates_t gnssState;
    xplrGnssLocation_t location;
} appHpg_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char mqttHost[] = "mqtts://pp.services.u-blox.com";

/*
 * Fill this struct with your desired settings and try to connect
 * This is used only if you wish to override the setting from KConfig
 * and use the function with custom options
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA_AP,
    .webserver = true
};

/* thingstream platform vars */
static xplr_thingstream_t thingstreamSettings;
static xplr_thingstream_pp_region_t thingstreamRegion;
static xplr_thingstream_pp_plan_t  thingstreamPlan;

/*
 * A struct where we can store our received MQTT message
 */
static char data[APP_MQTT_PAYLOAD_BUF_SIZE];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = APP_MQTT_PAYLOAD_BUF_SIZE
};

/*
 * MQTT client config and handler
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;
static xplrMqttWifiGetItemError_t xplrMqttWifiErr;
static bool receivedMqttData = false;

/**
 * XPLR NVS struct pointer
*/
xplrNvs_t nvs;

appHpg_t hpg;

/**
 * Static log configuration struct and variables
 */

static appLog_t appLogCfg = {
    .logOptions.value = ~0, // All modules selected to log
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .mqttLogIndex = -1,
    .gnssLogIndex = -1,
    .gnssAsyncLogIndex = -1,
    .lbandLogIndex = -1,
    .locHelperLogIndex = -1,
    .thingstreamLogIndex = -1,
    .wifiStarterLogIndex = -1,
    .wifiWebserverLogIndex = -1
};

#if (APP_SD_LOGGING_ENABLED == 1)
static bool cardDetect = false;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
#endif
static esp_err_t appInitBoard(void);
static esp_err_t appInitWiFi(void);
static esp_err_t appInitNvs(void);
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *cfg);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *cfg);
static esp_err_t appInitGnssDevice(void);
static esp_err_t appInitLbandDevice(void);
static esp_err_t appRestartLbandAsync(void);
static void appWaitGnssReady(void);
static void appRunHpgFsm(void);
static void appMqttInit(void);
static esp_err_t appPrintLocation(uint8_t periodSecs);
static esp_err_t appUpdateServerLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static esp_err_t appPrintImuData(uint8_t periodSecs);
#endif
static char *appTimeFromBoot(void);
static char *appTimeToFix();
static void appVersionUpdate();
static void appCheckLogOption(uint8_t periodSecs);
static void appCheckDrOption(void);
static void appCheckDrCalibrationStatus(char *status);
static bool appCheckGnssInactivity(void);
static void appUpdateWebserverInfo(char *sd, char *dr, char *drCalibration);
static void appHaltExecution(void);
static void appTerminate(void);
static void appFactoryResetTask(void *arg);
static esp_err_t thingstreamInit(const char *token, xplr_thingstream_t *instance);

void app_main(void)
{
    bool coldStart = true;
    xplrWifiStarterFsmStates_t wifiState;

    bool isMqttInitialized = false;
    xplrMqttWifiClientStates_t  mqttState;
    esp_err_t brokerErr = ESP_FAIL;
    char *region = NULL;
    char *plan = NULL;
    bool isCorrData = false;

    esp_err_t gnssErr;
    esp_err_t lbandErr;
#if (APP_SD_LOGGING_ENABLED == 1)
    esp_err_t logErr;
#endif
    bool mqttWifiConnectedInitial = true;
    bool mqttGetItemInitial = true;
    bool topicFound[3];

    int8_t tVal = -1;
    uint32_t mqttStats[1][2] = { {0, 0} }; //num of msgs and total bytes received
    char mqttStatsStr[64];

    hpg.lbandRegion = XPLR_LBAND_FREQUENCY_EU; //assign a default value to LBAND region.

    char sdInfo[256] = {0};
    char drInfo[32] =  {0};
    char drCalibrationInfo[32] =  {0};

#if (APP_SD_LOGGING_ENABLED == 1)
    logErr = appInitLogging();
    if (logErr != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif
    appInitBoard();
    appInitWiFi();
    appInitNvs();
    appInitGnssDevice();
    xplrMqttWifiInitState(&mqttClient);

    if (coldStart) {
        appVersionUpdate(); //update fw version shown in webserver
        coldStart = false;
    }

    //wait until GNSS is in ready state (blocking)
    appWaitGnssReady();

    while (1) {
        appRunHpgFsm();
        xplrWifiStarterFsm();
        wifiState = xplrWifiStarterGetCurrentFsmState();
        if ((wifiState == XPLR_WIFISTARTER_STATE_CONNECT_OK) && (!isMqttInitialized)) {
            /*
            * We have connected to user's AP thus all config data should be available.
            * Lets configure the MQTT client to communicate with thingstream's PointPerfect service.
            */
            xplrWifiStarterWebserverDiagnosticsGet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
            if (tVal == 0) {
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                isMqttInitialized = true;
            } else {
                tVal = -1;
                xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
            }

            appUpdateWebserverInfo(sdInfo, drInfo, drCalibrationInfo);
        }

        if ((wifiState == XPLR_WIFISTARTER_STATE_CONNECT_OK) && (isMqttInitialized)) {
            /*
             * Module has access to the web and mqtt client is ready/initialized.
             * Lets connect to the Thingstream's PointPerfect broker and subscribe to
             * correction topics according to our region.
             */

            xplrMqttWifiFsm(&mqttClient);
            mqttState = xplrMqttWifiGetCurrentState(&mqttClient);

            appUpdateWebserverInfo(sdInfo, drInfo, drCalibrationInfo);

            switch (mqttState) {
                case XPLR_MQTTWIFI_STATE_CONNECTED:
                    if (mqttWifiConnectedInitial) {
                        XPLR_CI_CONSOLE(508, "OK");
                        mqttWifiConnectedInitial = false;
                    }
                    /* ok, we are connected to the broker. Lets subscribe to correction topics */
                    region = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_CLIENTREGION);
                    plan = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_CLIENTPLAN);
                    thingstreamRegion = xplrThingstreamRegionFromStr(region);
                    thingstreamPlan = xplrThingstreamPlanFromStr(plan);
                    brokerErr = thingstreamInit(NULL, &thingstreamSettings);
                    if (brokerErr != ESP_OK) {
                        APP_CONSOLE(E, "Thingstream module initialization failed!");
                        appHaltExecution();
                    } else {
                        //do nothing
                    }
                    brokerErr = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClient,
                                                                     &thingstreamSettings.pointPerfect);
                    if (brokerErr != ESP_OK) {
                        APP_CONSOLE(E, "Failed to subscribe to required topics. Correction data will not be available.");
                        appHaltExecution();
                    } else {
                        tVal = 1;
                        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
                        APP_CONSOLE(D, "Subscription plan is %s.", plan);
                        APP_CONSOLE(D, "Subscribed to required topics successfully.");
                    }
                    break;
                case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                    /* check if we have a msg in any of the topics subscribed */
                    xplrMqttWifiErr = xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage);
                    if (xplrMqttWifiErr == XPLR_MQTTWIFI_ITEM_OK) {
                        if (mqttGetItemInitial) {
                            XPLR_CI_CONSOLE(510, "OK");
                            mqttGetItemInitial = false;
                        }
                        /* we have a message available. check origin of topic and fwd it to gnss */
                        mqttStats[0][0]++;
                        mqttStats[0][1] += mqttMessage.dataLength;
                        snprintf(mqttStatsStr, 64, "Messages: %u (%u bytes)", mqttStats[0][0], mqttStats[0][1]);
                        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_MQTTSTATS,
                                                               (void *)mqttStatsStr);
                        tVal = 1;
                        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
                        topicFound[0] = xplrThingstreamPpMsgIsKeyDist(mqttMessage.topic, &thingstreamSettings);
                        topicFound[1] = xplrThingstreamPpMsgIsCorrectionData(mqttMessage.topic, &thingstreamSettings);
                        topicFound[2] = xplrThingstreamPpMsgIsFrequency(mqttMessage.topic, &thingstreamSettings);
                        if (memcmp(plan, "IP", strlen(plan)) == 0) {
                            if (hpg.isLbandInit) {
                                //lband module was initialized but we are in IP plan now. Lets switch correction data source
                                gnssErr = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_IP);
                                if (gnssErr != ESP_OK) {
                                    APP_CONSOLE(E, "Failed to set correction data source to IP");
                                    appHaltExecution();
                                }
                                hpg.isLbandInit = false;
                            }

                            if (topicFound[0]) {
                                gnssErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                                if (gnssErr != ESP_OK) {
                                    APP_CONSOLE(E, "Failed to send decryption keys!");
                                    appHaltExecution();
                                } else {
                                    // Do nothing
                                }
                            } else { //should be correction data
                                if (topicFound[1]) {
                                    isCorrData = true;
                                } else {
                                    APP_CONSOLE(E, "Region selected not supported...");
                                }

                                if (isCorrData) {
                                    hpg.gnssState = xplrGnssGetCurrentState(0);
                                    if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                                        hpg.gnssLastAction = esp_timer_get_time();
                                        gnssErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                                        if (gnssErr != ESP_OK) {
                                            APP_CONSOLE(E, "Failed to send correction data!");
                                            XPLR_CI_CONSOLE(511, "ERROR");
                                        } else {
                                            if (receivedMqttData == false) {
                                                XPLR_CI_CONSOLE(511, "OK");
                                                receivedMqttData = true;
                                            }
                                        }
                                    } else {
                                        APP_CONSOLE(W, "GNSS not READY or in ERROR");
                                        if (appCheckGnssInactivity()) {
                                            appTerminate();
                                        }
                                    }
                                } else {
                                    // Do nothing
                                }
                            }
                        } else if (memcmp(plan, "IP+LBAND", strlen(plan)) == 0) {
                            if (topicFound[0]) {
                                hpg.gnssState = xplrGnssGetCurrentState(0);
                                if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                                    hpg.gnssLastAction = esp_timer_get_time();
                                    gnssErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                                    if (gnssErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to send decryption keys!");
                                        appHaltExecution();
                                    }
                                } else {
                                    if (appCheckGnssInactivity()) {
                                        appTerminate();
                                    }
                                    APP_CONSOLE(W, "GNSS not READY or in ERROR");
                                }
                            } else if (topicFound[1]) { //should be correction data
                                hpg.gnssState = xplrGnssGetCurrentState(0);
                                if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                                    hpg.gnssLastAction = esp_timer_get_time();
                                    gnssErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                                    if (gnssErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to send correction data!");
                                        XPLR_CI_CONSOLE(511, "ERROR");
                                    } else {
                                        if (receivedMqttData == false) {
                                            XPLR_CI_CONSOLE(511, "OK");
                                            receivedMqttData = true;
                                        }
                                    }
                                } else {
                                    APP_CONSOLE(W, "GNSS not READY or in ERROR");
                                    if (appCheckGnssInactivity()) {
                                        appTerminate();
                                    }
                                }
                            } else if (topicFound[2]) {
                                // frequency topic, do nothing
                            }
                        } else if (memcmp(plan, "LBAND", strlen(plan)) == 0) {
                            //initialize lband module
                            if (!hpg.isLbandInit) {
                                appInitLbandDevice();
                                hpg.isLbandInit = true;
                            }
                            if (topicFound[0]) {
                                hpg.gnssState = xplrGnssGetCurrentState(0);
                                if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                                    hpg.gnssLastAction = esp_timer_get_time();
                                    gnssErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                                    if (gnssErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to send decryption keys!");
                                        appHaltExecution();
                                    }
                                } else {
                                    if (appCheckGnssInactivity()) {
                                        appTerminate();
                                    }
                                    APP_CONSOLE(W, "GNSS not READY or in ERROR");
                                }
                            } else { //should be lband frequency data
                                if (topicFound[2]) {
                                    lbandErr = xplrLbandSetFrequencyFromMqtt(0, mqttMessage.data, hpg.lbandConfig.corrDataConf.region);
                                    if (lbandErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to set frequency to LBAND module");
                                        appHaltExecution();
                                    } else {
                                        hpg.lbandFrequency = xplrLbandGetFrequency(0);
                                        if (hpg.lbandFrequency == 0) {
                                            APP_CONSOLE(I, "No LBAND frequency is set");
                                        }
                                        APP_CONSOLE(D, "LBAND frequency of %d Hz was set to module", hpg.lbandFrequency);
                                    }
                                }
                            }
                        } else {
                            APP_CONSOLE(E, "Subscription plan %s not supported.", plan);
                            APP_CONSOLE(E, "Failed to send correction data!");
                        }
                    } else if (xplrMqttWifiErr == XPLR_MQTTWIFI_ITEM_ERROR) {
                        XPLR_CI_CONSOLE(510, "ERROR");
                    }
                    break;
                case XPLR_MQTTWIFI_STATE_ERROR:
                    XPLR_CI_CONSOLE(508, "ERROR");
                    break;
                default:
                    tVal = -1;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
                    break;
            }
        }

        /* Print (to console) and update (webserver) location data */
        appPrintLocation(APP_LOCATION_PRINT_PERIOD);
#if 1 == APP_PRINT_IMU_DATA
        appPrintDeadReckoning(APP_DEADRECK_PRINT_PERIOD);
#endif
        appUpdateServerLocation(APP_LOCATION_UPDATE_PERIOD);
        appTimeFromBoot();
        appTimeToFix();
        appCheckLogOption(APP_SD_DETECT_UPDATE_PERIOD);
        appCheckDrOption();

#if 0
        /**
         * This part of code should be enabled in debugging, thus requires debug messages to be active
         * The function prints the list of tasks with their stack size, name, number and state
         * It also prints the currently available heap, the minimum heap size and the total heap size
         * The function can be modified to accept more arguments and keep any of the values needed.
        */
        xplrMemUsagePrint(60);
#endif
        vTaskDelay(pdMS_TO_TICKS(25));  /* A window so other tasks can run */
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
    xplrSd_error_t sdErr;

    /* Configure the SD card */
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

    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                "main_app.log",
                                                XPLRLOG_FILE_SIZE_INTERVAL,
                                                XPLRLOG_NEW_FILE_ON_BOOT);
            if (appLogCfg.appLogIndex >= 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.nvsLog == 1) {
            appLogCfg.nvsLogIndex = xplrNvsInitLogModule(NULL);
            if (appLogCfg.nvsLogIndex >= 0) {
                APP_CONSOLE(D, "NVS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.mqttLog == 1) {
            appLogCfg.mqttLogIndex = xplrMqttWifiInitLogModule(NULL);
            if (appLogCfg.mqttLogIndex >= 0) {
                APP_CONSOLE(D, "MQTT WiFi logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssLog == 1) {
            appLogCfg.gnssLogIndex = xplrGnssInitLogModule(NULL);
            if (appLogCfg.gnssLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssAsyncLog == 1) {
            appLogCfg.gnssAsyncLogIndex = xplrGnssAsyncLogInit(NULL);
            if (appLogCfg.gnssAsyncLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS Async logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.lbandLog == 1) {
            appLogCfg.lbandLogIndex = xplrLbandInitLogModule(NULL);
            if (appLogCfg.lbandLogIndex >= 0) {
                APP_CONSOLE(D, "LBAND logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.locHelperLog == 1) {
            appLogCfg.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
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
            appLogCfg.wifiStarterLogIndex = xplrWifiStarterInitLogModule(NULL);
            if (appLogCfg.wifiStarterLogIndex >= 0) {
                APP_CONSOLE(D, "WiFi Starter logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.wifiWebserverLog == 1) {
            appLogCfg.wifiWebserverLogIndex = xplrWifiWebserverInitLogModule(NULL);
            if (appLogCfg.wifiWebserverLogIndex >= 0) {
                APP_CONSOLE(D, "WiFi Webserver logging instance initialized");
            }
        }
    }

    return ret;
}
#endif

/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t ret;

    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        XPLR_CI_CONSOLE(501, "ERROR");
        appHaltExecution();
    } else {
        /* config boot0 pin as input */
        XPLR_CI_CONSOLE(501, "OK");
        io_conf.pin_bit_mask = 1ULL << APP_FACTORY_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        ret = gpio_config(&io_conf);
    }

    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        xTaskCreate(appFactoryResetTask, "factoryRstTask", 2 * 2048, NULL, 10, NULL);
        APP_CONSOLE(D, "Boot0 pin configured as button OK");
    }

    return ret;
}

/*
 * Initialize WiFi
 */
static esp_err_t appInitWiFi(void)
{
    esp_err_t ret;

    APP_CONSOLE(I, "Starting WiFi in station mode.");
    ret = xplrWifiStarterInitConnection(&wifiOptions);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    }

    return ret;
}

/*
 * Initialize NVS for all modules
 */
static esp_err_t appInitNvs(void)
{
    esp_err_t ret;
    xplrNvs_error_t err;

    err = xplrNvsInit(&nvs, "app");
    if (err == XPLR_NVS_OK) {
        ret = ESP_OK;
    } else {
        ret = ESP_FAIL;
    }

    return ret;
}

/*
 * Config GNSS settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *cfg)
{
    cfg->hw.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    cfg->hw.dvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
    cfg->hw.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    cfg->hw.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    cfg->hw.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    cfg->hw.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_GNSS_I2C_ADDR;
    cfg->hw.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    cfg->hw.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    cfg->hw.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    cfg->hw.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    cfg->hw.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    cfg->hw.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    cfg->hw.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    cfg->hw.dvcNetwork.devicePinPwr = -1;
    cfg->hw.dvcNetwork.devicePinDataReady = -1;

    xplrWifiStarterWebserverOptionsGet(XPLR_WIFISTARTER_SERVEOPTS_DR, &cfg->dr.enable);
    APP_CONSOLE(I, "DR FLAG IN MEMORY is %u", cfg->dr.enable);
    //cfg->dr.enable = drEnabled;
    cfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    cfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    cfg->corrData.keys.size = 0;
    cfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
}

/*
 * Config LBAND settings
 */
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *cfg)
{
    cfg->hwConf.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    cfg->hwConf.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    cfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    cfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    cfg->hwConf.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_LBAND_I2C_ADDR;
    cfg->hwConf.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    cfg->hwConf.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    cfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    cfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    cfg->hwConf.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    cfg->hwConf.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    cfg->hwConf.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    cfg->hwConf.dvcNetwork.devicePinPwr = -1;
    cfg->hwConf.dvcNetwork.devicePinDataReady = -1;

    cfg->destHandler = NULL;

    cfg->corrDataConf.freq = 0;
    switch (thingstreamRegion) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            cfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            cfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_US;
            break;
        default:
            cfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
            break;
    }
}

/*
 * Initialize GNSS
 */
static esp_err_t appInitGnssDevice(void)
{
    esp_err_t ret;
    ret = xplrGnssUbxlibInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    appConfigGnssSettings(&hpg.gnssConfig);

    ret = xplrGnssStartDevice(0, &hpg.gnssConfig);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    APP_CONSOLE(D, "GNSS init OK.");

    return ret;
}

/*
 * Initialize LBAND
 */
static esp_err_t appInitLbandDevice(void)
{
    uDeviceHandle_t *handler;
    esp_err_t ret;

    appWaitGnssReady();

    //set correction data source to LBand
    ret = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_LBAND);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set correction data source to LBAND");
        appHaltExecution();
    }

    handler = xplrGnssGetHandler(0);    //check if gnss module present
    if (handler != NULL) {
        APP_CONSOLE(D, "Init LBAND device");
        appConfigLbandSettings(&hpg.lbandConfig);
        hpg.lbandConfig.destHandler = handler;
        ret = xplrLbandStartDevice(0, &hpg.lbandConfig);
        if (ret != ESP_OK) {
            APP_CONSOLE(E, "Lband device config failed!");
            appHaltExecution();
        }

        APP_CONSOLE(I, "LBand module initialized successfully");
    } else {
        APP_CONSOLE(E, "Could not get GNSS device handler!");
        appHaltExecution();
    }

    return ret;
}

static esp_err_t appRestartLbandAsync(void)
{
    uDeviceHandle_t *handler;
    esp_err_t ret;

    appWaitGnssReady();

    handler = xplrGnssGetHandler(0);    //check if gnss module present
    if (handler != NULL) {
        APP_CONSOLE(D, "Restarting LBAND Async");
        hpg.lbandConfig.destHandler = handler;
        ret = xplrLbandSendCorrectionDataAsyncStart(0);
        if (ret != ESP_OK) {
            APP_CONSOLE(E, "Lband restart async failed!");
            appHaltExecution();
        }

        APP_CONSOLE(I, "LBand async restarted successfully");
    } else {
        APP_CONSOLE(E, "Could not get GNSS device handler!");
        appHaltExecution();
    }

    return ret;
}

/*
 * Wait for GNSS ready state
 */
static void appWaitGnssReady(void)
{
    while (hpg.gnssState != XPLR_GNSS_STATE_DEVICE_READY) {
        if (hpg.gnssState == XPLR_GNSS_STATE_ERROR) {
            APP_CONSOLE(E, "GNSS in error state");
            appHaltExecution();
        } else {
            xplrGnssFsm(0);
            hpg.gnssState = xplrGnssGetCurrentState(0);
            if (appCheckGnssInactivity()) {
                appTerminate();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(25));  /* A window so other tasks can run */
    }
}

/*
 * Run and control HPG modules
 */
static void appRunHpgFsm(void)
{
    xplrGnssFsm(0);
    hpg.gnssState = xplrGnssGetCurrentState(0);

    switch (hpg.gnssState) {
        case XPLR_GNSS_STATE_DEVICE_RESTART:
            if (hpg.isLbandInit && xplrLbandIsSendCorrectionDataAsyncRunning(0)) {
                xplrLbandSendCorrectionDataAsyncStop(0);
            }
            break;
        case XPLR_GNSS_STATE_DEVICE_READY:
            hpg.gnssLastAction = esp_timer_get_time();
            if (hpg.isLbandInit && !xplrLbandIsSendCorrectionDataAsyncRunning(0)) {
                appRestartLbandAsync();
            }
            break;
        case XPLR_GNSS_STATE_ERROR:
            APP_CONSOLE(E, "GNSS in error state");
            appTerminate();
            break;

        default:
            if (appCheckGnssInactivity()) {
                appTerminate();
            }
            break;
    }
}

/*
 * Initialize MQTT client
 */
static void appMqttInit(void)
{
    /**
     * We declare how many slots a ring buffer should have.
     * You can choose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    mqttClient.ucd.ringBufferSlotsNumber = 3;

    /**
     * Settings for mqtt client
     */
    mqttClientConfig.uri = mqttHost;
    mqttClientConfig.client_id = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_CLIENTID);
    mqttClientConfig.client_cert_pem = xplrWifiStarterWebserverDataGet(
                                           XPLR_WIFISTARTER_SERVERDATA_CLIENTCERT);
    mqttClientConfig.client_key_pem = xplrWifiStarterWebserverDataGet(
                                          XPLR_WIFISTARTER_SERVERDATA_CLIENTKEY);
    mqttClientConfig.cert_pem = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_ROOTCA);

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Initialize mqtt client
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/*
 * Periodic console print location (in seconds)
 */
static esp_err_t appPrintLocation(uint8_t periodSecs)
{
    static uint64_t prevTime = 0;
    esp_err_t ret;
    static bool locRTKFirstTime = true;

    hpg.gnssState = xplrGnssGetCurrentState(0);
    if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
        hpg.gnssLastAction = esp_timer_get_time();
        if ((MICROTOSEC(esp_timer_get_time() - prevTime) >= periodSecs) &&
            xplrGnssHasMessage(0)) {
            ret = xplrGnssGetLocationData(0, &hpg.location);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get gnss location data!");
                XPLR_CI_CONSOLE(512, "ERROR");
            } else {
                if (locRTKFirstTime) {
                    if ((hpg.location.locFixType == XPLR_GNSS_LOCFIX_FLOAT_RTK) ||
                        (hpg.location.locFixType == XPLR_GNSS_LOCFIX_FIXED_RTK)) {
                        locRTKFirstTime = false;
                        XPLR_CI_CONSOLE(10, "OK");
                    }
                }
                ret = xplrGnssPrintLocationData(&hpg.location);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not print gnss location data!");
                    XPLR_CI_CONSOLE(512, "ERROR");
                } else {
                    XPLR_CI_CONSOLE(512, "OK");
                }
            }

            ret = xplrGnssPrintGmapsLocation(0);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Gmaps location!");
                XPLR_CI_CONSOLE(512, "ERROR");
            }

            prevTime = esp_timer_get_time();
        } else {
            ret = ESP_ERR_NOT_FINISHED;
        }

    } else {
        if (appCheckGnssInactivity()) {
            appTerminate();
        }
        ret = ESP_ERR_NOT_FINISHED;
    }

    return ret;
}

/*
 * Periodic webserver update function with location data (in seconds)
 */
static esp_err_t appUpdateServerLocation(uint8_t periodSecs)
{
    xplrGnssLocation_t gnssInfo;
    char gMapStr[256] = {0};
    char timestamp[32] = {0};
    cJSON *jDoc;
    char *jBuff = NULL;
    int8_t i8Val;

    static uint64_t prevTime = 0;
    esp_err_t err[2];
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs) && xplrGnssHasMessage(0)) {
        err[0] = xplrGnssGetLocationData(0, &gnssInfo);
        if (err[0] != ESP_OK) {
            APP_CONSOLE(E, "Could not get gnss location");
        } else {
            err[1] = xplrGnssGetGmapsLocation(0, gMapStr, 256);
            if (err[1] != ESP_OK) {
                APP_CONSOLE(E, "Could not build Gmap string");
            }
        }

        for (int i = 0; i < 2; i++) {
            if (err[i] != ESP_OK) {
                ret = ESP_FAIL;
                break;
            }
            ret = ESP_OK;
        }

        if (ret != ESP_FAIL) {
            xplrTimestampToTime(gnssInfo.location.timeUtc,
                                timestamp,
                                32);

            jDoc = cJSON_CreateObject();
            if (jDoc != NULL) {
                cJSON_AddStringToObject(jDoc, "rsp", "dvcLocation");
                cJSON_AddNumberToObject(jDoc, "lat", (double)gnssInfo.location.latitudeX1e7 * (1e-7));
                cJSON_AddNumberToObject(jDoc, "lon", (double)gnssInfo.location.longitudeX1e7 * (1e-7));
                cJSON_AddNumberToObject(jDoc, "alt", (double)gnssInfo.location.altitudeMillimetres * (1e-3));
                cJSON_AddNumberToObject(jDoc, "speed", (double)gnssInfo.location.speedMillimetresPerSecond);
                cJSON_AddNumberToObject(jDoc, "accuracy", (double)gnssInfo.accuracy.horizontal * (1e-4));
                cJSON_AddNumberToObject(jDoc, "type", (double)gnssInfo.locFixType);
                cJSON_AddStringToObject(jDoc, "timestamp", timestamp);
                cJSON_AddStringToObject(jDoc, "gMap", gMapStr);

                jBuff = cJSON_Print(jDoc);
                if (jBuff != NULL) {
                    xplrWifiStarterWebserverLocationSet(jBuff);
                    i8Val = (int8_t)gnssInfo.locFixType;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_READY,
                                                           (void *)&i8Val);

                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_GNSS_ACCURACY,
                                                           (void *)&gnssInfo.accuracy.horizontal);
                    cJSON_free(jBuff);
                    jBuff = NULL;
                } else {
                    ret = ESP_FAIL;
                    APP_CONSOLE(E, "Failed to create json buffer");
                }
                cJSON_Delete(jDoc);
            } else {
                ret = ESP_FAIL;
                APP_CONSOLE(E, "Failed to create json doc");
            }
        }

        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        ret = ESP_ERR_NOT_FINISHED;
    }

    return ret;
}

#if 1 == APP_PRINT_IMU_DATA
static esp_err_t appPrintImuData(uint8_t periodSecs)
{
    static uint64_t prevTime = 0;
    esp_err_t ret;

    hpg.gnssState = xplrGnssGetCurrentState(0);
    if (hpg.gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
        hpg.gnssLastAction = esp_timer_get_time();
        if ((MICROTOSEC(esp_timer_get_time() - prevTime) >= periodSecs) &&
            xplrGnssIsDrEnabled(0)) {
            ret = xplrGnssGetImuAlignmentInfo(0, &hpg.imuAlignmentInfo);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu alignment info!");
            }

            ret = xplrGnssPrintImuAlignmentInfo(&hpg.imuAlignmentInfo);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment data!");
            }

            ret = xplrGnssGetImuAlignmentStatus(0, &hpg.imuFusionStatus);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu alignment status!");
            }
            ret = xplrGnssPrintImuAlignmentStatus(&hpg.imuFusionStatus);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment status!");
            }

            if (xplrGnssIsDrCalibrated(0)) {
                ret = xplrGnssGetImuVehicleDynamics(0, &hpg.imuVehicleDynamics);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
                }

                ret = xplrGnssPrintImuVehicleDynamics(&hpg.imuVehicleDynamics);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
                }
            }

            prevTime = esp_timer_get_time();
        } else {
            ret = ESP_ERR_NOT_FINISHED;
        }
    } else {
        if (appCheckGnssInactivity()) {
            appTerminate();
        }
        APP_CONSOLE(W, "GNSS not READY or in ERROR")
        ret = ESP_ERR_NOT_FINISHED;
    }

    return ret;
}
#endif

static char *appTimeFromBoot(void)
{
    static char uptime[32];
    static uint32_t prevTime = 0;
    int sec;
    int min;
    int hour;
    uint64_t timeNow = MICROTOSEC(esp_timer_get_time());
    char *ret;

    if (timeNow - prevTime >= 1) {
        sec = timeNow % 60;
        min = (timeNow / 60) % 60;
        hour = (timeNow / 60) / 60;

        memset(uptime, 0x00, 32);
        snprintf(uptime, 32, "%d:%02d:%02d", hour, min, sec);

        prevTime = MICROTOSEC(esp_timer_get_time());

        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_UPTIME,
                                               (void *)&uptime);

        ret = uptime;
    } else {
        ret = NULL;
    }

    return ret;
}

static char *appTimeToFix()
{
    static char fixTime[32];
    static bool coldBoot = true;
    static bool gotFix = false;
    static uint64_t bootTime = 0;
    static uint32_t prevTime = 0;
    xplrGnssLocation_t gnssInfo;
    uint32_t timeToFix;
    int8_t i8Val;
    esp_err_t err;
    int sec;
    int min;
    int hour;
    uint64_t timeNow = MICROTOSEC(esp_timer_get_time());
    char *ret;

    if (coldBoot) {
        bootTime = MICROTOSEC(esp_timer_get_time());
        coldBoot = false;
    }

    if (timeNow - prevTime >= 1) {
        err = xplrGnssGetLocationData(0, &gnssInfo);
        if (err != ESP_OK) {
            ret = NULL;
            APP_CONSOLE(E, "Could not get gnss location");
        } else {
            if (gnssInfo.locFixType != XPLR_GNSS_LOCFIX_INVALID && !gotFix) {
                timeToFix = timeNow - bootTime;
                gotFix = true;

                sec = timeToFix % 60;
                min = (timeToFix / 60) % 60;
                hour = (timeToFix / 60) / 60;

                memset(fixTime, 0x00, 32);
                snprintf(fixTime, 32, "%d:%02d:%02d", hour, min, sec);

                xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_FIXTIME,
                                                       (void *)&fixTime);

                ret = fixTime;
                APP_CONSOLE(I, "Device got FIX");
            } else {
                if (gnssInfo.locFixType == XPLR_GNSS_LOCFIX_INVALID && gotFix) {
                    // we had a fix but its gone... reset;
                    gotFix = false;
                    bootTime = MICROTOSEC(esp_timer_get_time());
                    i8Val = (int8_t)gnssInfo.locFixType;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_READY,
                                                           (void *)&i8Val);
                    APP_CONSOLE(E, "Device lost FIX");
                }
                ret = NULL;
            }
        }

        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        ret = NULL;
    }

    return ret;
}

static void appVersionUpdate()
{
    static char version[32];

    memset(version, 0x00, 32);
    if (INTERNAL_APP_VER > 0) {
        snprintf(version, 32, "%u.%u.%u", MAJOR_APP_VER, MINOR_APP_VER, INTERNAL_APP_VER);
    } else {
        snprintf(version, 32, "%u.%u", MAJOR_APP_VER, MINOR_APP_VER);
    }

    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_FWVERSION,
                                           (void *)&version);
}

/**
 * Check if SD logging has been enabled
*/
static void appCheckLogOption(uint8_t periodSecs)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    bool logActive;
    bool optChanged = false;
    static bool currentOpt = false;
    static bool isLogActive;
    static uint64_t prevTime = 0;
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr;
    esp_err_t espErr;

    xplrWifiStarterWebserverOptionsGet(XPLR_WIFISTARTER_SERVEOPTS_SD, &logActive);

    if (logActive != currentOpt) {
        currentOpt = logActive;
        optChanged = true;
    }

    if ((MICROTOSEC(esp_timer_get_time() - prevTime) >= periodSecs)) {
        cardDetect = xplrSdIsCardOn();
        prevTime = esp_timer_get_time();
    }

    if (logActive && cardDetect) {
        if (!xplrSdIsCardInit()) {
            espErr = appInitLogging();
            if (espErr == ESP_OK) {
                APP_CONSOLE(I, "SD ON and initialized");
            } else {
                APP_CONSOLE(E, "SD Log failed to reactivate");
            }
        }
    } else {
        if (optChanged && (!isLogActive)) {
            logErr = xplrLogDisableAll();
            if (logErr == XPLR_LOG_OK) {
                APP_CONSOLE(I, "SD Log de-activated");
            } else {
                APP_CONSOLE(E, "Failed to disable the SD logging");
            }
        }
        if (xplrSdIsCardInit()) {
            sdErr = xplrSdDeInit();
            if (sdErr == XPLR_SD_OK) {
                APP_CONSOLE(I, "SD OFF and de-initialized");
            } else {
                APP_CONSOLE(E, "SD OFF but failed to de-initialize");
            }
        }
    }
#endif
}

/**
 * Check if Dead Reckoning has been enabled
*/
static void appCheckDrOption(void)
{
    bool drActive;
    bool optChanged = false;
    static bool currentOpt = false;

    xplrWifiStarterWebserverOptionsGet(XPLR_WIFISTARTER_SERVEOPTS_DR, &drActive);

    if (drActive != currentOpt) {
        currentOpt = drActive;
        optChanged = true;
    }

    if (optChanged) {
        appWaitGnssReady();
    }

    if (drActive && !xplrGnssIsDrEnabled(0)) {
        if (optChanged) {
            xplrGnssEnableDeadReckoning(0);
            APP_CONSOLE(I, "DR activated");
        }
    } else {
        if (xplrGnssIsDrEnabled(0)) {
            if (optChanged) {
                xplrGnssDisableDeadReckoning(0);
                APP_CONSOLE(W, "DR de-activated");
            }
        }
    }
}

/**
 * Check if Dead Reckoning IMU calibration is completed
*/
static void appCheckDrCalibrationStatus(char *status)
{
    bool calibrated;

    //wait until GNSS is in ready state
    appWaitGnssReady();

    if (xplrGnssIsDrCalibrated(0)) {
        calibrated = true;
        xplrWifiStarterWebserverOptionsSet(XPLR_WIFISTARTER_SERVEOPTS_DR_CALIBRATION, &calibrated);
        sprintf(status, "True");
        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_DR_CALIB_INFO, status);
    } else {
        calibrated = false;
        xplrWifiStarterWebserverOptionsSet(XPLR_WIFISTARTER_SERVEOPTS_DR_CALIBRATION, &calibrated);
        sprintf(status, "False");
        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_DR_CALIB_INFO, status);
    }
}

/**
 * Check if GNSS module inactivity has reached the TIMEOUT
*/
static bool appCheckGnssInactivity(void)
{
    bool hasGnssTimedOut;

    if (MICROTOSEC(esp_timer_get_time() - hpg.gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
        hasGnssTimedOut = true;
    } else {
        hasGnssTimedOut = false;
    }

    return hasGnssTimedOut;
}

/**
 * Update webserver info
*/
static void appUpdateWebserverInfo(char *sd, char *dr, char *drCalibration)
{
    bool isGnssDrActive;

#if (1 == APP_SD_LOGGING_ENABLED)
    if (xplrSdIsCardOn()) {
        sprintf(sd,
                "free:%llukb / used:%llukb / total:%llukb",
                xplrSdGetFreeSpace(),
                xplrSdGetUsedSpace(),
                xplrSdGetTotalSpace());
    } else {
        sprintf(sd, "log is disabled");
    }
    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_SDSTATS, sd);
#endif
    xplrWifiStarterWebserverOptionsGet(XPLR_WIFISTARTER_SERVEOPTS_DR, &isGnssDrActive);
    if (isGnssDrActive) {
        sprintf(dr, "Enabled");
    } else {
        sprintf(dr, "Disabled");
    }
    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_DRINFO, dr);

    appCheckDrCalibrationStatus(drCalibration);
}

/*
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    xplrWifiStarterDisconnect();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appTerminate(void)
{
    xplrGnssError_t gnssErr;
    esp_err_t espErr;
    uint64_t timePrevLoc;

    APP_CONSOLE(E, "GNSS module has reached an inactivity timeout. Reseting...");

    xplrWifiStarterDisconnect();
    espErr = xplrGnssStopDevice(0);
    timePrevLoc = esp_timer_get_time();
    do {
        gnssErr = xplrGnssFsm(0);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
            gnssErr == XPLR_GNSS_ERROR &&
            espErr != ESP_OK) {
            break;
        }
    } while (gnssErr != XPLR_GNSS_STOPPED);
#if (APP_RESTART_ON_ERROR == 1)
    esp_restart();
#else
    appHaltExecution();
#endif
}

static void appFactoryResetTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_FACTORY_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_FACTORY_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;

            if (btnPressDuration >= APP_FACTORY_MODE_TRIGGER) {
                APP_CONSOLE(W, "Factory reset triggered");
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrWifiStarterDeviceErase();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}

static esp_err_t thingstreamInit(const char *token, xplr_thingstream_t *instance)
{
    const char *ztpToken = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
    esp_err_t ret;
    xplr_thingstream_error_t err;

    /* initialize thingstream instance with dummy token */
    instance->connType = XPLR_THINGSTREAM_PP_CONN_WIFI;
    err = xplrThingstreamInit(ztpToken, instance);

    if (err != XPLR_THINGSTREAM_OK) {
        ret = ESP_FAIL;
    } else {
        /* Config thingstream topics according to region and subscription plan*/
        err = xplrThingstreamPpConfigTopics(thingstreamRegion, thingstreamPlan, false, instance);
        if (err == XPLR_THINGSTREAM_OK) {
            ret = ESP_OK;
        } else {
            ret = ESP_FAIL;
        }
    }

    return ret;
}