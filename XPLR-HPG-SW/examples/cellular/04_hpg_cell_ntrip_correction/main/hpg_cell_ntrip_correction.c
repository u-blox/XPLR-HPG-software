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
 * An example utilizing NTRIP Cellular Client module to fetch correction data.
 *
 * In the current example U-blox XPLR-HPG kit is initialized using boards component,
 * connects to the cellular network using xplr_com component,
 * connects to the NTRIP caster, using xplr_ntrip component.
 *
 */

#include <stdio.h>
#include <string.h>
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/ntripCellClient_service/xplr_cell_ntrip_client.h"
#include "driver/timer.h"

/* if paths not found run: <ctrl+shift+p> ESP-IDF: Add vscode configuration folder */

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
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

#define APP_GNSS_LOC_INTERVAL           (1 * 1)                         /* frequency of location info logging to console in seconds */
#if 1 == APP_PRINT_IMU_DATA
#define APP_GNSS_DR_INTERVAL            (5 * 1)                         /* frequency of dead reckoning info logging to console in seconds */
#endif
#define APP_NTRIP_STATE_INTERVAL_SEC    (15)                            /* frequency of NTRIP client state logging to console in seconds */
#define APP_RUN_TIME                    (120)                           /* period of app (in seconds) before exiting */
#define APP_DEVICE_OFF_MODE_BTN         (BOARD_IO_BTN1)                 /* Button for shutting down device */
#define APP_DEVICE_OFF_MODE_TRIGGER     (3U)                            /* Device off press duration in sec */
#define APP_RESTART_ON_ERROR            (1U)            /* Trigger soft reset if device in error state*/
#define APP_INACTIVITY_TIMEOUT          (30)            /* Time in seconds to trigger an inactivity timeout and cause a restart */

#define APP_GNSS_I2C_ADDR 0x42
#define APP_SD_HOT_PLUG_FUNCTIONALITY   (1U) & APP_SD_LOGGING_ENABLED   /* Option to enable/disable the hot plug functionality for the SD card */
#define APP_RESTART_ON_ERROR            (1U)                            /* Trigger soft reset if device in error state*/

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -6,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
    APP_ERROR_NTRIP_INIT,
    APP_ERROR_NTRIP_TASK,
    APP_ERROR_NETWORK_OFFLINE,
    APP_ERROR_OK = 0,
} app_error_t;

/* application states */
typedef enum {
    APP_FSM_INACTIVE = -2,
    APP_FSM_ERROR,
    APP_FSM_INIT_HW = 0,
    APP_FSM_INIT_PERIPHERALS,
    APP_FSM_CONFIG_GNSS,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_SET_GREETING_MESSAGE,
    APP_FSM_INIT_NTRIP_CLIENT,
    APP_FSM_RUN,
    APP_FSM_MQTT_DISCONNECT,
    APP_FSM_TERMINATE,
} app_fsm_t;

typedef struct app_type {
    app_error_t error;
    app_fsm_t state[2];
    uint64_t time;
    uint64_t timeOut;
} app_t;


typedef union __attribute__((packed))
{
    struct {
        uint8_t appLog          : 1;
        uint8_t nvsLog          : 1;
        uint8_t ntripLog        : 1;
        uint8_t gnssLog         : 1;
        uint8_t gnssAsyncLog    : 1;
        uint8_t locHelperLog    : 1;
        uint8_t comLog          : 1;
    } singleLogOpts;
    uint8_t allLogOpts;
}
appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          ntripLogIndex;
    int8_t          gnssLogIndex;
    int8_t          gnssAsyncLogIndex;
    int8_t          locHelperLogIndex;
    int8_t          comLogIndex;
} appLog_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

app_t app;

/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;
/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;
/* gnss device id */
const uint8_t gnssDvcPrfId = 0;
/* location modules */
xplrGnssStates_t gnssState;
xplrLocDvcInfo_t gnssDvcInfo;
xplrGnssLocation_t gnssLocation;
#if 1 == APP_PRINT_IMU_DATA
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif
/**
 * Keeps time at "this" point of variable
 * assignment in code
 * Used for periodic printing
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

char ntripBuffer[XPLRCELL_NTRIP_RECEIVE_DATA_SIZE];
uint32_t ntripSize;
SemaphoreHandle_t ntripSemaphore;

/**
 * You can use KConfig to set up these values.
 */
static const char ntripHost[] = CONFIG_XPLR_NTRIP_HOST;
static const int ntripPort = CONFIG_XPLR_NTRIP_PORT;
static const char ntripMountpoint[] = CONFIG_XPLR_NTRIP_MOUNTPOINT;
static const char ntripUserAgent[] = CONFIG_XPLR_NTRIP_USERAGENT;
#ifdef CONFIG_XPLR_NTRIP_GGA_MSG
static const bool ntripSendGga = true;
#else
static const bool ntripSendGga = false;
#endif
#ifdef CONFIG_XPLR_NTRIP_USE_AUTH
static const bool ntripUseAuth = true;
#else
static const bool ntripUseAuth = false;
#endif
static const char ntripUser[] = CONFIG_XPLR_NTRIP_USERNAME;
static const char ntripPass[] = CONFIG_XPLR_NTRIP_PASSWORD;

int64_t gnssLastAction = 0;

xplrCell_ntrip_client_t ntripClient;
xplr_ntrip_config_t ntripConfig;
xplr_ntrip_error_t ntripClientError;
xplr_ntrip_detailed_error_t ntripClientDetailedError;

/*INDENT-OFF*/
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
    .comLogIndex = -1
};
/*INDENT-ON*/

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif

const char cellGreetingMessage[] = "LARA JUST WOKE UP";
int32_t cellReboots = 0;    /**< Count of total reboots of the cellular module */
static bool failedRecover = false;

char GgaMsg[256];
char *pGgaMsg = GgaMsg;
char **ppGgaMsg = &pGgaMsg;
int32_t len;
bool cellHasRebooted = false;
static bool deviceOffRequested = false;

xplr_ntrip_error_t xplrNtripErr;
esp_err_t esp_err;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* initialize hw */
static esp_err_t appInitBoard(void);
/* configure free running timer for calculating time intervals in app */
static void timerInit(void);
/* configure gnss related settings*/
static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
/* configure cellular related settings */
static void configCellSettings(xplrCom_cell_config_t *cfg);
/* initialize cellular module */
static app_error_t cellInit(void);
/* perform cell module reset */
static app_error_t cellRestart(void);
/* configure greeting message and callback for cellular module */
static app_error_t cellSetGreeting(void);
/* runs the fsm for the GNSS module */
static app_error_t gnssRunFsm(void);
/* register cellular module to the network */
static app_error_t cellNetworkRegister(void);
/* check if cellular module is connected to the network */
static app_error_t cellNetworkConnected(void);
/* initialize the GNSS module */
static app_error_t gnssInit(void);
/* print location info to console */
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
/* print dead reckoning info to console */
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
/* initialize app */
static void appInit(void);
/* initialize ntrip client */
static app_error_t ntripInit(void);
#if (APP_SD_LOGGING_ENABLED == 1)
/* initialize logging */
static esp_err_t appInitLogging(void);
/* de-initialize logging */
static void appDeInitLogging(void);
#endif
/* terminate app */
static app_error_t appTerminate(void);
/* powerdown device modules */
static void appDeviceOffTask(void *arg);
/* A dummy function to pause on error */
static void appHaltExecution(void);
/* card detect task */
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appCardDetectTask(void *arg);
#endif

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* callback triggered when Cell module has rebooted (intentionally or unintentionally) */
static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{
#if (APP_SD_LOGGING_ENABLED == 1)
    esp_err_t espErr;
#endif
    static bool receivedNtripDataInitial = true;
    static bool SentCorrectionDataInitial = true;
    timePrevLoc = 0;
    bool isRstControlled = false;
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = 0;
#endif

#if (APP_SD_LOGGING_ENABLED == 1)
    espErr = appInitLogging();
    if (espErr != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif

    APP_CONSOLE(I, "XPLR-HPG-SW Demo: NTRIP Cellular Client\n");

    while (1) {
        switch (app.state[0]) {
            case APP_FSM_INIT_HW:
                app.state[1] = app.state[0];
                appInitBoard();
                appInit();
                app.timeOut = MICROTOSEC(esp_timer_get_time());
                app.state[0] = APP_FSM_INIT_PERIPHERALS;
                break;
            case APP_FSM_INIT_PERIPHERALS:
                app.state[1] = app.state[0];
                app.error = gnssInit();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.error = cellInit();
                    app.state[0] = APP_FSM_CONFIG_GNSS;
                }
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_CONFIG_GNSS;
                }
                break;
            case APP_FSM_CONFIG_GNSS:
                app.state[1] = app.state[0];
                app.error = gnssRunFsm();
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        gnssLastAction = esp_timer_get_time();
                        app.state[0] = APP_FSM_CHECK_NETWORK;
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                            app.state[1] = app.state[0];
                            app.state[0] = APP_FSM_ERROR;
                        }
                        /* module still configuring. do nothing */
                    }
                }
                break;
            case APP_FSM_CHECK_NETWORK:
                app.state[1] = app.state[0];
                app.error = cellNetworkRegister();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_SET_GREETING_MESSAGE;
                    XPLR_CI_CONSOLE(2403, "OK");
                } else if (app.error == APP_ERROR_NETWORK_OFFLINE) {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2403, "ERROR");
                } else {
                    /* module still trying to connect. do nothing */
                }
                break;
            case APP_FSM_SET_GREETING_MESSAGE:
                app.state[1] = app.state[0];
                app.error = cellSetGreeting();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INIT_NTRIP_CLIENT;
                }
                break;
            case APP_FSM_INIT_NTRIP_CLIENT:
                app.state[1] = app.state[0];
                app.error = ntripInit();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_RUN;
                    XPLR_CI_CONSOLE(2404, "OK");
                } else {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2404, "ERROR");
                }
                break;
            case APP_FSM_RUN:
                app.state[1] = app.state[0];
                /* run GNSS FSM */
                app.error = gnssRunFsm();
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    appPrintLocation(APP_GNSS_LOC_INTERVAL);
#if 1 == APP_PRINT_IMU_DATA
                    appPrintDeadReckoning(APP_GNSS_DR_INTERVAL);
#endif
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        gnssLastAction = esp_timer_get_time();
                        switch (xplrCellNtripGetClientState(&ntripClient)) {
                            case XPLR_NTRIP_STATE_READY:
                                // NTRIP client operates normally no action needed from APP
                                break;
                            case XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                                // NTRIP client has received correction data
                                xplrNtripErr = xplrCellNtripGetCorrectionData(&ntripClient,
                                                                              ntripBuffer,
                                                                              XPLRCELL_NTRIP_RECEIVE_DATA_SIZE,
                                                                              &ntripSize);
                                if (xplrNtripErr != XPLR_NTRIP_ERROR) {
                                    APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                                    failedRecover = false;
                                    esp_err = xplrGnssSendRtcmCorrectionData(gnssDvcPrfId, ntripBuffer, ntripSize);
                                    if (receivedNtripDataInitial) {
                                        XPLR_CI_CONSOLE(2405, "OK");
                                        receivedNtripDataInitial = false;
                                    }
                                    if (esp_err != ESP_OK) {
                                        XPLR_CI_CONSOLE(2406, "ERROR");
                                    } else if (SentCorrectionDataInitial) {
                                        XPLR_CI_CONSOLE(2406, "OK");
                                        SentCorrectionDataInitial = false;
                                    }
                                } else {
                                    XPLR_CI_CONSOLE(2405, "ERROR");
                                }
                                break;
                            case XPLR_NTRIP_STATE_REQUEST_GGA:
                                // NTRIP client requires GGA to send back to server
                                memset(GgaMsg, 0x00, strlen(GgaMsg));
                                len = xplrGnssGetGgaMessage(gnssDvcPrfId, ppGgaMsg, ELEMENTCNT(GgaMsg));
                                xplrCellNtripSendGGA(&ntripClient, GgaMsg, len);
                                break;
                            case XPLR_NTRIP_STATE_ERROR:
                                // NTRIP client encountered an error
                                APP_CONSOLE(E, "NTRIP Client encountered error");
                                // Check the detailer error code
                                ntripClientDetailedError = xplrCellNtripGetDetailedError(&ntripClient);
                                // Handle specific error
                                // ...
                                app.state[0] = APP_FSM_ERROR;
                                break;
                            case XPLR_NTRIP_STATE_BUSY:
                                // NTRIP client busy, retry until state changes
                                break;
                            default:
                                break;
                        }
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                            app.state[1] = app.state[0];
                            app.state[0] = APP_FSM_ERROR;
                        }
                    }

                    vTaskDelay(pdMS_TO_TICKS(25));
                    /* Check if its time to terminate the app */
                    if (MICROTOSEC(esp_timer_get_time()) - app.timeOut >= APP_RUN_TIME) {
                        app.state[0] = APP_FSM_TERMINATE;
                    }
                }
                break;
            case APP_FSM_TERMINATE:
                app.state[1] = app.state[0];
                app.error = appTerminate();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INACTIVE;
                }
                break;
            case APP_FSM_MQTT_DISCONNECT:
                app.state[1] = app.state[0];
                xplrCellNtripDeInit(&ntripClient);
                /* Reboot cell */
                app.error = cellRestart();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_TERMINATE;
                } else {
                    app.state[0] = APP_FSM_CHECK_NETWORK;
                }
                /* Check if there has been a failed recover */
                if (failedRecover) {
                    /* Not able to recover -> Restart */
                    esp_restart();
                } else {
                    /* Try to recover from disconnected state */
                    failedRecover = true;
                }
                break;
            case APP_FSM_INACTIVE:
                APP_CONSOLE(I, "ALL DONE!!!");
                appHaltExecution();
                break;
            case APP_FSM_ERROR:
#if (APP_RESTART_ON_ERROR == 1)
                APP_CONSOLE(E, "Unrecoverable FSM Error. Restarting device.");
                vTaskDelay(10);
                esp_restart();
#endif
                APP_CONSOLE(E, "Halting execution");
                appHaltExecution();
                break;
            default:
                break;
        }
        if (cellHasRebooted) {
            app.state[1] = app.state[0];
            isRstControlled = xplrComIsRstControlled(cellConfig.profileIndex);
            if (isRstControlled) {
                APP_CONSOLE(I, "Controlled LARA restart triggered");
                isRstControlled = false;
            } else {
                APP_CONSOLE(W, "Uncontrolled LARA restart triggered");
                app.state[0] = APP_FSM_CHECK_NETWORK;
                /* De-init mqtt client */
                xplrComPowerResetHard(cellConfig.profileIndex);
                app.state[0] = APP_FSM_CHECK_NETWORK;
            }
            cellHasRebooted = false;
            APP_CONSOLE(W, "Cell Module has rebooted! Number of total reboots: <%d>", cellReboots);
        }
        if (deviceOffRequested) {
            app.state[1] = app.state[0];
            app.state[0] = APP_FSM_TERMINATE;
            deviceOffRequested = false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
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
    gnssCfg->hw.dvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
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

static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t ret;

    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_DEVICE_OFF_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        ret = gpio_config(&io_conf);
    }

    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        if (xTaskCreate(appDeviceOffTask, "deviceOffTask", 2 * 2048, NULL, 10, NULL) == pdPASS) {
            APP_CONSOLE(D, "Boot0 pin configured as button OK");
            APP_CONSOLE(D, "Board Initialized");
        } else {
            APP_CONSOLE(D, "Failed to start deviceOffTask task");
            APP_CONSOLE(E, "Board initialization failed!");
            ret = ESP_FAIL;
        }
    }

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
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
#endif

    return ret;
}

static void configCellSettings(xplrCom_cell_config_t *cfg)
{
    /* Config hardware pins connected to cellular module */
    cfg->hwSettings = &cellHwConfig;
    cfg->comSettings = &cellComConfig;
    cfg->netSettings = &netConfig;

    /*
     * Pin numbers are those of the MCU: if you
     * are using an MCU inside a u-blox module the IO pin numbering
     * for the module is likely different that from the MCU: check
     * the data sheet for the module to determine the mapping
     * DEVICE i.e. module/chip configuration: in this case a cellular
     * module connected via UART
     */

    cfg->hwSettings->moduleType = U_CELL_MODULE_TYPE_LARA_R6;
    cfg->hwSettings->pSimPinCode = NULL;
    cfg->hwSettings->pinEnablePower = -1;
    cfg->hwSettings->pinPwrOn = BOARD_IO_LTE_PWR_ON;

    cfg->hwSettings->pinVInt = BOARD_IO_LTE_ON_nSENSE;
    cfg->hwSettings->pinDtrPowerSaving = -1;

    cfg->comSettings->uart = 1;
    cfg->comSettings->baudRate = U_CELL_UART_BAUD_RATE;
    cfg->comSettings->pinTxd = BOARD_IO_UART_LTE_TX;
    cfg->comSettings->pinRxd = BOARD_IO_UART_LTE_RX;
    cfg->comSettings->pinCts = BOARD_IO_UART_LTE_CTS;
    cfg->comSettings->pinRts = BOARD_IO_UART_LTE_RTS;

    cfg->netSettings->type = U_NETWORK_TYPE_CELL;
    cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN;
    cfg->netSettings->timeoutSeconds = 240; /* Connection timeout in seconds */
    cfg->mno = 100;

    cfg->ratList[0] = U_CELL_NET_RAT_LTE;
    cfg->ratList[1] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[2] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    cfg->bandList[0] = 0;
    cfg->bandList[1] = 0;
    cfg->bandList[2] = 0;
    cfg->bandList[3] = 0;
    cfg->bandList[4] = 0;
    cfg->bandList[5] = 0;
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

static app_error_t cellInit(void)
{
    /* initialize ubxlib and cellular module */

    app_error_t ret;
    xplrCom_error_t err;

    /* initialize ubxlib and cellular module */
    err = xplrUbxlibInit(); /* Initialise the ubxlib APIs we will need */
    if (err == XPLR_COM_OK) {
        configCellSettings(&cellConfig);    /* Setup configuration parameters for hpg com */
        err = xplrComCellInit(&cellConfig); /* Initialize hpg com */
        XPLR_CI_CONSOLE(2401, "OK");
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_CELL_INIT;
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", err);
        XPLR_CI_CONSOLE(2401, "ERROR");
    }

    return ret;
}

static app_error_t cellRestart(void)
{
    app_error_t ret;
    xplrCom_error_t comErr;

    comErr = xplrComPowerResetHard(cellConfig.profileIndex);
    if (comErr == XPLR_COM_OK) {
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_NETWORK_OFFLINE;
    }
    return ret;
}

static app_error_t cellSetGreeting(void)
{
    app_error_t ret;
    xplrCom_error_t err;

    err = xplrComSetGreetingMessage(cellConfig.profileIndex,
                                    cellGreetingMessage,
                                    cellGreetingCallback,
                                    &cellReboots);
    if (err != XPLR_COM_OK) {
        APP_CONSOLE(E, "Could not set up Greeting message");
        ret = APP_ERROR_CELL_INIT;
    } else {
        APP_CONSOLE(I, "Greeting message Set to <%s>", cellGreetingMessage);
        ret = APP_ERROR_OK;
    }

    return ret;
}

static app_error_t gnssRunFsm(void)
{
    app_error_t ret;
    xplrGnssStates_t state;

    xplrGnssFsm(gnssDvcPrfId);
    state = xplrGnssGetCurrentState(gnssDvcPrfId);

    switch (state) {
        case XPLR_GNSS_STATE_DEVICE_READY:
            ret = APP_ERROR_OK;
            break;

        case XPLR_GNSS_STATE_ERROR:
            ret = APP_ERROR_GNSS_INIT;
            break;

        default:
            ret = APP_ERROR_OK;
    }

    return ret;
}

static app_error_t cellNetworkRegister(void)
{
    app_error_t ret;
    xplrCom_cell_connect_t comState;

    xplrComCellFsmConnect(cellConfig.profileIndex);
    comState = xplrComCellFsmConnectGetState(cellConfig.profileIndex);

    switch (comState) {
        case XPLR_COM_CELL_CONNECTED:
            APP_CONSOLE(I, "Cell module is Online.");
            /* quick blink 5 times*/
            for (int i = 0; i < 5; i++) {
                xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            xplrBoardSetLed(XPLR_BOARD_LED_ON);
            ret = APP_ERROR_OK;
            break;
        case XPLR_COM_CELL_CONNECT_TIMEOUT:
        case XPLR_COM_CELL_CONNECT_ERROR:
            APP_CONSOLE(W, "Cell module is Offline.");
#if (APP_SHUTDOWN_CELL_AFTER_REGISTRATION == 1)
            APP_CONSOLE(E, "Cellular registration not completed. Shutting down cell dvc.");
            xplrComCellPowerDown(cellConfig.profileIndex);
#endif
            /* slow blink 5 times*/
            for (int i = 0; i < 5; i++) {
                xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            xplrBoardSetLed(XPLR_BOARD_LED_ON);
            ret = APP_ERROR_NETWORK_OFFLINE;
            break;

        default:
            ret = APP_ERROR_UNKNOWN;
            break;
    }

    return ret;
}

static app_error_t cellNetworkConnected(void)
{
    int8_t id = cellConfig.profileIndex;
    xplrCom_cell_connect_t comState;
    app_error_t ret;

    xplrComCellFsmConnect(id);
    comState = xplrComCellFsmConnectGetState(id);
    if (comState == XPLR_COM_CELL_CONNECTED) {
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_NETWORK_OFFLINE;
    }

    return ret;
}

static app_error_t gnssInit(void)
{
    esp_err_t err;
    app_error_t ret;

    err = xplrGnssUbxlibInit();
    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "UbxLib init (GNSS) failed!");
    } else {
        APP_CONSOLE(W, "Waiting for GNSS device to come online!");
        configGnssSettings(&dvcGnssConfig);
        err = xplrGnssStartDevice(gnssDvcPrfId, &dvcGnssConfig);
    }

    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "Failed to start GNSS");
        XPLR_CI_CONSOLE(2402, "ERROR");
    } else {
        ret = APP_ERROR_OK;
        XPLR_CI_CONSOLE(2402, "OK");
    }

    return ret;
}

static app_error_t ntripInit(void)
{
    xplr_ntrip_error_t err;
    app_error_t ret = APP_ERROR_OK;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        xplrCellNtripSetConfig(&ntripClient,
                               &ntripConfig,
                               ntripHost,
                               ntripPort,
                               ntripMountpoint,
                               0,
                               ntripSendGga);
        xplrCellNtripSetCredentials(&ntripClient, ntripUseAuth, ntripUser, ntripPass, ntripUserAgent);

        ntripSemaphore = xSemaphoreCreateMutex();
        err = xplrCellNtripInit(&ntripClient, ntripSemaphore);

        if (err != XPLR_NTRIP_OK) {
            APP_CONSOLE(E, "NTRIP client initialization failed!");
            ret = APP_ERROR_NTRIP_INIT;
        }
    } else if (ret == APP_ERROR_NETWORK_OFFLINE) {
        APP_CONSOLE(E, "Cellular network offline");
    }

    return ret;
}

/**
 * Prints locations according to period
 */
static void appPrintLocation(uint8_t periodSecs)
{
    esp_err_t ret;
    static bool locRTKFirstTime = true;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) && xplrGnssHasMessage(0)) {
        ret = xplrGnssGetLocationData(0, &gnssLocation);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
            XPLR_CI_CONSOLE(2407, "ERROR");
        } else {
            if (locRTKFirstTime) {
                if ((gnssLocation.locFixType == XPLR_GNSS_LOCFIX_FLOAT_RTK) ||
                    (gnssLocation.locFixType == XPLR_GNSS_LOCFIX_FIXED_RTK)) {
                    locRTKFirstTime = false;
                    XPLR_CI_CONSOLE(10, "OK");
                }
            }
            ret = xplrGnssPrintLocationData(&gnssLocation);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
                XPLR_CI_CONSOLE(2407, "ERROR");
            } else {
                XPLR_CI_CONSOLE(2407, "OK");
            }
        }

        ret = xplrGnssPrintGmapsLocation(0);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
            XPLR_CI_CONSOLE(2407, "ERROR");
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

static void appInit(void)
{
    app.state[0] = APP_FSM_INIT_HW;
    timerInit();
    app.state[0] = APP_FSM_INIT_PERIPHERALS;
}

static app_error_t appTerminate(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrGnssError_t gnssErr;
    xplr_ntrip_error_t ntripRet;
    uint64_t startTime;

    ntripRet = xplrCellNtripDeInit(&ntripClient);
    if (ntripRet != XPLR_NTRIP_ERROR) {
        espErr = xplrGnssStopDevice(gnssDvcPrfId);
        startTime = esp_timer_get_time();
        do {
            gnssErr = xplrGnssFsm(gnssDvcPrfId);
            if (MICROTOSEC(esp_timer_get_time() - startTime >= APP_INACTIVITY_TIMEOUT) ||
                gnssErr == XPLR_GNSS_ERROR) {
                break;
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } while (gnssErr != XPLR_GNSS_STOPPED);
        if (espErr != ESP_OK || gnssErr != XPLR_GNSS_STOPPED) {
            APP_CONSOLE(E, "App could not stop gnss device.");
            ret = APP_ERROR_GNSS_INIT;
        } else {
            ret = APP_ERROR_OK;
        }
    } else {
        APP_CONSOLE(E, "App could not de-init the NTRIP client.");
        ret = APP_ERROR_NTRIP_INIT;
    }

    APP_CONSOLE(W, "App disconnected the NTRIP client.");
    xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif

    return ret;
}


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
        if (appLogCfg.logOptions.singleLogOpts.ntripLog == 1) {
            appLogCfg.ntripLogIndex = xplrCellNtripInitLogModule(NULL);
            if (appLogCfg.ntripLogIndex >= 0) {
                APP_CONSOLE(D, "Cell NTRIP Client logging instance initialized");
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
        if (appLogCfg.logOptions.singleLogOpts.locHelperLog == 1) {
            appLogCfg.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            if (appLogCfg.locHelperLogIndex >= 0) {
                APP_CONSOLE(D, "Location Helper Service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.comLog == 1) {
            appLogCfg.comLogIndex = xplrComCellInitLogModule(NULL);
            if (appLogCfg.comLogIndex >= 0) {
                APP_CONSOLE(D, "Com Cellular service logging instance initialized");
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

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdStopCardDetectTask();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error stopping the the SD card detect task");
        }
    }
#endif

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

static void appDeviceOffTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        //Check button button state
        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;
        } else {
            //reset hold duration on release
            btnPressDuration = 0;
        }

        /*
         *  Check button hold duration.
         * Power down device if:
         *  button hold duration >= APP_DEVICE_OFF_MODE_TRIGGER
         *  and
         *  not already powered down by the app
        */
        if (btnPressDuration >= APP_DEVICE_OFF_MODE_TRIGGER) {
            if (app.state[0] != APP_FSM_INACTIVE) {
                APP_CONSOLE(W, "Device OFF triggered");
                deviceOffRequested = true;
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
            } else {
                APP_CONSOLE(D, "Device is powered down, nothing to do...");
            }
        } else {
            //nothing to do...
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
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

static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam)
{
    int32_t *param = (int32_t *) callbackParam;

    (void) handler;
    (*param)++;
    cellHasRebooted = true;
}