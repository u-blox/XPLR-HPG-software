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
 * and finally subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component. 
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

/**
 * If paths not found in VScode: 
 *      press keys --> <ctrl+shift+p> 
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_MAX_RETRIES_ON_ERROR        (5)             /* number of retries to recover from error before exiting */

#define APP_STATISTICS_INTERVAL         (10 * 1)        /* frequency of statistics logging to console in seconds */
#define APP_GNSS_INTERVAL               (1 * 1)         /* frequency of location info logging to console in seconds */
#define APP_RUN_TIME                    (60 * 1)        /* period of app (in seconds) before exiting */
#define APP_MQTT_BUFFER_SIZE_LARGE      (10 * 1024)     /* size of MQTT buffer used for large payloads */
#define APP_MQTT_BUFFER_SIZE_SMALL      (2 * 1024)      /* size of MQTT buffer used for normal payloads */
#define APP_HTTP_BUFFER_SIZE            (6 * 1024)      /* size of HTTP(S) buffer used for storing ZTP response */
#define APP_CERTIFICATE_BUFFER_SIZE     (2 * 1024)      /* size of buffer used for storing certificates */

#define  APP_SERIAL_DEBUG_ENABLED 1U /* used to print debug messages in console. Set to 0 for disabling */
#if defined (APP_SERIAL_DEBUG_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)   esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

#define XPLR_GNSS_I2C_ADDR  0x42

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -7,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
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
    APP_FSM_INIT_PERIPHERALS,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_INIT_HTTP_CLIENT,
    APP_FSM_GET_ROOT_CA,
    APP_FSM_APPLY_ROOT_CA,
    APP_FSM_CONNECT_TO_THINGSTREAM,
    APP_FSM_GET_THINGSTREAM_CREDS,
    APP_FSM_APPLY_THINGSTREAM_CREDS,
    APP_FSM_INIT_MQTT_CLIENT,
    APP_FSM_RUN,
    APP_FSM_TERMINATE,
} app_fsm_t;

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
} app_statistics_t;

typedef struct app_type {
    app_error_t         error;
    app_fsm_t           state[2];
    app_statistics_t    stats;
    app_pp_msg_t        ppMsg;
} app_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
// *INDENT-OFF*
app_t app;
/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;
/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;
/* location modules */
xplrGnssDevInfo_t gnssDvcInfo;
xplrGnssLocation_t gnssLocation;
const uint8_t gnssDvcPrfId = 0;
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
const char ztpPpToken[] = CONFIG_XPLR_TS_PP_ZTP_TOKEN; /* ztp token */
/* mqtt client related  */
static xplrCell_mqtt_client_t mqttClient;
const char ztpPpCertName[] = "ztpPp.crt";   /* name of ztp cert as stored in cellular module */
const char ztpKeyName[] = "ztpPp.key";      /* name of ztp key as stored in cellular module */
xplrCell_mqtt_topic_t topics[XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX]; /* max number of topics expected */
char rxBuffSmall[XPLR_THINGSTREAM_PP_NUMOF_TOPICS_MAX - 3][APP_MQTT_BUFFER_SIZE_SMALL]; /* 2D array holding normal topics payload*/
char rxBuffLarge[3][APP_MQTT_BUFFER_SIZE_LARGE];    /* 2D array holding large topics payload*/
bool mqttSessionDisconnected = false;
bool mqttMsgAvailable = false;
/* md5 hash of certificates used, leave empty for overwriting the certificate*/
const char ztpRootCaHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    //" ";
const char ztpPpCertHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    //" ";
const char ztpPpKeyHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";     //" ";
// *INDENT-ON*

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
/* configure free running timer for calculating time intervals in app */
static void timerInit(void);
/* configure cellular related settings */
static void configCellSettings(xplrCom_cell_config_t *cfg);
/* configure HTTP(S) related settings */
static void configCellHttpSettings(xplrCell_http_client_t *client);
/* configure MQTT related settings */
static void configCellMqttSettings(xplrCell_mqtt_client_t *client, xplr_thingstream_t *settings);
/* initialize cellular module */
static app_error_t cellInit(void);
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
/* retrieve point perfect credentials from ztp server */
static app_error_t cellHttpClientGetThingstreamCredsZtp(void);
/* apply obtained root ca */
static app_error_t cellHttpClientApplyRootCa(void);
/* apply obtained point perfect credentials */
static app_error_t cellHttpClientApplyThingstreamCreds(void);
/* initialize MQTT client */
static app_error_t cellMqttClientInit(void);
/* check MQTT client for new messages */
static app_error_t cellMqttClientMsgUpdate(void);
/* print network data statistics to console */
static void cellMqttClientStatisticsPrint(void);
/* initialize thingstream service */
static app_error_t thingstreamInit(const char *token, xplr_thingstream_t *instance);
/* create ztp request message */
static app_error_t thingstreamCreatePpZtpMsg(xplr_thingstream_t *instance,
                                             xplrCell_http_dataTransfer_t *data);
/* update mqtt client with new parameters obtained from ztp response */
static void thingstreamUpdateMqttClient(xplr_thingstream_t *instance,
                                        xplrCell_mqtt_client_t *client);
/* initialize the GNSS module */
static app_error_t gnssInit(void);
/* forward correction data to GNSS module */
static void gnssFwdPpData(void);
/* print location info to console */
static void gnssLocationPrint(void);
/* initialize app */
static void appInit(void);
/* terminate app */
static app_error_t appTerminate(void);

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

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{
    APP_CONSOLE(I, "XPLR-HPG-SW Demo: Thingstream Point Perfect with ZTP\n");

    double secCnt = 0; /* timer counter */
    double appTime = 0; /* for printing mqtt statistics at APP_STATISTICS_INTERVAL */
    double gnssTime = 0; /* for printing geolocation at APP_GNSS_INTERVAL */
    size_t retries = 0; /* for error handling in APP_FSM_ERROR */

    while (1) {
        switch (app.state[0]) {
            case APP_FSM_INIT_HW:
                app.state[1] = app.state[0];
                xplrBoardInit();
                appInit();
                app.state[0] = APP_FSM_INIT_PERIPHERALS;
                break;
            case APP_FSM_INIT_PERIPHERALS:
                app.state[1] = app.state[0];
                app.error = gnssInit();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.error = cellInit();
                    app.state[0] = APP_FSM_CHECK_NETWORK;
                }
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_CHECK_NETWORK;
                }
                break;
            case APP_FSM_CHECK_NETWORK:
                app.state[1] = app.state[0];
                app.error = cellNetworkRegister();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_INIT_HTTP_CLIENT;
                } else if (app.error == APP_ERROR_NETWORK_OFFLINE) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    /* module still trying to connect. do nothing */
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
                        app.state[0] = APP_FSM_CONNECT_TO_THINGSTREAM;
                    } else {
                        app.state[0] = APP_FSM_ERROR;
                    }
                }
                break;
            case APP_FSM_CONNECT_TO_THINGSTREAM:
                app.state[1] = app.state[0];
                cellHttpClientSetServer(thingstreamSettings.server.serverUrl,
                                        XPLR_CELL_HTTP_CERT_METHOD_ROOTCA,
                                        true);
                app.error = cellHttpClientConnect();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_GET_THINGSTREAM_CREDS;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_GET_THINGSTREAM_CREDS:
                app.state[1] = app.state[0];
                app.error = thingstreamCreatePpZtpMsg(&thingstreamSettings, &httpClient.session->data);
                if (app.error == APP_ERROR_OK) {
                    app.error = cellHttpClientGetThingstreamCredsZtp();
                }

                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_APPLY_THINGSTREAM_CREDS;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_APPLY_THINGSTREAM_CREDS:
                if (!httpClient.session->requestPending) {
                    app.state[1] = app.state[0];
                    app.error = cellHttpClientApplyThingstreamCreds();
                    if (app.error == APP_ERROR_OK) {
                        cellHttpClientDisconnect();
                        app.state[0] = APP_FSM_INIT_MQTT_CLIENT;
                    } else {
                        app.state[0] = APP_FSM_ERROR;
                    }
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
                /* check for new messages */
                app.error = cellMqttClientMsgUpdate();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    /* fwd msg to GNSS */
                    gnssFwdPpData();
                    /* update time counters for reporting */
                    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &secCnt);
                    if (secCnt >= 1) {
                        appTime++;
                        gnssTime++;

                        timer_pause(TIMER_GROUP_0, TIMER_0);
                        timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
                        timer_start(TIMER_GROUP_0, TIMER_0);
                    }
                    /* Print app stats every APP_STATISTICS_INTERVAL sec */
                    if (appTime >= APP_STATISTICS_INTERVAL) {
                        appTime = 0;
                        cellMqttClientStatisticsPrint();
                    }
                    /* Print location data every APP_GNSS_INTERVAL sec */
                    if (gnssTime >= APP_GNSS_INTERVAL) {
                        gnssTime = 0;
                        gnssLocationPrint();
                    }
                    /* Check if its time to terminate the app */
                    if (app.stats.time >= APP_RUN_TIME) {
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
            case APP_FSM_INACTIVE:
                /* code */
                break;
            case APP_FSM_ERROR:
                retries++;
                if (retries < APP_MAX_RETRIES_ON_ERROR) {
                    if (app.state[1] == APP_FSM_APPLY_THINGSTREAM_CREDS) {
                        /* http status code might return -1. In that case, retry*/
                        if (httpClient.session->error == -1) {
                            app.state[0] = APP_FSM_GET_THINGSTREAM_CREDS;
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
                    /* exceeded retries, stay here forever */
                    retries = APP_MAX_RETRIES_ON_ERROR;
                }
                break;

            default:
                break;
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
    } else {
        ret = APP_ERROR_CELL_INIT;
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", err);
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
        } else {
            ret = APP_ERROR_OK;
            APP_CONSOLE(D, "Device %d, client %d (http) GET REQUEST to %s, ok.\n", cellConfig.profileIndex,
                        httpClient.id,
                        httpClient.session->data.path);
        }
    }

    return ret;
}

static app_error_t cellHttpClientGetThingstreamCredsZtp(void)
{
    app_error_t ret;
    xplrCell_http_error_t err;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        httpClient.session->data.path = thingstreamSettings.pointPerfect.urlPath;
        memcpy(httpClient.session->data.contentType, "application/json", 17);
        httpClient.session->data.bufferSizeIn = APP_HTTP_BUFFER_SIZE;
        err = xplrCellHttpPostRequest(cellConfig.profileIndex, httpClient.id, NULL);
        vTaskDelay(1);
        if (err == XPLR_CELL_HTTP_ERROR) {
            ret = APP_ERROR_HTTP_CLIENT;
            APP_CONSOLE(E, "Device %d, client %d (http) POST REQUEST to %s, failed.\n", cellConfig.profileIndex,
                        httpClient.id,
                        httpClient.session->data.path);
        } else {
            ret = APP_ERROR_OK;
            APP_CONSOLE(D, "Device %d, client %d (http) POST REQUEST to %s, ok.\n", cellConfig.profileIndex,
                        httpClient.id,
                        httpClient.session->data.path);
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
    xplrCell_http_session_t *httpSession = httpClient.session;
    xplr_thingstream_error_t tsErr;
    app_error_t ret;

    /** check that a post response is available.
     *  parse the response content and see if its a ztp response.
     *  If ztp response, configure thingstream instance and apply parsed settings
     *  to mqtt client.
     */

    if (httpSession->rspAvailable) {
        httpSession->rspAvailable = false;
        httpSession->data.bufferSizeOut = APP_HTTP_BUFFER_SIZE;

        switch (httpSession->statusCode) {
            case 200:
                /* get thingstream point perfect configuration based on ztp post-response content */
                tsErr = xplrThingstreamPpConfig(httpSession->data.buffer,
                                                XPLR_THINGSTREAM_PP_REGION_EU,
                                                &thingstreamSettings.pointPerfect);

                if (tsErr != XPLR_THINGSTREAM_OK) {
                    ret = APP_ERROR_THINGSTREAM;
                    break;
                } else {
                    ret = APP_ERROR_OK;
                }

                /* format certificates and broker address to make them "cell-compatible" */
                if (ret != APP_ERROR_THINGSTREAM) {
                    /* remove LFs from certificates */
                    xplrRemoveChar(thingstreamSettings.pointPerfect.clientCert, '\n');
                    xplrRemoveChar(thingstreamSettings.pointPerfect.clientKey, '\n');
                    /* add port number to broker url */
                    xplrAddPortInfo(thingstreamSettings.pointPerfect.brokerAddress,
                                    thingstreamSettings.pointPerfect.brokerPort);
                }

                /* all done, clear response buffer and reset response size */
                memset(httpSession->data.buffer, 0x00, APP_HTTP_BUFFER_SIZE);
                httpSession->rspSize = APP_HTTP_BUFFER_SIZE;

                break;

            default:
                APP_CONSOLE(W, "Device %d, client %d POST REQUEST returned code %d.\n",
                            cellConfig.profileIndex, httpClient.id, httpClient.session->error);
                ret = APP_ERROR_HTTP_CLIENT;
                break;
        }

        /* check if thingstream instance is configured and update mqtt client */
        if (ret == APP_ERROR_OK) {
            thingstreamUpdateMqttClient(&thingstreamSettings, &mqttClient);
        }
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
                        } else if (xplrThingstreamPpMsgIsAssistNow(topicName, &thingstreamSettings)) {
                            app.ppMsg.type.assistNow = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <assist now topic>.", topicName);
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

    err = xplrThingstreamInit(token, instance);

    if (err != XPLR_THINGSTREAM_OK) {
        ret = APP_ERROR_THINGSTREAM;
    } else {
        ret = APP_ERROR_OK;
    }

    return ret;
}

static app_error_t thingstreamCreatePpZtpMsg(xplr_thingstream_t *instance,
                                             xplrCell_http_dataTransfer_t *data)
{
    app_error_t ret;
    xplr_thingstream_error_t err;

    err = xplrThingstreamApiMsgCreate(XPLR_THINGSTREAM_API_LOCATION_ZTP,
                                      data->buffer,
                                      &data->bufferSizeOut,
                                      instance);

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
    char *topicCorrDataEu, *topicCorrDataUs, *topicAssistNow, *topicPath;
    const char *correctionDataEuFilter = "correction topic for EU";
    const char *correctionDataUsFilter = "correction topic for US";
    const char *assistNowFilter = "AssistNow topic";
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
        topicAssistNow = strstr(instance->pointPerfect.topicList[i].description, assistNowFilter);
        topicPath = strstr(instance->pointPerfect.topicList[i].path, pathFilter);

        if (topicPath != NULL) {
            /* currently not supported, skip it */
        } else {
            topics[i].index = i;
            topics[i].name = instance->pointPerfect.topicList[i].path;
            /* assign buffers according to content size expected */
            if ((topicAssistNow != NULL) ||
                (topicCorrDataEu != NULL) ||
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
        err = xplrGnssStartDeviceDefaultSettings(gnssDvcPrfId, XPLR_GNSS_I2C_ADDR);
    }

    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "Failed to start GNSS device at address (0x%02x)", XPLR_GNSS_I2C_ADDR);
    } else {
        err = xplrGnssSetCorrectionDataSource(gnssDvcPrfId, XPLR_GNSS_CORRECTION_FROM_IP);
    }

    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "Failed to set correction data source!");
    } else {
        ret = APP_ERROR_OK;
        xplrGnssPrintDeviceInfo(gnssDvcPrfId);
        APP_CONSOLE(D, "Location service initialized ok");
    }

    return ret;
}

static void gnssFwdPpData(void)
{
    bool topicFound[8];
    const char *topicName;
    esp_err_t  err;

    if (app.ppMsg.msgAvailable) {
        for (int i = 0; i < mqttClient.numOfTopics; i++) {
            topicName = mqttClient.topicList[i].name;
            topicFound[0] = xplrThingstreamPpMsgIsKeyDist(topicName, &thingstreamSettings);
            topicFound[1] = xplrThingstreamPpMsgIsAssistNow(topicName, &thingstreamSettings);
            topicFound[2] = xplrThingstreamPpMsgIsCorrectionData(topicName, &thingstreamSettings);
            topicFound[3] = xplrThingstreamPpMsgIsGAD(topicName, &thingstreamSettings);
            topicFound[4] = xplrThingstreamPpMsgIsHPAC(topicName, &thingstreamSettings);
            topicFound[5] = xplrThingstreamPpMsgIsOCB(topicName, &thingstreamSettings);
            topicFound[6] = xplrThingstreamPpMsgIsClock(topicName, &thingstreamSettings);
            topicFound[7] = xplrThingstreamPpMsgIsFrequency(topicName, &thingstreamSettings);

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
            } else if (topicFound[1] && app.ppMsg.type.assistNow) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.assistNow = 0;
                    APP_CONSOLE(D, "AssistNow data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd AssistNow data to GNSS module.");
                }
            } else if (topicFound[2] && app.ppMsg.type.correctionData) {
                /* skip since we are sending all subtopics  */
                app.ppMsg.type.correctionData = 0;
            } else if (topicFound[3] && app.ppMsg.type.gad) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.gad = 0;
                    APP_CONSOLE(D, "GAD data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd GAD data to GNSS module.");
                }
            } else if (topicFound[4] && app.ppMsg.type.hpac) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.hpac = 0;
                    APP_CONSOLE(D, "HPAC data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd HPAC data to GNSS module.");
                }
            } else if (topicFound[5] && app.ppMsg.type.ocb) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.ocb = 0;
                    APP_CONSOLE(D, "OCB data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd OCB data to GNSS module.");
                }
            } else if (topicFound[6] && app.ppMsg.type.clock) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClient.topicList[i].rxBuffer,
                                                 mqttClient.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    app.ppMsg.type.clock = 0;
                    APP_CONSOLE(D, "CLK data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd CLK data to GNSS module.");
                }
            } else if (topicFound[7] && app.ppMsg.type.frequency) {
                app.ppMsg.type.frequency = 0;
            } else {
                /* topic name invalid or data already sent. Do nothing */
            }

            /* end of parsing, clear buffer */
            memset(mqttClient.topicList[i].rxBuffer, 0x00, mqttClient.topicList[i].msgSize);
        }
        app.ppMsg.msgAvailable = false;
    }
}

static void gnssLocationPrint(void)
{
    esp_err_t err;
    bool hasMessage = xplrGnssHasMessage(gnssDvcPrfId);

    if (hasMessage) {
        err = xplrGnssPrintLocation(gnssDvcPrfId);
        if (err != ESP_OK) {
            APP_CONSOLE(W, "Could not print gnss location!");
        }

        err = xplrGnssPrintGmapsLocation(0);
        if (err != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }
    }
}

static void appInit(void)
{
    app.state[0] = APP_FSM_INIT_HW;
    timerInit();
    app.state[0] = APP_FSM_INIT_PERIPHERALS;
}

static app_error_t appTerminate(void)
{
    app_error_t ret;
    esp_err_t gnssErr;
    xplrCell_mqtt_error_t err;
    err = xplrCellMqttUnsubscribeFromTopicList(cellConfig.profileIndex, mqttClient.id);
    if (err != XPLR_CELL_MQTT_OK) {
        ret = APP_ERROR_MQTT_CLIENT;
    } else {
        err = xplrCellMqttDisconnect(cellConfig.profileIndex, mqttClient.id);
        if (err != XPLR_CELL_MQTT_OK) {
            ret = APP_ERROR_MQTT_CLIENT;
        } else {
            gnssErr = xplrGnssStopDevice(gnssDvcPrfId);
            if (gnssErr != ESP_OK) {
                APP_CONSOLE(E, "App could not stop gnss device.");
                ret = APP_ERROR_GNSS_INIT;
            } else {
                ret = APP_ERROR_OK;
            }
        }
    }

    APP_CONSOLE(I, "App MQTT Statistics.");
    APP_CONSOLE(D, "Messages Received: %d.", app.stats.msgReceived);
    APP_CONSOLE(D, "Bytes Received: %d.", app.stats.bytesReceived);
    APP_CONSOLE(D, "Uptime: %d seconds.", app.stats.time);
    APP_CONSOLE(W, "App disconnected the MQTT client.");

    return ret;
}

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
