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
 * An example for board initialization and info printing
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig by choosing the appropriate board from the menu,
 * uses boards component to initialize the devkit and display information
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "esp_task_wdt.h"
#include "xplr_common.h"
#include "xplr_gnss.h"
#include "xplr_at_parser.h"
#include "xplr_wifi_starter.h"
#include "xplr_mqtt.h"

#if defined(CONFIG_BOARD_XPLR_HPG2_C214)
#define XPLR_BOARD_SELECTED_IS_C214
#elif defined(CONFIG_BOARD_XPLR_HPG1_C213)
#define XPLR_BOARD_SELECTED_IS_C213
#elif defined(CONFIG_BOARD_MAZGCH_HPG_SOLUTION)
#define XPLR_BOARD_SELECTED_IS_MAZGCH
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/mqttClient_service/xplr_mqtt_client.h"
#include "./../../../components/hpglib/src/thingstream_service/xplr_thingstream.h"
#include "./../../../components/hpglib/src/ntripWiFiClient_service/xplr_wifi_ntrip_client.h"
#include "./../../../components/hpglib/src/ntripCellClient_service/xplr_cell_ntrip_client.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

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

/**
 * Time in seconds to trigger an inactivity timeout and cause a restart
 */
#define APP_INACTIVITY_TIMEOUT 30

/*
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR        (0x42)

/*
 * LBAND I2C address in hex
 */
#define APP_LBAND_I2C_ADDR       (0x43)

/**
 * Option to enable the correction message watchdog mechanism.
 * When set to 1, if no correction data are forwarded to the GNSS
 * module (either via IP or SPARTN) for a defined amount of time
 * (MQTT_MESSAGE_TIMEOUT macro defined in hpglib/xplr_mqtt/include/xplr_mqtt.h)
 * an error event will be triggered
*/
#define APP_ENABLE_CORR_MSG_WDG (1U)

/**
 * Trigger soft reset if device in error state
 */
#define APP_RESTART_ON_ERROR     (1U)

/*
 * Simple macros used to set buffer size in bytes
 */
#define KIB (1024U)
#define APP_MQTT_BUFFER_SIZE ((10U) * (KIB))

#define APP_MAX_TOPICLEN 64
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog         : 1;
        uint8_t nvsLog         : 1;
        uint8_t mqttLog        : 1;
        uint8_t gnssLog        : 1;
        uint8_t gnssAsyncLog   : 1;
        uint8_t lbandLog       : 1;
        uint8_t locHelperLog   : 1;
        uint8_t thingstreamLog : 1;
        uint8_t wifiStarterLog : 1;
        uint8_t comLog         : 1;
        uint8_t ntripLog       : 1;
        uint8_t atParserLog    : 1;
        uint8_t atServerLog    : 1;
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
    int8_t          comLogIndex;
    int8_t          ntripLogIndex;
    int8_t          atParserLogIndex;
    int8_t          atServerLogIndex;
} appLog_t;

/* cell application states */
typedef enum {
    APP_FSM_INACTIVE = -2,
    APP_FSM_ERROR,
    APP_FSM_SET_GREETING_MESSAGE,
    APP_FSM_INIT_NTRIP_CLIENT,
    APP_FSM_INIT_CELL,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_THINGSTREAM_INIT,
    APP_FSM_INIT_MQTT_CLIENT,
    APP_FSM_RUN,
    APP_FSM_MQTT_DISCONNECT,
    APP_FSM_TERMINATE,
} app_cell_fsm_t;

/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -7,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
    APP_ERROR_MQTT_CLIENT,
    APP_ERROR_NETWORK_OFFLINE,
    APP_ERROR_THINGSTREAM,
    APP_ERROR_NTRIP,
    APP_ERROR_INVALID_PLAN,
    APP_ERROR_OK = 0,
} app_cell_error_t;

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

xplr_at_server_uartCfg_t uartCfg;
xplr_at_parser_t *profile;

static xplrWifiStarterOpts_t wifiOptions;
static xplrWifiStarterError_t wifistarterErr;

static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClientWifi;
static xplrCell_mqtt_client_t mqttClientCell;
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
static xplr_thingstream_t *thingstreamSettings;
static xplrCell_mqtt_topic_t topics[3];
char rxBuff[2][APP_MQTT_BUFFER_SIZE];
static app_cell_fsm_t appCellState[2];

static bool isWifiInit = false;
static bool isMqttWifiInit = false;
static bool isCellInit = false;
static bool isNtripWifiInit = false;
static bool isCellNtripInit = false;
static bool isLbandAsyncInit = false;
static bool cellInitAfterPowerDown = false;
static bool mqttSessionDisconnected = false;
static bool mqttMsgAvailable = false;
static bool cellHasRebooted = false;
static bool isRstControlled = false;
static bool cellMqttMsgAvailable = false;
static bool cellKeyDistribution = 0;
static bool cellCorrectionData = 0;
static bool cellLbandFrequency = 0;

static xplr_thingstream_pp_plan_t prevThingstreamPlan = XPLR_THINGSTREAM_PP_PLAN_INVALID;
// *INDENT-OFF*
static xplr_at_parser_correction_mod_type_t prevCorrectionMod = XPLR_ATPARSER_CORRECTION_MOD_INVALID;
// *INDENT-ON*

const char brokerName[] = "Thingstream";
const char rootName[] = "rootPp.crt"; /* name of root ca as stored in cellular module */
const char certName[] = "mqttPp.crt"; /* name of mqtt cert as stored in cellular module */
const char keyName[] = "mqttPp.key";  /* name of mqtt key as stored in cellular module */
/* md5 hash of certificates used, leave empty for overwriting the certificate*/
const char rootHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; //" ";
const char certHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; //" ";
const char keyHash[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";  //" ";
const char *ztpToken = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";

const char cellGreetingMessage[] = "LARA JUST WOKE UP";
int32_t cellReboots = 0;    /**< Count of total reboots of the cellular module */
static bool failedRecover = false;

static uint64_t gnssLastAction;

static xplrGnssDeviceCfg_t dvcGnssConfig;
static xplrLbandDeviceCfg_t dvcLbandConfig;
const uint8_t gnssDvcPrfId = 0;
const uint8_t lbandDvcPrfId = 0;

/**
 * Frequency read from LBAND module
 */
static uint32_t frequency;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/**
 * Boolean flags for different functions
 */
static bool requestDc;
/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;

/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;
static xplr_at_parser_hpg_status_type_t currentStatus = XPLR_ATPARSER_STATHPG_INIT;

static xplrMqttWifiPayload_t mqttMessage = {
    .data = rxBuff[0],
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = APP_MQTT_BUFFER_SIZE
};

xplrWifi_ntrip_client_t ntripWifiClient;
xplrCell_ntrip_client_t ntripCellClient;
SemaphoreHandle_t ntripSemaphore;
uint32_t ntripSize;
char GgaMsg[256];
char *ntripGgaMsgPtr = GgaMsg;

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
    .comLogIndex = -1,
    .ntripLogIndex = -1,
    .atParserLogIndex = -1,
    .atServerLogIndex = -1
};

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
static void appInitLocationDevices(void);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
static void appRestartGnssDevices(void);
static void configCellSettings(xplrCom_cell_config_t *cfg);
static void appInitAtParser(void);
static void appDeInitAtParser(void);
static void appWaitGnssReady(void);
static void appInitWiFi(void);
static void appMqttInit(void);
static void appWifiStarterFsm(void);
static esp_err_t appInitBoard(void);
static void appSetGnssDestinationHandler(void);
static void appUnsetGnssDestinationHandler(void);
#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
static void appDeInitLogging(void);
#endif
static void appStopWifiMqtt(void);
static void appTerminate(void);
static void appHaltExecution(void);
static app_cell_error_t cellNetworkRegister(void);
static app_cell_error_t cellSetGreeting(void);
static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam);
static void appCellFsm(void);
static app_cell_error_t thingstreamInit(const char *token, xplr_thingstream_t *instance);
static app_cell_error_t appCellMqttClientInit(void);
static app_cell_error_t appCellMqttClientMsgUpdate(void);
static void appCellgnssFwdPpData(void);
static app_cell_error_t appCellRestart(void);
static app_cell_error_t appCellNetworkConnected(void);
static void appConfigCellMqttSettings(xplrCell_mqtt_client_t *client);
static void mqttMsgReceivedCallback(int32_t numUnread, void *received);
static void mqttDisconnectCallback(int32_t status, void *cbParam);
static app_cell_error_t appStopCell(void);
static void appNtripWifiInit(void);
static void appNtripWifiDeInit(void);
static void appNtripWifiFsm(void);
static void appNtripCellFsm(void);
static app_cell_error_t appNtripCellInit(void);
static app_cell_error_t appNtripCellDeInit(void);
static void configureCorrectionSource();
static void initializeTsConfig();

void app_main(void)
{
    esp_err_t espRet;
    app_cell_error_t appCellError;

    gnssLastAction = esp_timer_get_time();
    appInitBoard();
    appInitAtParser();
    APP_CONSOLE(I, "Done initializing AT Parser module");

#if (APP_SD_LOGGING_ENABLED == 1)
    if (profile->data.misc.sdLogEnable == true) {
        espRet = appInitLogging();
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Logging failed to initialize");
        } else {
            APP_CONSOLE(I, "Logging initialized!");
        }
    } else {
        // do nothing
    }
#endif

    appInitLocationDevices();
    APP_CONSOLE(I, "Done initializing Location Devices");
    appWaitGnssReady();
    APP_CONSOLE(I, "GNSS ready");

    // get GNSS device info for BRDNFO command
    espRet = xplrGnssGetDeviceInfo(gnssDvcPrfId, &profile->data.dvcInfoGnss);
    if (espRet == ESP_OK) {
        //do nothing
    } else {
        APP_CONSOLE(E, "Failed getting GNSS device info!");
    }

    espRet = xplrLbandGetDeviceInfo(lbandDvcPrfId, &profile->data.dvcInfoLband);
    if (espRet == ESP_OK) {
        //do nothing
    } else {
        APP_CONSOLE(E, "Failed getting LBAND device info!");
    }

    while (1) {
        if (profile->data.restartSignal == true) {
            esp_restart();
        }

        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                gnssLastAction = esp_timer_get_time();
                break;

            case XPLR_GNSS_STATE_ERROR:
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_GNSS, XPLR_ATPARSER_STATUS_ERROR);
                APP_CONSOLE(E, "GNSS in error state");
                appHaltExecution();
                break;

            default:
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
                }
                break;
        }

        espRet = xplrGnssGetLocationData(gnssDvcPrfId, &profile->data.location);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
        } else {
            //do nothing
        }
#if 0
        xplrMemUsagePrint(10);
#endif
        xplrAtParserStatusUpdate(currentStatus, 1);
        switch (profile->data.mode) {
            case XPLR_ATPARSER_MODE_START:
                if ((profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_WIFI) &&
                    (profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM)) {
                    //connect using wifi and thingstream
                    if ((isMqttWifiInit == false) &&
                        ((xplrAtParserWifiIsReady() == false) ||
                         (xplrAtParserTsIsReady() == false))) {
                        profile->data.mode = XPLR_ATPARSER_MODE_CONFIG;
                        if (setDeviceModeBusyStatus(false) != XPLR_AT_PARSER_OK) {
                            APP_CONSOLE(E, "Error setting app device mode");
                        }
                        break;
                    } else {
                        configureCorrectionSource();
                    }
                    if (isWifiInit == false) {
                        appInitWiFi();
                        isWifiInit = true;
                    }
                    if (isMqttWifiInit == false) {
                        (void)xplrMqttWifiInitState(&mqttClientWifi);
                        currentStatus = XPLR_ATPARSER_STATHPG_WIFI_INIT;
                        isMqttWifiInit = true;
                    }
                    appWifiStarterFsm();
                } else if ((profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_CELL) &&
                           (profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM)) {
                    //connect using cell and thingstream
                    if ((isMqttWifiInit == false) &&
                        ((xplrAtParserCellIsReady() == false) ||
                         (xplrAtParserTsIsReady() == false))) {
                        profile->data.mode = XPLR_ATPARSER_MODE_CONFIG;
                        if (setDeviceModeBusyStatus(false) != XPLR_AT_PARSER_OK) {
                            APP_CONSOLE(E, "Error setting app device mode");
                        }
                        break;
                    } else {
                        configureCorrectionSource();
                    }
                    if (isCellInit == false) {
                        if (cellInitAfterPowerDown == true) {
                            xplrComCellPowerResume(cellConfig.profileIndex);
                        }
                        appCellState[0] = APP_FSM_INIT_CELL;
                        isCellInit = true;
                    }
                    appCellFsm();
                } else if ((profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_WIFI) &&
                           (profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP)) {
                    //connect using wifi and ntrip
                    if ((isMqttWifiInit == false) &&
                        ((xplrAtParserWifiIsReady() == false) ||
                         (xplrAtParserNtripIsReady() == false))) {
                        profile->data.mode = XPLR_ATPARSER_MODE_CONFIG;
                        if (setDeviceModeBusyStatus(false) != XPLR_AT_PARSER_OK) {
                            APP_CONSOLE(E, "Error setting app device mode");
                        }
                        break;
                    } else {
                        configureCorrectionSource();
                    }
                    if (isWifiInit == false) {
                        appInitWiFi();
                        isWifiInit = true;
                    }
                    appNtripWifiFsm();
                } else if ((profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_CELL) &&
                           (profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP)) {
                    //connect using cell and ntrip
                    if ((isMqttWifiInit == false) &&
                        ((xplrAtParserCellIsReady() == false) ||
                         (xplrAtParserNtripIsReady() == false))) {
                        profile->data.mode = XPLR_ATPARSER_MODE_CONFIG;
                        if (setDeviceModeBusyStatus(false) != XPLR_AT_PARSER_OK) {
                            APP_CONSOLE(E, "Error setting app device mode");
                        }
                        break;
                    } else {
                        configureCorrectionSource();
                    }
                    if (isCellNtripInit == false) {
                        // call xplrComCellPowerResume after xplrComCellPowerDown
                        if (cellInitAfterPowerDown == true) {
                            xplrComCellPowerResume(cellConfig.profileIndex);
                        }
                        appCellState[0] = APP_FSM_INIT_CELL;
                        isCellNtripInit = true;
                    }
                    appNtripCellFsm();
                }
                break;
            case XPLR_ATPARSER_MODE_STOP:
                if (isMqttWifiInit == true) {
                    currentStatus = XPLR_ATPARSER_STATHPG_STOP;
                    appStopWifiMqtt();
                    isMqttWifiInit = false;
                } else if (isCellInit == true) {
                    currentStatus = XPLR_ATPARSER_STATHPG_STOP;
                    appCellError = appStopCell();
                    if (appCellError != APP_ERROR_OK) {
                        APP_CONSOLE(I, "Error deinitializing mqtt cell subsystem");
                    }
                    isCellInit = false;
                } else if (isNtripWifiInit == true) {
                    currentStatus = XPLR_ATPARSER_STATHPG_STOP;
                    appNtripWifiDeInit();
                    isNtripWifiInit = false;
                } else if (isCellNtripInit == true) {
                    currentStatus = XPLR_ATPARSER_STATHPG_STOP;
                    appCellError = appNtripCellDeInit();
                    if (appCellError != APP_ERROR_OK) {
                        APP_CONSOLE(I, "Error deinitializing ntrip cell subsystem");
                    }
                    isCellNtripInit = false;
                }
                if (isLbandAsyncInit == true) {
                    appUnsetGnssDestinationHandler();
                    isLbandAsyncInit = false;
                }
                break;
            case XPLR_ATPARSER_MODE_CONFIG:
                currentStatus = XPLR_ATPARSER_STATHPG_CONFIG;
                break;
            case XPLR_ATPARSER_MODE_INVALID:
                break;
            case XPLR_ATPARSER_MODE_NOT_SET:
                break;
            case XPLR_ATPARSER_MODE_ERROR:
                currentStatus = XPLR_ATPARSER_STATHPG_ERROR;
                if (setDeviceModeBusyStatus(false) != XPLR_AT_PARSER_OK) {
                    APP_CONSOLE(E, "Error setting app device mode");
                }
                break;
        }
        vTaskDelay(25);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

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

    gnssCfg->dr.enable = profile->data.misc.dr.enable;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;

    if (isLbandAsyncInit) {
        gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_LBAND;
    } else {
        gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
    }
}

static void appInitLocationDevices(void)
{
    esp_err_t espRet;

    /**
     * Init ubxlib
     */
    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    appConfigGnssSettings(&dvcGnssConfig);
    espRet = xplrGnssStartDevice(gnssDvcPrfId, &dvcGnssConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Waiting for LBAND device to come online!");
    appConfigLbandSettings(&dvcLbandConfig);
    espRet = xplrLbandStartDevice(gnssDvcPrfId, &dvcLbandConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "LBAND device config failed!\n");
        appHaltExecution();
    }

    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_GNSS, XPLR_ATPARSER_STATUS_INIT);
    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

static void appRestartGnssDevices(void)
{
    esp_err_t espRet;

    espRet = xplrGnssStopDevice(gnssDvcPrfId);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "App could not stop gnss device.");
    } else {
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
        while (gnssState != XPLR_GNSS_STATE_UNCONFIGURED) {
            xplrGnssFsm(gnssDvcPrfId);
            gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
            if (gnssState == XPLR_GNSS_STATE_UNCONFIGURED) {
                APP_CONSOLE(D, "GNSS device stopped successfully");
            }
        }
        appConfigGnssSettings(&dvcGnssConfig);
        espRet = xplrGnssStartDevice(gnssDvcPrfId, &dvcGnssConfig);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to start GNSS device!");
            appHaltExecution();
        }
    }

    espRet = xplrLbandStopDevice(gnssDvcPrfId);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Could not stop LBAND device!\n");
        appHaltExecution();
    } else {
        APP_CONSOLE(I, "Waiting for LBAND device to come online!");
        appConfigLbandSettings(&dvcLbandConfig);
        espRet = xplrLbandStartDevice(gnssDvcPrfId, &dvcLbandConfig);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "LBAND device config failed!\n");
            appHaltExecution();
        }
    }
}

static void appInitAtParser(void)
{
    uartCfg.uart = 0;
    uartCfg.baudRate = 115200;
    uartCfg.pinTxd = BOARD_IO_UART_DBG_TX;
    uartCfg.pinRxd = BOARD_IO_UART_DBG_RX;
    uartCfg.rxBufferSize = 2048;
    profile = xplrAtParserInit(&uartCfg);
    if (profile == NULL) {
        APP_CONSOLE(E, "Error initializing AT parser");
    } else {
        thingstreamSettings = &profile->data.correctionData.thingstreamCfg.thingstream;
    }

    if (xplrAtParserAdd(XPLR_ATPARSER_ALL) != XPLR_AT_PARSER_OK) {
        APP_CONSOLE(E, "Error adding At command parser");
    }

    if (xplrAtParserLoadNvsConfig() != XPLR_AT_PARSER_OK) {
        APP_CONSOLE(W, "Some AT Parser configuration failed to load from NVS");
    } else {
        //do nothing
    }

    // check startOnBoot variable and set mode
    if (profile->data.startOnBoot == true) {
        profile->data.mode = XPLR_ATPARSER_MODE_START;
    } else {
        //do nothing
    }
}

static void appDeInitAtParser(void)
{
    xplrAtParserRemove(XPLR_ATPARSER_ALL);
    xplrAtParserDeInit();
}

static void appWaitGnssReady(void)
{
    gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
    gnssLastAction = esp_timer_get_time();
    while (gnssState != XPLR_GNSS_STATE_DEVICE_READY) {
        if (gnssState == XPLR_GNSS_STATE_ERROR) {
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_GNSS, XPLR_ATPARSER_STATUS_ERROR);
            APP_CONSOLE(E, "GNSS in error state");
            appHaltExecution();
        } else {
            xplrGnssFsm(gnssDvcPrfId);
            gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
            if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                appTerminate();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(25));  /* A window so other tasks can run */
    }
    gnssLastAction = esp_timer_get_time();
    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_GNSS, XPLR_ATPARSER_STATUS_READY);
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    static esp_err_t espRet;
    xplrWifiStarterError_t errorWifi;

    wifiOptions.ssid = profile->data.net.ssid;
    wifiOptions.password = profile->data.net.password;
    wifiOptions.mode = XPLR_WIFISTARTER_MODE_STA;
    wifiOptions.webserver = false;

    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    }
    errorWifi = xplrWifiStarterDeviceForceSaveWifi();
    if (errorWifi != XPLR_WIFISTARTER_OK) {
        APP_CONSOLE(E, "Error saving wifi credentials!");
    } else {
        // do nothing
    }
    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_INIT);
}

static void appWifiStarterFsm(void)
{
    xplrMqttWifiError_t mqttErr;
    esp_err_t espRet;
    app_cell_error_t tsError;
    bool topicFound[3];

    wifistarterErr = xplrWifiStarterFsm();

    if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
        if (xplrMqttWifiGetCurrentState(&mqttClientWifi) == XPLR_MQTTWIFI_STATE_UNINIT ||
            xplrMqttWifiGetCurrentState(&mqttClientWifi) == XPLR_MQTTWIFI_STATE_DISCONNECTED_OK) {
            tsError = thingstreamInit(NULL, thingstreamSettings);
            if (tsError != APP_ERROR_OK) {
                APP_CONSOLE(E, "Thingstream module initialization failed!");
                appHaltExecution();
            } else {
                // do nothing
            }
            appMqttInit();
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_CONNECTING);
            xplrMqttWifiStart(&mqttClientWifi);
            requestDc = false;
        }
    }

    mqttErr = xplrMqttWifiFsm(&mqttClientWifi);
    if (mqttErr == XPLR_MQTTWIFI_ERROR) {
        APP_CONSOLE(E, "Error in xplrMqttWifiFsm!");
    } else {
        //do nothing
    }
    gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

    switch (xplrMqttWifiGetCurrentState(&mqttClientWifi)) {
        /**
         * Subscribe to some topics
         * We subscribe after the GNSS device is ready. This way we do not
         * lose the first message which contains the decryption keys
         */
        case XPLR_MQTTWIFI_STATE_CONNECTED:
            currentStatus = XPLR_ATPARSER_STATHPG_WIFI_CONNECTED;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_CONNECTED);
            if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                gnssLastAction = esp_timer_get_time();
                espRet = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClientWifi,
                                                              &thingstreamSettings->pointPerfect);
                if (espRet != ESP_OK) {
                    APP_CONSOLE(E, "Subscribing to topics failed!");
                    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
                    appHaltExecution();
                } else {
                    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_CONNECTED);
                }
            } else {
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
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
            if (xplrMqttWifiReceiveItem(&mqttClientWifi, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                topicFound[0] = xplrThingstreamPpMsgIsKeyDist(mqttMessage.topic, thingstreamSettings);
                topicFound[1] = xplrThingstreamPpMsgIsCorrectionData(mqttMessage.topic, thingstreamSettings);
                topicFound[2] = xplrThingstreamPpMsgIsFrequency(mqttMessage.topic, thingstreamSettings);
                /**
                 * Do not send data if GNSS is not in ready state.
                 * The device might not be initialized and the device handler
                 * would be NULL
                 */
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    gnssLastAction = esp_timer_get_time();
                    currentStatus = XPLR_ATPARSER_STATHPG_TS_CONNECTED;
                    if (topicFound[0]) {
                        espRet = xplrGnssSendDecryptionKeys(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to send decryption keys!");
                            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
                            appHaltExecution();
                        }
                    }
                    if (topicFound[1] && !isLbandAsyncInit) {
                        espRet = xplrGnssSendCorrectionData(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to send correction data!");
                            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
                        }
                    }
                    if (topicFound[2] && isLbandAsyncInit) {
                        espRet = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                               mqttMessage.data,
                                                               dvcLbandConfig.corrDataConf.region);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to set frequency!");
                            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
                            appHaltExecution();
                        } else {
                            frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                            if (frequency == 0) {
                                APP_CONSOLE(I, "No LBAND frequency is set");
                                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
                            }
                            APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
                        }
                    }
                } else {
                    if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) >= APP_INACTIVITY_TIMEOUT) {
                        appTerminate();
                    }
                }
            }
            break;

        default:
            break;
    }

    /**
     * Check if any LBAND messages have been forwarded to the GNSS module
     * and if there are feed the MQTT module's watchdog.
    */
    if (xplrLbandHasFrwdMessage()) {
        xplrMqttWifiFeedWatchdog(&mqttClientWifi);
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
        if (mqttClientWifi.handler != NULL) {
            if (mqttClientWifi.handler != NULL) {
                xplrMqttWifiHardDisconnect(&mqttClientWifi);
                initializeTsConfig();
            }
            requestDc = true;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_RECONNECTING);
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_RECONNECTING);
        }
    }
}

/**
 * Populate MQTT Wi-Fi client settings
 */
static void appMqttInit(void)
{
    esp_err_t ret;
    xplr_at_parser_thingstream_config_t *tsConfig = &profile->data.correctionData.thingstreamCfg;

    mqttClientWifi.ucd.enableWatchdog = (bool) APP_ENABLE_CORR_MSG_WDG;

    /**
     * We declare how many slots a ring buffer should have.
     * You can chose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    ret = xplrMqttWifiSetRingbuffSlotsCount(&mqttClientWifi, 6);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set MQTT ringbuffer slots!");
        appHaltExecution();
    }

    // reload certs from NVS in case they were overwritten by previous execution
    (void)xplrAtParserLoadNvsTsCerts();

    /**
     * Settings for mqtt client
     */
    // Concatenate mqtt url prefix, needed for wifi mqttClientConfig
    strcpy(tsConfig->thingstream.server.serverUrl, "mqtts://");
    strcat(tsConfig->thingstream.server.serverUrl, tsConfig->thingstream.pointPerfect.brokerAddress);
    mqttClientConfig.uri = tsConfig->thingstream.server.serverUrl;
    mqttClientConfig.client_id = tsConfig->thingstream.pointPerfect.deviceId;

    ret = xplrPpConfigFileFormatCert(tsConfig->thingstream.pointPerfect.clientCert,
                                     XPLR_COMMON_CERT,
                                     true);
    ret |= xplrPpConfigFileFormatCert(tsConfig->thingstream.pointPerfect.clientKey,
                                      XPLR_COMMON_CERT_KEY,
                                      true);
    ret |= xplrPpConfigFileFormatCert(tsConfig->thingstream.server.rootCa,
                                      XPLR_COMMON_CERT,
                                      true);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to format certificate");
        appHaltExecution();
    }

    mqttClientConfig.client_cert_pem = tsConfig->thingstream.pointPerfect.clientCert;
    mqttClientConfig.client_key_pem = tsConfig->thingstream.pointPerfect.clientKey;
    mqttClientConfig.cert_pem = tsConfig->thingstream.server.rootCa;

    mqttClientConfig.user_context = &mqttClientWifi.ucd;

    /**
     * Start MQTT Wi-Fi Client
     */
    xplrMqttWifiInitClient(&mqttClientWifi, &mqttClientConfig);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to initialize Mqtt client!");
        appHaltExecution();
    }
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
    * module connected via UART
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
    switch (profile->data.correctionData.thingstreamCfg.tsRegion) {
        case XPLR_THINGSTREAM_PP_REGION_EU:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
            break;
        case XPLR_THINGSTREAM_PP_REGION_US:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_US;
            break;
        default:
            lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
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
    cfg->netSettings->pApn = profile->data.net.apn;
    cfg->netSettings->timeoutSeconds = 240; /* Connection timeout in seconds */
    cfg->mno = 90;

    cfg->ratList[0] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[1] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[2] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    cfg->bandList[0] = 0;
    cfg->bandList[1] = 0;
    cfg->bandList[2] = 0;
    cfg->bandList[3] = 0;
    cfg->bandList[4] = 0;
    cfg->bandList[5] = 0;
}

static app_cell_error_t cellNetworkRegister(void)
{
    app_cell_error_t ret;
    xplrCom_error_t err;
    xplrCom_cell_connect_t comState;

    xplrComCellFsmConnect(cellConfig.profileIndex);
    comState = xplrComCellFsmConnectGetState(cellConfig.profileIndex);

    //get Cell module info
    err = xplrComCellGetDeviceInfo(cellConfig.profileIndex,
                                   profile->data.cellInfo.cellModel,
                                   profile->data.cellInfo.cellFw,
                                   profile->data.cellInfo.cellImei);
    if (err != XPLR_COM_OK) {
        APP_CONSOLE(E, "Error getting cell device info!");
    } else {
        //do nothing
    }

    switch (comState) {
        case XPLR_COM_CELL_CONNECTED:
            APP_CONSOLE(I, "Cell module is Online.");
#if 0 //Delete stored certificates
            appConfigCellMqttSettings(&mqttClientCell);
            hpgCellMqttRes = xplrCellMqttInit(cellConfig.profileIndex, 0, &mqttClientCell);
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

static app_cell_error_t cellSetGreeting(void)
{
    app_cell_error_t ret;
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

static void cellGreetingCallback(uDeviceHandle_t handler, void *callbackParam)
{
    int32_t *param = (int32_t *) callbackParam;

    (void) handler;
    (*param)++;
    cellHasRebooted = true;
}

static void appCellFsm(void)
{
    static app_cell_error_t appCellError;
    xplrCom_error_t err;

    switch (appCellState[0]) {
        case APP_FSM_INIT_CELL:
            /*
             * Config Cell module
             */
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_INIT);
            configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
            err = xplrComCellInit(&cellConfig); /* Initialize hpg com */
            if (err != XPLR_COM_OK) {
                APP_CONSOLE(E, "Error initializing hpg com!");
                appCellState[0] = APP_FSM_ERROR;
            } else {
                appCellState[0] = APP_FSM_CHECK_NETWORK;
            }
            break;
        case APP_FSM_CHECK_NETWORK:
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_CONNECTING);
            appCellState[1] = appCellState[0];
            appCellError = cellNetworkRegister();
            if (appCellError == APP_ERROR_OK) {
                appCellState[0] = APP_FSM_SET_GREETING_MESSAGE;
                currentStatus = XPLR_ATPARSER_STATHPG_CELL_CONNECTED;
            } else if (appCellError == APP_ERROR_NETWORK_OFFLINE) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                /* module still trying to connect. do nothing */
            }
            break;
        case APP_FSM_SET_GREETING_MESSAGE:
            appCellState[1] = appCellState[0];
            appCellError = cellSetGreeting();
            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                appCellState[0] = APP_FSM_THINGSTREAM_INIT;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_CONNECTED);
            }
            break;
        case APP_FSM_THINGSTREAM_INIT:
            appCellState[1] = appCellState[0];
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_INIT);
            appCellError = thingstreamInit(NULL, thingstreamSettings);
            if (appCellError == APP_ERROR_OK) {
                appCellState[0] = APP_FSM_INIT_MQTT_CLIENT;
            } else if (appCellError == APP_ERROR_NETWORK_OFFLINE) {
                appCellState[0] = APP_FSM_ERROR;
            } else if (appCellError == APP_ERROR_INVALID_PLAN) {
                appCellState[0] = APP_FSM_TERMINATE;
            } else {
                /* module still trying to connect. do nothing */
            }
            break;
        case APP_FSM_INIT_MQTT_CLIENT:
            appCellState[1] = appCellState[0];
            appCellError = appCellMqttClientInit();
            if (appCellError == APP_ERROR_OK) {
                appCellState[0] = APP_FSM_RUN;
                currentStatus = XPLR_ATPARSER_STATHPG_TS_CONNECTED;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_CONNECTED);
            } else {
                appCellState[0] = APP_FSM_ERROR;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
            }
            break;
        case APP_FSM_RUN:
            appCellState[1] = appCellState[0];

            /* check for new messages */
            if ((appCellError == APP_ERROR_OK) && (gnssState == XPLR_GNSS_STATE_DEVICE_READY)) {
                gnssLastAction = esp_timer_get_time();
                appCellError = appCellMqttClientMsgUpdate();
                if (appCellError != APP_ERROR_OK) {
                    appCellState[0] = APP_FSM_MQTT_DISCONNECT;
                } else {
                    /* fwd msg to GNSS */
                    appCellgnssFwdPpData();
                    /* Check for mqtt disconnect flag */
                    if (mqttSessionDisconnected) {
                        appCellState[0] = APP_FSM_MQTT_DISCONNECT;
                    }
                }

                /* If LBAND module has forwarded messages then feed MQTT watchdog (if enabled) */
                if (xplrLbandHasFrwdMessage()) {
                    xplrCellMqttFeedWatchdog(cellConfig.profileIndex, mqttClientCell.id);
                }
            }
            break;
        case APP_FSM_MQTT_DISCONNECT:
            appCellState[1] = appCellState[0];
            /* De-init mqtt client */
            xplrCellMqttDeInit(cellConfig.profileIndex, mqttClientCell.id);
            initializeTsConfig();

            /* Reboot cell */
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_RECONNECTING);
            appCellError = appCellRestart();
            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_TERMINATE;
            } else {
                appCellState[0] = APP_FSM_CHECK_NETWORK;
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
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_READY);
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_READY);
            appCellState[1] = appCellState[0];
            appCellError = appStopCell();
            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_ERROR;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_ERROR);
            } else {
                appCellState[0] = APP_FSM_INACTIVE;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_READY);
            }
            break;
        case APP_FSM_INACTIVE:
            /* code */
            break;
        case APP_FSM_ERROR:
            APP_CONSOLE(E, "APP FSM ERROR");
            /* code */
            break;

        default:
            break;
    }
    if (cellHasRebooted && (appCellState[0] == APP_FSM_RUN)) {
        appCellState[1] = appCellState[0];
        isRstControlled = xplrComIsRstControlled(cellConfig.profileIndex);
        if (isRstControlled) {
            APP_CONSOLE(I, "Controlled LARA restart triggered");
            isRstControlled = false;
        } else {
            APP_CONSOLE(W, "Uncontrolled LARA restart triggered");
            appCellState[0] = APP_FSM_MQTT_DISCONNECT;
        }
        cellHasRebooted = false;
        APP_CONSOLE(W, "Cell Module has rebooted! Number of total reboots: <%d>", cellReboots);
    }
}

static app_cell_error_t thingstreamInit(const char *token, xplr_thingstream_t *instance)
{
    app_cell_error_t ret;
    xplr_thingstream_error_t err;

    /* initialize thingstream instance with dummy token */
    if (profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_WIFI) {
        instance->connType = XPLR_THINGSTREAM_PP_CONN_WIFI;
    } else if (profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_CELL) {
        instance->connType = XPLR_THINGSTREAM_PP_CONN_CELL;
    } else {
        instance->connType = XPLR_THINGSTREAM_PP_CONN_INVALID;
    }
    err = xplrThingstreamInit(ztpToken, instance);

    if (err != XPLR_THINGSTREAM_OK) {
        ret = APP_ERROR_THINGSTREAM;
    } else {
        /* Config thingstream topics according to region and subscription plan*/
        err = xplrThingstreamPpConfigTopics(profile->data.correctionData.thingstreamCfg.tsRegion,
                                            profile->data.correctionData.thingstreamCfg.tsPlan,
                                            isLbandAsyncInit,
                                            instance);
        if (err == XPLR_THINGSTREAM_OK) {
            if (profile->data.net.interface == XPLR_ATPARSER_NET_INTERFACE_CELL) {
                for (uint8_t i = 0; i < instance->pointPerfect.numOfTopics; i++) {
                    topics[i].index = i;
                    topics[i].name = instance->pointPerfect.topicList[i].path;
                    topics[i].rxBuffer = &rxBuff[i][0];
                    topics[i].rxBufferSize = APP_MQTT_BUFFER_SIZE;
                }
            } else {
                //do nothing
            }
            ret = APP_ERROR_OK;
        } else {
            ret = APP_ERROR_INVALID_PLAN;
        }
    }

    return ret;
}

static void initializeTsConfig()
{
    if (thingstreamSettings != NULL) {
        /* De-init thingstream struct-instance */
        memset(&thingstreamSettings->server.serverUrl, 0x00, XPLR_THINGSTREAM_URL_SIZE_MAX);
        memset(&thingstreamSettings->server.deviceId, 0x00, XPLR_THINGSTREAM_DEVICEUID_SIZE);
        memset(&thingstreamSettings->server.ppToken, 0x00, XPLR_THINGSTREAM_PP_TOKEN_SIZE);
        memset(&thingstreamSettings->pointPerfect.urlPath, 0x00, XPLR_THINGSTREAM_URL_SIZE_MAX);
        thingstreamSettings->pointPerfect.mqttSupported = 0;
        thingstreamSettings->pointPerfect.lbandSupported = 0;
        memset(&thingstreamSettings->pointPerfect.dynamicKeys, 0x00, sizeof(xplr_thingstream_pp_dKeys_t));
        memset(&thingstreamSettings->pointPerfect.topicList, 0x00, sizeof(xplr_thingstream_pp_topic_t));
        thingstreamSettings->pointPerfect.numOfTopics = 0;
        thingstreamSettings->connType = 0;
    } else {
        APP_CONSOLE(E, "NULL thingstream settings pointer!");
    }
}

static app_cell_error_t appCellMqttClientInit(void)
{
    app_cell_error_t ret;
    xplrCell_mqtt_error_t err;

    mqttClientCell.enableWdg = (bool) APP_ENABLE_CORR_MSG_WDG;

    ret = appCellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        appConfigCellMqttSettings(&mqttClientCell);
        err = xplrCellMqttInit(cellConfig.profileIndex, 0, &mqttClientCell);
        if (err == XPLR_CELL_MQTT_OK) {
            ret = APP_ERROR_OK;
        } else {
            ret = APP_ERROR_MQTT_CLIENT;
        }
    }

    return ret;
}

static app_cell_error_t appCellNetworkConnected(void)
{
    int8_t id = cellConfig.profileIndex;
    xplrCom_cell_connect_t comState;
    app_cell_error_t ret;

    xplrComCellFsmConnect(id);
    comState = xplrComCellFsmConnectGetState(id);
    if (comState == XPLR_COM_CELL_CONNECTED) {
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_NETWORK_OFFLINE;
    }

    return ret;
}

static app_cell_error_t appCellMqttClientMsgUpdate(void)
{
    const char *topicName;
    app_cell_error_t ret;
    xplrCell_mqtt_error_t err;

    ret = appCellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        err = xplrCellMqttFsmRun(cellConfig.profileIndex, mqttClientCell.id);
        if (err == XPLR_CELL_MQTT_ERROR) {
            ret = APP_ERROR_MQTT_CLIENT;
        } else if (err == XPLR_CELL_MQTT_BUSY) {
            ret = APP_ERROR_OK; /* skip */
        } else {
            /* check for new messages */
            if (mqttClientCell.fsm[0] == XPLR_CELL_MQTT_CLIENT_FSM_READY) {
                for (uint8_t msg = 0; msg < mqttClientCell.numOfTopics; msg++) {
                    if (mqttClientCell.topicList[msg].msgAvailable) {
                        mqttClientCell.topicList[msg].msgAvailable = false;
                        topicName = mqttClientCell.topicList[msg].name;
                        cellMqttMsgAvailable = true;
                        /* update app regarding msg type received */
                        if (xplrThingstreamPpMsgIsKeyDist(topicName, thingstreamSettings)) {
                            cellKeyDistribution = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <key distribution topic>.",
                                        topicName);
                        }  else if (xplrThingstreamPpMsgIsCorrectionData(topicName, thingstreamSettings)) {
                            cellCorrectionData = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <correction data topic>.",
                                        topicName);
                        } else if (xplrThingstreamPpMsgIsFrequency(topicName, thingstreamSettings)) {
                            cellLbandFrequency = 1;
                            APP_CONSOLE(D, "Topic name <%s> identified as <frequency distribution topic>.",
                                        topicName);
                        } else {
                            cellMqttMsgAvailable = false;
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

static void appConfigCellMqttSettings(xplrCell_mqtt_client_t *client)
{
    esp_err_t ret;
    xplr_at_parser_thingstream_config_t *tsConfig = &profile->data.correctionData.thingstreamCfg;

    // reload certs from NVS in case they were overwritten by previous execution
    (void)xplrAtParserLoadNvsTsCerts();

    strcpy(tsConfig->thingstream.server.serverUrl, tsConfig->thingstream.pointPerfect.brokerAddress);
    strcat(tsConfig->thingstream.server.serverUrl, ":");
    sprintf(tsConfig->thingstream.server.serverUrl + strlen(tsConfig->thingstream.server.serverUrl),
            "%d", tsConfig->thingstream.pointPerfect.brokerPort);
    client->settings.brokerAddress = tsConfig->thingstream.server.serverUrl;

    client->settings.qos = U_MQTT_QOS_AT_MOST_ONCE;
    client->settings.useFlexService = false;
    client->settings.retainMsg = false;
    client->settings.keepAliveTime = 60;
    client->settings.inactivityTimeout = client->settings.keepAliveTime * 2;

    client->credentials.registerMethod = XPLR_CELL_MQTT_CERT_METHOD_TLS;
    client->credentials.name = brokerName;
    client->credentials.user = NULL;
    client->credentials.password = NULL;
    client->credentials.token = tsConfig->thingstream.pointPerfect.deviceId;
    client->credentials.rootCaName = rootName;
    client->credentials.certName = certName;
    client->credentials.keyName = keyName;
    client->credentials.rootCaHash = rootHash;
    client->credentials.certHash = certHash;
    client->credentials.keyHash = keyHash;

    ret = xplrPpConfigFileFormatCert(tsConfig->thingstream.pointPerfect.clientCert,
                                     XPLR_COMMON_CERT,
                                     false);
    ret |= xplrPpConfigFileFormatCert(tsConfig->thingstream.pointPerfect.clientKey,
                                      XPLR_COMMON_CERT_KEY,
                                      false);
    ret |= xplrPpConfigFileFormatCert(tsConfig->thingstream.server.rootCa,
                                      XPLR_COMMON_CERT,
                                      false);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to format certificate");
        appHaltExecution();
    }

    client->credentials.cert = tsConfig->thingstream.pointPerfect.clientCert;
    client->credentials.key = tsConfig->thingstream.pointPerfect.clientKey;
    client->credentials.rootCa = tsConfig->thingstream.server.rootCa;

    client->numOfTopics = thingstreamSettings->pointPerfect.numOfTopics;
    client->topicList = topics;

    mqttClientCell.msgReceived = &mqttMsgReceivedCallback;
    mqttClientCell.disconnected = &mqttDisconnectCallback;
}

static void appCellgnssFwdPpData(void)
{
    bool topicFound[3];
    const char *topicName;
    esp_err_t  err;

    if (cellMqttMsgAvailable) {
        for (int i = 0; i < mqttClientCell.numOfTopics; i++) {
            topicName = mqttClientCell.topicList[i].name;
            topicFound[0] = xplrThingstreamPpMsgIsKeyDist(topicName, thingstreamSettings);
            topicFound[1] = xplrThingstreamPpMsgIsCorrectionData(topicName, thingstreamSettings);
            topicFound[2] = xplrThingstreamPpMsgIsFrequency(topicName, thingstreamSettings);

            if (topicFound[0] && cellKeyDistribution) {
                err = xplrGnssSendDecryptionKeys(gnssDvcPrfId,
                                                 mqttClientCell.topicList[i].rxBuffer,
                                                 mqttClientCell.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    cellKeyDistribution = 0;
                    APP_CONSOLE(D, "Decryption keys forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd decryption keys to GNSS module.");
                }

            } else if (topicFound[1] && cellCorrectionData && (!isLbandAsyncInit)) {
                err = xplrGnssSendCorrectionData(gnssDvcPrfId,
                                                 mqttClientCell.topicList[i].rxBuffer,
                                                 mqttClientCell.topicList[i].msgSize);
                if (err != ESP_FAIL) {
                    cellCorrectionData = 0;
                    APP_CONSOLE(D, "Correction data forwarded to GNSS module.");
                } else {
                    APP_CONSOLE(W, "Failed to fwd correction data to GNSS module.");
                }
            } else if (topicFound[2] && cellLbandFrequency && isLbandAsyncInit) {
                err = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                    mqttClientCell.topicList[i].rxBuffer,
                                                    dvcLbandConfig.corrDataConf.region);
                if (err == ESP_OK) {
                    cellLbandFrequency = 0;
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
            memset(mqttClientCell.topicList[i].rxBuffer, 0x00, mqttClientCell.topicList[i].msgSize);
        }
        cellMqttMsgAvailable = false;
        mqttSessionDisconnected = false;
        failedRecover = false;
    }
}

static app_cell_error_t appCellRestart(void)
{
    app_cell_error_t ret;
    xplrCom_error_t comErr;

    comErr = xplrComPowerResetHard(cellConfig.profileIndex);
    if (comErr == XPLR_COM_OK) {
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_NETWORK_OFFLINE;
    }
    return ret;
}

/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    esp_err_t espRet;

    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    } else {
        // do nothing
    }

    return espRet;
}

static void appSetGnssDestinationHandler(void)
{
    esp_err_t espRet;

    if (dvcLbandConfig.destHandler == NULL) {
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
}

static void appUnsetGnssDestinationHandler(void)
{
    esp_err_t ret;

    ret = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to stop Lband Async sender!");
    } else {
        //do nothing
    }
    dvcLbandConfig.destHandler = NULL;
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
                APP_CONSOLE(D, "LBAND service logging instance initialized");
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
            appLogCfg.wifiStarterLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            if (appLogCfg.wifiStarterLogIndex >= 0) {
                APP_CONSOLE(D, "Wifi starter service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.comLog == 1) {
            appLogCfg.comLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            if (appLogCfg.comLogIndex >= 0) {
                APP_CONSOLE(D, "Com service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.ntripLog == 1) {
            appLogCfg.ntripLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            if (appLogCfg.ntripLogIndex >= 0) {
                APP_CONSOLE(D, "Ntrip logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.atParserLog == 1) {
            appLogCfg.atParserLogIndex = xplrAtParserInitLogModule(NULL);
            if (appLogCfg.atParserLogIndex >= 0) {
                APP_CONSOLE(D, "AT Parser logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.atServerLog == 1) {
            appLogCfg.atServerLogIndex = xplrAtServerInitLogModule(NULL);
            if (appLogCfg.atServerLogIndex >= 0) {
                APP_CONSOLE(D, "AT Server logging instance initialized");
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

    /*#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
        vTaskDelete(cardDetectTaskHandler);
    #endif*/
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

static void appStopWifiMqtt(void)
{
    esp_err_t espRet;

    APP_CONSOLE(D, "Disconnecting from MQTT");
    if (mqttClientWifi.handler != NULL) {
        requestDc = true;
        espRet = xplrMqttWifiUnsubscribeFromTopicArrayZtp(&mqttClientWifi,
                                                          &thingstreamSettings->pointPerfect);
        if (mqttClientWifi.handler != NULL && espRet == ESP_OK) {
            espRet = xplrMqttWifiHardDisconnect(&mqttClientWifi);
            if (espRet != ESP_OK) {
                APP_CONSOLE(E, "Error disconnecting Mqtt");
            } else {
                APP_CONSOLE(D, "xplrMqttWifiHardDisconnect returned %d", espRet);
            }
            initializeTsConfig();
        } else {
            /*Unsubscribing failed*/
            APP_CONSOLE(E, "Null mqttClientWifi handler. Can't perform hard disconnect");
        }
    } else {
        espRet = ESP_FAIL;
        APP_CONSOLE(E, "Error in unsubscribing from MQTT topics");
    }

    if (espRet == ESP_OK) {
        APP_CONSOLE(D, "Disconnected from MQTT");
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_READY);
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_READY);
    } else {
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_ERROR);
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
    }
}

/**
 * Function that handles the event of an inactivity timeout
 * of the GNSS module
*/
static void appTerminate(void)
{
    xplrGnssError_t gnssErr;
    esp_err_t espErr;
    uint64_t timePrevLoc;

    APP_CONSOLE(E, "Unrecoverable error in application. Terminating and restarting...");
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
#if (APP_RESTART_ON_ERROR == 1)
    esp_restart();
#else
    appHaltExecution();
#endif
}

/*
 * Function to halt app execution
 */
static void appHaltExecution(void)
{
#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif
    appDeInitAtParser();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static app_cell_error_t appStopCell(void)
{
    xplrCell_mqtt_error_t err;
    xplrCom_error_t comErr;
    app_cell_error_t appCellError;

    err = xplrCellMqttUnsubscribeFromTopicList(cellConfig.profileIndex, 0);
    if (err != XPLR_CELL_MQTT_OK) {
        APP_CONSOLE(E, "Error unsubscribing from MQTT topics.");
        appCellError = APP_ERROR_UNKNOWN;
    } else {
        err = xplrCellMqttDisconnect(cellConfig.profileIndex, 0);
        if (err != XPLR_CELL_MQTT_OK) {
            APP_CONSOLE(E, "Error disconnecting from cell MQTT.");
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_ERROR);
            appCellError = APP_ERROR_UNKNOWN;
        } else {
            appCellState[0] = APP_FSM_MQTT_DISCONNECT;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_TS, XPLR_ATPARSER_STATUS_READY);
            appCellError = APP_ERROR_OK;
        }
    }
    APP_CONSOLE(D, "Deinitializing Cell Mqtt");
    /* De-init mqtt client */
    xplrCellMqttDeInit(cellConfig.profileIndex, mqttClientCell.id);
    if (err == XPLR_CELL_MQTT_ERROR) {
        APP_CONSOLE(E, "Error Deinitializing Cell Mqtt.");
    } else {
        //do nothing
    }
    initializeTsConfig();

    comErr = xplrComCellPowerDown(cellConfig.profileIndex);
    if (comErr != XPLR_COM_OK) {
        APP_CONSOLE(D, "Error powering down cell device!");
    } else {
        comErr =  xplrComCellDeInit(cellConfig.profileIndex);
        if (comErr != XPLR_COM_OK) {
            appCellError = APP_ERROR_CELL_INIT;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_ERROR);
        } else {
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_READY);
        }
        // set boolean flag for calling xplrComCellPowerResume on next cell start
        cellInitAfterPowerDown = true;
    }

    return appCellError;
}

static void appNtripWifiInit(void)
{
    xplr_ntrip_error_t err;

    ntripWifiClient.config = &profile->data.correctionData.ntripConfig;
    if ((ntripWifiClient.config->credentials.username[0] == '\0') &&
        (ntripWifiClient.config->credentials.password[0] == '\0')) {
        ntripWifiClient.config->credentials.useAuth = false;
    } else {
        ntripWifiClient.config->credentials.useAuth = true;
    }
    ntripWifiClient.config_set = true;
    ntripWifiClient.credentials_set = true;
    ntripSemaphore = xSemaphoreCreateMutex();
    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_INIT);
    err = xplrWifiNtripInit(&ntripWifiClient, ntripSemaphore);

    if (err != XPLR_NTRIP_OK) {
        APP_CONSOLE(E, "NTRIP client initialization failed!");
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
        appHaltExecution();
    } else {
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_CONNECTED);
    }
}

/**
 * NTRIP client de-initialization
 */
static void appNtripWifiDeInit(void)
{
    xplr_ntrip_error_t err;

    err = xplrWifiNtripDeInit(&ntripWifiClient);

    if (err != XPLR_NTRIP_OK) {
        APP_CONSOLE(E, "NTRIP client de-init failed!");
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_ERROR);
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
        appHaltExecution();
    } else {
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_READY);
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_READY);
    }

    vSemaphoreDelete(ntripSemaphore);
}

static void appNtripWifiFsm(void)
{
    xplr_ntrip_state_t ntripState;
    esp_err_t espRet;
    int32_t len;

    wifistarterErr = xplrWifiStarterFsm();
    if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
        if (!ntripWifiClient.socketIsValid) {
            appNtripWifiInit();
            currentStatus = XPLR_ATPARSER_STATHPG_WIFI_CONNECTED;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_WIFI, XPLR_ATPARSER_STATUS_CONNECTED);
            isNtripWifiInit = true;
        } else {
            ntripState = xplrWifiNtripGetClientState(&ntripWifiClient);
            switch (ntripState) {
                case XPLR_NTRIP_STATE_READY:
                    // NTRIP client operates normally no action needed from APP
                    break;
                case XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                    // NTRIP client has received correction data
                    xplrWifiNtripGetCorrectionData(&ntripWifiClient,
                                                   rxBuff[0],
                                                   XPLRNTRIP_RECEIVE_DATA_SIZE,
                                                   &ntripSize);
                    APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                    espRet = xplrGnssSendRtcmCorrectionData(gnssDvcPrfId, rxBuff[0], ntripSize);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "Error %d sending Rtcm correction data to gnss device", espRet);
                    } else {
                        //do nothing
                    }
                    currentStatus = XPLR_ATPARSER_STATHPG_NTRIP_CONNECTED;
                    break;
                case XPLR_NTRIP_STATE_REQUEST_GGA:
                    // NTRIP client requires GGA to send back to server
                    memset(GgaMsg, 0x00, strlen(GgaMsg));
                    len = xplrGnssGetGgaMessage(gnssDvcPrfId, &ntripGgaMsgPtr, ELEMENTCNT(GgaMsg));
                    xplrWifiNtripSendGGA(&ntripWifiClient, GgaMsg, len);
                    break;
                case XPLR_NTRIP_STATE_ERROR:
                    // NTRIP client encountered an error
                    APP_CONSOLE(E, "NTRIP Client returned error state");
                    xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
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
}

static app_cell_error_t appNtripCellInit(void)
{
    xplr_ntrip_error_t err;
    app_cell_error_t ret;

    ret = appCellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        ntripCellClient.config = &profile->data.correctionData.ntripConfig;
        if ((ntripCellClient.config->credentials.username[0] == '\0') &&
            (ntripCellClient.config->credentials.password[0] == '\0')) {
            ntripCellClient.config->credentials.useAuth = false;
        } else {
            ntripCellClient.config->credentials.useAuth = true;
        }
        ntripCellClient.config_set = true;
        ntripCellClient.credentials_set = true;
        ntripSemaphore = xSemaphoreCreateMutex();
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_INIT);
        err = xplrCellNtripInit(&ntripCellClient, ntripSemaphore);

        if (err != XPLR_NTRIP_OK) {
            APP_CONSOLE(E, "NTRIP client initialization failed!");
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
            ret = APP_ERROR_NTRIP;
            appHaltExecution();
        } else {
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_CONNECTED);
        }
    } else {
        ret = APP_ERROR_NTRIP;
    }

    return ret;
}

static app_cell_error_t appNtripCellDeInit(void)
{
    app_cell_error_t ret;
    xplr_ntrip_error_t ntripRet;
    xplrCom_error_t comError;

    ntripRet = xplrCellNtripDeInit(&ntripCellClient);
    if (ntripRet != XPLR_NTRIP_ERROR) {
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_READY);
        xplrComCellPowerDown(cellConfig.profileIndex);
        // set boolean flag for calling xplrComCellPowerResume on next cell start
        comError =  xplrComCellDeInit(cellConfig.profileIndex);
        if (comError != XPLR_COM_OK) {
            ret = APP_ERROR_CELL_INIT;
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_ERROR);
        } else {
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_READY);
        }
        cellInitAfterPowerDown = true;
        vSemaphoreDelete(ntripSemaphore);
        ret = APP_ERROR_OK;

    } else {
        ret = APP_ERROR_NTRIP;
        xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
    }

    return ret;
}

static void appNtripCellFsm(void)
{
    static app_cell_error_t appCellError;
    esp_err_t espRet;
    xplrCom_error_t err;
    int32_t len;

    switch (appCellState[0]) {
        case APP_FSM_INIT_CELL:
            /*
             * Config Cell module
             */
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_INIT);
            configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
            err = xplrComCellInit(&cellConfig); /* Initialize hpg com */
            if (err != XPLR_COM_OK) {
                APP_CONSOLE(E, "Error initializing hpg com!");
                appCellState[0] = APP_FSM_ERROR;
            } else {
                appCellState[0] = APP_FSM_CHECK_NETWORK;
            }
            break;
        case APP_FSM_CHECK_NETWORK:
            appCellState[1] = appCellState[0];
            appCellError = cellNetworkRegister();
            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_CONNECTING);
            if (appCellError == APP_ERROR_OK) {
                appCellState[0] = APP_FSM_SET_GREETING_MESSAGE;
                currentStatus = XPLR_ATPARSER_STATHPG_CELL_CONNECTED;
            } else if (appCellError == APP_ERROR_NETWORK_OFFLINE) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                /* module still trying to connect. do nothing */
            }
            break;
        case APP_FSM_SET_GREETING_MESSAGE:
            appCellState[1] = appCellState[0];
            appCellError = cellSetGreeting();
            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                appCellState[0] = APP_FSM_INIT_NTRIP_CLIENT;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_CONNECTED);
            }
            break;
        case APP_FSM_INIT_NTRIP_CLIENT:
            appCellState[1] = appCellState[0];
            appCellError = appNtripCellInit();
            if (appCellError == APP_ERROR_OK) {
                appCellState[0] = APP_FSM_RUN;
                currentStatus = XPLR_ATPARSER_STATHPG_NTRIP_CONNECTED;
            } else {
                appCellState[0] = APP_FSM_ERROR;
            }
            break;
        case APP_FSM_RUN:
            appCellState[1] = appCellState[0];

            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    switch (xplrCellNtripGetClientState(&ntripCellClient)) {
                        case XPLR_NTRIP_STATE_READY:
                            // NTRIP client operates normally no action needed from APP
                            break;
                        case XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                            // NTRIP client has received correction data
                            xplrCellNtripGetCorrectionData(&ntripCellClient,
                                                           rxBuff[0],
                                                           XPLRNTRIP_RECEIVE_DATA_SIZE,
                                                           &ntripSize);
                            APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                            espRet = xplrGnssSendRtcmCorrectionData(gnssDvcPrfId, rxBuff[0], ntripSize);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Error %d sending Rtcm correction data to gnss device", espRet);
                            } else {
                                //do nothing
                            }
                            break;
                        case XPLR_NTRIP_STATE_REQUEST_GGA:
                            // NTRIP client requires GGA to send back to server
                            memset(GgaMsg, 0x00, strlen(GgaMsg));
                            len = xplrGnssGetGgaMessage(gnssDvcPrfId, &ntripGgaMsgPtr, ELEMENTCNT(GgaMsg));
                            xplrCellNtripSendGGA(&ntripCellClient, GgaMsg, len);
                            break;
                        case XPLR_NTRIP_STATE_ERROR:
                            xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_NTRIP, XPLR_ATPARSER_STATUS_ERROR);
                            // NTRIP client encountered an error
                            APP_CONSOLE(E, "NTRIP Client encountered error");
                            appCellState[0] = APP_FSM_ERROR;
                            break;
                        case XPLR_NTRIP_STATE_BUSY:
                            // NTRIP client busy, retry until state changes
                            break;
                        default:
                            break;
                    }
                } else {
                    //do nothing
                }
            }
            break;
        case APP_FSM_TERMINATE:
            appCellState[1] = appCellState[0];
            if (appCellError != APP_ERROR_OK) {
                appCellState[0] = APP_FSM_ERROR;
            } else {
                appCellState[0] = APP_FSM_INACTIVE;
                xplrAtParserSetSubsystemStatus(XPLR_ATPARSER_SUBSYSTEM_CELL, XPLR_ATPARSER_STATUS_READY);
            }
            break;
        case APP_FSM_INACTIVE:
            APP_CONSOLE(I, "ALL DONE!!!");
            appHaltExecution();
            break;
        case APP_FSM_ERROR:
            APP_CONSOLE(E, "Halting execution");
            appHaltExecution();
            break;
        default:
            break;
    }
    if (cellHasRebooted && (appCellState[0] == APP_FSM_RUN)) {
        appCellState[1] = appCellState[0];
        isRstControlled = xplrComIsRstControlled(cellConfig.profileIndex);
        if (isRstControlled) {
            APP_CONSOLE(I, "Controlled LARA restart triggered");
            isRstControlled = false;
        } else {
            APP_CONSOLE(W, "Uncontrolled LARA restart triggered");
            appCellState[0] = APP_FSM_CHECK_NETWORK;
            /* De-init mqtt client */
            xplrComPowerResetHard(cellConfig.profileIndex);
            appCellState[0] = APP_FSM_CHECK_NETWORK;
        }
        cellHasRebooted = false;
        APP_CONSOLE(W, "Cell Module has rebooted! Number of total reboots: <%d>", cellReboots);
    }
}

static void configureCorrectionSource()
{
    esp_err_t espRet;

    //configure handler for LBAND plan
    if ((profile->data.correctionData.thingstreamCfg.tsPlan == XPLR_THINGSTREAM_PP_PLAN_LBAND) &&
        (profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM)) {
        if (isLbandAsyncInit == false) {
            //set correction data source to LBand
            isLbandAsyncInit = true;
            appRestartGnssDevices();
            appWaitGnssReady();
            appSetGnssDestinationHandler();
        }
        //configure handler for IPBLAND plan when correction module is LBAND
    } else if (profile->data.correctionData.correctionMod == XPLR_ATPARSER_CORRECTION_MOD_LBAND &&
               profile->data.correctionData.thingstreamCfg.tsPlan == XPLR_THINGSTREAM_PP_PLAN_IPLBAND &&
               profile->data.correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM) {
        if (isLbandAsyncInit == false) {
            //set correction data source to LBand
            isLbandAsyncInit = true;
            appRestartGnssDevices();
            appWaitGnssReady();
            appSetGnssDestinationHandler();
        }
    } else { //configure handler for IP or NTRIP
        if ((prevThingstreamPlan != profile->data.correctionData.thingstreamCfg.tsPlan) ||
            (prevCorrectionMod != profile->data.correctionData.correctionMod)) {
            //set correction data source to IP
            espRet = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_IP);
            if (espRet != ESP_OK) {
                APP_CONSOLE(E, "Failed to set correction data source to IP");
                appHaltExecution();
            } else {
                //do nothing
            }
        }
    }
    prevThingstreamPlan = profile->data.correctionData.thingstreamCfg.tsPlan;
    prevCorrectionMod = profile->data.correctionData.correctionMod;
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
}

static void mqttDisconnectCallback(int32_t status, void *cbParam)
{
    (void)cbParam;
    (void)status;
    mqttSessionDisconnected = true;
    APP_CONSOLE(W, "MQTT client disconnected");
}