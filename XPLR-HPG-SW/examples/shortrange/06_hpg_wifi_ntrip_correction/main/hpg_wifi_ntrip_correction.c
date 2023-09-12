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
 * An example utilising NTRIP WiFi Client module to fetch correction data.
 *
 * In the current example U-blox XPLR-HPG kit is initialized using boards component,
 * connects to WiFi network using wifi_starter component,
 * connects to the NTRIP caster, using xplr_ntrip component.
 *
 * XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE and XPLRWIFI_NTRIP_GGA_INTERVAL_S
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
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)\
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

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
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR 0x42

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * GNSS configuration.
 * This is an example for a GNSS ZED-F9 module.
 * The same structure can be used with an LBAND NEO-D9s module.
 * Depending on your device and board you might have to change these values.
 */
static xplrGnssDeviceCfg_t dvcConfig;

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
static const char ntripHost[] = CONFIG_XPLR_WIFI_NTRIP_HOST;
static const int ntripPort = CONFIG_XPLR_WIFI_NTRIP_PORT;
static const char ntripMountpoint[] = CONFIG_XPLR_WIFI_NTRIP_MOUNTPOINT;
static const char ntripUserAgent[] = CONFIG_XPLR_WIFI_NTRIP_USERAGENT;

#ifdef CONFIG_XPLR_WIFI_NTRIP_GGA_MSG
static const bool ntripSendGga = true;
#else
static const bool ntripSendGga = false;
#endif
#ifdef CONFIG_XPLR_WIFI_NTRIP_USE_AUTH
static const bool ntripUseAuth = true;
#else
static const bool ntripUseAuth = false;
#endif
static const char ntripUser[] = CONFIG_XPLR_WIFI_NTRIP_USERNAME;
static const char ntripPass[] = CONFIG_XPLR_WIFI_NTRIP_PASSWORD;

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
xplrWifi_ntrip_error_t ntripClientError;
xplrWifi_ntrip_detailed_error_t ntripClientDetailedError;
static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;

char ntripBuffer[XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE];
uint32_t ntripSize;
SemaphoreHandle_t ntripSemaphore;

char GgaMsg[256];
char *ntripGgaMsgPtr = GgaMsg;
char **ntripGgaMsgPtrPtr = &ntripGgaMsgPtr;
int32_t len;

/*INDENT-OFF*/
/**
 * Static log configuration struct and variables
 */
#if (1 == APP_SD_LOGGING_ENABLED)
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_LARGE];
static char appLogFilename[] = "/APPLOG.TXT";               /**< Follow the same format if changing the filename*/
static char errorLogFilename[] = "/ERRORLOG.TXT";           /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize = 100;                        /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType = XPLR_SIZE_MB;    /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif
/*INDENT-ON*/

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitLog(void);
static void appInitBoard(void);
static void appInitWiFi(void);
static void timerInit(void);
static void appInitGnssDevice(void);
static void appNtripInit(void);
static void appNtripDeInit(void);
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appDeInitLog(void);
static void appHaltExecution(void);

void app_main(void)
{
    timePrevLoc = MICROTOSEC(esp_timer_get_time());
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = MICROTOSEC(esp_timer_get_time());
#endif

    appInitLog();
    appInitBoard();
    timeOut = MICROTOSEC(esp_timer_get_time());
    appInitWiFi();
    appInitGnssDevice();

    while ((MICROTOSEC(esp_timer_get_time()) - timeOut <= APP_TIMEOUT)) {
        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                wifistarterErr = xplrWifiStarterFsm();
                if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
                    if (!ntripClient.socketIsValid) {
                        appNtripInit();
                    } else {
                        switch (xplrWifiNtripGetClientState(&ntripClient)) {
                            case XPLR_WIFI_NTRIP_STATE_READY:
                                // NTRIP client operates normally no action needed from APP
                                break;
                            case XPLR_WIFI_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                                // NTRIP client has received correction data
                                xplrWifiNtripGetCorrectionData(&ntripClient,
                                                               ntripBuffer,
                                                               XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE,
                                                               &ntripSize);
                                APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                                xplrGnssSendRtcmCorrectionData(0, ntripBuffer, ntripSize);
                                break;
                            case XPLR_WIFI_NTRIP_STATE_REQUEST_GGA:
                                // NTRIP client requires GGA to send back to server
                                memset(GgaMsg, 0x00, strlen(GgaMsg));
                                len = xplrGnssGetGgaMessage(0, ntripGgaMsgPtrPtr, ELEMENTCNT(GgaMsg));
                                xplrWifiNtripSendGGA(&ntripClient, GgaMsg, len);
                                break;
                            case XPLR_WIFI_NTRIP_STATE_ERROR:
                                // NTRIP client encountered an error
                                APP_CONSOLE(E, "NTRIP Client returned error state");
                                // Check the detailer error code
                                ntripClientDetailedError = xplrWifiNtripGetDetailedError(&ntripClient);
                                // Handle specific error
                                // ...
                                APP_CONSOLE(I, "NTRIP client error, halting execution");
                                appHaltExecution();
                                break;
                            case XPLR_WIFI_NTRIP_STATE_BUSY:
                                // NTRIP client busy, retry until state changes
                                break;
                            default:
                                break;
                        }
                    }
                } else {
                    // Continue trying to connect to Wifi
                }

                appPrintLocation(APP_LOCATION_PRINT_PERIOD);
#if 1 == APP_PRINT_IMU_DATA
                appPrintDeadReckoning(APP_DEAD_RECKONING_PRINT_PERIOD);
#endif
                break;

            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                appHaltExecution();
                break;

            default:
                break;
        }

        /**
         * A window so other tasks can run
         */
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    appNtripDeInit();
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
    /**
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case a gnss
    * module connected via UART
    */
    gnssCfg->hw.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
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

    gnssCfg->dr.enable = CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
}

/**
 * Initialize logging to the SD card
 */
static void appInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLog_error_t err = xplrLogInit(&errorLog,
                                      XPLR_LOG_DEVICE_ERROR,
                                      errorLogFilename,
                                      logFileMaxSize,
                                      logFileMaxSizeType);
    if (err == XPLR_LOG_OK) {
        errorLog.logEnable = true;
        err = xplrLogInit(&appLog,
                          XPLR_LOG_DEVICE_INFO,
                          appLogFilename,
                          logFileMaxSize,
                          logFileMaxSizeType);
    }
    if (err == XPLR_LOG_OK) {
        appLog.logEnable = true;
    } else {
        APP_CONSOLE(E, "Error initializing logging...");
    }
#endif
}

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

    appConfigGnssSettings(&dvcConfig);

    espRet = xplrGnssStartDevice(gnssDvcPrfId, &dvcConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
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
    xplrWifi_ntrip_error_t err;

    xplrWifiNtripSetConfig(&ntripClient, ntripHost, ntripPort, ntripMountpoint, ntripSendGga);
    xplrWifiNtripSetCredentials(&ntripClient, ntripUseAuth, ntripUser, ntripPass, ntripUserAgent);
    ntripSemaphore = xSemaphoreCreateMutex();
    err = xplrWifiNtripInit(&ntripClient, ntripSemaphore);

    if (err != XPLR_WIFI_NTRIP_OK) {
        APP_CONSOLE(E, "NTRIP client initialization failed!");
        appHaltExecution();
    }
}

/**
 * NTRIP client de-initialization
 */
static void appNtripDeInit(void)
{
    xplrWifi_ntrip_error_t err;

    err = xplrWifiNtripDeInit(&ntripClient);

    if (err != XPLR_WIFI_NTRIP_OK) {
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
    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) &&
        xplrGnssHasMessage(gnssDvcPrfId)) {
        espRet = xplrGnssGetLocationData(gnssDvcPrfId, &locData);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
        } else {
            espRet = xplrGnssPrintLocationData(&locData);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
            }
        }

        espRet = xplrGnssPrintGmapsLocation(gnssDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
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
 * Deinitialize logging service
*/
static void appDeInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
#endif
}

/**
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    appDeInitLog();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}