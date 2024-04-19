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
 * An example for demonstration of the configuration of the LARA R6 cellular module το register to a network provider,
 * execute a Zero Touch Provisioning (ZTP) request and connect to Thingstream PointPerfect MQTT broker
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * registers to a network provider using the xplr_com component,
 * executes an HTTPS request to ZTP using httpClient_service component,
 * fetches all required data for an MQTT connection by parsing the JSON response using thingstream_service component,
 * and finally subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * and/or subscribes to PointPerfect frequencies topic enabling LBAND correction (if subscription plan is applicable).
 *
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
#include "driver/timer.h"
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
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/httpClient_service/xplr_http_client.h"
#include "./../../../components/hpglib/src/mqttClient_service/xplr_mqtt_client.h"
#include "./../../../components/hpglib/src/thingstream_service/xplr_thingstream.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/ztp_service/xplr_ztp.h"

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
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

#define APP_MAX_RETRIES_ON_ERROR        (5)                             /* number of retries to recover from error before exiting */

#define APP_STATISTICS_INTERVAL         (10 * 1)                        /* frequency of statistics logging to console in seconds */
#define APP_GNSS_LOC_INTERVAL           (1)                         /* frequency of location info logging to console in seconds */
#if 1 == APP_PRINT_IMU_DATA
#define APP_GNSS_DR_INTERVAL            (5 * 1)                         /* frequency of dead reckoning info logging to console in seconds */
#endif
#define APP_RUN_TIME                    (60 * 1)                        /* period of app (in seconds) before exiting */
#define APP_MQTT_BUFFER_SIZE_LARGE      (10 * 1024)                     /* size of MQTT buffer used for large payloads */
#define APP_MQTT_BUFFER_SIZE_SMALL      (2 * 1024)                      /* size of MQTT buffer used for normal payloads */
#define APP_HTTP_BUFFER_SIZE            (6 * 1024)                      /* size of HTTP(S) buffer used for storing ZTP response */
#define APP_CERTIFICATE_BUFFER_SIZE     (2 * 1024)                      /* size of buffer used for storing certificates */
#define APP_DEVICE_OFF_MODE_BTN         (BOARD_IO_BTN1)                 /* Button for shutting down device */
#define APP_DEVICE_OFF_MODE_TRIGGER     (3U)                            /* Device off press duration in sec */
#define APP_RESTART_ON_ERROR            (1U)            /* Trigger soft reset if device in error state*/
#define APP_INACTIVITY_TIMEOUT          (30)            /* Time in seconds to trigger an inactivity timeout and cause a restart */

#define APP_GNSS_I2C_ADDR               0x42
#define APP_LBAND_I2C_ADDR              0x43
#define APP_SD_HOT_PLUG_FUNCTIONALITY   (1U) & APP_SD_LOGGING_ENABLED   /* Option to enable/disable the hot plug functionality for the SD card */
#define APP_RESTART_ON_ERROR            (1U)                            /* Trigger soft reset if device in error state*/

/**
 * Option to enable the correction message watchdog mechanism.
 * When set to 1, if no correction data are forwarded to the GNSS
 * module (either via IP or SPARTN) for a defined amount of time
 * (MQTT_MESSAGE_TIMEOUT macro defined in hpglib/xplr_mqtt/include/xplr_mqtt.h)
 * an error event will be triggered
*/
#define APP_ENABLE_CORR_MSG_WDG (1U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -11,
    APP_ERROR_LOGGING,
    APP_ERROR_SD_INIT,
    APP_ERROR_SD_CONFIG_NOT_FOUND,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
    APP_ERROR_LBAND_INIT,
    APP_ERROR_MQTT_CLIENT,
    APP_ERROR_HTTP_CLIENT,
    APP_ERROR_NETWORK_OFFLINE,
    APP_ERROR_THINGSTREAM,
    APP_ERROR_OK = 0,
} app_error_t;

/* application states */
typedef enum {
    APP_FSM_INACTIVE = -2,
    APP_FSM_ERROR,
    APP_FSM_INIT_HW = 0,
    APP_FSM_CHECK_SD_CONFIG,
    APP_FSM_APPLY_CONFIG,
    APP_FSM_INIT_LOGGING,
    APP_FSM_INIT_PERIPHERALS,
    APP_FSM_CONFIG_GNSS,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_SET_GREETING_MESSAGE,
    APP_FSM_INIT_HTTP_CLIENT,
    APP_FSM_GET_ROOT_CA,
    APP_FSM_APPLY_ROOT_CA,
    APP_FSM_PERFORM_ZTP,
    APP_FSM_APPLY_THINGSTREAM_CREDS,
    APP_FSM_INIT_MQTT_CLIENT,
    APP_FSM_CONFIG_LBAND,
    APP_FSM_RUN,
    APP_FSM_MQTT_DISCONNECT,
    APP_FSM_TERMINATE,
} app_fsm_t;

/* mqtt pont perfect topic types */
typedef union __attribute__((packed))
{
    struct {
        uint8_t keyDistribution: 1;
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
    uint64_t gnssLastAction;
} app_statistics_t;

/* application options */
typedef struct app_options_type {
    uint32_t locPrintInterval;
    uint32_t imuPrintInterval;
    uint32_t statPrintInterval;
    uint64_t runtime;
} app_options_t;

typedef struct app_type {
    app_error_t         error;
    app_fsm_t           state[2];
    app_statistics_t    stats;
    app_options_t       options;
    app_pp_msg_t        ppMsg;
} app_t;

typedef union __attribute__((packed))
{
    struct {
        uint8_t appLog          : 1;
        uint8_t nvsLog          : 1;
        uint8_t ztpLog          : 1;
        uint8_t mqttLog         : 1;
        uint8_t gnssLog         : 1;
        uint8_t gnssAsyncLog    : 1;
        uint8_t lbandLog        : 1;
        uint8_t locHelperLog    : 1;
        uint8_t comLog          : 1;
        uint8_t httpClientLog   : 1;
        uint8_t thingstreamLog  : 1;
    } singleLogOpts;
    uint16_t allLogOpts;
}
appLog_Opt_t;

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
    int8_t          comLogIndex;
    int8_t          httpClientLogIndex;
    int8_t          thingstreamLogIndex;
} appLog_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
// *INDENT-OFF*
app_t app;
/**
 * Region for Thingstream's correction data
 */
xplr_thingstream_pp_region_t ppRegion = XPLR_THINGSTREAM_PP_REGION_EU;
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
xplrGnssStates_t gnssState;
static xplrLocDeviceType_t gnssDvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
static xplrGnssCorrDataSrc_t gnssCorrSrc = (xplrGnssCorrDataSrc_t)
                                           CONFIG_XPLR_CORRECTION_DATA_SOURCE;
static bool gnssDrEnable = CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE;
xplrGnssLocation_t gnssLocation;
#if 1 == APP_PRINT_IMU_DATA
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif
const uint8_t lbandDvcPrfId = 0;
const uint8_t gnssDvcPrfId = 0;
static uint32_t frequency;
/* HTTP Client vars */
static xplrCell_http_client_t httpClient;
static xplrCell_http_session_t httpSession;
char httpBuff[APP_HTTP_BUFFER_SIZE];
/* thingstream platform vars */
xplr_thingstream_t thingstreamSettings;
/* pp ztp related */
const char urlAwsRootCa[] = CONFIG_XPLR_AWS_ROOTCA_URL;
const char urlAwsRootCaPath[] = CONFIG_XPLR_AWS_ROOTCA_PATH;
const char ztpRootCaName[] = "amazonAwsRootCa.crt"; /* name of root ca as stored in cellular module */
char *ztpPpToken = CONFIG_XPLR_TS_PP_ZTP_TOKEN; /* ztp token */
static char ztpPayload[APP_HTTP_BUFFER_SIZE];
static xplrZtpData_t ztpData ={
    .payload = ztpPayload,
    .payloadLength = APP_HTTP_BUFFER_SIZE
};
/* mqtt client related  */
static xplrCell_mqtt_client_t mqttClient;
const char ztpPpCertName[] = "ztpPp.crt";   /* name of ztp cert as stored in cellular module */
const char ztpKeyName[] = "ztpPp.key";      /* name of ztp key as stored in cellular module */
xplrCell_mqtt_topic_t topics[XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX]; /* max number of topics expected */
char rxBuffSmall[XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX - 3][APP_MQTT_BUFFER_SIZE_SMALL]; /* 2D array holding normal topics payload*/
char rxBuffLarge[3][APP_MQTT_BUFFER_SIZE_LARGE];    /* 2D array holding large topics payload*/
bool mqttSessionDisconnected = false;
bool mqttMsgAvailable = false;
bool cellHasRebooted = false;
/* md5 hash of certificates used, leave empty for overwriting the certificate*/
const char ztpRootCaHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    //" ";
const char ztpPpCertHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    //" ";
const char ztpPpKeyHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";     //" ";

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
    .comLogIndex = -1,
    .httpClientLogIndex = -1,
    .thingstreamLogIndex = -1
};

static bool deviceOffRequested = false;
const char cellGreetingMessage[] = "LARA JUST WOKE UP";

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif
// *INDENT-ON*
int32_t cellReboots = 0;    /**< Count of total reboots of the cellular module */
static bool failedRecover = false;
static bool enableLband = false;
static char *configData = ztpPayload;
/* The name of the configuration file */
static char configFilename[] = "xplr_config.json";
/* Application configuration options */
xplr_cfg_t appOptions;
/* Flag indicating the board is setup by the SD config file */
static bool isConfiguredFromFile = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* configure free running timer for calculating time intervals in app */
static void timerInit(void);
/* configure gnss related settings*/
static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
/* configure lband related settings */
static void configLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
/* configure cellular related settings */
static void configCellSettings(xplrCom_cell_config_t *cfg);
/* configure HTTP(S) related settings */
static void configCellHttpSettings(xplrCell_http_client_t *client);
/* configure MQTT related settings */
static void configCellMqttSettings(xplrCell_mqtt_client_t *client, xplr_thingstream_t *settings);
/* initialize cellular module */
static app_error_t cellInit(void);
/* cell re-init cell connection */
static app_error_t cellRestart(void);
/* configure greeting message and callback for cellular module */
static app_error_t cellSetGreeting(void);
/* runs the fsm for the GNSS module */
static app_error_t gnssRunFsm(void);
/* register cellular module to the network */
static app_error_t cellNetworkRegister(void);
/* check if cellular module is connected to the network */
static app_error_t cellNetworkConnected(void);
/* configure webserver settings to connect to */
static void cellHttpClientSetServer(const char *address,
                                    xplrCell_http_cert_method_t security,
                                    bool async);
/* connect to the http server */
static app_error_t cellHttpClientConnect(void);
/* disconnect from the http server */
static void cellHttpClientDisconnect(void);
/* retrieve root ca from webserver */
static app_error_t cellHttpClientGetRootCa(void);
/* apply obtained root ca */
static app_error_t cellHttpClientApplyRootCa(void);
/* apply obtained PointPerfect credentials */
static app_error_t cellHttpClientApplyThingstreamCreds(void);
/* initialize MQTT client */
static app_error_t cellMqttClientInit(void);
/* check MQTT client for new messages */
static app_error_t cellMqttClientMsgUpdate(void);
/* print network data statistics to console */
static void cellMqttClientStatisticsPrint(void);
/* initialize thingstream service */
static app_error_t thingstreamInit(const char *token, xplr_thingstream_t *instance);
/* update mqtt client with new parameters obtained from ztp response */
static void thingstreamUpdateMqttClient(xplr_thingstream_t *instance,
                                        xplrCell_mqtt_client_t *client);
/* initialize the GNSS module */
static app_error_t gnssInit(void);
/* initialize the LBand module */
static app_error_t lbandInit(void);
/* forward correction data to GNSS module */
static void gnssFwdPpData(void);
/* print location info to console */
static void gnssLocationPrint(void);
#if 1 == APP_PRINT_IMU_DATA
/* print dead reckoning info to console */
static void gnssDeadReckoningPrint(void);
#endif
/* initialize SD card */
static app_error_t sdInit(void);
/* initialize hw */
static esp_err_t appInitBoard(void);
/* initialize app */
static void appInit(void);
/* fetch configuration from SD file */
static app_error_t appFetchConfigFromFile(void);
/* apply configuration from file */
static void appApplyConfigFromFile(void);
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
/* card detect task */
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appInitHotPlugTask(void);
static void appCardDetectTask(void *arg);
#endif
/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
/* callback triggered when new MQTT message is available */
static void mqttMsgReceivedCallback(int32_t numUnread, void *received);
/* callback triggered when MQTT client is disconnected */
static void mqttDisconnectCallback(int32_t status, void *cbParam);
/* callback triggered when http(s) response is available */
static void httpResponseCb(uDeviceHandle_t devHandle,
                           int32_t statusCodeOrError,
                           size_t responseSize,
                           void *pResponseCallbackParam);
/* callback triggered when Cell module has rebooted (intentionally or unintentionally) */
static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{
    esp_err_t espErr;
    double secCnt = 0; /* timer counter */
    double appTime = 0; /* for printing mqtt statistics at APP_STATISTICS_INTERVAL */
    double gnssLocTime = 0; /* for printing geolocation at APP_GNSS_LOC_INTERVAL */
    bool isRstControlled = false;
    bool lbandConfigured = false;

#if 1 == APP_PRINT_IMU_DATA
    double gnssDrTime = 0; /* for printing dead reckoning at APP_GNSS_DR_INTERVAL */
#endif
    size_t retries = 0; /* for error handling in APP_FSM_ERROR */
    static bool mqttDataFetchedInitial = true;

    APP_CONSOLE(I, "XPLR-HPG-SW Demo: Thingstream PointPerfect with ZTP\n");

    while (1) {
        switch (app.state[0]) {
            case APP_FSM_INIT_HW:
                app.state[1] = app.state[0];
                appInitBoard();
                appInit();
                app.state[0] = APP_FSM_CHECK_SD_CONFIG;
                break;
            case APP_FSM_CHECK_SD_CONFIG:
                app.state[1] = app.state[0];
                app.error = appFetchConfigFromFile();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_APPLY_CONFIG;
                } else {
                    app.state[0] = APP_FSM_INIT_LOGGING;
                }
                break;
            case APP_FSM_APPLY_CONFIG:
                app.state[1] = app.state[0];
                appApplyConfigFromFile();
                app.state[0] = APP_FSM_INIT_LOGGING;
                break;
            case APP_FSM_INIT_LOGGING:
                app.state[1] = app.state[0];
#if (1 == APP_SD_LOGGING_ENABLED)
                espErr = appInitLogging();
                if (espErr == ESP_OK) {
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
                    appInitHotPlugTask();
#endif
                    APP_CONSOLE(I, "Logging initialized");
                } else {
                    APP_CONSOLE(E, "Failed to initialize logging");
                }
#endif
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
                        app.stats.gnssLastAction = esp_timer_get_time();
                        app.state[0] = APP_FSM_CHECK_NETWORK;
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - app.stats.gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
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
                    XPLR_CI_CONSOLE(2304, "OK");
                } else if (app.error == APP_ERROR_NETWORK_OFFLINE) {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2304, "ERROR");
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
                    app.state[0] = APP_FSM_INIT_HTTP_CLIENT;
                }
                break;
            case APP_FSM_INIT_HTTP_CLIENT:
                app.state[1] = app.state[0];
                configCellHttpSettings(&httpClient);
                cellHttpClientSetServer(urlAwsRootCa, XPLR_CELL_HTTP_CERT_METHOD_NONE, true);
                app.error = thingstreamInit(ztpPpToken, &thingstreamSettings);
                if (app.error == APP_ERROR_OK) {
                    httpClient.credentials.rootCa = thingstreamSettings.server.rootCa;
                    app.error = cellHttpClientConnect();
                }

                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_GET_ROOT_CA;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_GET_ROOT_CA:
                app.state[1] = app.state[0];
                app.error = cellHttpClientGetRootCa();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_APPLY_ROOT_CA;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_APPLY_ROOT_CA:
                if (!httpClient.session->requestPending) {
                    app.state[1] = app.state[0];
                    app.error = cellHttpClientApplyRootCa();
                    if (app.error == APP_ERROR_OK) {
                        cellHttpClientDisconnect();
                        app.state[0] = APP_FSM_PERFORM_ZTP;
                        XPLR_CI_CONSOLE(2306, "OK");
                    } else {
                        app.state[0] = APP_FSM_ERROR;
                        XPLR_CI_CONSOLE(2306, "ERROR");
                    }
                }
                break;
            case APP_FSM_PERFORM_ZTP:
                app.state[1] = app.state[0];
                espErr = xplrZtpGetPayloadCell(ztpRootCaName, &thingstreamSettings, &ztpData, &cellConfig);
                if (espErr == ESP_OK) {
                    app.state[0] =  APP_FSM_APPLY_THINGSTREAM_CREDS;
                    XPLR_CI_CONSOLE(2307, "OK");
                } else {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2307, "ERROR");
                }
                break;
            case APP_FSM_APPLY_THINGSTREAM_CREDS:
                app.state[1] = app.state[0];
                app.error = cellHttpClientApplyThingstreamCreds();
                if (app.error == APP_ERROR_OK) {
                    XPLR_CI_CONSOLE(2308, "OK");
                    if (thingstreamSettings.pointPerfect.lbandSupported && (!lbandConfigured) && enableLband) {
                        app.state[0] = APP_FSM_CONFIG_LBAND;
                    } else {
                        app.state[0] = APP_FSM_INIT_MQTT_CLIENT;
                    }
                } else {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2308, "ERROR");
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
                    XPLR_CI_CONSOLE(2303, "ERROR");
                }
                break;
            case APP_FSM_INIT_MQTT_CLIENT:
                app.state[1] = app.state[0];
                app.error = cellMqttClientInit();
                if (app.error == APP_ERROR_OK) {
                    XPLR_CI_CONSOLE(2309, "OK");
                    app.state[0] = APP_FSM_RUN;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                    XPLR_CI_CONSOLE(2309, "ERROR");
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
                    app.error = cellMqttClientMsgUpdate();
                    if (mqttDataFetchedInitial) {
                        XPLR_CI_CONSOLE(2310, "OK");
                        mqttDataFetchedInitial = false;
                    }
                } else {
                    if (MICROTOSEC(esp_timer_get_time() - app.stats.gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                        app.state[1] = app.state[0];
                        app.state[0] = APP_FSM_ERROR;
                        XPLR_CI_CONSOLE(2310, "ERROR");
                    }
                }

                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    /* fwd msg to GNSS */
                    gnssFwdPpData();
                    /* update time counters for reporting */
                    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &secCnt);
                    if (secCnt >= 1) {
                        appTime++;
                        gnssLocTime++;
#if 1 == APP_PRINT_IMU_DATA
                        gnssDrTime++;
#endif

                        timer_pause(TIMER_GROUP_0, TIMER_0);
                        timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
                        timer_start(TIMER_GROUP_0, TIMER_0);
                    }
                    /* Print app stats every APP_STATISTICS_INTERVAL sec */
                    if (appTime >= app.options.statPrintInterval) {
                        appTime = 0;
                        cellMqttClientStatisticsPrint();
                    }
                    /* Print location data every APP_GNSS_INTERVAL sec */
                    if (gnssLocTime >= app.options.locPrintInterval) {
                        gnssLocTime = 0;
                        gnssLocationPrint();
                    }
#if 1 == APP_PRINT_IMU_DATA
                    /* Print location data every APP_GNSS_INTERVAL sec */
                    if (gnssDrTime >= app.options.imuPrintInterval) {
                        gnssDrTime = 0;
                        gnssDeadReckoningPrint();
                    }
#endif
                    /* Check if its time to terminate the app */
                    if (app.stats.time >= app.options.runtime) {
                        app.state[0] = APP_FSM_TERMINATE;
                    }

                    /* If LBAND module has forwarded messages then feed MQTT watchdog (if enabled) */
                    if (xplrLbandHasFrwdMessage()) {
                        xplrCellMqttFeedWatchdog(cellConfig.profileIndex, mqttClient.id);
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
                break;
            case APP_FSM_ERROR:
                retries++;
                if (retries < APP_MAX_RETRIES_ON_ERROR) {
                    if (app.state[1] == APP_FSM_APPLY_THINGSTREAM_CREDS) {
                        /* http status code might return -1. In that case, retry*/
                        if (httpClient.session->error == -1) {
                            app.state[0] = APP_FSM_PERFORM_ZTP;
                            APP_CONSOLE(W, "Device %d, client %d returned %d, retry post request.\n",
                                        cellConfig.profileIndex,
                                        httpClient.id,
                                        httpClient.session->error);
                        } else {
                            /* unknown error, stay in error state */
                            app.state[0] = APP_FSM_ERROR;
                            retries = APP_MAX_RETRIES_ON_ERROR;
                        }
                    } else if (app.state[1] == APP_FSM_INIT_MQTT_CLIENT) {
                        app.state[0] = APP_FSM_INIT_MQTT_CLIENT;
                    }
                } else {
#if (APP_RESTART_ON_ERROR == 1)
                    APP_CONSOLE(E, "Unrecoverable FSM Error. Restarting device.");
                    vTaskDelay(10);
                    esp_restart();
#endif
                    retries = APP_MAX_RETRIES_ON_ERROR;
#if (APP_RESTART_ON_ERROR == 1)
                    APP_CONSOLE(E, "Unrecoverable FSM Error. Restarting device.");
                    vTaskDelay(10);
                    esp_restart();
#endif
                }
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

static void timerInit(void)
{
    /** initialize timer
     * no irq or alarm.
     * timer in free running mode.
     * timer remains halted after config.
     */
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

    switch (ppRegion) {
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

    /**
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
    cfg->netSettings->type = U_NETWORK_TYPE_CELL;
    if (isConfiguredFromFile) {
        cfg->netSettings->pApn = appOptions.cellCfg.apn;
    } else {
        cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN;
    }
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

static void configCellHttpSettings(xplrCell_http_client_t *client)
{
    client->settings.errorOnBusy = false;
    client->settings.timeoutSeconds = 30;

    client->credentials.token = ztpPpToken;
    client->credentials.rootCaName = ztpRootCaName;
    client->credentials.certName = ztpPpCertName;
    client->credentials.keyName = ztpKeyName;
    client->credentials.rootCaHash = ztpRootCaHash;
    client->credentials.certHash = ztpPpCertHash;
    client->credentials.keyHash = ztpPpKeyHash;

    httpSession.data.buffer = httpBuff;
    httpSession.data.bufferSizeOut = APP_HTTP_BUFFER_SIZE;
    client->session = &httpSession;

    client->responseCb = &httpResponseCb;

    /** rootCa certificate of client to be configured by
     *  the thingstream component.
    */
}

static void configCellMqttSettings(xplrCell_mqtt_client_t *client, xplr_thingstream_t *settings)
{
    client->settings.brokerAddress = settings->pointPerfect.brokerAddress;
    client->settings.qos = U_MQTT_QOS_AT_MOST_ONCE;
    client->settings.useFlexService = false;
    client->settings.retainMsg = false;
    client->settings.keepAliveTime = 60;
    client->settings.inactivityTimeout = client->settings.keepAliveTime * 2;

    client->credentials.registerMethod = XPLR_CELL_MQTT_CERT_METHOD_TLS;
    client->credentials.name = "Thingstream";
    client->credentials.user = NULL;
    client->credentials.password = NULL;
    client->credentials.token = settings->pointPerfect.deviceId;
    client->credentials.rootCaName = ztpRootCaName;
    client->credentials.certName = ztpPpCertName;
    client->credentials.keyName = ztpKeyName;
    client->credentials.rootCaHash = ztpRootCaHash;
    client->credentials.certHash = ztpPpCertHash;
    client->credentials.keyHash = ztpPpKeyHash;

    client->msgReceived = &mqttMsgReceivedCallback;
    client->disconnected = &mqttDisconnectCallback;

    /** certificates and topics to be configured
     * when thingstream component is updated
    */
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
        XPLR_CI_CONSOLE(2301, "OK");
    } else {
        ret = APP_ERROR_CELL_INIT;
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", err);
        XPLR_CI_CONSOLE(2301, "ERROR");
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

static void cellHttpClientSetServer(const char *address,
                                    xplrCell_http_cert_method_t security,
                                    bool async)
{
    httpClient.settings.serverAddress = address;
    httpClient.settings.registerMethod = security;
    httpClient.settings.async = async;
}

static app_error_t cellHttpClientConnect(void)
{
    app_error_t ret;
    xplrCell_http_error_t err;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        err = xplrCellHttpConnect(cellConfig.profileIndex, 0, &httpClient);
        if (err == XPLR_CELL_HTTP_ERROR) {
            APP_CONSOLE(E, "Device %d, client %d (http) failed to Connect.\n", cellConfig.profileIndex,
                        httpClient.id);
            ret = APP_ERROR_HTTP_CLIENT;
        } else {
            ret = APP_ERROR_OK;
            APP_CONSOLE(D, "Device %d, client %d (http) connected ok.\n", cellConfig.profileIndex,
                        httpClient.id);
        }
    }

    return ret;
}

static void cellHttpClientDisconnect(void)
{
    int8_t deviceId = cellConfig.profileIndex;
    int8_t clientId = httpClient.id;

    xplrCellHttpDisconnect(deviceId, clientId);
}

static app_error_t cellHttpClientGetRootCa(void)
{
    app_error_t ret;
    xplrCell_http_error_t err;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        httpClient.session->data.path = (char *)urlAwsRootCaPath;
        httpClient.session->data.bufferSizeOut = APP_HTTP_BUFFER_SIZE;
        err = xplrCellHttpGetRequest(cellConfig.profileIndex, httpClient.id, NULL);
        vTaskDelay(1);
        if (err == XPLR_CELL_HTTP_ERROR) {
            ret = APP_ERROR_HTTP_CLIENT;
            APP_CONSOLE(E, "Device %d, client %d (http) GET REQUEST to %s, failed.\n", cellConfig.profileIndex,
                        httpClient.id,
                        httpClient.session->data.path);
            XPLR_CI_CONSOLE(2305, "ERROR");
        } else {
            ret = APP_ERROR_OK;
            APP_CONSOLE(D, "Device %d, client %d (http) GET REQUEST to %s, ok.\n", cellConfig.profileIndex,
                        httpClient.id,
                        httpClient.session->data.path);
            XPLR_CI_CONSOLE(2305, "OK");
        }
    }

    return ret;
}

static app_error_t cellHttpClientApplyRootCa(void)
{
    xplrCell_http_error_t err;
    app_error_t ret;

    if (httpClient.session->rspAvailable) {
        httpClient.session->rspAvailable = false;
        httpClient.session->data.bufferSizeOut = APP_HTTP_BUFFER_SIZE;

        switch (httpClient.session->statusCode) {
            case 200:
                if (httpClient.session->rspSize <= APP_CERTIFICATE_BUFFER_SIZE) {
                    /* copy certificate to thingstream instance */
                    memcpy(thingstreamSettings.server.rootCa,
                           httpClient.session->data.buffer,
                           httpClient.session->rspSize);
                    memset(httpClient.session->data.buffer, 0x00, httpClient.session->rspSize);
                    xplrRemoveChar(thingstreamSettings.server.rootCa, '\n'); /* remove LFs from certificate */
                    APP_CONSOLE(D, "Device %d, client %d (http) received %d bytes for rootCA.",
                                cellConfig.profileIndex, httpClient.id, httpClient.session->rspSize);
                    ret = APP_ERROR_OK;
                } else {
                    APP_CONSOLE(W, "Device %d, client %d (http) GET REQUEST returned code %d.\n",
                                cellConfig.profileIndex, httpClient.id, httpClient.session->error);
                    ret = APP_ERROR_HTTP_CLIENT;
                }
                break;

            default:
                APP_CONSOLE(W, "Device %d, client %d GET REQUEST returned code %d.\n",
                            cellConfig.profileIndex, httpClient.id, httpClient.session->error);
                ret = APP_ERROR_HTTP_CLIENT;
                break;
        }

        if (ret == APP_ERROR_OK) {
            err = xplrCellHttpCertificateCheckRootCA(cellConfig.profileIndex,
                                                     httpClient.id);
            if (err != XPLR_CELL_HTTP_OK) {
                err = xplrCellHttpCertificateSaveRootCA(cellConfig.profileIndex,
                                                        httpClient.id,
                                                        NULL);
                if (err != XPLR_CELL_HTTP_OK) {
                    ret = APP_ERROR_HTTP_CLIENT;
                } else {
                    ret = APP_ERROR_OK;
                }
            }
        }
    } else {
        APP_CONSOLE(E, "Device %d, client %d has nothing to parse.\n",
                    cellConfig.profileIndex, httpClient.id);
        ret = APP_ERROR_HTTP_CLIENT;
    }

    return ret;
}

static app_error_t cellHttpClientApplyThingstreamCreds(void)
{
    xplr_thingstream_error_t tsErr;
    app_error_t ret;

    tsErr = xplrThingstreamPpConfig(ztpData.payload,
                                    ppRegion,
                                    (bool)gnssCorrSrc,
                                    &thingstreamSettings);
    if (thingstreamSettings.pointPerfect.lbandSupported) {
        enableLband = (bool)gnssCorrSrc;
    }
    if (tsErr == XPLR_THINGSTREAM_OK) {
        APP_CONSOLE(I, "Thingstream credentials are parsed correctly");
        ret = APP_ERROR_OK;
    } else {
        APP_CONSOLE(E, "Error in ZTP payload parse");
        ret = APP_ERROR_THINGSTREAM;
    }


    /* check if thingstream instance is configured and update mqtt client */
    if (ret == APP_ERROR_OK) {
        thingstreamUpdateMqttClient(&thingstreamSettings, &mqttClient);
    } else {
        APP_CONSOLE(E, "Device %d, client %d has nothing to parse.\n",
                    cellConfig.profileIndex, httpClient.id);
        ret = APP_ERROR_HTTP_CLIENT;
    }
    return ret;
}

static app_error_t cellMqttClientInit(void)
{
    app_error_t ret;
    xplrCell_mqtt_error_t err;

    mqttClient.enableWdg = (bool) APP_ENABLE_CORR_MSG_WDG;
    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        configCellMqttSettings(&mqttClient, &thingstreamSettings);
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
            ret = APP_ERROR_OK; /* skip */
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
                            APP_CONSOLE(D, "Topic name <%s> identified as <key distribution topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsCorrectionData(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.correctionData = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <correction data topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsGAD(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.gad = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <GAD topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsHPAC(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.hpac = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <HPAC topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsOCB(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.ocb = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <OCB topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsClock(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.clock = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <clock topic>.", topicName);
                        } else if (xplrThingstreamPpMsgIsFrequency(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.frequency = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <frequency topic>.", topicName);
                        } else {
                            app.ppMsg.msgAvailable = false;
                            APP_CONSOLE(W, "MQTT client parsed unknown msg...");
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
    app.stats.time += APP_STATISTICS_INTERVAL;
    APP_CONSOLE(I, "App MQTT Statistics.");
    APP_CONSOLE(D, "Messages Received: %d.", app.stats.msgReceived);
    APP_CONSOLE(D, "Bytes Received: %d.", app.stats.bytesReceived);
    APP_CONSOLE(D, "Uptime: %d seconds.", app.stats.time);
}

static app_error_t thingstreamInit(const char *token, xplr_thingstream_t *instance)
{
    app_error_t ret;
    xplr_thingstream_error_t err;

    instance->connType = XPLR_THINGSTREAM_PP_CONN_CELL;
    err = xplrThingstreamInit(token, instance);

    if (err != XPLR_THINGSTREAM_OK) {
        ret = APP_ERROR_THINGSTREAM;
    } else {
        ret = APP_ERROR_OK;
    }

    return ret;
}

static void thingstreamUpdateMqttClient(xplr_thingstream_t *instance,
                                        xplrCell_mqtt_client_t *client)
{
    size_t numOfTopics = instance->pointPerfect.numOfTopics;
    char *topicCorrDataEu, *topicCorrDataUs, *topicPath;
    const char *correctionDataEuFilter = "correction topic for EU";
    const char *correctionDataUsFilter = "correction topic for US";
    const char *pathFilter = ";";
    size_t smallBuffIndex = 0;
    size_t largeBuffIndex = 0;

    /* update client certificate and key */
    client->credentials.rootCa = instance->server.rootCa;
    client->credentials.cert = instance->pointPerfect.clientCert;
    client->credentials.key = instance->pointPerfect.clientKey;
    client->numOfTopics = 0; /* reset num of available topics */
    /* update topic list */
    for (int8_t i = 0; i < numOfTopics; i++) {
        topicCorrDataEu = strstr(instance->pointPerfect.topicList[i].description, correctionDataEuFilter);
        topicCorrDataUs = strstr(instance->pointPerfect.topicList[i].description, correctionDataUsFilter);
        topicPath = strstr(instance->pointPerfect.topicList[i].path, pathFilter);

        if (topicPath != NULL) {
            /* currently not supported, skip it */
        } else {
            topics[i].index = i;
            topics[i].name = instance->pointPerfect.topicList[i].path;
            /* assign buffers according to content size expected */
            if ((topicCorrDataEu != NULL) ||
                (topicCorrDataUs != NULL)) {
                /* these topics might exceed 5kb of data. assign a large buffer */
                topics[i].rxBuffer = &rxBuffLarge[largeBuffIndex++][0];
                topics[i].rxBufferSize = APP_MQTT_BUFFER_SIZE_LARGE;
                client->numOfTopics++;
            }  else {
                topics[i].rxBuffer = &rxBuffSmall[smallBuffIndex++][0];
                topics[i].rxBufferSize = APP_MQTT_BUFFER_SIZE_SMALL;
                client->numOfTopics++;
            }
        }
    }

    client->topicList = topics;
#if 0
    for (int i = 0; i < 8; i++) {
        APP_CONSOLE(D, "Topic %d (index:%d) path is%s.",
                    i,
                    topics[i].index,
                    topics[i].name);
    }
    APP_CONSOLE(D, "Num of topics subscribed (%d).", smallBuffIndex + largeBuffIndex);
#endif
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
        XPLR_CI_CONSOLE(2302, "ERROR");
    } else {
        ret = APP_ERROR_OK;
        APP_CONSOLE(D, "Location service initialized ok");
        XPLR_CI_CONSOLE(2302, "OK");
    }

    return ret;
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

static void gnssFwdPpData(void)
{
    bool topicFound[7];
    const char *topicName;
    esp_err_t  err = ESP_OK; //initialized to ESP_OK for use in CI as some below cases do not use it
    static bool correctionDataSentInitial = true;

    if (app.ppMsg.msgAvailable) {
        for (int i = 0; i < mqttClient.numOfTopics; i++) {
            topicName = mqttClient.topicList[i].name;
            topicFound[0] = xplrThingstreamPpMsgIsKeyDist(topicName, &thingstreamSettings);
            topicFound[1] = xplrThingstreamPpMsgIsCorrectionData(topicName, &thingstreamSettings);
            topicFound[2] = xplrThingstreamPpMsgIsGAD(topicName, &thingstreamSettings);
            topicFound[3] = xplrThingstreamPpMsgIsHPAC(topicName, &thingstreamSettings);
            topicFound[4] = xplrThingstreamPpMsgIsOCB(topicName, &thingstreamSettings);
            topicFound[5] = xplrThingstreamPpMsgIsClock(topicName, &thingstreamSettings);
            topicFound[6] = xplrThingstreamPpMsgIsFrequency(topicName, &thingstreamSettings);

            if (topicFound[0] && app.ppMsg.type.keyDistribution) {
                err = xplrGnssSendDecryptionKeys(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.keyDistribution = 0;
                    APP_CONSOLE(D, "Decryption keys forwarded to GNSS module.");
                    if (correctionDataSentInitial) {
                        XPLR_CI_CONSOLE(2311, "OK");
                        correctionDataSentInitial = false;
                    }
                } else {
                    APP_CONSOLE(W, "Failed to fwd decryption keys to GNSS module.");
                    XPLR_CI_CONSOLE(2311, "ERROR");
                }
            } else if (topicFound[1] && app.ppMsg.type.correctionData && (!enableLband)) {
                /* skip since we are sending all subtopics  */
                app.ppMsg.type.correctionData = 0;
            } else if (topicFound[2] && app.ppMsg.type.gad) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.gad = 0;
                    APP_CONSOLE(D, "GAD data forwarded to GNSS module.");
                    if (correctionDataSentInitial) {
                        XPLR_CI_CONSOLE(11, "OK");
                        correctionDataSentInitial = false;
                    }
                } else {
                    APP_CONSOLE(W, "Failed to fwd GAD data to GNSS module.");
                    XPLR_CI_CONSOLE(11, "ERROR");
                }
            } else if (topicFound[3] && app.ppMsg.type.hpac && (!enableLband)) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.hpac = 0;
                    APP_CONSOLE(D, "HPAC data forwarded to GNSS module.");
                    if (correctionDataSentInitial) {
                        XPLR_CI_CONSOLE(11, "OK");
                        correctionDataSentInitial = false;
                    }
                } else {
                    APP_CONSOLE(W, "Failed to fwd HPAC data to GNSS module.");
                    XPLR_CI_CONSOLE(11, "ERROR");
                }
            } else if (topicFound[4] && app.ppMsg.type.ocb && (!enableLband)) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.ocb = 0;
                    APP_CONSOLE(D, "OCB data forwarded to GNSS module.");
                    if (correctionDataSentInitial) {
                        XPLR_CI_CONSOLE(11, "OK");
                        correctionDataSentInitial = false;
                    }
                } else {
                    APP_CONSOLE(W, "Failed to fwd OCB data to GNSS module.");
                    XPLR_CI_CONSOLE(11, "ERROR");
                }
            } else if (topicFound[5] && app.ppMsg.type.clock && (!enableLband)) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.clock = 0;
                    APP_CONSOLE(D, "CLK data forwarded to GNSS module.");
                    XPLR_CI_CONSOLE(11, "OK");
                } else {
                    APP_CONSOLE(W, "Failed to fwd CLK data to GNSS module.");
                    XPLR_CI_CONSOLE(11, "ERROR");
                }
            } else if (topicFound[6] && app.ppMsg.type.frequency && enableLband) {
                err = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                    mqttClient.topicList[i].rxBuffer,
                                                    dvcLbandConfig.corrDataConf.region);
                if (err == ESP_OK) {
                    app.ppMsg.type.frequency = 0;
                    frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                    if (frequency == 0) {
                        APP_CONSOLE(E, "No LBAND frequency is set");
                        XPLR_CI_CONSOLE(2312, "ERROR");
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
    static bool locRTKFirstTime = true;
    static bool allowedPrint = false;
    static double initialTime = 0;
    bool hasMessage = xplrGnssHasMessage(gnssDvcPrfId);

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
        if (hasMessage) {
            err = xplrGnssGetLocationData(gnssDvcPrfId, &gnssLocation);
            if (err != ESP_OK) {
                APP_CONSOLE(W, "Could not get gnss location!");
                XPLR_CI_CONSOLE(2314, "ERROR");
            } else {
                if (locRTKFirstTime) {
                    if ((gnssLocation.locFixType == XPLR_GNSS_LOCFIX_FLOAT_RTK) ||
                        (gnssLocation.locFixType == XPLR_GNSS_LOCFIX_FIXED_RTK)) {
                        locRTKFirstTime = false;
                        XPLR_CI_CONSOLE(10, "OK");
                    }
                }
                err = xplrGnssPrintLocationData(&gnssLocation);
                if (err != ESP_OK) {
                    APP_CONSOLE(W, "Could not print gnss location data!");
                    XPLR_CI_CONSOLE(2314, "ERROR");
                } else {
                    XPLR_CI_CONSOLE(2314, "OK");
                }
            }

            err = xplrGnssPrintGmapsLocation(gnssDvcPrfId);
            if (err != ESP_OK) {
                APP_CONSOLE(W, "Could not print Gmaps location!");
                XPLR_CI_CONSOLE(2314, "ERROR");
            }
        }
    }
}

#if 1 == APP_PRINT_IMU_DATA
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
#endif

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
    timerInit();
    app.options.runtime = APP_RUN_TIME;
    app.options.statPrintInterval = APP_STATISTICS_INTERVAL;
    app.options.locPrintInterval = APP_GNSS_LOC_INTERVAL;
#if 1 == APP_PRINT_IMU_DATA
    app.options.imuPrintInterval = APP_GNSS_DR_INTERVAL;
#endif
}

static app_error_t appFetchConfigFromFile(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrSd_error_t sdErr;
    xplr_board_error_t boardErr = xplrBoardDetectSd();

    if (boardErr == XPLR_BOARD_ERROR_OK) {
        ret = sdInit();
        if (ret == APP_ERROR_OK) {
            memset(configData, 0, APP_HTTP_BUFFER_SIZE);
            sdErr = xplrSdReadFileString(configFilename, configData, APP_HTTP_BUFFER_SIZE);
            if (sdErr == XPLR_SD_OK) {
                espErr = xplrParseConfigSettings(configData, &appOptions);
                if (espErr == ESP_OK) {
                    APP_CONSOLE(I, "Successfully parsed application and module configuration");
                } else {
                    APP_CONSOLE(E, "Failed to parse application and module configuration from <%s>", configFilename);
                }
            } else {
                APP_CONSOLE(E, "Unable to get configuration from the SD card");
                ret = APP_ERROR_SD_CONFIG_NOT_FOUND;
            }
        } else {
            // Do nothing
        }
    } else {
        APP_CONSOLE(D, "SD is not mounted. Keeping Kconfig configuration");
        ret = APP_ERROR_SD_CONFIG_NOT_FOUND;
    }
    /* Empty buffer for the next functions that use it */
    memset(configData, 0, APP_HTTP_BUFFER_SIZE);

    return ret;
}

static void appApplyConfigFromFile(void)
{
    xplr_cfg_logInstance_t *instance = NULL;
    /* Applying the options that are relevant to the example */
    /* Application settings */
    app.options.runtime = appOptions.appCfg.runTime;
    app.options.statPrintInterval = appOptions.appCfg.statInterval;
    app.options.locPrintInterval = appOptions.appCfg.locInterval;
#if (1 == APP_PRINT_IMU_DATA)
    app.options.imuPrintInterval = appOptions.drCfg.printInterval;
#endif
    /* Thingstream Settings */
    ztpPpToken = appOptions.tsCfg.ztpToken;
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
        } else if (strstr(instance->description, "COM Cell") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.comLog = 1;
                appLogCfg.comLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "MQTT Cell") != NULL) {
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
        } else if (strstr(instance->description, "HTTP") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.httpClientLog = 1;
                appLogCfg.httpClientLogIndex = i;
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
static app_error_t sdInit(void)
{
    app_error_t ret;
    xplrSd_error_t sdErr;

    sdErr = xplrSdConfigDefaults();
    if (sdErr != XPLR_SD_OK) {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = APP_ERROR_SD_INIT;
    } else {
        /* Create the card detect task */
        sdErr = xplrSdStartCardDetectTask();
        /* A time window so that the card gets detected*/
        vTaskDelay(pdMS_TO_TICKS(50));
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Failed to start the card detect task");
            ret = APP_ERROR_SD_INIT;
        } else {
            /* Initialize the SD card */
            sdErr = xplrSdInit();
            if (sdErr != XPLR_SD_OK) {
                APP_CONSOLE(E, "Failed to initialize the SD card");
                ret = APP_ERROR_SD_INIT;
            } else {
                APP_CONSOLE(D, "SD card initialized");
                ret = APP_ERROR_OK;
            }
        }
    }

    return ret;
}

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplr_cfg_logInstance_t *instance = NULL;

    /* Initialize the SD card */
    if (!xplrSdIsCardInit()) {
        ret = sdInit();
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
                appLogCfg.mqttLogIndex = xplrCellMqttInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.mqttLogIndex = xplrCellMqttInitLogModule(NULL);
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
        if (appLogCfg.logOptions.singleLogOpts.comLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.comLogIndex];
                appLogCfg.comLogIndex = xplrComCellInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.comLogIndex = xplrComCellInitLogModule(NULL);
            }

            if (appLogCfg.comLogIndex >= 0) {
                APP_CONSOLE(D, "Cellular module logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.httpClientLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.httpClientLogIndex];
                appLogCfg.httpClientLogIndex = xplrCellHttpInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.httpClientLogIndex = xplrCellHttpInitLogModule(NULL);
            }

            if (appLogCfg.httpClientLogIndex >= 0) {
                APP_CONSOLE(D, "Cell HTTP Client service logging instance initialized");
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
        if (appLogCfg.logOptions.singleLogOpts.ztpLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.ztpLogIndex];
                appLogCfg.ztpLogIndex = xplrZtpInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.ztpLogIndex = xplrZtpInitLogModule(NULL);
            }

            if (appLogCfg.ztpLogIndex >= 0) {
                APP_CONSOLE(D, "ZTP service logging instance initialized");
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

static app_error_t appTerminate(void)
{
    app_error_t ret;
    esp_err_t espErr;
    xplrGnssError_t gnssErr;
    uint64_t startTime;

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
#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif
    return ret;
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

/* ----------------------------------------------------------------
 * STATIC CALLBACKS
 * -------------------------------------------------------------- */

static void httpResponseCb(uDeviceHandle_t devHandle,
                           int32_t statusCodeOrError,
                           size_t responseSize,
                           void *pResponseCallbackParam)
{
    APP_CONSOLE(I, "Http response callback fired with code (%d).", statusCodeOrError);
    APP_CONSOLE(D, "Message size of %d bytes.\n", responseSize);

    httpClient.session->error = statusCodeOrError;
    if (statusCodeOrError > -1) {
        httpClient.session->statusCode = statusCodeOrError;
        httpClient.session->rspAvailable = true;
        httpClient.session->rspSize = responseSize;
        httpClient.session->data.bufferSizeOut = APP_HTTP_BUFFER_SIZE;
    }

    if (httpClient.session->requestPending) {
        httpClient.session->requestPending = false;
    }
}

static void mqttMsgReceivedCallback(int32_t numUnread, void *received)
{
    /** It is important to keep stack usage in this callback
     *  to a minimum.  If you want to do more than set a flag
     *  (e.g. you want to call into another ubxlib API) then send
     *  an event to one of your own tasks, where you have allocated
     *  sufficient stack, and do those things there.
    */
    //APP_CONSOLE(I, "There are %d message(s) unread.\n", numUnread);

    mqttMsgAvailable = (bool *) received;
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