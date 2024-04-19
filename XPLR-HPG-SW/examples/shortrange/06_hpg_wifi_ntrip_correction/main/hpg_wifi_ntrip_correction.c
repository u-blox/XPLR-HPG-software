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

/*
 * An example utilizing NTRIP WiFi Client module to fetch correction data.
 *
 * In the current example U-blox XPLR-HPG kit is initialized using boards component,
 * connects to WiFi network using wifi_starter component,
 * connects to the NTRIP caster, using xplr_ntrip component.
 *
 * XPLRNTRIP_RECEIVE_DATA_SIZE and XPLRNTRIP_GGA_INTERVAL_S
 * are defined into xplr_ntrip_client.c of ntripWifiClient_service component.
 */

#include <stdio.h>
#include <string.h>
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "xplr_wifi_starter.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/ntripWiFiClient_service/xplr_wifi_ntrip_client.h"
#include "driver/timer.h"

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
 * Seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD 5

#if 1 == APP_PRINT_IMU_DATA
/**
 * Period in seconds to print Dead Reckoning data
 */
#define APP_DEAD_RECKONING_PRINT_PERIOD 5
#endif

/**
 * Application timeout
 */
#define APP_TIMEOUT 120

/**
 * Time in seconds to trigger an inactivity timeout and cause a restart
 */
#define APP_INACTIVITY_TIMEOUT 30

/**
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR 0x42

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
        uint8_t ntripLog         : 1;
        uint8_t gnssLog          : 1;
        uint8_t gnssAsyncLog     : 1;
        uint8_t locHelperLog     : 1;
        uint8_t wifiStarterLog   : 1;
    } singleLogOpts;
    uint8_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          ntripLogIndex;
    int8_t          gnssLogIndex;
    int8_t          gnssAsyncLogIndex;
    int8_t          locHelperLogIndex;
    int8_t          wifiStarterLogIndex;
} appLog_t;

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* Application settings */
static uint64_t appRunTime = APP_TIMEOUT;
static uint32_t locPrintInterval = APP_LOCATION_PRINT_PERIOD;
#if (APP_PRINT_IMU_DATA == 1)
static uint32_t imuPrintInterval = APP_DEAD_RECKONING_PRINT_PERIOD;
#endif

/**
 * GNSS configuration.
 * This is an example for a GNSS ZED-F9 module.
 * The same structure can be used with an LBAND NEO-D9s module.
 * Depending on your device and board you might have to change these values.
 */
static xplrGnssDeviceCfg_t dvcConfig;
static xplrLocDeviceType_t gnssDvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
static bool gnssDrEnable = CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/**
 * Gnss device profile id
 */
const uint8_t gnssDvcPrfId = 0;

/**
 * Location data struct
 */
xplrGnssLocation_t locData;

#if 1 == APP_PRINT_IMU_DATA
/**
 * Dead Reckoning data structs
 */
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif

/**
 * You can use KConfig to set up these values.
 */
static char *ntripHost = CONFIG_XPLR_NTRIP_HOST;
static int ntripPort = CONFIG_XPLR_NTRIP_PORT;
static char *ntripMountpoint = CONFIG_XPLR_NTRIP_MOUNTPOINT;
static char *ntripUserAgent = CONFIG_XPLR_NTRIP_USERAGENT;

#ifdef CONFIG_XPLR_NTRIP_GGA_MSG
static bool ntripSendGga = true;
#else
static bool ntripSendGga = false;
#endif
#ifdef CONFIG_XPLR_NTRIP_USE_AUTH
static bool ntripUseAuth = true;
#else
static bool ntripUseAuth = false;
#endif
static char *ntripUser = CONFIG_XPLR_NTRIP_USERNAME;
static char *ntripPass = CONFIG_XPLR_NTRIP_PASSWORD;

/**
 * Keeps time at "this" point of variable
 * assignment in code
 * Used for periodic printing
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif
static uint64_t timeOut;
static uint64_t gnssLastAction;

/*
 * Fill this struct with your desired settings and try to connect
 * This is used only if you wish to override the setting from KConfig
 * and use the function with custom options
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword
};

/**
 * NTIRP client
 */
xplrWifi_ntrip_client_t ntripClient;
xplr_ntrip_config_t ntripConfig;
xplr_ntrip_error_t ntripClientError;
xplr_ntrip_detailed_error_t ntripClientDetailedError;
static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;

char ntripBuffer[XPLRNTRIP_RECEIVE_DATA_SIZE];
uint32_t ntripSize;
SemaphoreHandle_t ntripSemaphore;

char GgaMsg[256];
char *ntripGgaMsgPtr = GgaMsg;
char **ntripGgaMsgPtrPtr = &ntripGgaMsgPtr;
int32_t len;

/**
 * Static log configuration struct and variables
 */

static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0, // All modules selected to log
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .ntripLogIndex = -1,
    .gnssLogIndex = -1,
    .gnssAsyncLogIndex = -1,
    .locHelperLogIndex = -1,
    .wifiStarterLogIndex = -1,
};

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
static void appInitBoard(void);
static esp_err_t appFetchConfigFromFile(void);
static void appApplyConfigFromFile(void);
static esp_err_t appInitSd(void);
static void appInitWiFi(void);
static void timerInit(void);
static void appInitGnssDevice(void);
static void appNtripInit(void);
static void appNtripDeInit(void);
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appTerminate(void);
static void appHaltExecution(void);
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appInitHotPlugTask(void);
static void appCardDetectTask(void *arg);
#endif

void app_main(void)
{
    esp_err_t espErr;
    xplrGnssError_t gnssErr;
    static bool receivedNtripDataInitial = true;
    static bool SentCorrectionDataInitial = true;
    timePrevLoc = MICROTOSEC(esp_timer_get_time());
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = MICROTOSEC(esp_timer_get_time());
#endif

    appInitBoard();
    espErr = appFetchConfigFromFile();
    if (espErr == ESP_OK) {
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
    timeOut = MICROTOSEC(esp_timer_get_time());
    appInitWiFi();
    appInitGnssDevice();

    while ((MICROTOSEC(esp_timer_get_time()) - timeOut <= appRunTime)) {
        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                gnssLastAction = esp_timer_get_time();
                wifistarterErr = xplrWifiStarterFsm();
                if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
                    if (!ntripClient.socketIsValid) {
                        appNtripInit();
                    } else {
                        switch (xplrWifiNtripGetClientState(&ntripClient)) {
                            case XPLR_NTRIP_STATE_READY:
                                // NTRIP client operates normally no action needed from APP
                                break;
                            case XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                                // NTRIP client has received correction data
                                ntripClientError = xplrWifiNtripGetCorrectionData(&ntripClient,
                                                                                  ntripBuffer,
                                                                                  XPLRNTRIP_RECEIVE_DATA_SIZE,
                                                                                  &ntripSize);
                                if (ntripClientError == XPLR_NTRIP_OK) {
                                    APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                                    if (receivedNtripDataInitial) {
                                        XPLR_CI_CONSOLE(605, "OK");
                                        receivedNtripDataInitial = false;
                                    }
                                } else {
                                    XPLR_CI_CONSOLE(605, "ERROR");
                                }
                                espRet = xplrGnssSendRtcmCorrectionData(0, ntripBuffer, ntripSize);
                                if (espRet != ESP_OK) {
                                    XPLR_CI_CONSOLE(606, "ERROR");
                                } else if (SentCorrectionDataInitial) {
                                    XPLR_CI_CONSOLE(606, "OK");
                                    SentCorrectionDataInitial = false;
                                }
                                break;
                            case XPLR_NTRIP_STATE_REQUEST_GGA:
                                // NTRIP client requires GGA to send back to server
                                memset(GgaMsg, 0x00, strlen(GgaMsg));
                                len = xplrGnssGetGgaMessage(0, ntripGgaMsgPtrPtr, ELEMENTCNT(GgaMsg));
                                xplrWifiNtripSendGGA(&ntripClient, GgaMsg, len);
                                break;
                            case XPLR_NTRIP_STATE_ERROR:
                                // NTRIP client encountered an error
                                APP_CONSOLE(E, "NTRIP Client returned error state");
                                // Check the detailer error code
                                ntripClientDetailedError = xplrWifiNtripGetDetailedError(&ntripClient);
                                // Handle specific error
                                // ...
                                APP_CONSOLE(I, "NTRIP client error, halting execution");
                                appHaltExecution();
                                break;
                            case XPLR_NTRIP_STATE_BUSY:
                                // NTRIP client busy, retry until state changes
                                break;
                            default:
                                break;
                        }
                    }
                } else {
                    // Continue trying to connect to Wifi
                }

                appPrintLocation(locPrintInterval);
#if 1 == APP_PRINT_IMU_DATA
                if (appOptions.drCfg.printImuData) {
                    appPrintDeadReckoning(imuPrintInterval);
                }
#endif
                break;

            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                appHaltExecution();
                break;
            default:
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
                }
                break;
        }

        /**
         * A window so other tasks can run
         */
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    appNtripDeInit();
    espErr = xplrGnssStopDevice(gnssDvcPrfId);
    timePrevLoc = esp_timer_get_time();
    do {
        gnssErr = xplrGnssFsm(gnssDvcPrfId);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) >= APP_INACTIVITY_TIMEOUT) ||
            gnssErr == XPLR_GNSS_ERROR ||
            espErr != ESP_OK) {
            break;
        }
    } while (gnssErr != XPLR_GNSS_STOPPED);
#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif
    APP_CONSOLE(I, "ALL DONE!!!");
    appHaltExecution();
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

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
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
}

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
        if (appLogCfg.logOptions.singleLogOpts.ntripLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.ntripLogIndex];
                appLogCfg.ntripLogIndex = xplrWifiNtripInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.ntripLogIndex = xplrWifiNtripInitLogModule(NULL);
            }

            if (appLogCfg.ntripLogIndex >= 0) {
                APP_CONSOLE(D, "NTRIP WiFi logging instance initialized");
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
        XPLR_CI_CONSOLE(609, "ERROR");
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
static void appInitBoard(void)
{
    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    }
    timerInit();
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
    /* NTRIP Settings */
    ntripHost = appOptions.ntripCfg.host;
    ntripPort = appOptions.ntripCfg.port;
    ntripMountpoint = appOptions.ntripCfg.mountpoint;
    ntripUserAgent = appOptions.ntripCfg.userAgent;
    ntripSendGga = appOptions.ntripCfg.sendGGA;
    ntripUseAuth = appOptions.ntripCfg.useAuth;
    ntripUser = appOptions.ntripCfg.username;
    ntripPass = appOptions.ntripCfg.password;
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
        } else if (strstr(instance->description, "NTRIP Wifi") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.ntripLog = 1;
                appLogCfg.ntripLogIndex = i;
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
        } else if (strstr(instance->description, "Location") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.locHelperLog = 1;
                appLogCfg.locHelperLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else {
            // Do nothing module not used in example
        }
    }
    /* GNSS and DR settings */
    gnssDvcType = (xplrLocDeviceType_t)appOptions.gnssCfg.module;
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
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        XPLR_CI_CONSOLE(603, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(603, "OK");
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
        XPLR_CI_CONSOLE(601, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(601, "OK");
    }

    appConfigGnssSettings(&dvcConfig);

    espRet = xplrGnssStartDevice(gnssDvcPrfId, &dvcConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        XPLR_CI_CONSOLE(602, "ERROR");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
    XPLR_CI_CONSOLE(602, "OK");
}

static void timerInit(void)
{
    /* initialize timer
     * no irq or alarm.
     * timer in free running mode.
     * timer remains halted after config. */
    timer_config_t timerCfg = {
        .divider = 16,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_DIS,
        .auto_reload = TIMER_AUTORELOAD_EN
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &timerCfg);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
}

/**
 * NTRIP client initialization
 */
static void appNtripInit(void)
{
    xplr_ntrip_error_t err;

    xplrWifiNtripSetConfig(&ntripClient,
                           &ntripConfig,
                           (const char *)ntripHost,
                           ntripPort,
                           (const char *)ntripMountpoint,
                           ntripSendGga);
    xplrWifiNtripSetCredentials(&ntripClient,
                                ntripUseAuth,
                                (const char *)ntripUser,
                                (const char *)ntripPass,
                                (const char *)ntripUserAgent);
    ntripSemaphore = xSemaphoreCreateMutex();
    err = xplrWifiNtripInit(&ntripClient, ntripSemaphore);

    if (err != XPLR_NTRIP_OK) {
        APP_CONSOLE(E, "NTRIP client initialization failed!");
        XPLR_CI_CONSOLE(604, "ERROR");
        appHaltExecution();
    } else {
        XPLR_CI_CONSOLE(604, "OK");
    }
}

/**
 * NTRIP client de-initialization
 */
static void appNtripDeInit(void)
{
    xplr_ntrip_error_t err;

    err = xplrWifiNtripDeInit(&ntripClient);

    if (err != XPLR_NTRIP_OK) {
        APP_CONSOLE(E, "NTRIP client de-init failed!");
        appHaltExecution();
    }
}

/**
 * Prints location data over a period (seconds).
 * Declare a period in seconds to print location.
 */
static void appPrintLocation(uint8_t periodSecs)
{
    static bool locRTKFirstTime = true;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) &&
        xplrGnssHasMessage(gnssDvcPrfId)) {
        espRet = xplrGnssGetLocationData(gnssDvcPrfId, &locData);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
            XPLR_CI_CONSOLE(607, "ERROR");
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
                XPLR_CI_CONSOLE(607, "ERROR");
            } else {
                XPLR_CI_CONSOLE(607, "OK");
            }
        }

        espRet = xplrGnssPrintGmapsLocation(gnssDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
            XPLR_CI_CONSOLE(607, "ERROR");
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
    if ((MICROTOSEC(esp_timer_get_time()) - timePrevDr >= periodSecs) &&
        xplrGnssIsDrEnabled(gnssDvcPrfId)) {
        espRet = xplrGnssGetImuAlignmentInfo(gnssDvcPrfId, &imuAlignmentInfo);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment info!");
        }

        espRet = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment data!");
        }

        espRet = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        }
        espRet = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment status!");
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            espRet = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            }

            espRet = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
            }
        }

        timePrevDr = MICROTOSEC(esp_timer_get_time());
    }
}
#endif

/**
 * Function that handles the event of an inactivity timeout
 * of the GNSS module
*/
static void appTerminate(void)
{
    xplrGnssError_t gnssErr;
    esp_err_t espErr;

    APP_CONSOLE(E, "Unrecoverable error in application. Terminating and restarting...");
    appNtripDeInit();
    espErr = xplrGnssStopDevice(gnssDvcPrfId);
    timePrevLoc = esp_timer_get_time();
    do {
        gnssErr = xplrGnssFsm(gnssDvcPrfId);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) >= APP_INACTIVITY_TIMEOUT) ||
            gnssErr == XPLR_GNSS_ERROR ||
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

/**
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    xplrMemUsagePrint(0);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
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
