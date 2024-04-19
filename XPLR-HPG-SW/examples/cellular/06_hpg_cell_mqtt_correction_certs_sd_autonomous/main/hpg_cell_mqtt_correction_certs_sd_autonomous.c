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

/* An example for MQTT connection to Thingstream (U-blox broker), via the cellular module LARA-R6
 *
 * In the current example u-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is initially setup by Kconfig (to select the board),
 * is setup by a configuration file in the SD card (for application variables such as APN, logging interval and credentials),
 * registers to a network provider using the xplr_com component,
 * uses certificates (again fetched by the SD card) to achieve a connection to the Thingstream MQTT broker
 * and subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component.
 * All messages and prints (that use the hpglib macros for printing/logging) can be logged in the SD card
 * (by enabling the corresponding macro). The filename of the GNSS Message logging is updated with the timestamp,
 * after a user-selected predefined interval.
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_system.h"
#include "./../../../components/ubxlib/ubxlib.h"
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
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/mqttClient_service/xplr_mqtt_client.h"
#include "./../../../components/hpglib/src/thingstream_service/xplr_thingstream.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "driver/timer.h"

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define APP_RESTART_ON_ERROR            (1U)                            /* Trigger soft reset if device in error state*/
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(app.log.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define APP_GNSS_I2C_ADDR               0x42                            /* I2C address of the GNSS receiver module */
#define APP_LBAND_I2C_ADDR              0x43                            /* I2C address of the LBAND receiver module */

#define APP_MQTT_BUFFER_SIZE            (10 * 1024)                     /* size of each MQTT buffer */

#define APP_DEVICE_OFF_MODE_BTN         (BOARD_IO_BTN1)                 /* Button for shutting down device */
#define APP_DEVICE_OFF_MODE_TRIGGER     (3U)                            /* Device off press duration in sec */

#define APP_JSON_PAYLOAD_BUF_SIZE       ((6U) * (1024U))                /*  Size of buffer to fetch the json configuration and credential data */
#define APP_INACTIVITY_TIMEOUT          30                              /*  Time in seconds to trigger an inactivity timeout and cause a restart */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -13,
    APP_ERROR_BUSY,
    APP_ERROR_SD,
    APP_ERROR_PARSE,
    APP_ERROR_CREDENTIALS_FETCH,
    APP_ERROR_LOGGING_INIT,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
    APP_ERROR_LBAND_INIT,
    APP_ERROR_MQTT_CLIENT,
    APP_ERROR_NETWORK_OFFLINE,
    APP_ERROR_THINGSTREAM,
    APP_ERROR_INVALID_PLAN,
    APP_ERROR_OK = 0,
} app_error_t;

/* application states */
typedef enum {
    APP_FSM_INACTIVE = -2,
    APP_FSM_ERROR,
    APP_FSM_INIT_HW = 0,
    APP_FSM_FETCH_CONFIG,
    APP_FSM_SETUP_CONFIG,
    APP_FSM_INIT_LOGGING,
    APP_FSM_INIT_PERIPHERALS,
    APP_FSM_CONFIG_GNSS,
    APP_FSM_CONFIG_LBAND,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_SET_GREETING_MESSAGE,
    APP_FSM_THINGSTREAM_GET_CREDS,
    APP_FSM_THINGSTREAM_INIT,
    APP_FSM_INIT_MQTT_CLIENT,
    APP_FSM_RUN,
    APP_FSM_MQTT_DISCONNECT,
    APP_FSM_TERMINATE,
} app_fsm_t;

typedef struct appLog_type {
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          mqttLogIndex;
    int8_t          gnssLogIndex;
    int8_t          gnssAsyncLogIndex;
    int8_t          lbandLogIndex;
    int8_t          locHelperLogIndex;
    int8_t          comLogIndex;
    int8_t          thingstreamLogIndex;
} appLog_t;

typedef xplr_cfg_t appCfg_t;

/* mqtt pont perfect topic types */
typedef union __attribute__((packed))
{
    struct {
        uint8_t keyDistribution: 1;
        uint8_t assistNow: 1;
        uint8_t correctionData: 1;
        uint8_t gad: 1;
        uint8_t hpac: 1;
        uint8_t ocb: 1;
        uint8_t clock: 1;
        uint8_t frequency: 1;
    };
    uint8_t msgType;
}
app_pp_msg_type_t;

/* mqtt message type */
typedef struct app_pp_msg_type {
    bool msgAvailable;
    app_pp_msg_type_t type;
} app_pp_msg_t;

/* application statistics */
typedef struct app_statistics_type {
    uint32_t msgSent;
    uint32_t msgReceived;
    uint32_t bytesSent;
    uint32_t bytesReceived;
    uint32_t time;
    uint64_t prevTimestamp;
    uint64_t gnssLastAction;
} app_statistics_t;

typedef struct app_type {
    app_error_t         error;
    app_fsm_t           state[2];
    app_statistics_t    stats;
    app_pp_msg_t        ppMsg;
    appCfg_t            options;
    appLog_t            log;
} app_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

app_t app;

/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static xplrLbandDeviceCfg_t dvcLbandConfig;
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;
/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;
/* location modules */
xplrGnssLocation_t gnssLocation;
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
xplrGnssStates_t gnssState;
static uint32_t frequency;
const uint8_t gnssDvcPrfId = 0;
const uint8_t lbandDvcPrfId = 0;
/* thingstream platform vars */
xplr_thingstream_t thingstreamSettings;
xplr_thingstream_pp_region_t region;
const char brokerName[] = "Thingstream";
const char rootName[] = "rootPp.crt"; /* name of root ca as stored in cellular module */
const char certName[] = "mqttPp.crt"; /* name of mqtt cert as stored in cellular module */
const char keyName[] = "mqttPp.key";  /* name of mqtt key as stored in cellular module */
/* md5 hash of certificates used, leave empty for overwriting the certificate*/
const char rootHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; //" ";
const char certHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; //" ";
const char keyHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";  //" ";
static xplrCell_mqtt_client_t mqttClient;

xplrCell_mqtt_topic_t topics[3];
char rxBuff[3][APP_MQTT_BUFFER_SIZE];
bool mqttSessionDisconnected = false;
bool mqttMsgAvailable = false;
bool cellHasRebooted = false;
static bool deviceOffRequested = false;
static bool enableLband = false;

TaskHandle_t cardDetectTaskHandler;

static int64_t timestampUtc;
static char configData[APP_JSON_PAYLOAD_BUF_SIZE];
/* The name of the configuration file */
static char configFilename[] = "xplr_config.json";
/* The greeting message of cell module when powering on (or restarting) */
const char cellGreetingMessage[] = "LARA JUST WOKE UP";
/* Count of total reboots of the cellular module */
int32_t cellReboots = 0;
/* Flag indicating a failed recovery attempt */
static bool failedRecover = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* fetch app and module configuration from SD card */
static app_error_t fetchConfigFromSd(void);
/* configure gnss related settings*/
static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
/* configure lband related settings */
static void configLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
/* configure cellular related settings */
static void configCellSettings(xplrCom_cell_config_t *cfg);
/* configure MQTT related settings */
static void configCellMqttSettings(xplrCell_mqtt_client_t *client);
/* configure free running timer for calculating time intervals in app */
static void timerInit(void);
/* initialize SD card driver */
app_error_t sdInit(void);
/* initialize cellular module */
static app_error_t cellInit(void);
/* cell re-init cell connection */
static app_error_t cellRestart(void);
/* register cellular module to the network */
static app_error_t cellNetworkRegister(void);
/* check if cellular module is connected to the network */
static app_error_t cellNetworkConnected(void);
/* configure greeting message and callback for cellular module */
static app_error_t cellSetGreeting(void);
/* initialize MQTT client */
static app_error_t cellMqttClientInit(void);
/* check MQTT client for new messages */
static app_error_t cellMqttClientMsgUpdate(void);
/* print network data statistics to console */
static void cellMqttClientStatisticsPrint(void);
/* initialize the thingstream component */
static app_error_t thingstreamInit(xplr_thingstream_t *instance);
/* initialize the GNSS module */
static app_error_t gnssInit(void);
/* runs the fsm for the GNSS module */
static app_error_t gnssRunFsm(void);
/* forward correction data to GNSS module */
static void gnssFwdPpData(void);
/* print location info to console */
static void gnssLocationPrint(void);
/* print dead reckoning info to console */
static void gnssDeadReckoningPrint(void);
/* initialize the LBand module */
static app_error_t lbandInit(void);
/* initialize hw */
static esp_err_t appInitBoard(void);
/* initialize app */
static void appInit(void);
/* setup app and module options based on the SD config file */
static app_error_t appSetupConfiguration(void);
/* terminate app */
static app_error_t appTerminate(void);
/* initialize logging*/
static app_error_t appInitLogging(void);
/* De-initialize logging */
static void appDeInitLogging(void);
/* Update logging filename based on timestamp (Format: YYYY-MM-DD-HH-MM-SS-filename) */
static void appUpdateFilename(uint64_t timestampUtc,
                              uint64_t intervalUtc,
                              char *filename,
                              int8_t logIndex);
/* Halt execution */
static void appHaltExecution(void);
/* power down device modules */
static void appDeviceOffTask(void *arg);
/* card detect task */
static void appCardDetectTask(void *arg);

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
/* callback triggered when new MQTT message is available */
static void mqttMsgReceivedCallback(int32_t numUnread, void *received);
/* callback triggered when MQTT client is disconnected */
static void mqttDisconnectCallback(int32_t status, void *cbParam);
/* callback triggered when Cell module has rebooted (intentionally or unintentionally) */
static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */

void app_main(void)
{
    double secCnt = 0;              /* timer counter */
    double appTime = 0;             /* for printing mqtt statistics at app statistics print interval  */
    double gnssLocTime = 0;         /* for printing geolocation at location print interval */
    double gnssDrTime = 0;          /* for printing dead reckoning at IMU data print interval */
    app.stats.prevTimestamp = 0;    /*  for updating the gnss async data filename
                                     *  at filename update interval
                                     */
    bool isRstControlled = false;   /* flag showing if a cell module restart is controlled
                                       (came from the reconnect mechanism), or uncontrolled
                                       (the module restarted unexpectedly) */
    bool lbandConfigured = false;   /* flag showing if the LBand module has been configured */

    APP_CONSOLE(I, "XPLR-HPG-SW Demo: MQTT Client\n");

    while (1) {
        switch (app.state[0]) {
            case APP_FSM_INIT_HW:
                app.state[1] = app.state[0];
                appInitBoard();
                appInit();
                app.state[0] = APP_FSM_FETCH_CONFIG;
                break;
            case APP_FSM_FETCH_CONFIG:
                app.state[1] = app.state[0];
                app.error = fetchConfigFromSd();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_SETUP_CONFIG;
                }
                break;
            case APP_FSM_SETUP_CONFIG:
                app.state[1] = app.state[0];
                app.error = appSetupConfiguration();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INIT_LOGGING;
                }
                break;
            case APP_FSM_INIT_LOGGING:
                app.state[1] = app.state[0];
                app.error = appInitLogging();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INIT_PERIPHERALS;
                }
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
                        app.stats.gnssLastAction = esp_timer_get_time();
                        app.state[0] = APP_FSM_CHECK_NETWORK;
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - app.stats.gnssLastAction) >= APP_INACTIVITY_TIMEOUT ||
                            app.error == APP_ERROR_GNSS_INIT) {
                            app.state[1] = app.state[0];
                            app.state[0] = APP_FSM_ERROR;
                        }

                    }
                }
                break;
            case APP_FSM_CHECK_NETWORK:
                app.state[1] = app.state[0];
                app.error = cellNetworkRegister();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_SET_GREETING_MESSAGE;
                } else if (app.error == APP_ERROR_NETWORK_OFFLINE) {
                    app.state[0] = APP_FSM_ERROR;
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
                    app.state[0] = APP_FSM_THINGSTREAM_INIT;
                }
                break;
            case APP_FSM_THINGSTREAM_INIT:
                app.state[1] = app.state[0];
                app.error = thingstreamInit(&thingstreamSettings);
                if (app.error == APP_ERROR_OK) {
                    if (enableLband && (!lbandConfigured)) {
                        app.state[0] = APP_FSM_CONFIG_LBAND;
                    } else {
                        app.state[0] = APP_FSM_INIT_MQTT_CLIENT;
                    }
                } else if (app.error == APP_ERROR_THINGSTREAM) {
                    app.state[0] = APP_FSM_ERROR;
                } else if (app.error == APP_ERROR_INVALID_PLAN) {
                    app.state[0] = APP_FSM_TERMINATE;
                } else {
                    /* module still trying to connect. do nothing */
                }
                break;
            case APP_FSM_CONFIG_LBAND:
                app.state[1] = app.state[0];
                app.error = lbandInit();
                if (app.error == APP_ERROR_OK) {
                    lbandConfigured = true;
                    app.state[0] = APP_FSM_INIT_MQTT_CLIENT;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_INIT_MQTT_CLIENT:
                app.state[1] = app.state[0];
                app.error = cellMqttClientInit();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_RUN;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_RUN:
                app.state[1] = app.state[0];
                /* run GNSS FSM */
                app.error = gnssRunFsm();
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

                /* check for new messages */
                if ((app.error == APP_ERROR_OK) && (gnssState == XPLR_GNSS_STATE_DEVICE_READY)) {
                    app.stats.gnssLastAction = esp_timer_get_time();
                    timestampUtc = xplrGnssGetTimestampUTC(gnssDvcPrfId);
                    if (timestampUtc > 0 && app.log.gnssAsyncLogIndex >= 0) {
                        appUpdateFilename(timestampUtc,
                                          app.options.logCfg.filenameInterval,
                                          XPLR_GNSS_UBX_DEFAULT_FILENAME,
                                          app.log.gnssAsyncLogIndex);
                    }
                    app.error = cellMqttClientMsgUpdate();

                    if (app.error == APP_ERROR_MQTT_CLIENT) {
                        app.state[0] = APP_FSM_MQTT_DISCONNECT;
                    } else {
                        /* fwd msg to GNSS */
                        gnssFwdPpData();
                        /* update time counters for reporting */
                        timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &secCnt);
                        if (secCnt >= 1) {
                            appTime++;
                            gnssLocTime++;
                            gnssDrTime++;

                            timer_pause(TIMER_GROUP_0, TIMER_0);
                            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
                            timer_start(TIMER_GROUP_0, TIMER_0);
                        }
                        /* Print app stats every APP_STATISTICS_INTERVAL sec */
                        if (appTime >= app.options.appCfg.statInterval) {
                            appTime = 0;
                            cellMqttClientStatisticsPrint();
                        }
                        /* Print location data every APP_GNSS_INTERVAL sec */
                        if (gnssLocTime >= app.options.appCfg.locInterval) {
                            gnssLocTime = 0;
                            gnssLocationPrint();
                        }
                        /* Print location data every APP_GNSS_INTERVAL sec */
                        if (gnssDrTime >= app.options.drCfg.printInterval) {
                            gnssDrTime = 0;
                            if (app.options.drCfg.printImuData) {
                                gnssDeadReckoningPrint();
                            }
                        }
                        /* If app has reached run time terminate */
                        if (app.stats.time >= app.options.appCfg.runTime) {
                            APP_CONSOLE(W, "Run time finished");
                            app.state[0] = APP_FSM_TERMINATE;
                        }
                        /* If LBAND module has forwarded messages then feed MQTT watchdog (if enabled) */
                        if (xplrLbandHasFrwdMessage()) {
                            xplrCellMqttFeedWatchdog(cellConfig.profileIndex, mqttClient.id);
                        }
                    }
                } else {
                    if (MICROTOSEC(esp_timer_get_time() - app.stats.gnssLastAction) >= APP_INACTIVITY_TIMEOUT ||
                        app.error == APP_ERROR_GNSS_INIT) {
                        app.state[1] = app.state[0];
                        app.state[0] = APP_FSM_ERROR;
                    }
                }
                break;
            case APP_FSM_MQTT_DISCONNECT:
                app.state[1] = app.state[0];
                /* De-init mqtt client */
                xplrCellMqttDeInit(cellConfig.profileIndex, mqttClient.id);
                /* De-init thingstream struct-instance */
                memset(&thingstreamSettings, 0x00, sizeof(xplr_thingstream_t));
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
            case APP_FSM_TERMINATE:
                app.state[1] = app.state[0];
                app.error = appTerminate();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INACTIVE;
                }
                break;
            case APP_FSM_INACTIVE:
                /* code */
                appHaltExecution();
                break;
            case APP_FSM_ERROR:
                /* code */
#if (APP_RESTART_ON_ERROR == 1)
                APP_CONSOLE(E, "Unrecoverable FSM Error. Restarting device.");
                vTaskDelay(10);
                esp_restart();
#endif
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
                app.state[0] = APP_FSM_MQTT_DISCONNECT;
            }
            cellHasRebooted = false;
            APP_CONSOLE(W, "Cell Module has rebooted! Number of total reboots: <%d>", cellReboots);
        }
        /* Check for mqtt disconnect flag */
        if (mqttSessionDisconnected) {
            app.state[0] = APP_FSM_MQTT_DISCONNECT;
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

static app_error_t fetchConfigFromSd(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrSd_error_t sdErr;

    ret = sdInit();
    if (ret == APP_ERROR_OK) {
        memset(configData, 0, APP_JSON_PAYLOAD_BUF_SIZE);
        sdErr = xplrSdReadFileString(configFilename, configData, APP_JSON_PAYLOAD_BUF_SIZE);
        if (sdErr == XPLR_SD_OK) {
            espErr = xplrParseConfigSettings(configData, &app.options);
            if (espErr == ESP_OK) {
                APP_CONSOLE(I, "Successfully parsed application and module configuration");
                ret = APP_ERROR_OK;
            } else {
                APP_CONSOLE(E, "Failed to parse application and module configuration from <%s>", configFilename);
                ret = APP_ERROR_PARSE;
            }
        } else {
            APP_CONSOLE(E, "Unable to get configuration from the SD card");
            ret = APP_ERROR_PARSE;
        }
    } else {
        APP_CONSOLE(E, "Failed to fetch configuration from SD card");
    }

    return ret;
}

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
    gnssCfg->hw.dvcType = (xplrLocDeviceType_t)app.options.gnssCfg.module;
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

    gnssCfg->dr.enable = app.options.drCfg.enable;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;
    gnssCfg->corrData.source = (xplrGnssCorrDataSrc_t)app.options.gnssCfg.corrDataSrc;
}

static void configLbandSettings(xplrLbandDeviceCfg_t *lbandCfg)
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

    switch (region) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_US;
            break;
        default:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
            enableLband = false;
            break;
    }
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
    cfg->netSettings->pApn = app.options.cellCfg.apn;
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

static void configCellMqttSettings(xplrCell_mqtt_client_t *client)
{
    client->settings.brokerAddress = thingstreamSettings.pointPerfect.brokerAddress;
    client->settings.qos = U_MQTT_QOS_AT_MOST_ONCE;
    client->settings.useFlexService = false;
    client->settings.retainMsg = false;
    client->settings.keepAliveTime = 60;
    client->settings.inactivityTimeout = client->settings.keepAliveTime * 2;

    client->credentials.registerMethod = XPLR_CELL_MQTT_CERT_METHOD_TLS;
    client->credentials.name = brokerName;
    client->credentials.user = NULL;
    client->credentials.password = NULL;
    client->credentials.token = thingstreamSettings.pointPerfect.deviceId;
    client->credentials.rootCaName = rootName;
    client->credentials.certName = certName;
    client->credentials.keyName = keyName;
    client->credentials.rootCaHash = rootHash;
    client->credentials.certHash = certHash;
    client->credentials.keyHash = keyHash;
    client->credentials.cert = thingstreamSettings.pointPerfect.clientCert;
    client->credentials.key = thingstreamSettings.pointPerfect.clientKey;
    client->credentials.rootCa = thingstreamSettings.server.rootCa;

    client->topicList = topics;
    mqttClient.msgReceived = &mqttMsgReceivedCallback;
    mqttClient.disconnected = &mqttDisconnectCallback;
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

app_error_t sdInit(void)
{
    app_error_t ret;
    xplrSd_error_t sdErr;

    sdErr = xplrSdConfigDefaults();
    if (sdErr == XPLR_SD_OK) {
        sdErr = xplrSdStartCardDetectTask();
        if (sdErr == XPLR_SD_OK) {
            /* A window so that card is detected */
            vTaskDelay(pdMS_TO_TICKS(25));
            sdErr = xplrSdInit();
            if (sdErr == XPLR_SD_OK) {
                APP_CONSOLE(I, "SD card Initialized");
                ret = APP_ERROR_OK;
            } else {
                APP_CONSOLE(E, "Failed to initialize SD card");
                ret = APP_ERROR_SD;
            }
        } else {
            APP_CONSOLE(E, "Failed to start card detect task");
            ret = APP_ERROR_SD;
        }
    } else {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = APP_ERROR_SD;
    }

    return ret;
}

static app_error_t cellInit(void)
{
    /* initialize ubxlib and cellular module */

    app_error_t ret;
    xplrCom_error_t err;

    /* initialize ubxlib and cellular module */
    err = xplrUbxlibInit(); /* Initialise the ubxlib APIs we will need */
    if (err == XPLR_COM_OK) {
        configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
        err = xplrComCellInit(&cellConfig); /* Initialize hpg com */
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_CELL_INIT;
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", err);
    }

    return ret;
}

static app_error_t cellSetGreeting(void)
{
    app_error_t ret;
    xplrCom_error_t err;

    err = xplrComSetGreetingMessage(cellConfig.profileIndex, cellGreetingMessage, cellGreetingCallback,
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

static app_error_t gnssRunFsm(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrGnssStates_t state;

    xplrGnssFsm(gnssDvcPrfId);
    state = xplrGnssGetCurrentState(gnssDvcPrfId);

    switch (state) {
        case XPLR_GNSS_STATE_DEVICE_READY:
            if ((dvcLbandConfig.destHandler == NULL) && enableLband) {
                dvcLbandConfig.destHandler = xplrGnssGetHandler(gnssDvcPrfId);
                if (dvcLbandConfig.destHandler != NULL) {
                    espErr = xplrLbandSetDestGnssHandler(lbandDvcPrfId, dvcLbandConfig.destHandler);
                    if (espErr == ESP_OK) {
                        espErr = xplrLbandSendCorrectionDataAsyncStart(lbandDvcPrfId);
                        if (espErr != ESP_OK) {
                            APP_CONSOLE(E, "Failed to get start Lband Async sender!");
                            ret = APP_ERROR_LBAND_INIT;
                        } else {
                            APP_CONSOLE(D, "Successfully started Lband Async sender!");
                            ret = APP_ERROR_OK;
                        }
                    } else {
                        APP_CONSOLE(E, "Failed to set LBAND handler!");
                        ret = APP_ERROR_LBAND_INIT;
                    }
                } else {
                    APP_CONSOLE(E, "Failed to get GNSS handler!");
                    ret = APP_ERROR_LBAND_INIT;
                }
            } else {
                ret = APP_ERROR_OK;
            }
            break;
        case XPLR_GNSS_STATE_DEVICE_RESTART:
            if ((dvcLbandConfig.destHandler != NULL) && enableLband) {
                espErr = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                if (espErr != ESP_OK) {
                    APP_CONSOLE(E, "Failed to get stop Lband Async sender!");
                    ret = APP_ERROR_LBAND_INIT;
                } else {
                    APP_CONSOLE(D, "Successfully stoped Lband Async sender!");
                    dvcLbandConfig.destHandler = NULL;
                    ret = APP_ERROR_OK;
                }
            } else {
                ret = APP_ERROR_OK;
            }
            break;
        case XPLR_GNSS_STATE_ERROR:
            if ((dvcLbandConfig.destHandler != NULL) && enableLband) {
                xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                dvcLbandConfig.destHandler = NULL;
            }
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
#if 0 //Delete stored certificates
            configCellMqttSettings(&mqttClient);
            hpgCellMqttRes = xplrCellMqttInit(cellConfig.profileIndex, 0, &mqttClient);
            xplrCellFactoryReset(cellConfig.profileIndex, 0);
#endif
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
#if(APP_SHUTDOWN_CELL_AFTER_REGISTRATION == 1)
            APP_CONSOLE(E,
                        "Cellular registration not completed. Shutting down cell dvc.");
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

static app_error_t cellMqttClientInit(void)
{
    app_error_t ret;
    xplrCell_mqtt_error_t err;

    mqttClient.enableWdg = app.options.appCfg.mqttWdgEnable;
    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        configCellMqttSettings(&mqttClient);
        err = xplrCellMqttInit(cellConfig.profileIndex, 0, &mqttClient);
        if (err == XPLR_CELL_MQTT_OK) {
            timer_start(TIMER_GROUP_0, TIMER_0);
            ret = APP_ERROR_OK;
        } else {
            ret = APP_ERROR_MQTT_CLIENT;
        }
    }

    return ret;
}

static app_error_t cellMqttClientMsgUpdate(void)
{
    const char *topicName;
    app_error_t ret;
    xplrCell_mqtt_error_t err;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        err = xplrCellMqttFsmRun(cellConfig.profileIndex, mqttClient.id);
        if (err == XPLR_CELL_MQTT_ERROR) {
            ret = APP_ERROR_MQTT_CLIENT;
        } else if (err == XPLR_CELL_MQTT_BUSY) {
            ret = APP_ERROR_BUSY; /* skip */
        } else {
            /* check for new messages */
            if (mqttClient.fsm[0] == XPLR_CELL_MQTT_CLIENT_FSM_READY) {
                for (uint8_t msg = 0; msg < mqttClient.numOfTopics; msg++) {
                    if (mqttClient.topicList[msg].msgAvailable) {
                        app.stats.msgReceived++;
                        app.stats.bytesReceived += mqttClient.topicList[msg].msgSize;
                        mqttClient.topicList[msg].msgAvailable = false;
                        topicName = mqttClient.topicList[msg].name;
                        app.ppMsg.msgAvailable = true;
                        /* update app regarding msg type received */
                        if (xplrThingstreamPpMsgIsKeyDist(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.keyDistribution = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <key distribution topic>.",
                                        topicName);
                        } else if (xplrThingstreamPpMsgIsCorrectionData(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.correctionData = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <correction data topic>.",
                                        topicName);
                        } else if (xplrThingstreamPpMsgIsFrequency(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.frequency = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <frequencies topic>.",
                                        topicName);
                        } else {
                            app.ppMsg.msgAvailable = false;
                            APP_CONSOLE(W, "MQTT client parsed unknown msg...<%s>", topicName);
                        }
                    }
                }
            }
            ret = APP_ERROR_OK;
        }
    }

    return ret;
}

static void cellMqttClientStatisticsPrint(void)
{
    app.stats.time += app.options.appCfg.statInterval;
    APP_CONSOLE(I, "App MQTT Statistics.");
    APP_CONSOLE(D, "Messages Received: %d.", app.stats.msgReceived);
    APP_CONSOLE(D, "Bytes Received: %d.", app.stats.bytesReceived);
    APP_CONSOLE(D, "Uptime: %d seconds.", app.stats.time);
}

static app_error_t thingstreamInit(xplr_thingstream_t *instance)
{
    app_error_t ret;
    xplr_thingstream_error_t err;
    xplrSd_error_t sdErr;
    bool foundCor = false, foundKey = false, foundFreq = false;
    uint8_t topicIndex = 0;
    const char *ztpToken = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";

    /* Fetch credentials from the SD card */
    memset(configData, 0, APP_JSON_PAYLOAD_BUF_SIZE);
    mqttClient.numOfTopics = 0;
    sdErr = xplrSdReadFileString(app.options.tsCfg.uCenterConfigFilename,
                                 configData,
                                 APP_JSON_PAYLOAD_BUF_SIZE);
    if (sdErr == XPLR_SD_OK) {
        /* Set up thingstream module's connection type */
        instance->connType = XPLR_THINGSTREAM_PP_CONN_CELL;
        xplrThingstreamInit(ztpToken, instance);
        err = xplrThingstreamPpConfigFromFile(configData,
                                              region,
                                              (bool)app.options.gnssCfg.corrDataSrc,
                                              instance);
        if (err != XPLR_THINGSTREAM_OK) {
            APP_CONSOLE(E, "Error in thingstream module configuration");
            ret = APP_ERROR_THINGSTREAM;
        } else {
            /* Configure MQTT topics */
            for (uint8_t i = 0; i < instance->pointPerfect.numOfTopics; i++) {
                if (xplrThingstreamPpMsgIsKeyDist(instance->pointPerfect.topicList[i].path, instance)) {
                    /* key distribution topic */
                    topics[topicIndex].index = topicIndex;
                    topics[topicIndex].name = instance->pointPerfect.topicList[i].path;
                    topics[topicIndex].rxBuffer = &rxBuff[topicIndex][0];
                    topics[topicIndex].rxBufferSize = APP_MQTT_BUFFER_SIZE;
                    foundKey = true;
                    topicIndex++;
                } else if (xplrThingstreamPpMsgIsCorrectionData(instance->pointPerfect.topicList[i].path,
                                                                instance)) {
                    /* correction topic */
                    topics[topicIndex].index = topicIndex;
                    topics[topicIndex].name = instance->pointPerfect.topicList[i].path;
                    topics[topicIndex].rxBuffer = &rxBuff[topicIndex][0];
                    topics[topicIndex].rxBufferSize = APP_MQTT_BUFFER_SIZE;
                    foundCor = true;
                    topicIndex++;
                } else if (xplrThingstreamPpMsgIsFrequency(instance->pointPerfect.topicList[i].path, instance)) {
                    /* frequencies topic */
                    topics[topicIndex].index = topicIndex;
                    topics[topicIndex].name = instance->pointPerfect.topicList[i].path;
                    topics[topicIndex].rxBuffer = &rxBuff[topicIndex][0];
                    topics[topicIndex].rxBufferSize = APP_MQTT_BUFFER_SIZE;
                    foundFreq = true;
                    topicIndex++;
                } else {
                    //Do nothing
                }
            }

            if ((foundCor || foundFreq) && foundKey) {
                if (instance->pointPerfect.lbandSupported) {
                    enableLband = (bool)app.options.gnssCfg.corrDataSrc;
                } else {
                    enableLband = false;
                }
                mqttClient.numOfTopics = topicIndex;
                ret = APP_ERROR_OK;
            } else {
                APP_CONSOLE(E, "Failed to parse the required MQTT topics");
                ret = APP_ERROR_THINGSTREAM;
            }
        }
    } else {
        APP_CONSOLE(E,
                    "Failed to read file <%s> from the SD card with error <%d>",
                    app.options.tsCfg.uCenterConfigFilename,
                    sdErr);
        ret = APP_ERROR_THINGSTREAM;
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
        APP_CONSOLE(E, "Failed to set correction data source!");
    } else {
        ret = APP_ERROR_OK;
        APP_CONSOLE(D, "Location service initialized ok");
    }

    return ret;
}

static void gnssFwdPpData(void)
{
    bool topicFound[3];
    const char *topicName;
    esp_err_t  err;

    if (app.ppMsg.msgAvailable) {
        for (int i = 0; i < mqttClient.numOfTopics; i++) {
            topicName = mqttClient.topicList[i].name;
            topicFound[0] = xplrThingstreamPpMsgIsKeyDist(topicName, &thingstreamSettings);
            topicFound[1] = xplrThingstreamPpMsgIsCorrectionData(topicName, &thingstreamSettings);
            topicFound[2] = xplrThingstreamPpMsgIsFrequency(topicName, &thingstreamSettings);

            if (topicFound[0] && app.ppMsg.type.keyDistribution) {
                err = xplrGnssSendDecryptionKeys(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.keyDistribution = 0;
                    APP_CONSOLE(D, "Decryption keys forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd decryption keys to GNSS module.");
                }

            } else if (topicFound[1] && app.ppMsg.type.correctionData && (!enableLband)) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.correctionData = 0;
                    APP_CONSOLE(D, "Correction data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd correction data to GNSS module.");
                }
            } else if (topicFound[2] && app.ppMsg.type.frequency && enableLband) {
                err = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                    mqttClient.topicList[i].rxBuffer,
                                                    dvcLbandConfig.corrDataConf.region);
                if (err == ESP_OK) {
                    app.ppMsg.type.frequency = 0;
                    frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                    if (frequency == 0) {
                        APP_CONSOLE(E, "No LBAND frequency is set");
                    } else {
                        APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
                    }
                } else {
                    APP_CONSOLE(W, "Failed to fwd frequency to LBAND module.");
                }
            } else {
                /* topic name invalid or data already sent. Do nothing */
            }

            /* end of parsing, clear buffer */
            memset(mqttClient.topicList[i].rxBuffer, 0x00, mqttClient.topicList[i].msgSize);
        }
        app.ppMsg.msgAvailable = false;
        mqttSessionDisconnected = false;
        failedRecover = false;
    }
}

static void gnssLocationPrint(void)
{
    esp_err_t err;
    bool hasMessage = xplrGnssHasMessage(gnssDvcPrfId);

    if (hasMessage) {
        err = xplrGnssGetLocationData(gnssDvcPrfId, &gnssLocation);
        if (err != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location!");
        } else {
            err = xplrGnssPrintLocationData(&gnssLocation);
            if (err != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
            }
        }

        err = xplrGnssPrintGmapsLocation(gnssDvcPrfId);
        if (err != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }
    }
}

static void gnssDeadReckoningPrint(void)
{
    esp_err_t ret;

    if (xplrGnssIsDrEnabled(gnssDvcPrfId)) {
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
    }
}

static app_error_t lbandInit(void)
{
    esp_err_t espRet;
    app_error_t ret;

    APP_CONSOLE(D, "Waiting for LBAND device to come online!");
    configLbandSettings(&dvcLbandConfig);
    espRet = xplrLbandStartDevice(lbandDvcPrfId, &dvcLbandConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Lband device config failed!");
        ret = APP_ERROR_LBAND_INIT;
    } else {
        espRet = xplrLbandPrintDeviceInfo(lbandDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print LBAND device info!");
            ret = APP_ERROR_LBAND_INIT;
        } else {
            ret = APP_ERROR_OK;
        }
    }

    return ret;
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

    return ret;
}

static void appInit(void)
{
    app.state[0] = APP_FSM_INIT_HW;
    timerInit();
    app.state[0] = APP_FSM_INIT_PERIPHERALS;
}

static app_error_t appSetupConfiguration(void)
{
    app_error_t ret;
    BaseType_t taskCreated;

    /* Check configuration for hot plug functionality */
    if (app.options.logCfg.hotPlugEnable) {
        taskCreated = xTaskCreate(appCardDetectTask,
                                  "appCardDetectTask",
                                  4 * 1024,
                                  NULL,
                                  20,
                                  &cardDetectTaskHandler);
        if (taskCreated == pdPASS) {
            APP_CONSOLE(D, "Hot plug functionality is enabled");
            ret = APP_ERROR_OK;
        } else {
            APP_CONSOLE(E, "Could not enable hot plug functionality");
            ret = APP_ERROR_CREDENTIALS_FETCH;
        }
    } else {
        //Hot plug option not selected.
        APP_CONSOLE(D, "Hot plug functionality not selected");
        ret = APP_ERROR_OK;
    }

    if (ret == APP_ERROR_OK) {
        /* Setup thingstream region base on configuration */
        if (strstr(app.options.tsCfg.region, "EU") != NULL) {
            region = XPLR_THINGSTREAM_PP_REGION_EU;
        } else if (strstr(app.options.tsCfg.region, "US") != NULL) {
            region = XPLR_THINGSTREAM_PP_REGION_US;
        } else if (strstr(app.options.tsCfg.region, "KR") != NULL) {
            region = XPLR_THINGSTREAM_PP_REGION_KR;
        } else if (strstr(app.options.tsCfg.region, "AU")) {
            region = XPLR_THINGSTREAM_PP_REGION_AU;
        } else if (strstr(app.options.tsCfg.region, "JP")) {
            region = XPLR_THINGSTREAM_PP_REGION_JP;
        } else {
            region = XPLR_THINGSTREAM_PP_REGION_INVALID;
        }
        if (region != XPLR_THINGSTREAM_PP_REGION_INVALID) {
            ret = APP_ERROR_OK;
        } else {
            ret = APP_ERROR_THINGSTREAM;
        }
    } else {
        //Do nothing. Error has occurred in setup
    }

    return ret;
}

static app_error_t appTerminate(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrGnssError_t gnssErr;
    uint64_t startTime = 0;

    xplrCellMqttDeInit(cellConfig.profileIndex, mqttClient.id);

    if (enableLband) {
        espErr = xplrLbandPowerOffDevice(lbandDvcPrfId);
    } else {
        espErr = ESP_OK;
    }

    if (espErr == ESP_OK) {
        espErr = xplrGnssPowerOffDevice(gnssDvcPrfId);
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
        APP_CONSOLE(E, "App could not stop lband device.");
        ret = APP_ERROR_LBAND_INIT;
    }
    APP_CONSOLE(I, "App MQTT Statistics.");
    APP_CONSOLE(D, "Messages Received: %d.", app.stats.msgReceived);
    APP_CONSOLE(D, "Bytes Received: %d.", app.stats.bytesReceived);
    APP_CONSOLE(D, "Uptime: %d seconds.", app.stats.time);
    APP_CONSOLE(W, "App disconnected the MQTT client.");
    xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
    appDeInitLogging();
    return ret;
}

static app_error_t appInitLogging(void)
{
    app_error_t ret;
    xplr_cfg_logInstance_t *instance = NULL;
    bool abort = false;
    int i;

    /* Initialize logging instances based on configuration */
    for (i = 0; i < app.options.logCfg.numOfInstances; i++) {
        instance = &app.options.logCfg.instance[i];
        if (strstr(instance->description, "Application") != NULL) {
            if (instance->enable) {
                app.log.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                  instance->filename,
                                                  instance->sizeInterval,
                                                  instance->erasePrev);
                if (app.log.appLogIndex >= 0) {
                    APP_CONSOLE(D, "Application logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize application logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.appLogIndex = -1;
            }
        } else if (strstr(instance->description, "NVS") != NULL) {
            if (instance->enable) {
                app.log.nvsLogIndex = xplrNvsInitLogModule(instance);
                if (app.log.nvsLogIndex >= 0) {
                    APP_CONSOLE(D, "NVS logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize NVS logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.nvsLogIndex = -1;
            }
        } else if (strstr(instance->description, "MQTT Cell") != NULL) {
            if (instance->enable) {
                app.log.mqttLogIndex = xplrCellMqttInitLogModule(instance);
                if (app.log.mqttLogIndex >= 0) {
                    APP_CONSOLE(D, "MQTT logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize MQTT logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.mqttLogIndex = -1;
            }
        } else if (strstr(instance->description, "GNSS Async") != NULL) {
            if (instance->enable) {
                app.log.gnssAsyncLogIndex = xplrGnssAsyncLogInit(instance);
                if (app.log.gnssAsyncLogIndex >= 0) {
                    APP_CONSOLE(D, "GNSS Async logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize GNSS Async logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.gnssAsyncLogIndex = -1;
            }
        } else if (strstr(instance->description, "GNSS Info") != NULL) {
            if (instance->enable) {
                app.log.gnssLogIndex = xplrGnssInitLogModule(instance);
                if (app.log.gnssLogIndex >= 0) {
                    APP_CONSOLE(D, "GNSS logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize GNSS logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.gnssLogIndex = -1;
            }
        } else if (strstr(instance->description, "Location Helpers") != NULL) {
            if (instance->enable) {
                app.log.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(instance);
                if (app.log.locHelperLogIndex >= 0) {
                    APP_CONSOLE(D, "Location Helpers Service logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize Location Helpers logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.locHelperLogIndex = -1;
            }
        } else if (strstr(instance->description, "COM Cell") != NULL) {
            if (instance->enable) {
                app.log.comLogIndex = xplrComCellInitLogModule(instance);
                if (app.log.comLogIndex >= 0) {
                    APP_CONSOLE(D, "COM Cell logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize COM Cell logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.comLogIndex = -1;
            }
        } else if (strstr(instance->description, "Thingstream") != NULL) {
            if (instance->enable) {
                app.log.thingstreamLogIndex = xplrThingstreamInitLogModule(instance);
                if (app.log.thingstreamLogIndex >= 0) {
                    APP_CONSOLE(D, "Thingstream module logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize Thingstream logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.thingstreamLogIndex = -1;
            }
        } else if (strstr(instance->description, "Lband") != NULL) {
            if (instance->enable) {
                app.log.lbandLogIndex = xplrLbandInitLogModule(instance);
                if (app.log.lbandLogIndex >= 0) {
                    APP_CONSOLE(D, "LBand module logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize LBand logging");
                    abort = true;
                    break;
                }
            } else {
                app.log.lbandLogIndex = -1;
            }
        } else {
            /* Module not used by the current application. Continue to the next */
        }
    }

    if (abort) {
        /* A module failed to initialize */
        APP_CONSOLE(E, "Failed to initialize logging instance with index <%d>", i);
        ret = APP_ERROR_LOGGING_INIT;
    } else {
        APP_CONSOLE(I, "Logging initialized successfully for all selected modules in configuration file");
        ret = APP_ERROR_OK;
    }

    return ret;
}

static void appDeInitLogging(void)
{
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr = XPLR_SD_ERROR;
    esp_err_t espErr;

    if (app.options.logCfg.hotPlugEnable) {
        vTaskDelete(cardDetectTaskHandler);
    }

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

static void appUpdateFilename(uint64_t timestampUtc,
                              uint64_t intervalUtc,
                              char *filename,
                              int8_t logIndex)
{
    char formattedFilename[64];
    int8_t len;
    xplrLog_error_t logErr;
    /* Check for interval */
    if ((timestampUtc - app.stats.prevTimestamp) >= intervalUtc || app.stats.prevTimestamp == 0) {
        /* Time has come to change the filename */
        memset(formattedFilename, 0, 64);
        /* First we create the format of the timestamp */
        len = xplrTimestampToDateTimeForFilename(timestampUtc, formattedFilename, 64);
        if (len > 0) {
            /* Now we create the full filename */
            strncat(&formattedFilename[len], filename, 64 - len - 1);
            /* And finally we update the log instance */
            logErr = xplrLogSetFilename(logIndex, formattedFilename);
            if (logErr == XPLR_LOG_OK) {
                APP_CONSOLE(I, "Filename updated for log instance <%d>", logIndex);
                /* Save current timestamp */
                app.stats.prevTimestamp = timestampUtc;
            } else {
                APP_CONSOLE(E, "Error in setting the updated filename");
            }
        } else {
            APP_CONSOLE(E, "Could not format timestamp to filename");
        }
    } else {
        /* Nothing to do. Not the time to update the filename yet. */
    }
}

static void appHaltExecution(void)
{
    xplrMemUsagePrint(0);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

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

static void appCardDetectTask(void *arg)
{
    bool prvState = xplrSdIsCardOn();
    bool currState;
    app_error_t appErr;

    for (;;) {
        currState = xplrSdIsCardOn();

        /* Check if state has changed */
        if (currState ^ prvState) {
            if (currState) {
                if (!xplrSdIsCardInit()) {
                    appErr = sdInit();
                    if (appErr == APP_ERROR_OK) {
                        APP_CONSOLE(I, "SD re-initialized");
                    } else {
                        APP_CONSOLE(E, "Failed to initialize SD card");
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

/* ----------------------------------------------------------------
 * STATIC CALLBACKS
 * -------------------------------------------------------------- */

static void mqttMsgReceivedCallback(int32_t numUnread, void *received)
{
    mqttMsgAvailable = (bool *) received;

    // It is important to keep stack usage in this callback
    // to a minimum.  If you want to do more than set a flag
    // (e.g. you want to call into another ubxlib API) then send
    // an event to one of your own tasks, where you have allocated
    // sufficient stack, and do those things there.
    //APP_CONSOLE(I, "There are %d message(s) unread.\n", numUnread);
}

static void mqttDisconnectCallback(int32_t status, void *cbParam)
{
    (void)cbParam;
    (void)status;
    mqttSessionDisconnected = true;
    APP_CONSOLE(W, "MQTT client disconnected");
}

static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam)
{
    int32_t *param = (int32_t *) callbackParam;

    (void) handler;
    (*param)++;
    cellHasRebooted = true;
}