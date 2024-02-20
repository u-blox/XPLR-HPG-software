/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include "string.h"
#include "xplr_at_parser.h"
#include "freertos/task.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "../../../xplr_hpglib_cfg.h"

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

#if (1 == XPLRATPARSER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRATPARSER_LOG_ACTIVE))
#define XPLRATPARSER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtParser", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRATPARSER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRATPARSER_LOG_ACTIVE)
#define XPLRATPARSER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtParser", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRATPARSER_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRATPARSER_LOG_ACTIVE)
#define XPLRATPARSER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtParser", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRATPARSER_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

const char atParserOkResponse[] = "OK";
const char atParserErrorResponse[] = "ERROR";
const char atParserErrorBusyResponse[] = "+ERROR:BUSY";
const char atParserNvsNamespace[] = "atParser";
const char at[] = "AT\r";
const char atWifiSet[] = "AT+WIFI=";
const char atWifiGet[] = "AT+WIFI=?";
const char atApnSet[] = "AT+APN=";
const char atApnGet[] = "AT+APN=?";
const char atRootSet[] = "AT+ROOT=";
const char atRootGet[] = "AT+ROOT=?";
const char atTsBrokerSet[] = "AT+TSBROKER=";
const char atTsBrokerGet[] = "AT+TSBROKER=?";
const char atTsidSet[] = "AT+TSID=";
const char atTsidGet[] = "AT+TSID=?";
const char atTscertSet[] = "AT+TSCERT=";
const char atTscertGet[] = "AT+TSCERT=?";
const char atTskeySet[] = "AT+TSKEY=";
const char atTskeyGet[] = "AT+TSKEY=?";
const char atTsplanSet[] = "AT+TSPLAN=";
const char atTsplanGet[] = "AT+TSPLAN=?";
const char atTsregionSet[] = "AT+TSREGION=";
const char atTsregionGet[] = "AT+TSREGION=?";
const char atNtripsrvSet[] = "AT+NTRIPSRV=";
const char atNtripsrvGet[] = "AT+NTRIPSRV=?";
const char atNtripggaSet[] = "AT+NTRIPGGA=";
const char atNtripggaGet[] = "AT+NTRIPGGA=?";
const char atNtripuaSet[] = "AT+NTRIPUA=";
const char atNtripuaGet[] = "AT+NTRIPUA=?";
const char atNtripmpSet[] = "AT+NTRIPMP=";
const char atNtripmpGet[] = "AT+NTRIPMP=?";
const char atNtripcredsSet[] = "AT+NTRIPCREDS=";
const char atNtripcredsGet[] = "AT+NTRIPCREDS=?";
const char atGnssdrSet[] = "AT+GNSSDR=";
const char atGnssdrGet[] = "AT+GNSSDR=?";
const char atSdSet[] = "AT+SD=";
const char atSdGet[] = "AT+SD=?";
const char atBaudSet[] = "AT+BAUD=";
const char atBaudGet[] = "AT+BAUD=?";
const char atInterfaceSet[] = "AT+IF=";
const char atInterfaceGet[] = "AT+IF=?";
const char atCorsrcSet[] = "AT+CORSRC=";
const char atCorsrcGet[] = "AT+CORSRC=?";
const char atCormodSet[] = "AT+CORMOD=";
const char atCormodGet[] = "AT+CORMOD=?";
const char atHpgmodeSet[] = "AT+HPGMODE=";
const char atHpgmodeGet[] = "AT+HPGMODE=?";
const char atErase[] = "AT+ERASE=";
const char atStat[] = "AT+STAT";
const char atLoc[] = "AT+LOC=?";
const char atBoardinfo[] = "AT+BRDNFO=?";
const char atBoardRestart[] = "AT+BRD=RST";
const char atStartOnBootSet[] = "AT+STARTONBOOT=";
const char atStartOnBootGet[] = "AT+STARTONBOOT=?";

const char atWifiResponse[] = "+WIFI=";
const char atApnResponse[] = "+APN=";
const char atmqttBrokerResponse[] = "+TSBROKER:";
const char atRootResponse[] = "+ROOT=";
const char atTsidResponse[] = "+TSID=";
const char atTscertResponse[] = "+TSCERT=";
const char atTskeyResponse[] = "+TSKEY=";
const char atTsregionResponse[] = "+TSREGION=";
const char atTsplanResponse[] = "+TSPLAN=";
const char atNtripsrvResponse[] = "+NTRIPSRV=";
const char atNtripGgaResponse[] = "+NTRIPGGA=";
const char atNtripuaResponse[] = "+NTRIPUA=";
const char atNtripmpResponse[] = "+NTRIPMP=";
const char atNtripcredsResponse[] = "+NTRIPCREDS=";
const char atGnssdrResponse[] = "+GNSSDR=";
const char atSdResponse[] = "+SD=";
const char atBaudResponse[] = "+BAUD=";
const char atInterfaceResponse[] = "+IF=";
const char atCorsrcResponse[] = "+CORSRC=";
const char atCormodResponse[] = "+CORMOD=";
const char atHpgmodeResponse[] = "+HPGMODE=";
const char atStatwifiResponse[] = "+STATWIFI:";
const char atStatcellResponse[] = "+STATCELL:";
const char atStattsResponse[] = "+STATTS:";
const char atStatntripResponse[] = "+STATNTRIP:";
const char atStatgnssResponse[] = "+STATGNSS:";
const char atLocResponse[] = "+LOC:";
const char atBoardInfoResponse[] = "+BRDNFO:";
const char atStartOnBootResponse[] = "+STARTONBOOT:";

const char nvsKeySsid[] = "ssid";
const char nvsKeyPwd[] = "pwd";
const char nvsKeyApn[] = "apn";
const char nvsKeyMqttBroker[] = "mqttBroker";
const char nvsKeyMqttBrokerPort[] = "mqttBrokerPort";
const char nvsKeyRootcrt[] = "rootCrt";
const char nvsKeyClientid[] = "clientId";
const char nvsKeyClientcrt[] = "clientCert";
const char nvsKeyClientkey[] = "clientKey";
const char nvsKeyTsregion[] = "tsRegion";
const char nvsKeyTsplan[] = "tsPlan";
const char nvsKeyNtriphost[] = "ntripHost";
const char nvsKeyNtripGgaMessage[] = "ntripGga";
const char nvsKeyNtripua[] = "ntripUserAgent";
const char nvsKeyNtripmp[] = "ntripMountpoint";
const char nvsKeyNtripport[] = "ntripPort";
const char nvsKeyNtripusername[] = "ntripUsername";
const char nvsKeyNtrippassword[] = "ntripPassword";
const char nvsKeyDeadreckoning[] = "deadReckoning";
const char nvsKeySdlog[] = "sdLog";
const char nvsKeyInterface[] = "interface";
const char nvsKeyCorsource[] = "corSource";
const char nvsKeyCormod[] = "corMod";
const char nvsKeyStartOnBoot[] = "startOnBoot";

const char atPartCommandWifi[] = "WIFI";
const char atPartCommandMqttBroker[] = "TSBROKER";
const char atPartCommandApn[] = "APN";
const char atPartCommandRootcrt[] = "ROOT";
const char atPartCommandTsid[] = "TSID";
const char atPartCommandTscert[] = "TSCERT";
const char atPartCommandTskey[] = "TSKEY";
const char atPartCommandTsregion[] = "TSREGION";
const char atPartCommandTsplan[] = "TSPLAN";
const char atPartCommandNtripsrv[] = "NTRIPSRV";
const char atPartCommandNtripua[] = "NTRIPUA";
const char atPartCommandNtripmp[] = "NTRIPMP";
const char atPartCommandNtripcreds[] = "NTRIPCREDS";
const char atPartCommandAll[] = "ALL";

const char atPartWifi[] = "WIFI=?";
const char atPartCell[] = "CELL=?";
const char atPartTs[] = "TS=?";
const char atPartNtrip[] = "NTRIP=?";
const char atPartGnss[] = "GNSS=?";

const char statHpgMsgError[] = "+STATHPG:ERROR";
const char statHpgMsgInit[] = "+STATHPG:INIT";
const char statHpgMsgConfig[] = "+STATHPG:CONFIG";
const char statHpgMsgWifiInit[] = "+STATHPG:WIFI-INIT";
const char statHpgMsgCellInit[] = "+STATHPG:CELL-INIT";
const char statHpgMsgWifiConnected[] = "+STATHPG:WIFI-CONNECTED";
const char statHpgMsgCellConnected[] = "+STATHPG:CELL-CONNECTED";
const char statHpgMsgTsConnected[] = "+STATHPG:TS-CONNECTED";
const char statHpgMsgNtripConnected[] = "+STATHPG:NTRIP-CONNECTED";
const char statHpgMsgWifiError[] = "+STATHPG:WIFI-ERROR";
const char statHpgMsgCellError[] = "+STATHPG:CELL-ERROR";
const char statHpgMsgTsError[] = "+STATHPG:TS-ERROR";
const char statHpgMsgNtripError[] = "+STATHPG:NTRIP-ERROR";
const char statHpgMsgReconnecting[] = "+STATHPG:RECONNECTING";
const char statHpgMsgStop[] = "+STATHPG:STOP";

const char statusStrError[] = "Error";
const char statusStrNotSet[] = "Not Set";
const char statusStrReady[] = "Ready";
const char statusStrInit[] = "Init";
const char statusStrConnecting[] = "Connecting";
const char statusStrConnected[] = "Connected";
const char statusStrReconnecting[] = "Reconnecting";

const char modeStrConfig[] = "config";
const char modeStrStart[] = "start";
const char modeStrStop[] = "stop";
const char modeStrError[] = "error";

const char invalidStr[] = "invalid";

const char corSrcStrNtrip[] = "ntrip";
const char corSrcStrTs[] = "ts";
const char interfaceStrWifi[] = "wi-fi";
const char interfaceStrCell[] = "cell";
const char planStrIpLband[] = "ip+lband";
const char planStrIp[] = "ip";
const char planStrLband[] = "lband";

static int8_t logIndex = -1;

static SemaphoreHandle_t mutexSemaphore = NULL;
static SemaphoreHandle_t deviceModeBusySemaphore = NULL;
static bool deviceModeBusy = false;
xplr_at_parser_t parser = {.data.mode = XPLR_ATPARSER_MODE_NOT_SET,
                           .data.net.interface = XPLR_ATPARSER_NET_INTERFACE_NOT_SET,
                           .data.status.wifi = XPLR_ATPARSER_STATUS_NOT_SET,
                           .data.status.cell = XPLR_ATPARSER_STATUS_NOT_SET,
                           .data.status.ts = XPLR_ATPARSER_STATUS_NOT_SET,
                           .data.status.ntrip = XPLR_ATPARSER_STATUS_NOT_SET,
                           .data.status.gnss = XPLR_ATPARSER_STATUS_NOT_SET,
                           .data.correctionData.thingstreamCfg.tsPlan = XPLR_THINGSTREAM_PP_PLAN_INVALID,
                           .data.correctionData.thingstreamCfg.tsRegion = XPLR_THINGSTREAM_PP_REGION_INVALID,
                           .data.correctionData.ntripConfig.server.port = 0,
                           .data.correctionData.correctionSource = XPLR_ATPARSER_CORRECTION_SOURCE_NOT_SET,
                           .data.correctionData.correctionMod = XPLR_ATPARSER_CORRECTION_MOD_IP,
                           .data.restartSignal = false,
                           .data.startOnBoot = false,
                           .faults.value = 0,
                           .internalFaults.value = 0,
                           .data.misc.dr.enable = false,
                           .data.misc.sdLogEnable = false,
                           .server.profile = 0,
                           .server.uartCfg = NULL,
                          };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static inline void atParserCallbackWrapper(xplr_at_parser_t *parser,
                                           void (*callback)(void *, void *),
                                           void *callbackArg);

static inline void xplrAtServerWriteStrWrapper(xplr_at_parser_t *parser,
                                               const char *buffer,
                                               xplr_at_server_response_type_t responseType);

static inline void atParserReturnError(xplr_at_parser_subsystem_type_t errorType);
static inline void atParserReturnErrorBusy(xplr_at_parser_subsystem_type_t errorType);
static inline void atParserReturnOk(void);

static void atParserHandlerCheck(uAtClientHandle_t client, void *arg);
static void atParserCallbackCheck(uAtClientHandle_t client, void *arg);

static void atParserHandlerWifiSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackWifiSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerWifiGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackWifiGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerErase(uAtClientHandle_t client, void *arg);
static void atParserCallbackErase(uAtClientHandle_t client, void *arg);

static void atParserHandlerApnSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackApnSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerApnGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackApnGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerMqttBrokerSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackMqttBrokerSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerMqttBrokerGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackMqttBrokerGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerRootCrtSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackRootCrtSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerRootCrtGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackRootCrtGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientIdSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientIdSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientIdGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientIdGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientCrtSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientCrtSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientCrtGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientCrtGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientKeySet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientKeySet(uAtClientHandle_t client, void *arg);

static void atParserHandlerClientKeyGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackClientKeyGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerRegionSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackRegionSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerRegionGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackRegionGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerPlanSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackPlanSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerPlanGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackPlanGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripServerSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripServerSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripServerGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripServerGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripGgaSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripGgaSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripGgaGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripGgaGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripUserAgentSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripUserAgentSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripUserAgentGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripUserAgentGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripMountPointSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripMountPointSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripMountPointGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripMountPointGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripCredsSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripCredsSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerNtripCredsGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackNtripCredsGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerDRSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackDRSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerDRGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackDRGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerSDSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackSDSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerSDGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackSDGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerBaudrateSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackBaudrateSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerBaudrateGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackBaudrateGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerInterfaceSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackInterfaceSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerInterfaceGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackInterfaceGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerCorrectionSourceSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackCorrectionSourceSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerCorrectionSourceGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackCorrectionSourceGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerCorrectionModSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackCorrectionModSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerCorrectionModGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackCorrectionModGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerDeviceModeSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackDeviceModeSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerDeviceModeGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackDeviceModeGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerBoardInfoGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackBoardInfoGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerStatusGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackStatusGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerLocationGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackLocationGet(uAtClientHandle_t client, void *arg);

static void atParserHandlerBoardRestart(uAtClientHandle_t client, void *arg);
static void atParserCallbackBoardRestart(uAtClientHandle_t client, void *arg);

static void atParserHandlerStartOnBootSet(uAtClientHandle_t client, void *arg);
static void atParserCallbackStartOnBootSet(uAtClientHandle_t client, void *arg);

static void atParserHandlerStartOnBootGet(uAtClientHandle_t client, void *arg);
static void atParserCallbackStartOnBootGet(uAtClientHandle_t client, void *arg);

static esp_err_t atParserInitNvs(void);
static esp_err_t atParserDeInitNvs(void);

static void atParserInstanceArrayInit(void);

static xplr_at_parser_error_t xplrAtParserAddNet(void);
static xplr_at_parser_error_t xplrAtParserAddThingstream(void);
static xplr_at_parser_error_t xplrAtParserAddNtrip(void);
static xplr_at_parser_error_t xplrAtParserAddMisc(void);

static void xplrAtParserRemoveNet(void);
static void xplrAtParserRemoveThingstream(void);
static void xplrAtParserRemoveNtrip(void);
static void xplrAtParserRemoveMisc(void);

static inline bool xplrAtParserTryLock(bool isSetCommand);
static inline void xplrAtParserUnlock(void);

static inline void xplrAtParserFaultSet(xplr_at_parser_subsystem_type_t type);
static inline void xplrAtParserInternalFaultSet(xplr_at_parser_internal_driver_fault_type_t type);

xplr_at_parser_t *xplrAtParserInit(xplr_at_server_uartCfg_t *uartCfg)
{
    esp_err_t ret;
    int32_t xSemaphoreReturn[2];
    xplr_at_server_error_t atServerError;
    xplr_at_parser_error_t parserError;
    xplr_at_parser_t *returnValue;

    parser.data.id = atParserNvsNamespace;
    atParserInstanceArrayInit();
    /* A binary semaphore is used instead of a mutex as xSemaphoreCreateMutex uses a
     * priority inheritance mechanism so a task ‘taking’ a semaphore must always give
     * the semaphore back once the semaphore is no longer required. In this implementation
     * the semaphore is taken from the handler function and released on its corresponding callback.
     * Thus the semaphore is not given back from the same task that it was acquired from.
     */
    mutexSemaphore = xSemaphoreCreateBinary();
    deviceModeBusySemaphore = xSemaphoreCreateBinary();
    if (mutexSemaphore == NULL || deviceModeBusySemaphore == NULL) {
        XPLRATPARSER_CONSOLE(E, "Error initializing semaphore");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        xSemaphoreReturn[0] = xSemaphoreGive(mutexSemaphore);
        xSemaphoreReturn[1] = xSemaphoreGive(deviceModeBusySemaphore);
        if (xSemaphoreReturn[0] != pdTRUE || xSemaphoreReturn[1] != pdTRUE) {
            XPLRATPARSER_CONSOLE(E, "Error giving semaphore");
            parserError = XPLR_AT_PARSER_ERROR;
        } else {
            ret = atParserInitNvs();
            if (ret != ESP_OK) {
                XPLRATPARSER_CONSOLE(E, "Error initializing NVS");
                parserError = XPLR_AT_PARSER_ERROR;
            } else {
                parserError = XPLR_AT_PARSER_OK;
            }
        }
    }

    if (parserError == XPLR_AT_PARSER_OK) {
        parser.server.uartCfg = uartCfg;
        atServerError = xplrAtServerInit(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            XPLRATPARSER_CONSOLE(E, "Error initializing AT server");
            returnValue = NULL;
        } else {
            XPLRATPARSER_CONSOLE(D, "Initialized AT Parser");
            returnValue = &parser;
        }
    } else {
        returnValue = NULL;
    }

    return returnValue;
}

xplr_at_parser_error_t xplrAtParserDeInit(void)
{
    esp_err_t ret;
    xplr_at_parser_error_t parserError;

    xplrAtServerDeInit(&parser.server);

    vSemaphoreDelete(mutexSemaphore);
    ret = atParserDeInitNvs();
    if (ret != ESP_OK) {
        XPLRATPARSER_CONSOLE(E, "Error Deinitializing NVS");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

static xplr_at_parser_error_t xplrAtParserAddNet(void)
{
    xplr_at_server_error_t error;
    xplr_at_parser_error_t parserError;
    xplr_at_server_t *server = &parser.server;

    error = xplrAtServerSetCommandFilter(server,
                                         atWifiSet,
                                         atParserHandlerWifiSet,
                                         NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atWifiGet,
                                          atParserHandlerWifiGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atApnSet,
                                          atParserHandlerApnSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atApnGet,
                                          atParserHandlerApnGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atInterfaceSet,
                                          atParserHandlerInterfaceSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atInterfaceGet,
                                          atParserHandlerInterfaceGet,
                                          NULL);

    if (error != XPLR_AT_SERVER_OK) {
        XPLRATPARSER_CONSOLE(E, "Error adding AT Net parser");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

static xplr_at_parser_error_t xplrAtParserAddThingstream(void)
{
    xplr_at_server_error_t error;
    xplr_at_parser_error_t parserError;
    xplr_at_server_t *server = &parser.server;

    error = xplrAtServerSetCommandFilter(server,
                                         atTsBrokerSet,
                                         atParserHandlerMqttBrokerSet,
                                         NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsBrokerGet,
                                          atParserHandlerMqttBrokerGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atRootSet,
                                          atParserHandlerRootCrtSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atRootGet,
                                          atParserHandlerRootCrtGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsidSet,
                                          atParserHandlerClientIdSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsidGet,
                                          atParserHandlerClientIdGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTscertSet,
                                          atParserHandlerClientCrtSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTscertGet,
                                          atParserHandlerClientCrtGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTskeySet,
                                          atParserHandlerClientKeySet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTskeyGet,
                                          atParserHandlerClientKeyGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsregionSet,
                                          atParserHandlerRegionSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsregionGet,
                                          atParserHandlerRegionGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsplanSet,
                                          atParserHandlerPlanSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atTsplanGet,
                                          atParserHandlerPlanGet,
                                          NULL);

    if (error != XPLR_AT_SERVER_OK) {
        XPLRATPARSER_CONSOLE(E, "Error adding AT Thingstream parser");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

static xplr_at_parser_error_t xplrAtParserAddNtrip(void)
{
    xplr_at_server_error_t error;
    xplr_at_parser_error_t parserError;
    xplr_at_server_t *server = &parser.server;

    error = xplrAtServerSetCommandFilter(server,
                                         atNtripsrvSet,
                                         atParserHandlerNtripServerSet,
                                         NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripsrvGet,
                                          atParserHandlerNtripServerGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripggaSet,
                                          atParserHandlerNtripGgaSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripggaGet,
                                          atParserHandlerNtripGgaGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripuaSet,
                                          atParserHandlerNtripUserAgentSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripuaGet,
                                          atParserHandlerNtripUserAgentGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripmpSet,
                                          atParserHandlerNtripMountPointSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripmpGet,
                                          atParserHandlerNtripMountPointGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripcredsSet,
                                          atParserHandlerNtripCredsSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atNtripcredsGet,
                                          atParserHandlerNtripCredsGet,
                                          NULL);

    if (error != XPLR_AT_SERVER_OK) {
        XPLRATPARSER_CONSOLE(E, "Error adding AT Ntrip parser");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

static xplr_at_parser_error_t xplrAtParserAddMisc(void)
{
    xplr_at_server_error_t error;
    xplr_at_parser_error_t parserError;
    xplr_at_server_t *server = &parser.server;

    error = xplrAtServerSetCommandFilter(server,
                                         at,
                                         atParserHandlerCheck,
                                         NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atErase,
                                          atParserHandlerErase,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atStat,
                                          atParserHandlerStatusGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atGnssdrSet,
                                          atParserHandlerDRSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atGnssdrGet,
                                          atParserHandlerDRGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atSdSet,
                                          atParserHandlerSDSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atSdGet,
                                          atParserHandlerSDGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atCorsrcSet,
                                          atParserHandlerCorrectionSourceSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atCorsrcGet,
                                          atParserHandlerCorrectionSourceGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atCormodSet,
                                          atParserHandlerCorrectionModSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atCormodGet,
                                          atParserHandlerCorrectionModGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atHpgmodeSet,
                                          atParserHandlerDeviceModeSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atHpgmodeGet,
                                          atParserHandlerDeviceModeGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atBoardinfo,
                                          atParserHandlerBoardInfoGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atLoc,
                                          atParserHandlerLocationGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atBaudSet,
                                          atParserHandlerBaudrateSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atBaudGet,
                                          atParserHandlerBaudrateGet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atBoardRestart,
                                          atParserHandlerBoardRestart,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atStartOnBootSet,
                                          atParserHandlerStartOnBootSet,
                                          NULL);
    error |= xplrAtServerSetCommandFilter(server,
                                          atStartOnBootGet,
                                          atParserHandlerStartOnBootGet,
                                          NULL);

    if (error != XPLR_AT_SERVER_OK) {
        XPLRATPARSER_CONSOLE(E, "Error adding AT Misc parser");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

xplr_at_parser_error_t xplrAtParserAdd(xplr_at_parser_type_t parserType)
{
    xplr_at_parser_error_t parserError;

    switch (parserType) {
        case XPLR_ATPARSER_ALL:
            parserError = xplrAtParserAddNet();
            parserError |= xplrAtParserAddThingstream();
            parserError |= xplrAtParserAddNtrip();
            parserError |= xplrAtParserAddMisc();
            break;

        case XPLR_ATPARSER_NET:
            parserError = xplrAtParserAddNet();
            break;

        case XPLR_ATPARSER_THINGSTREAM:
            parserError = xplrAtParserAddThingstream();
            break;

        case XPLR_ATPARSER_NTRIP:
            parserError = xplrAtParserAddNtrip();
            break;

        case XPLR_ATPARSER_MISC:
            parserError = xplrAtParserAddMisc();
            break;

        default:
            XPLRATPARSER_CONSOLE(E, "Invalid At Parser command group");
            parserError = XPLR_AT_PARSER_ERROR;
    }

    return parserError;
}


static void xplrAtParserRemoveNet(void)
{
    xplr_at_server_t *server = &parser.server;

    xplrAtServerRemoveCommandFilter(server, atWifiSet);
    xplrAtServerRemoveCommandFilter(server, atWifiGet);
    xplrAtServerRemoveCommandFilter(server, atApnSet);
    xplrAtServerRemoveCommandFilter(server, atApnGet);
    xplrAtServerRemoveCommandFilter(server, atInterfaceSet);
    xplrAtServerRemoveCommandFilter(server, atInterfaceGet);
}

static void xplrAtParserRemoveThingstream(void)
{
    xplr_at_server_t *server = &parser.server;

    xplrAtServerRemoveCommandFilter(server, atTsBrokerSet);
    xplrAtServerRemoveCommandFilter(server, atTsBrokerGet);
    xplrAtServerRemoveCommandFilter(server, atRootSet);
    xplrAtServerRemoveCommandFilter(server, atRootGet);
    xplrAtServerRemoveCommandFilter(server, atTsidSet);
    xplrAtServerRemoveCommandFilter(server, atTsidGet);
    xplrAtServerRemoveCommandFilter(server, atTscertSet);
    xplrAtServerRemoveCommandFilter(server, atTscertGet);
    xplrAtServerRemoveCommandFilter(server, atTskeySet);
    xplrAtServerRemoveCommandFilter(server, atTskeyGet);
    xplrAtServerRemoveCommandFilter(server, atTsregionSet);
    xplrAtServerRemoveCommandFilter(server, atTsregionGet);
    xplrAtServerRemoveCommandFilter(server, atTsplanSet);
    xplrAtServerRemoveCommandFilter(server, atTsplanGet);
}


static void xplrAtParserRemoveNtrip(void)
{
    xplr_at_server_t *server = &parser.server;

    xplrAtServerRemoveCommandFilter(server, atNtripsrvSet);
    xplrAtServerRemoveCommandFilter(server, atNtripsrvGet);
    xplrAtServerRemoveCommandFilter(server, atNtripggaSet);
    xplrAtServerRemoveCommandFilter(server, atNtripggaGet);
    xplrAtServerRemoveCommandFilter(server, atNtripuaSet);
    xplrAtServerRemoveCommandFilter(server, atNtripuaGet);
    xplrAtServerRemoveCommandFilter(server, atNtripmpSet);
    xplrAtServerRemoveCommandFilter(server, atNtripmpGet);
    xplrAtServerRemoveCommandFilter(server, atNtripcredsSet);
    xplrAtServerRemoveCommandFilter(server, atNtripcredsGet);
}


static void xplrAtParserRemoveMisc(void)
{
    xplr_at_server_t *server = &parser.server;

    xplrAtServerRemoveCommandFilter(server, at);
    xplrAtServerRemoveCommandFilter(server, atErase);
    xplrAtServerRemoveCommandFilter(server, atStat);
    xplrAtServerRemoveCommandFilter(server, atGnssdrSet);
    xplrAtServerRemoveCommandFilter(server, atGnssdrGet);
    xplrAtServerRemoveCommandFilter(server, atSdSet);
    xplrAtServerRemoveCommandFilter(server, atSdGet);
    xplrAtServerRemoveCommandFilter(server, atCorsrcSet);
    xplrAtServerRemoveCommandFilter(server, atCorsrcGet);
    xplrAtServerRemoveCommandFilter(server, atCormodSet);
    xplrAtServerRemoveCommandFilter(server, atCormodGet);
    xplrAtServerRemoveCommandFilter(server, atHpgmodeSet);
    xplrAtServerRemoveCommandFilter(server, atHpgmodeGet);
    xplrAtServerRemoveCommandFilter(server, atBoardinfo);
    xplrAtServerRemoveCommandFilter(server, atLoc);
    xplrAtServerRemoveCommandFilter(server, atBaudSet);
    xplrAtServerRemoveCommandFilter(server, atBaudGet);
    xplrAtServerRemoveCommandFilter(server, atBoardRestart);
    xplrAtServerRemoveCommandFilter(server, atStartOnBootGet);
    xplrAtServerRemoveCommandFilter(server, atStartOnBootSet);
}

void xplrAtParserRemove(xplr_at_parser_type_t parserType)
{
    switch (parserType) {
        case XPLR_ATPARSER_ALL:
            xplrAtParserRemoveNet();
            xplrAtParserRemoveThingstream();
            xplrAtParserRemoveNtrip();
            xplrAtParserRemoveMisc();
            break;

        case XPLR_ATPARSER_NET:
            xplrAtParserRemoveNet();
            break;

        case XPLR_ATPARSER_THINGSTREAM:
            xplrAtParserRemoveThingstream();
            break;

        case XPLR_ATPARSER_NTRIP:
            xplrAtParserRemoveNtrip();
            break;

        case XPLR_ATPARSER_MISC:
            xplrAtParserRemoveMisc();
            break;

        default:
            XPLRATPARSER_CONSOLE(E, "Invalid At Parser command group");
    }
}

static inline void atParserCallbackWrapper(xplr_at_parser_t *parser,
                                           void (*callback)(void *, void *),
                                           void *callbackArg)
{
    xplr_at_parser_error_t atServerError;

    atServerError = xplrAtServerCallback(&parser->server, callback, callbackArg);
    if (atServerError != XPLR_AT_PARSER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error executing callback function");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_CALLBACK);
    } else {
        // do nothing
    }
}

static inline void atParserReturnError(xplr_at_parser_subsystem_type_t errorType)
{
    xplr_at_server_error_t error;

    // update fault struct
    xplrAtParserFaultSet(errorType);

    (void)xplrAtServerWrite(&parser.server,
                            atParserErrorResponse,
                            strlen(atParserErrorResponse));
    error = xplrAtServerGetError(&parser.server);
    if (error != XPLR_AT_SERVER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_UART);
    } else {
        // do nothing
    }
}

static inline void atParserReturnErrorBusy(xplr_at_parser_subsystem_type_t errorType)
{
    xplr_at_server_error_t error;

    // update fault struct
    xplrAtParserFaultSet(errorType);

    (void)xplrAtServerWrite(&parser.server,
                            atParserErrorBusyResponse,
                            strlen(atParserErrorBusyResponse));
    error = xplrAtServerGetError(&parser.server);
    if (error != XPLR_AT_SERVER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_UART);
    } else {
        // do nothing
    }
}

static inline void atParserReturnOk(void)
{
    xplr_at_server_error_t error;

    (void)xplrAtServerWrite(&parser.server,
                            atParserOkResponse,
                            strlen(atParserOkResponse));
    error = xplrAtServerGetError(&parser.server);
    if (error != XPLR_AT_SERVER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_UART);
    } else {
        // do nothing
    }
}

static inline void xplrAtServerWriteStrWrapper(xplr_at_parser_t *parser,
                                               const char *buffer,
                                               xplr_at_server_response_type_t responseType)
{
    xplr_at_server_error_t atServerError;
    size_t lengthBytes = strlen(buffer);

    (void)xplrAtServerWriteString(&parser->server,
                                  buffer,
                                  lengthBytes,
                                  responseType);
    atServerError = xplrAtServerGetError(&parser->server);
    if (atServerError != XPLR_AT_SERVER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_UART);
    } else {
        // do nothing
    }
}

static void atParserInstanceArrayInit(void)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;
    xplr_ntrip_config_t *ntrip = &data->correctionData.ntripConfig;

    memset(data->net.ssid, 0, sizeof(data->net.ssid));
    memset(data->net.password, 0, sizeof(data->net.password));
    memset(data->net.apn, 0, sizeof(data->net.apn));
    memset(thingstreamCfg->thingstream.pointPerfect.deviceId, 0,
           sizeof(thingstreamCfg->thingstream.pointPerfect.deviceId));
    memset(thingstreamCfg->thingstream.server.rootCa, 0,
           sizeof(thingstreamCfg->thingstream.server.rootCa));
    memset(thingstreamCfg->thingstream.pointPerfect.clientCert, 0,
           sizeof(thingstreamCfg->thingstream.pointPerfect.clientCert));
    memset(thingstreamCfg->thingstream.pointPerfect.clientKey, 0,
           sizeof(thingstreamCfg->thingstream.pointPerfect.clientKey));
    memset(ntrip->server.host, 0, sizeof(ntrip->server.host));
    memset(ntrip->server.mountpoint, 0, sizeof(ntrip->server.mountpoint));
    memset(ntrip->credentials.username, 0, sizeof(ntrip->credentials.username));
    memset(ntrip->credentials.password, 0, sizeof(ntrip->credentials.password));
}

static esp_err_t atParserInitNvs(void)
{
    esp_err_t ret;
    xplrNvs_error_t err;

    err = xplrNvsInit(&parser.data.nvs, atParserNvsNamespace);
    if (err == XPLR_NVS_OK) {
        ret = ESP_OK;
    } else {
        ret = ESP_FAIL;
    }

    return ret;
}

static esp_err_t atParserDeInitNvs(void)
{
    esp_err_t ret;
    xplrNvs_error_t err;

    err = xplrNvsDeInit(&parser.data.nvs);
    if (err == XPLR_NVS_OK) {
        ret = ESP_OK;
    } else {
        ret = ESP_FAIL;
    }

    return ret;
}


int8_t xplrAtParserInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_AT_PARSER_DEFAULT_FILENAME,
                                   XPLRLOG_FILE_SIZE_INTERVAL,
                                   XPLRLOG_NEW_FILE_ON_BOOT);
        } else {
            /* logCfg contains the instance settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   logCfg->filename,
                                   logCfg->sizeInterval,
                                   logCfg->erasePrev);
        }
        ret = logIndex;
    } else {
        /* logIndex is positive so logging has been initialized before */
        logErr = xplrLogEnable(logIndex);
        if (logErr != XPLR_LOG_OK) {
            ret = -1;
        } else {
            ret = logIndex;
        }
    }

    return ret;
}

esp_err_t xplrAtParserStopLogModule(void)
{
    esp_err_t ret;
    xplrLog_error_t logErr;

    logErr = xplrLogDisable(logIndex);
    if (logErr != XPLR_LOG_OK) {
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    return ret;
}

static inline bool xplrAtParserTryLock(bool isSetCommand)
{
    bool lockValue;
    int32_t xSemaphoreReturn;

    xSemaphoreReturn = xSemaphoreTake(mutexSemaphore, 0);
    if (xSemaphoreReturn == pdTRUE) {
        xSemaphoreReturn = xSemaphoreTake(deviceModeBusySemaphore, portMAX_DELAY);
        if (xSemaphoreReturn == pdTRUE) {
            if (isSetCommand && deviceModeBusy) {
                // Received command to alter configuration while application is running -> busy
                lockValue = false;
                xplrAtParserUnlock();
            } else {
                lockValue = true;
            }
            (void)xSemaphoreGive(deviceModeBusySemaphore);
        } else {
            xplrAtParserInternalDriverFaultReset(XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE);
            lockValue = false;
            xplrAtParserUnlock();
        }
    } else {
        lockValue = false;
    }

    return lockValue;
}

static inline void xplrAtParserUnlock(void)
{
    int32_t xSemaphoreReturn;

    xSemaphoreReturn = xSemaphoreGive(mutexSemaphore);
    if (xSemaphoreReturn != pdTRUE) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error giving semaphore");
        xplrAtParserInternalFaultSet(XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE);
    } else {
        // do nothing
    }
}

xplr_at_parser_error_t xplrAtParserLoadNvsTsCerts(void)
{
    xplrNvs_error_t nvsError;
    xplr_at_parser_error_t parserError;
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;
    xplr_thingstream_pp_settings_t *ppSettings = &thingstreamCfg->thingstream.pointPerfect;
    size_t size;

    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError = xplrNvsReadString(&data->nvs,
                                 nvsKeyRootcrt,
                                 thingstreamCfg->thingstream.server.rootCa,
                                 &size);
    size = XPLR_THINGSTREAM_CLIENTID_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientid, ppSettings->deviceId, &size);
    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientcrt, ppSettings->clientCert, &size);
    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientkey, ppSettings->clientKey, &size);

    if (nvsError != XPLR_NVS_OK) {
        XPLRATPARSER_CONSOLE(D, "Some configuration either failed to load or is not set");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

xplr_at_parser_error_t xplrAtParserLoadNvsConfig(void)
{
    xplrNvs_error_t nvsError;
    xplr_at_parser_error_t parserError;
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;
    xplr_thingstream_pp_settings_t *ppSettings = &thingstreamCfg->thingstream.pointPerfect;
    xplr_ntrip_config_t *ntrip = &data->correctionData.ntripConfig;
    size_t size;
    uint8_t uintValue;

    size = XPLR_AT_PARSER_SSID_LENGTH;
    nvsError = xplrNvsReadString(&data->nvs, nvsKeySsid, data->net.ssid, &size);
    size = XPLR_AT_PARSER_PASSWORD_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyPwd, data->net.password, &size);
    size = XPLR_AT_PARSER_APN_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyApn, data->net.apn, &size);
    size = XPLR_THINGSTREAM_URL_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyMqttBroker, ppSettings->brokerAddress, &size);
    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs,
                                  nvsKeyRootcrt,
                                  thingstreamCfg->thingstream.server.rootCa,
                                  &size);
    size = XPLR_THINGSTREAM_CLIENTID_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientid, ppSettings->deviceId, &size);
    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientcrt, ppSettings->clientCert, &size);
    size = XPLR_THINGSTREAM_CERT_SIZE_MAX;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyClientkey, ppSettings->clientKey, &size);
    size = XPLR_AT_PARSER_NTRIP_HOST_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtriphost, ntrip->server.host, &size);
    size = XPLR_AT_PARSER_NTRIP_USERAGENT_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtripua, ntrip->credentials.userAgent, &size);
    size = XPLR_AT_PARSER_NTRIP_MOUNTPOINT_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtripmp, ntrip->server.mountpoint, &size);
    size = XPLR_AT_PARSER_NTRIP_CREDENTIALS_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtripusername, ntrip->credentials.username, &size);
    size = XPLR_AT_PARSER_PASSWORD_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtrippassword, ntrip->credentials.password, &size);
    nvsError |= xplrNvsReadU16(&data->nvs, nvsKeyMqttBrokerPort, (uint16_t *) &ppSettings->brokerPort);
    nvsError |= xplrNvsReadI32(&data->nvs, nvsKeyTsregion, &thingstreamCfg->tsRegion);
    nvsError |= xplrNvsReadI32(&data->nvs, nvsKeyTsplan, &thingstreamCfg->tsPlan);
    nvsError |= xplrNvsReadU16(&data->nvs, nvsKeyNtripport, (uint16_t *) &ntrip->server.port);
    nvsError |= xplrNvsReadU8(&data->nvs, nvsKeyDeadreckoning, &uintValue);
    data->misc.dr.enable = (bool) uintValue;
    nvsError |= xplrNvsReadU8(&data->nvs, nvsKeyStartOnBoot, &uintValue);
    data->startOnBoot = (bool) uintValue;
    nvsError |= xplrNvsReadU8(&data->nvs, nvsKeySdlog, &uintValue);
    data->misc.sdLogEnable = (bool) uintValue;
    nvsError |= xplrNvsReadI32(&data->nvs, nvsKeyInterface, &data->net.interface);
    nvsError |= xplrNvsReadI32(&data->nvs, nvsKeyCorsource, &data->correctionData.correctionSource);
    nvsError |= xplrNvsReadI32(&data->nvs, nvsKeyCormod, &data->correctionData.correctionMod);

    if (nvsError != XPLR_NVS_OK) {
        XPLRATPARSER_CONSOLE(D, "Some configuration either failed to load or is not set");
        parserError = XPLR_AT_PARSER_ERROR;
    } else {
        parserError = XPLR_AT_PARSER_OK;
        // check subsystem status
        (void)xplrAtParserWifiIsReady();
        (void)xplrAtParserTsIsReady();
        (void)xplrAtParserCellIsReady();
        (void)xplrAtParserNtripIsReady();
    }

    return parserError;
}

bool xplrAtParserWifiIsReady(void)
{
    xplr_at_parser_data_t *data = &parser.data;
    bool isReady;

    if ((data->net.ssid[0] != '\0') && (data->net.interface == XPLR_ATPARSER_NET_INTERFACE_WIFI)) {
        if (data->status.wifi == XPLR_ATPARSER_STATUS_NOT_SET) {
            data->status.wifi = XPLR_ATPARSER_STATUS_READY;
        } else {
            //do nothing
        }
        isReady = true;
    } else {
        data->status.wifi = XPLR_ATPARSER_STATUS_NOT_SET;
        isReady = false;
    }

    return isReady;
}

bool xplrAtParserCellIsReady(void)
{
    xplr_at_parser_data_t *data = &parser.data;
    bool isReady;

    if ((data->net.apn[0] != '\0') && (data->net.interface == XPLR_ATPARSER_NET_INTERFACE_CELL)) {
        if (data->status.cell == XPLR_ATPARSER_STATUS_NOT_SET) {
            data->status.cell = XPLR_ATPARSER_STATUS_READY;
        } else {
            // do nothing
        }
        isReady = true;
    } else {
        data->status.cell = XPLR_ATPARSER_STATUS_NOT_SET;
        isReady = false;
    }

    return isReady;
}

bool xplrAtParserTsIsReady(void)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;
    bool isReady;

    if ((thingstreamCfg->thingstream.pointPerfect.deviceId[0] != '\0') &&
        (thingstreamCfg->thingstream.server.rootCa[0] != '\0') &&
        (thingstreamCfg->thingstream.pointPerfect.clientCert[0] != '\0') &&
        (thingstreamCfg->thingstream.pointPerfect.clientKey[0] != '\0') &&
        (thingstreamCfg->thingstream.pointPerfect.brokerAddress[0] != '\0') &&
        (thingstreamCfg->thingstream.pointPerfect.brokerPort != 0) &&
        (data->correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM)) {
        if (data->status.ts == XPLR_ATPARSER_STATUS_NOT_SET) {
            data->status.ts = XPLR_ATPARSER_STATUS_READY;
        } else {
            //do nothing
        }
        isReady = true;
    } else {
        data->status.ts = XPLR_ATPARSER_STATUS_NOT_SET;
        isReady = false;
    }

    return isReady;
}

bool xplrAtParserNtripIsReady(void)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_ntrip_config_t *ntrip = &data->correctionData.ntripConfig;
    bool isReady;

    if ((ntrip->server.mountpoint[0] != '\0') &&
        (ntrip->credentials.userAgent[0] != '\0') &&
        (ntrip->server.host[0] != '\0') &&
        (data->correctionData.correctionSource == XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP) &&
        (ntrip->server.port != 0)) {
        if (data->status.ntrip == XPLR_ATPARSER_STATUS_NOT_SET) {
            data->status.ntrip = XPLR_ATPARSER_STATUS_READY;
        } else {
            // do nothing
        }
        isReady = true;
    } else {
        data->status.ntrip = XPLR_ATPARSER_STATUS_NOT_SET;
        isReady = false;
    }

    return isReady;
}

xplr_at_parser_error_t  xplrAtParserSetNtripConfig(xplr_ntrip_config_t *ntripConfig)
{
    xplr_at_parser_error_t error;
    xplr_at_parser_data_t *data = &parser.data;
    xplr_ntrip_config_t *ntrip = &data->correctionData.ntripConfig;
    xplrNvs_error_t nvsError;

    if (ntripConfig == NULL) {
        error = XPLR_AT_PARSER_ERROR;
    } else {
        memcpy(ntrip, ntripConfig, sizeof(xplr_ntrip_config_t));

        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtriphost);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripua);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripmp);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripport);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripusername);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtrippassword);
        nvsError = xplrNvsWriteString(&data->nvs, nvsKeyNtriphost, ntrip->server.host);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyNtripua, ntrip->credentials.userAgent);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyNtripmp, ntrip->server.mountpoint);
        nvsError |= xplrNvsWriteU16(&data->nvs, nvsKeyNtripport, (uint16_t) ntrip->server.port);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyNtripusername, ntrip->credentials.username);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyNtrippassword, ntrip->credentials.password);

        if (nvsError != XPLR_NVS_OK) {
            error = XPLR_AT_PARSER_ERROR;
        } else {
            error = XPLR_AT_PARSER_OK;
            (void)xplrAtParserNtripIsReady();
        }
    }

    return error;
}

xplr_at_parser_error_t  xplrAtSetNetInterfaceConfig(xplr_at_parser_net_interface_config_t
                                                    *netInterfaceConfig)
{
    xplr_at_parser_error_t error;
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    if (netInterfaceConfig == NULL) {
        error = XPLR_AT_PARSER_ERROR;
    } else {
        memcpy(&data->net, netInterfaceConfig, sizeof(xplr_at_parser_net_interface_config_t));

        (void)xplrNvsEraseKey(&data->nvs, nvsKeySsid);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyPwd);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyApn);
        nvsError = xplrNvsWriteString(&data->nvs, nvsKeySsid, data->net.ssid);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyPwd, data->net.password);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyApn, data->net.apn);

        if (nvsError != XPLR_NVS_OK) {
            error = XPLR_AT_PARSER_ERROR;
        } else {
            error = XPLR_AT_PARSER_OK;
            (void)xplrAtParserWifiIsReady();
            (void)xplrAtParserCellIsReady();
        }
    }

    return error;
}

xplr_at_parser_error_t  xplrAtSetThingstreamConfig(xplr_at_parser_thingstream_config_t
                                                   *thingstreamConfig)
{
    xplr_at_parser_error_t error;
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;
    xplrNvs_error_t nvsError;

    if (thingstreamConfig == NULL) {
        error = XPLR_AT_PARSER_ERROR;
    } else {
        memcpy(thingstreamCfg,
               thingstreamConfig,
               sizeof(xplr_at_parser_thingstream_config_t));
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyMqttBroker);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyMqttBrokerPort);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyTsregion);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyTsplan);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientid);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientkey);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyRootcrt);
        (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientcrt);
        nvsError = xplrNvsWriteI32(&data->nvs, nvsKeyTsregion, thingstreamCfg->tsRegion);
        nvsError |= xplrNvsWriteI32(&data->nvs, nvsKeyTsplan, thingstreamCfg->tsPlan);
        nvsError |= xplrNvsWriteString(&data->nvs,
                                       nvsKeyMqttBroker,
                                       thingstreamCfg->thingstream.pointPerfect.brokerAddress);
        nvsError |= xplrNvsWriteU16(&data->nvs,
                                    nvsKeyMqttBrokerPort,
                                    thingstreamCfg->thingstream.pointPerfect.brokerPort);
        nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyClientid,
                                       thingstreamCfg->thingstream.pointPerfect.deviceId);
        nvsError |= xplrNvsWriteString(&data->nvs,
                                       nvsKeyClientkey,
                                       thingstreamCfg->thingstream.pointPerfect.clientKey);
        nvsError |= xplrNvsWriteString(&data->nvs,
                                       nvsKeyRootcrt,
                                       thingstreamCfg->thingstream.server.rootCa);
        nvsError |= xplrNvsWriteString(&data->nvs,
                                       nvsKeyClientcrt,
                                       thingstreamCfg->thingstream.pointPerfect.clientCert);

        if (nvsError != XPLR_NVS_OK) {
            error = XPLR_AT_PARSER_ERROR;
        } else {
            error = XPLR_AT_PARSER_OK;
            (void)xplrAtParserWifiIsReady();
            (void)xplrAtParserCellIsReady();
        }
    }

    return error;
}

xplr_at_parser_error_t xplrAtParserStatusUpdate(xplr_at_parser_hpg_status_type_t statusMessage,
                                                uint8_t periodSecs)
{
    xplr_at_server_error_t error;
    xplr_at_parser_error_t parserError;
    static uint64_t prevTime;

    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs)) {
        switch (statusMessage) {
            case XPLR_ATPARSER_STATHPG_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgError,
                                        strlen(statHpgMsgError));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_INIT:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgInit,
                                        strlen(statHpgMsgInit));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_CONFIG:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgConfig,
                                        strlen(statHpgMsgConfig));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_WIFI_INIT:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgWifiInit,
                                        strlen(statHpgMsgWifiInit));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_CELL_INIT:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgCellInit,
                                        strlen(statHpgMsgCellInit));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_WIFI_CONNECTED:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgWifiConnected,
                                        strlen(statHpgMsgWifiConnected));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_CELL_CONNECTED:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgCellConnected,
                                        strlen(statHpgMsgCellConnected));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_TS_CONNECTED:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgTsConnected,
                                        strlen(statHpgMsgTsConnected));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_NTRIP_CONNECTED:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgNtripConnected,
                                        strlen(statHpgMsgNtripConnected));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_WIFI_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgWifiError,
                                        strlen(statHpgMsgWifiError));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_CELL_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgCellError,
                                        strlen(statHpgMsgCellError));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_TS_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgTsError,
                                        strlen(statHpgMsgTsError));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_NTRIP_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgNtripError,
                                        strlen(statHpgMsgNtripError));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_RECONNECTING:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgReconnecting,
                                        strlen(statHpgMsgReconnecting));
                parserError = XPLR_AT_PARSER_OK;
                break;
            case XPLR_ATPARSER_STATHPG_STOP:
                (void)xplrAtServerWrite(&parser.server,
                                        statHpgMsgStop,
                                        strlen(statHpgMsgStop));
                parserError = XPLR_AT_PARSER_OK;
                break;
            default:
                XPLRATPARSER_CONSOLE(E, "Invalid message type");
                parserError = XPLR_AT_PARSER_ERROR;
        }

        error = xplrAtServerGetError(&parser.server);
        if (error != XPLR_AT_SERVER_OK) {
            XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            parserError = XPLR_AT_PARSER_ERROR;
        } else {
            // do nothing
        }
        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        parserError = XPLR_AT_PARSER_OK;
    }

    return parserError;
}

static inline void xplrAtParserFaultSet(xplr_at_parser_subsystem_type_t type)
{
    switch (type) {
        case XPLR_ATPARSER_SUBSYSTEM_ALL:
            parser.faults.value = 255; //uint8_t max
            break;
        case XPLR_ATPARSER_SUBSYSTEM_WIFI:
            parser.faults.fault.wifi = 1;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_CELL:
            parser.faults.fault.cell = 1;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_TS:
            parser.faults.fault.thingstream = 1;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_NTRIP:
            parser.faults.fault.ntrip = 1;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_GNSS:
            parser.faults.fault.gnss = 1;
            break;
        default:
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Invalid fault type");
            parser.faults.value = 255;
    }
}

static inline void xplrAtParserInternalFaultSet(xplr_at_parser_internal_driver_fault_type_t type)
{
    switch (type) {
        case XPLR_ATPARSER_INTERNAL_FAULT_ALL:
            parser.internalFaults.value = 255; //uint8_t max
            break;
        case XPLR_ATPARSER_INTERNAL_FAULT_UART:
            parser.internalFaults.fault.uart = 1;
            break;
        case XPLR_ATPARSER_INTERNAL_FAULT_CALLBACK:
            parser.internalFaults.fault.callback = 1;
            break;
        case XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE:
            parser.internalFaults.fault.semaphore = 1;
            break;
        default:
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Invalid fault type");
            parser.internalFaults.value = 255;
    }
}

static void atParserHandlerCheck(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackCheck, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackCheck(uAtClientHandle_t client, void *arg)
{
    atParserReturnOk();
    xplrAtParserUnlock();
}

static void atParserHandlerWifiSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->net.ssid,
                                       XPLR_AT_PARSER_SSID_LENGTH,
                                       false);
        error |= xplrAtServerReadString(&parser.server,
                                        data->net.password,
                                        XPLR_AT_PARSER_PASSWORD_LENGTH,
                                        false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_WIFI);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackWifiSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_WIFI);
    }
}

static void atParserCallbackWifiSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeySsid);
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyPwd);
    nvsError = xplrNvsWriteString(&data->nvs, nvsKeySsid, data->net.ssid);
    nvsError |= xplrNvsWriteString(&data->nvs, nvsKeyPwd, data->net.password);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_WIFI);
    } else {
        (void)xplrAtParserWifiIsReady();
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerWifiGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackWifiGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_WIFI);
    }
}

static void atParserCallbackWifiGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_SSID_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeySsid, data->net.ssid, &size);
    size = XPLR_AT_PARSER_PASSWORD_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyPwd, data->net.password, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_WIFI);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atWifiResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    data->net.ssid,
                                    XPLR_AT_SERVER_RESPONSE_MID);
        xplrAtServerWriteStrWrapper(&parser,
                                    data->net.password,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerErase(uAtClientHandle_t client, void *arg)
{
    int32_t error;

    /* Variable used to pass part of the AT command to the callback funtion.
    * Since the callback function is asynchronous, a static keyword is
    * required so that the data of the variable remain in memory after the
    * finished execution of this handler.
    */
    static char eraseCommand[16];

    if (xplrAtParserTryLock(true)) {
        memset(eraseCommand, 0, sizeof(eraseCommand));
        error = xplrAtServerReadString(&parser.server, eraseCommand, sizeof(eraseCommand), false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading command");
        } else {
            atParserCallbackWrapper(&parser,
                                    atParserCallbackErase,
                                    (void *) &eraseCommand);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackErase(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_thingstream_config_t *thingstreamCfg = &data->correctionData.thingstreamCfg;

    xplrNvs_error_t nvsError;
    char *eraseCommand = (char *) arg;

    if (strncmp(eraseCommand, atPartCommandWifi, strlen(atPartCommandWifi)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeySsid);
        nvsError |= xplrNvsEraseKey(&data->nvs, nvsKeyPwd);
        memset(data->net.ssid, 0x00, strlen(data->net.ssid));
        memset(data->net.password, 0x00, strlen(data->net.password));
    } else if (strncmp(eraseCommand, atPartCommandApn, strlen(atPartCommandApn)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyApn);
        memset(data->net.apn, 0x00, strlen(data->net.apn));
    } else if (strncmp(eraseCommand, atPartCommandMqttBroker, strlen(atPartCommandMqttBroker)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyMqttBroker);
        nvsError |= xplrNvsEraseKey(&data->nvs, nvsKeyMqttBrokerPort);
        memset(thingstreamCfg->thingstream.pointPerfect.brokerAddress,
               0x00,
               strlen(thingstreamCfg->thingstream.pointPerfect.brokerAddress));
    } else if (strncmp(eraseCommand, atPartCommandRootcrt, strlen(atPartCommandRootcrt)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyRootcrt);
        memset(thingstreamCfg->thingstream.server.rootCa,
               0x00,
               strlen(thingstreamCfg->thingstream.server.rootCa));
    } else if (strncmp(eraseCommand, atPartCommandTsid, strlen(atPartCommandTsid)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyClientid);
        memset(thingstreamCfg->thingstream.pointPerfect.deviceId,
               0x00,
               strlen(thingstreamCfg->thingstream.pointPerfect.deviceId));
    } else if (strncmp(eraseCommand, atPartCommandTscert, strlen(atPartCommandTscert)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyClientcrt);
        memset(thingstreamCfg->thingstream.pointPerfect.clientCert,
               0x00,
               strlen(thingstreamCfg->thingstream.pointPerfect.clientCert));
    } else if (strncmp(eraseCommand, atPartCommandTskey, strlen(atPartCommandTskey)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyClientkey);
        memset(thingstreamCfg->thingstream.pointPerfect.clientKey,
               0x00,
               strlen(thingstreamCfg->thingstream.pointPerfect.clientKey));
    } else if (strncmp(eraseCommand, atPartCommandTsregion, strlen(atPartCommandTsregion)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyTsregion);
    } else if (strncmp(eraseCommand, atPartCommandTsplan, strlen(atPartCommandTsplan)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyTsplan);
    } else if (strncmp(eraseCommand, atPartCommandNtripsrv, strlen(atPartCommandNtripsrv)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyNtriphost);
        nvsError |= xplrNvsEraseKey(&data->nvs, nvsKeyNtripport);
        memset(data->correctionData.ntripConfig.server.host,
               0x00,
               strlen(data->correctionData.ntripConfig.server.host));
    } else if (strncmp(eraseCommand, atPartCommandNtripua, strlen(atPartCommandNtripua)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyNtripua);
        memset(data->correctionData.ntripConfig.credentials.userAgent,
               0x00,
               strlen(data->correctionData.ntripConfig.credentials.userAgent));
    } else if (strncmp(eraseCommand, atPartCommandNtripmp, strlen(atPartCommandNtripmp)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyNtripmp);
        memset(data->correctionData.ntripConfig.server.mountpoint,
               0x00,
               strlen(data->correctionData.ntripConfig.server.mountpoint));
    } else if (strncmp(eraseCommand, atPartCommandNtripcreds, strlen(atPartCommandNtripcreds)) == 0) {
        nvsError = xplrNvsEraseKey(&data->nvs, nvsKeyNtripusername);
        nvsError |= xplrNvsEraseKey(&data->nvs, nvsKeyNtrippassword);
        memset(data->correctionData.ntripConfig.credentials.username,
               0x00,
               strlen(data->correctionData.ntripConfig.credentials.username));
        memset(data->correctionData.ntripConfig.credentials.password,
               0x00,
               strlen(data->correctionData.ntripConfig.credentials.password));
    } else if (strncmp(eraseCommand, atPartCommandAll, strlen(atPartCommandAll)) == 0) {
        nvsError = xplrNvsErase(&data->nvs);
    } else {
        nvsError = XPLR_NVS_ERROR;
    }

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
        // check subsystem status
        (void)xplrAtParserWifiIsReady();
        (void)xplrAtParserTsIsReady();
        (void)xplrAtParserCellIsReady();
        (void)xplrAtParserNtripIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerApnSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server, data->net.apn, XPLR_AT_PARSER_APN_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_CELL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackApnSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_CELL);
    }
}

static void atParserCallbackApnSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyApn);
    nvsError = xplrNvsWriteString(&data->nvs, nvsKeyApn, data->net.apn);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_CELL);
    } else {
        atParserReturnOk();
        (void)xplrAtParserCellIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerApnGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackApnGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_CELL);
    }
}

static void atParserCallbackApnGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_APN_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyApn, data->net.apn, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_CELL);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atApnResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    data->net.apn,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerMqttBrokerSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char strPort[XPLR_AT_PARSER_PORT_LENGTH];
    memset(strPort, 0, sizeof(strPort));

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.thingstreamCfg.thingstream.pointPerfect.brokerAddress,
                                       XPLR_THINGSTREAM_URL_SIZE_MAX,
                                       false);
        error |= xplrAtServerReadString(&parser.server,
                                        strPort,
                                        XPLR_THINGSTREAM_URL_SIZE_MAX,
                                        false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            data->correctionData.thingstreamCfg.thingstream.pointPerfect.brokerPort = strtoul(strPort, NULL,
                                                                                              10);
            atParserCallbackWrapper(&parser, atParserCallbackMqttBrokerSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}
static void atParserCallbackMqttBrokerSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyMqttBroker);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyMqttBroker,
                                  data->correctionData.thingstreamCfg.thingstream.pointPerfect.brokerAddress);
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyMqttBrokerPort);
    nvsError |= xplrNvsWriteU16(&data->nvs,
                                nvsKeyMqttBrokerPort,
                                data->correctionData.thingstreamCfg.thingstream.pointPerfect.brokerPort);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerMqttBrokerGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackMqttBrokerGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}
static void atParserCallbackMqttBrokerGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_thingstream_pp_settings_t *ppSettings =
        &data->correctionData.thingstreamCfg.thingstream.pointPerfect;
    xplrNvs_error_t nvsError;
    xplr_at_server_error_t atServerError;
    size_t size = XPLR_THINGSTREAM_URL_SIZE_MAX;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyMqttBroker, ppSettings->brokerAddress, &size);
    nvsError = xplrNvsReadU16(&data->nvs, nvsKeyMqttBrokerPort, (uint16_t *) &ppSettings->brokerPort);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atmqttBrokerResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    ppSettings->brokerAddress,
                                    XPLR_AT_SERVER_RESPONSE_MID);
        xplrAtServerWriteUint(&parser.server,
                              ppSettings->brokerPort,
                              XPLR_AT_SERVER_RESPONSE_END);
        atServerError = xplrAtServerGetError(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        } else {
            // do nothing
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerRootCrtSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.thingstreamCfg.thingstream.server.rootCa,
                                       XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackRootCrtSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackRootCrtSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyRootcrt);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyRootcrt,
                                  data->correctionData.thingstreamCfg.thingstream.server.rootCa);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerRootCrtGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackRootCrtGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackRootCrtGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *rootCa = data->correctionData.thingstreamCfg.thingstream.server.rootCa;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_THINGSTREAM_CERT_SIZE_MAX;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyRootcrt, rootCa, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atRootResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    rootCa,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientIdSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.thingstreamCfg.thingstream.pointPerfect.deviceId,
                                       XPLR_THINGSTREAM_CLIENTID_MAX,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackClientIdSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientIdSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientid);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyClientid,
                                  data->correctionData.thingstreamCfg.thingstream.pointPerfect.deviceId);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientIdGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackClientIdGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientIdGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *clientId = data->correctionData.thingstreamCfg.thingstream.pointPerfect.deviceId;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_THINGSTREAM_CLIENTID_MAX;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyClientid, clientId, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atTsidResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    clientId,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientCrtSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientCert,
                                       XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackClientCrtSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientCrtSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientcrt);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyClientcrt,
                                  data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientCert);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientCrtGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackClientCrtGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientCrtGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *clientCert = data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientCert;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_THINGSTREAM_CERT_SIZE_MAX;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyClientcrt, clientCert, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atTscertResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    clientCert,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientKeySet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientKey,
                                       XPLR_THINGSTREAM_CERT_SIZE_MAX,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackClientKeySet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientKeySet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyClientkey);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyClientkey,
                                  data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientKey);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerClientKeyGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackClientKeyGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackClientKeyGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *clientKey = data->correctionData.thingstreamCfg.thingstream.pointPerfect.clientKey;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_THINGSTREAM_CERT_SIZE_MAX;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyClientkey, clientKey, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atTskeyResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    clientKey,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerRegionSet(uAtClientHandle_t client, void *arg)
{
    xplr_thingstream_pp_region_t *tsRegion = &parser.data.correctionData.thingstreamCfg.tsRegion;
    int32_t error;
    char regionString[XPLR_AT_PARSER_TSREGION_LENGTH];
    memset(regionString, 0, sizeof(regionString));

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server, regionString, XPLR_AT_PARSER_TSREGION_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            if (strncmp(regionString, "eu", 2) == 0) {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_EU;
            } else if (strncmp(regionString, "us", 2) == 0) {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_US;
            } else if (strncmp(regionString, "au", 2) == 0) {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_AU;
            } else if (strncmp(regionString, "kr", 2) == 0) {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_KR;
            } else if (strncmp(regionString, "jp", 2) == 0) {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_JP;
            } else {
                *tsRegion = XPLR_THINGSTREAM_PP_REGION_INVALID;
            }

            atParserCallbackWrapper(&parser, atParserCallbackRegionSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackRegionSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyTsregion);
    nvsError = xplrNvsWriteI32(&data->nvs,
                               nvsKeyTsregion,
                               data->correctionData.thingstreamCfg.tsRegion);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerRegionGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackRegionGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackRegionGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_thingstream_pp_region_t *tsRegion = &data->correctionData.thingstreamCfg.tsRegion;
    xplrNvs_error_t nvsError;
    char regionString[XPLR_AT_PARSER_TSREGION_LENGTH];
    memset(regionString, 0, sizeof(regionString));

    nvsError = xplrNvsReadI32(&data->nvs,
                              nvsKeyTsregion,
                              tsRegion);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        switch (*tsRegion) {
            case XPLR_THINGSTREAM_PP_REGION_EU:
                strcpy(regionString, "eu");
                break;

            case XPLR_THINGSTREAM_PP_REGION_US:
                strcpy(regionString, "us");
                break;

            case XPLR_THINGSTREAM_PP_REGION_AU:
                strcpy(regionString, "au");
                break;

            case XPLR_THINGSTREAM_PP_REGION_KR:
                strcpy(regionString, "kr");
                break;

            case XPLR_THINGSTREAM_PP_REGION_JP:
                strcpy(regionString, "jp");
                break;

            case XPLR_THINGSTREAM_PP_REGION_INVALID:
                strcpy(regionString, "-1");
                break;
            default:
                strcpy(regionString, "-1");
        }

        xplrAtServerWriteStrWrapper(&parser,
                                    atTsregionResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    regionString,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerPlanSet(uAtClientHandle_t client, void *arg)
{
    xplr_thingstream_pp_plan_t *tsPlan = &parser.data.correctionData.thingstreamCfg.tsPlan;
    int32_t error;
    char planString[XPLR_AT_PARSER_TSPLAN_LENGTH];
    memset(planString, 0, sizeof(planString));

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server, planString, XPLR_AT_PARSER_TSPLAN_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            if (strcmp(planString, planStrIpLband) == 0) {
                *tsPlan = XPLR_THINGSTREAM_PP_PLAN_IPLBAND;
            } else if (strcmp(planString, planStrIp) == 0) {
                *tsPlan = XPLR_THINGSTREAM_PP_PLAN_IP;
            } else if (strcmp(planString, planStrLband) == 0) {
                *tsPlan = XPLR_THINGSTREAM_PP_PLAN_LBAND;
            } else {
                *tsPlan = XPLR_THINGSTREAM_PP_PLAN_INVALID;
            }

            atParserCallbackWrapper(&parser, atParserCallbackPlanSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackPlanSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyTsplan);
    nvsError = xplrNvsWriteI32(&data->nvs, nvsKeyTsplan, data->correctionData.thingstreamCfg.tsPlan);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        atParserReturnOk();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerPlanGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackPlanGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_TS);
    }
}

static void atParserCallbackPlanGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_thingstream_pp_plan_t *tsPlan = &data->correctionData.thingstreamCfg.tsPlan;
    xplrNvs_error_t nvsError;
    char planString[XPLR_AT_PARSER_TSPLAN_LENGTH];
    memset(planString, 0, sizeof(planString));

    nvsError = xplrNvsReadI32(&data->nvs, nvsKeyTsplan,
                              tsPlan);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_TS);
    } else {
        switch (*tsPlan) {
            case XPLR_THINGSTREAM_PP_PLAN_INVALID:
                strcpy(planString, invalidStr);
                break;

            case XPLR_THINGSTREAM_PP_PLAN_IPLBAND:
                strcpy(planString, planStrIpLband);
                break;

            case XPLR_THINGSTREAM_PP_PLAN_IP:
                strcpy(planString, planStrIp);
                break;

            case XPLR_THINGSTREAM_PP_PLAN_LBAND:
                strcpy(planString, planStrLband);
                break;
            default:
                strcpy(planString, invalidStr);
        }

        xplrAtServerWriteStrWrapper(&parser,
                                    atTsplanResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    planString,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripServerSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char strPort[XPLR_AT_PARSER_PORT_LENGTH];
    memset(strPort, 0, sizeof(strPort));

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.ntripConfig.server.host,
                                       XPLR_AT_PARSER_NTRIP_HOST_LENGTH,
                                       false);
        error |= xplrAtServerReadString(&parser.server, strPort, XPLR_AT_PARSER_PORT_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            data->correctionData.ntripConfig.server.port = strtoul(strPort, NULL, 10);
            atParserCallbackWrapper(&parser, atParserCallbackNtripServerSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripServerSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtriphost);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyNtriphost,
                                  data->correctionData.ntripConfig.server.host);
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripport);
    nvsError |= xplrNvsWriteU16(&data->nvs,
                                nvsKeyNtripport,
                                (uint16_t) data->correctionData.ntripConfig.server.port);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        atParserReturnOk();
        (void)xplrAtParserNtripIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripServerGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackNtripServerGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripServerGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *host = data->correctionData.ntripConfig.server.host;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_NTRIP_HOST_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyNtriphost, host, &size);
    nvsError |= xplrNvsReadU16(&data->nvs,
                               nvsKeyNtripport,
                               (uint16_t *) &data->correctionData.ntripConfig.server.port);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atNtripsrvResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    host,
                                    XPLR_AT_SERVER_RESPONSE_MID);
        xplrAtServerWriteUint(&parser.server,
                              data->correctionData.ntripConfig.server.port,
                              XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripGgaSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char ggaStr[XPLR_AT_PARSER_BOOL_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(ggaStr, 0, sizeof(ggaStr));

        error = xplrAtServerReadString(&parser.server, ggaStr, XPLR_AT_PARSER_BOOL_OPTION_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            if (strncmp(ggaStr, "1", 1) == 0) {
                data->correctionData.ntripConfig.server.ggaNecessary = true;
                atParserCallbackWrapper(&parser, atParserCallbackNtripGgaSet, NULL);
            } else if (strncmp(ggaStr, "0", 1) == 0) {
                data->correctionData.ntripConfig.server.ggaNecessary = false;
                atParserCallbackWrapper(&parser, atParserCallbackNtripGgaSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid Gga message value");
                xplrAtParserUnlock();
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackNtripGgaSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    uint8_t gga;

    gga = data->correctionData.ntripConfig.server.ggaNecessary;
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripGgaMessage);
    nvsError = xplrNvsWriteU8(&data->nvs, nvsKeyNtripGgaMessage, gga);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripGgaGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackNtripGgaGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackNtripGgaGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    xplr_at_server_error_t atServerError;
    uint8_t gga;

    nvsError = xplrNvsReadU8(&data->nvs, nvsKeyNtripGgaMessage, &gga);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        data->correctionData.ntripConfig.server.ggaNecessary = !!gga;
        xplrAtServerWriteStrWrapper(&parser,
                                    atNtripGgaResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        (void)xplrAtServerWriteUint(&parser.server,
                                    gga,
                                    XPLR_AT_SERVER_RESPONSE_END);
        atServerError = xplrAtServerGetError(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        } else {
            // do nothing
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripUserAgentSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.ntripConfig.credentials.userAgent,
                                       XPLR_AT_PARSER_NTRIP_USERAGENT_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackNtripUserAgentSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripUserAgentSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripua);
    nvsError = xplrNvsWriteString(&data->nvs, nvsKeyNtripua,
                                  data->correctionData.ntripConfig.credentials.userAgent);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        atParserReturnOk();
        (void)xplrAtParserNtripIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripUserAgentGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackNtripUserAgentGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripUserAgentGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *userAgent = data->correctionData.ntripConfig.credentials.userAgent;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_NTRIP_USERAGENT_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyNtripua, userAgent, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atNtripuaResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    userAgent,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripMountPointSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.ntripConfig.server.mountpoint,
                                       XPLR_AT_PARSER_NTRIP_MOUNTPOINT_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackNtripMountPointSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripMountPointSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripmp);
    nvsError = xplrNvsWriteString(&data->nvs, nvsKeyNtripmp,
                                  data->correctionData.ntripConfig.server.mountpoint);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        atParserReturnOk();
        (void)xplrAtParserNtripIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripMountPointGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackNtripMountPointGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripMountPointGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char *mountpoint = data->correctionData.ntripConfig.server.mountpoint;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_NTRIP_MOUNTPOINT_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyNtripmp, mountpoint, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atNtripmpResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    mountpoint,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripCredsSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;

    if (xplrAtParserTryLock(true)) {
        error = xplrAtServerReadString(&parser.server,
                                       data->correctionData.ntripConfig.credentials.username,
                                       XPLR_AT_PARSER_NTRIP_CREDENTIALS_LENGTH,
                                       false);
        error |= xplrAtServerReadString(&parser.server,
                                        data->correctionData.ntripConfig.credentials.password,
                                        XPLR_AT_PARSER_NTRIP_CREDENTIALS_LENGTH,
                                        false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            atParserCallbackWrapper(&parser, atParserCallbackNtripCredsSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripCredsSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtripusername);
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyNtrippassword);
    nvsError = xplrNvsWriteString(&data->nvs,
                                  nvsKeyNtripusername,
                                  data->correctionData.ntripConfig.credentials.username);
    nvsError |= xplrNvsWriteString(&data->nvs,
                                   nvsKeyNtrippassword,
                                   data->correctionData.ntripConfig.credentials.password);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        atParserReturnOk();
        (void)xplrAtParserNtripIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerNtripCredsGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackNtripCredsGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    }
}

static void atParserCallbackNtripCredsGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_ntrip_config_t *ntrip = &data->correctionData.ntripConfig;
    xplrNvs_error_t nvsError;
    size_t size = XPLR_AT_PARSER_NTRIP_CREDENTIALS_LENGTH;

    nvsError = xplrNvsReadString(&data->nvs, nvsKeyNtripusername, ntrip->credentials.username, &size);
    size = XPLR_AT_PARSER_PASSWORD_LENGTH;
    nvsError |= xplrNvsReadString(&data->nvs, nvsKeyNtrippassword, ntrip->credentials.password, &size);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_NTRIP);
    } else {
        xplrAtServerWriteStrWrapper(&parser,
                                    atNtripcredsResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    ntrip->credentials.username,
                                    XPLR_AT_SERVER_RESPONSE_MID);
        xplrAtServerWriteStrWrapper(&parser,
                                    ntrip->credentials.password,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerDRSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char drStr[XPLR_AT_PARSER_BOOL_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(drStr, 0, sizeof(drStr));

        error = xplrAtServerReadString(&parser.server, drStr, XPLR_AT_PARSER_BOOL_OPTION_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_GNSS);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            if (strncmp(drStr, "1", 1) == 0) {
                data->misc.dr.enable = true;
                atParserCallbackWrapper(&parser, atParserCallbackDRSet, NULL);
            } else if (strncmp(drStr, "0", 1) == 0) {
                data->misc.dr.enable = false;
                atParserCallbackWrapper(&parser, atParserCallbackDRSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid DR value");
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_GNSS);
                xplrAtParserUnlock();
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    }
}


static void atParserCallbackDRSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    uint8_t dr;

    dr = data->misc.dr.enable;
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyDeadreckoning);
    nvsError = xplrNvsWriteU8(&data->nvs, nvsKeyDeadreckoning, dr);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerDRGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackDRGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    }
}

static void atParserCallbackDRGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    xplr_at_server_error_t atServerError;
    uint8_t dr;

    nvsError = xplrNvsReadU8(&data->nvs, nvsKeyDeadreckoning, &dr);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    } else {
        data->misc.dr.enable = !!dr;
        xplrAtServerWriteStrWrapper(&parser,
                                    atGnssdrResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        (void)xplrAtServerWriteUint(&parser.server,
                                    dr,
                                    XPLR_AT_SERVER_RESPONSE_END);
        atServerError = xplrAtServerGetError(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        } else {
            // do nothing
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerSDSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char sdStr[XPLR_AT_PARSER_BOOL_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(sdStr, 0, sizeof(sdStr));

        error = xplrAtServerReadString(&parser.server, sdStr, XPLR_AT_PARSER_BOOL_OPTION_LENGTH, false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            if (strncmp(sdStr, "1", 1) == 0) {
                data->misc.sdLogEnable = true;
                atParserCallbackWrapper(&parser, atParserCallbackSDSet, NULL);
            } else if (strncmp(sdStr, "0", 1) == 0) {
                data->misc.sdLogEnable = false;
                atParserCallbackWrapper(&parser, atParserCallbackSDSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid SD log value");
                xplrAtParserUnlock();
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackSDSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    uint8_t sd;

    sd = data->misc.sdLogEnable;
    (void)xplrNvsEraseKey(&data->nvs, nvsKeySdlog);
    nvsError = xplrNvsWriteU8(&data->nvs, nvsKeySdlog, sd);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerSDGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackSDGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackSDGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    xplr_at_server_error_t atServerError;
    uint8_t sd;

    nvsError = xplrNvsReadU8(&data->nvs, nvsKeySdlog, &sd);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        data->misc.sdLogEnable = !!sd;
        xplrAtServerWriteStrWrapper(&parser,
                                    atSdResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        (void)xplrAtServerWriteUint(&parser.server,
                                    sd,
                                    XPLR_AT_SERVER_RESPONSE_END);
        atServerError = xplrAtServerGetError(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        } else {
            // do nothing
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerBaudrateSet(uAtClientHandle_t client, void *arg)
{
    int32_t error;
    char baudrateStr[XPLR_AT_PARSER_USER_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(baudrateStr, 0, sizeof(baudrateStr));

        error = xplrAtServerReadString(&parser.server,
                                       baudrateStr,
                                       XPLR_AT_PARSER_USER_OPTION_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            parser.server.uartCfg->baudRate = strtol(baudrateStr, NULL, 10);
            atParserCallbackWrapper(&parser, atParserCallbackBaudrateSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackBaudrateSet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtServerUartReconfig(&parser.server) != XPLR_AT_SERVER_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerBaudrateGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackBaudrateGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackBaudrateGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_server_error_t atServerError;

    xplrAtServerWriteStrWrapper(&parser,
                                atBaudResponse,
                                XPLR_AT_SERVER_RESPONSE_START);
    (void)xplrAtServerWriteUint(&parser.server,
                                parser.server.uartCfg->baudRate,
                                XPLR_AT_SERVER_RESPONSE_END);
    atServerError = xplrAtServerGetError(&parser.server);
    if (atServerError != XPLR_AT_SERVER_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
        xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        // do nothing
    }
    xplrAtParserUnlock();
}

static void atParserHandlerInterfaceSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char strInterface[XPLR_AT_PARSER_USER_OPTION_LENGTH];
    bool validInterface;

    if (xplrAtParserTryLock(true)) {
        memset(strInterface, 0, sizeof(strInterface));

        error = xplrAtServerReadString(&parser.server,
                                       strInterface,
                                       XPLR_AT_PARSER_USER_OPTION_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            if (strcmp(strInterface, interfaceStrWifi) == 0) {
                data->net.interface = XPLR_ATPARSER_NET_INTERFACE_WIFI;
                validInterface = true;
            } else if (strcmp(strInterface, interfaceStrCell) == 0) {
                data->net.interface = XPLR_ATPARSER_NET_INTERFACE_CELL;
                validInterface = true;
            } else {
                validInterface = false;
            }
            if (validInterface) {
                atParserCallbackWrapper(&parser, atParserCallbackInterfaceSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid interface");
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
                xplrAtParserUnlock();
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackInterfaceSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyInterface);
    nvsError = xplrNvsWriteI32(&data->nvs, nvsKeyInterface, data->net.interface);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
        (void)xplrAtParserWifiIsReady();
        (void)xplrAtParserCellIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerInterfaceGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackInterfaceGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackInterfaceGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_net_interface_type_t *interface = &data->net.interface;
    xplrNvs_error_t nvsError;
    char strInterface[XPLR_AT_PARSER_USER_OPTION_LENGTH];
    memset(strInterface, 0, sizeof(strInterface));

    nvsError = xplrNvsReadI32(&data->nvs, nvsKeyInterface, interface);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        switch (*interface) {
            case XPLR_ATPARSER_NET_INTERFACE_WIFI:
                strcpy(strInterface, interfaceStrWifi);
                break;

            case XPLR_ATPARSER_NET_INTERFACE_CELL:
                strcpy(strInterface, interfaceStrCell);
                break;

            case XPLR_ATPARSER_NET_INTERFACE_INVALID:
                strcpy(strInterface, invalidStr);
                break;
            default:
                strcpy(strInterface, invalidStr);
        }

        xplrAtServerWriteStrWrapper(&parser,
                                    atInterfaceResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    strInterface,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerCorrectionSourceSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    bool validCorrectionSource;
    char correctionSource[XPLR_AT_PARSER_USER_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(correctionSource, 0, sizeof(correctionSource));

        error = xplrAtServerReadString(&parser.server,
                                       correctionSource,
                                       XPLR_AT_PARSER_USER_OPTION_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            if (strcmp(correctionSource, corSrcStrTs) == 0) {
                data->correctionData.correctionSource = XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM;
                validCorrectionSource = true;
            } else if (strcmp(correctionSource, corSrcStrNtrip) == 0) {
                data->correctionData.correctionSource = XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP;
                validCorrectionSource = true;
            } else {
                validCorrectionSource = false;
            }
            if (validCorrectionSource) {
                atParserCallbackWrapper(&parser, atParserCallbackCorrectionSourceSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid correction source");
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
                xplrAtParserUnlock();
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackCorrectionSourceSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyCorsource);
    nvsError = xplrNvsWriteI32(&data->nvs,
                               nvsKeyCorsource,
                               data->correctionData.correctionSource);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
        (void)xplrAtParserNtripIsReady();
        (void)xplrAtParserTsIsReady();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerCorrectionSourceGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackCorrectionSourceGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackCorrectionSourceGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_correction_source_type_t *correctionSourceLocal =
        &data->correctionData.correctionSource;
    xplrNvs_error_t nvsError;
    char correctionSource[XPLR_AT_PARSER_USER_OPTION_LENGTH];
    memset(correctionSource, 0, sizeof(correctionSource));

    nvsError = xplrNvsReadI32(&data->nvs, nvsKeyCorsource,
                              correctionSourceLocal);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        switch (*correctionSourceLocal) {
            case XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM:
                strcpy(correctionSource, corSrcStrTs);
                break;

            case XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP:
                strcpy(correctionSource, corSrcStrNtrip);
                break;

            case XPLR_ATPARSER_CORRECTION_SOURCE_INVALID:
                strcpy(correctionSource, invalidStr);
                break;
            default:
                strcpy(correctionSource, invalidStr);
        }

        xplrAtServerWriteStrWrapper(&parser,
                                    atCorsrcResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    correctionSource,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerCorrectionModSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    bool validCorrectionMod;
    char correctionMod[XPLR_AT_PARSER_TSPLAN_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(correctionMod, 0, sizeof(correctionMod));

        error = xplrAtServerReadString(&parser.server,
                                       correctionMod,
                                       XPLR_AT_PARSER_TSPLAN_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading AT command");
        } else {
            if (strcmp(correctionMod, planStrIp) == 0) {
                data->correctionData.correctionMod = XPLR_ATPARSER_CORRECTION_MOD_IP;
                validCorrectionMod = true;
            } else if (strcmp(correctionMod, planStrLband) == 0) {
                data->correctionData.correctionMod = XPLR_ATPARSER_CORRECTION_MOD_LBAND;
                validCorrectionMod = true;
            } else {
                validCorrectionMod = false;
            }
            if (validCorrectionMod) {
                atParserCallbackWrapper(&parser, atParserCallbackCorrectionModSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid correction mode");
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
                xplrAtParserUnlock();
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackCorrectionModSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;

    (void)xplrNvsEraseKey(&data->nvs, nvsKeyCormod);
    nvsError = xplrNvsWriteI32(&data->nvs,
                               nvsKeyCormod,
                               data->correctionData.correctionMod);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerCorrectionModGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackCorrectionModGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}
static void atParserCallbackCorrectionModGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_correction_mod_type_t *correctionModLocal =
        &data->correctionData.correctionMod;
    xplrNvs_error_t nvsError;
    char correctionMod[XPLR_AT_PARSER_USER_OPTION_LENGTH];
    memset(correctionMod, 0, sizeof(correctionMod));

    nvsError = xplrNvsReadI32(&data->nvs, nvsKeyCormod,
                              correctionModLocal);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        switch (*correctionModLocal) {
            case XPLR_ATPARSER_CORRECTION_MOD_IP:
                strcpy(correctionMod, planStrIp);
                break;

            case XPLR_ATPARSER_CORRECTION_MOD_LBAND:
                strcpy(correctionMod, planStrLband);
                break;

            case XPLR_ATPARSER_CORRECTION_MOD_INVALID:
                strcpy(correctionMod, invalidStr);
                break;
            default:
                strcpy(correctionMod, invalidStr);
        }

        xplrAtServerWriteStrWrapper(&parser,
                                    atCormodResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        xplrAtServerWriteStrWrapper(&parser,
                                    correctionMod,
                                    XPLR_AT_SERVER_RESPONSE_END);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerDeviceModeSet(uAtClientHandle_t client, void *arg)
{
    int32_t error, xSemaphoreReturn;
    xplr_at_parser_data_t *data = &parser.data;
    char deviceMode[XPLR_AT_PARSER_USER_OPTION_LENGTH];

    if (xplrAtParserTryLock(false)) {
        memset(deviceMode, 0, sizeof(deviceMode));

        error = xplrAtServerReadString(&parser.server,
                                       deviceMode,
                                       XPLR_AT_PARSER_USER_OPTION_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            if (strcmp(deviceMode, modeStrConfig) == 0) {
                data->mode = XPLR_ATPARSER_MODE_CONFIG;
            } else if (strcmp(deviceMode, modeStrStart) == 0) {
                data->mode = XPLR_ATPARSER_MODE_START;
                xSemaphoreReturn = xSemaphoreTake(deviceModeBusySemaphore, portMAX_DELAY);
                if (xSemaphoreReturn == pdTRUE) {
                    deviceModeBusy = true;
                    (void)xSemaphoreGive(deviceModeBusySemaphore);
                } else {
                    xplrAtParserInternalDriverFaultReset(XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE);
                }
            } else if (strcmp(deviceMode, modeStrStop) == 0) {
                data->mode = XPLR_ATPARSER_MODE_STOP;
                xSemaphoreReturn = xSemaphoreTake(deviceModeBusySemaphore, portMAX_DELAY);
                if (xSemaphoreReturn == pdTRUE) {
                    deviceModeBusy = false;
                    (void)xSemaphoreGive(deviceModeBusySemaphore);
                } else {
                    xplrAtParserInternalDriverFaultReset(XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE);
                }
            } else if (strcmp(deviceMode, modeStrError) == 0) {
                data->mode = XPLR_ATPARSER_MODE_ERROR;
            } else {
                data->mode = XPLR_ATPARSER_MODE_INVALID;
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid device mode");
                xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
            }

            atParserCallbackWrapper(&parser, atParserCallbackDeviceModeSet, NULL);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackDeviceModeSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;

    if (data->mode != XPLR_ATPARSER_MODE_INVALID) {
        atParserReturnOk();
    } else {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
    xplrAtParserUnlock();
}

static void atParserHandlerDeviceModeGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackDeviceModeGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackDeviceModeGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char deviceMode[XPLR_AT_PARSER_USER_OPTION_LENGTH];
    memset(deviceMode, 0, sizeof(deviceMode));

    switch (data->mode) {
        case XPLR_ATPARSER_MODE_CONFIG:
            strcpy(deviceMode, modeStrConfig);
            break;
        case XPLR_ATPARSER_MODE_START:
            strcpy(deviceMode, modeStrStart);
            break;
        case XPLR_ATPARSER_MODE_STOP:
            strcpy(deviceMode, modeStrStop);
            break;
        case XPLR_ATPARSER_MODE_ERROR:
            strcpy(deviceMode, modeStrError);
            break;
        case XPLR_ATPARSER_MODE_INVALID:
            strcpy(deviceMode, invalidStr);
            break;
        default:
            strcpy(deviceMode, invalidStr);
    }

    xplrAtServerWriteStrWrapper(&parser,
                                atHpgmodeResponse,
                                XPLR_AT_SERVER_RESPONSE_START);
    xplrAtServerWriteStrWrapper(&parser,
                                deviceMode,
                                XPLR_AT_SERVER_RESPONSE_END);
    xplrAtParserUnlock();
}

static void atParserHandlerStartOnBootSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    int32_t error;
    char startOnBootStr[XPLR_AT_PARSER_BOOL_OPTION_LENGTH];

    if (xplrAtParserTryLock(true)) {
        memset(startOnBootStr, 0, sizeof(startOnBootStr));

        error = xplrAtServerReadString(&parser.server,
                                       startOnBootStr,
                                       XPLR_AT_PARSER_BOOL_OPTION_LENGTH,
                                       false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading At command");
        } else {
            if (strncmp(startOnBootStr, "1", 1) == 0) {
                data->startOnBoot = true;
                atParserCallbackWrapper(&parser, atParserCallbackStartOnBootSet, NULL);
            } else if (strncmp(startOnBootStr, "0", 1) == 0) {
                data->startOnBoot = false;
                atParserCallbackWrapper(&parser, atParserCallbackStartOnBootSet, NULL);
            } else {
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid bool value");
                atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
                xplrAtParserUnlock();
            }
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackStartOnBootSet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    uint8_t startOnBoot;

    startOnBoot = data->startOnBoot;
    (void)xplrNvsEraseKey(&data->nvs, nvsKeyStartOnBoot);
    nvsError = xplrNvsWriteU8(&data->nvs, nvsKeyStartOnBoot, startOnBoot);

    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        atParserReturnOk();
    }
    xplrAtParserUnlock();
}

static void atParserHandlerStartOnBootGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackStartOnBootGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackStartOnBootGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    xplrNvs_error_t nvsError;
    xplr_at_server_error_t atServerError;
    uint8_t startOnBoot;

    nvsError = xplrNvsReadU8(&data->nvs, nvsKeyStartOnBoot, &startOnBoot);
    if (nvsError != XPLR_NVS_OK) {
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    } else {
        data->startOnBoot = !!startOnBoot;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStartOnBootResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
        (void)xplrAtServerWriteUint(&parser.server,
                                    startOnBoot,
                                    XPLR_AT_SERVER_RESPONSE_END);
        atServerError = xplrAtServerGetError(&parser.server);
        if (atServerError != XPLR_AT_SERVER_OK) {
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error writing AT response");
            xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        } else {
            // do nothing
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerBoardInfoGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackBoardInfoGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackBoardInfoGet(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;
    char buff_to_print[64], mac[8];

    memset(buff_to_print, 0, sizeof(buff_to_print));
    memset(mac, 0, sizeof(mac));
    xplrAtServerWriteStrWrapper(&parser,
                                atBoardInfoResponse,
                                XPLR_AT_SERVER_RESPONSE_START);

    xplrBoardGetInfo(XPLR_BOARD_INFO_NAME, buff_to_print);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_VERSION, buff_to_print);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_MCU, buff_to_print);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrGetDeviceMac((uint8_t *)mac);
    snprintf(buff_to_print,
             sizeof(buff_to_print),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_FLASH_SIZE, buff_to_print);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));
    xplrBoardGetInfo(XPLR_BOARD_INFO_RAM_SIZE, buff_to_print);
    xplrAtServerWriteStrWrapper(&parser,
                                buff_to_print,
                                XPLR_AT_SERVER_RESPONSE_MID);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrAtServerWriteStrWrapper(&parser,
                                data->dvcInfoGnss.ver.mod,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->dvcInfoGnss.ver.ver,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->dvcInfoLband.ver.mod,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->dvcInfoLband.ver.ver,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->cellInfo.cellModel,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->cellInfo.cellFw,
                                XPLR_AT_SERVER_RESPONSE_MID);

    xplrAtServerWriteStrWrapper(&parser,
                                data->cellInfo.cellImei,
                                XPLR_AT_SERVER_RESPONSE_END);
    xplrAtParserUnlock();
}

static void atParserHandlerStatusGet(uAtClientHandle_t client, void *arg)
{
    int32_t error;

    /* Variable used to pass part of the AT command to the callback funtion.
    * Since the callback function is asynchronous, a static keyword is
    * required so that the data of the variable remain in memory after the
    * finished execution of this handler.
    */
    static char statusCommand[8];
    if (xplrAtParserTryLock(false)) {
        memset(statusCommand, 0, sizeof(statusCommand));
        error = xplrAtServerReadString(&parser.server, statusCommand, sizeof(statusCommand), false);
        if (error < 0) {
            atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
            xplrAtParserUnlock();
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Error reading command");
        } else {
            atParserCallbackWrapper(&parser,
                                    atParserCallbackStatusGet,
                                    (void *) &statusCommand);
        }
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackStatusGet(uAtClientHandle_t client, void *arg)
{
    char *statusCommand = (char *) arg;
    int subsystemStatus;
    xplr_at_parser_data_t *data = &parser.data;
    xplr_at_parser_error_t error = XPLR_AT_PARSER_OK;

    if (strncmp(statusCommand, atPartWifi, strlen(atPartWifi)) == 0) {
        (void)xplrAtParserWifiIsReady();
        subsystemStatus = data->status.wifi;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStatwifiResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
    } else if (strncmp(statusCommand, atPartCell, strlen(atPartCell)) == 0) {
        (void)xplrAtParserCellIsReady();
        subsystemStatus = data->status.cell;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStatcellResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
    } else if (strncmp(statusCommand, atPartTs, strlen(atPartTs)) == 0) {
        (void)xplrAtParserTsIsReady();
        subsystemStatus = data->status.ts;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStattsResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
    } else if (strncmp(statusCommand, atPartNtrip, strlen(atPartNtrip)) == 0) {
        (void)xplrAtParserNtripIsReady();
        subsystemStatus = data->status.ntrip;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStatntripResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
    } else if (strncmp(statusCommand, atPartGnss, strlen(atPartGnss)) == 0) {
        (void)xplrAtParserNtripIsReady();
        subsystemStatus = data->status.gnss;
        xplrAtServerWriteStrWrapper(&parser,
                                    atStatgnssResponse,
                                    XPLR_AT_SERVER_RESPONSE_START);
    } else {
        error = XPLR_AT_PARSER_ERROR;
    }

    if (error == XPLR_AT_PARSER_ERROR) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Invalid AT command");
        atParserReturnError(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        switch (subsystemStatus) {
            case XPLR_ATPARSER_STATUS_ERROR:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrError,
                                        strlen(statusStrError));
                break;

            case XPLR_ATPARSER_STATUS_NOT_SET:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrNotSet,
                                        strlen(statusStrNotSet));
                break;

            case XPLR_ATPARSER_STATUS_READY:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrReady,
                                        strlen(statusStrReady));
                break;

            case XPLR_ATPARSER_STATUS_INIT:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrInit,
                                        strlen(statusStrInit));
                break;

            case XPLR_ATPARSER_STATUS_CONNECTING:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrConnecting,
                                        strlen(statusStrConnecting));
                break;

            case XPLR_ATPARSER_STATUS_CONNECTED:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrConnected,
                                        strlen(statusStrConnected));
                break;

            case XPLR_ATPARSER_STATUS_RECONNECTING:
                (void)xplrAtServerWrite(&parser.server,
                                        statusStrReconnecting,
                                        strlen(statusStrReconnecting));
                break;
            default:
                //! BUG: results to spinlock if enabled
                //XPLRATPARSER_CONSOLE(E, "Invalid subsystem status");
                xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
        }
    }
    xplrAtParserUnlock();
}

static void atParserHandlerLocationGet(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackLocationGet, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_GNSS);
    }
}

static void atParserCallbackLocationGet(uAtClientHandle_t client, void *arg)
{
    char buffer[32];
    esp_err_t ret;
    xplrGnssLocation_t *locData = &parser.data.location;
    memset(buffer, 0, sizeof(buffer));

    xplrAtServerWriteStrWrapper(&parser,
                                atLocResponse,
                                XPLR_AT_SERVER_RESPONSE_START);
    ret = xplrTimestampToDateTime(locData->location.timeUtc,
                                  buffer,
                                  ELEMENTCNT(buffer));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    ret |= xplrGnssFixTypeToString(locData, buffer, ELEMENTCNT(buffer));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->location.latitudeX1e7  * (1e-7));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->location.longitudeX1e7  * (1e-7));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->location.altitudeMillimetres  * (1e-3));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->location.speedMillimetresPerSecond  * (1e-3));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->location.radiusMillimetres  * (1e-3));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->accuracy.horizontal  * (1e-4));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    snprintf(buffer,
             sizeof(buffer),
             "%f",
             locData->accuracy.vertical  * (1e-4));
    xplrAtServerWriteStrWrapper(&parser,
                                buffer,
                                XPLR_AT_SERVER_RESPONSE_MID);
    (void)xplrAtServerWriteInt(&parser.server,
                               locData->location.svs,
                               XPLR_AT_SERVER_RESPONSE_END);

    if (ret != ESP_OK) {
        //! BUG: results to spinlock if enabled
        //XPLRATPARSER_CONSOLE(E, "Error printing location");
        xplrAtParserFaultSet(XPLR_ATPARSER_SUBSYSTEM_ALL);
    } else {
        // do nothing
    }
    xplrAtParserUnlock();
}

static void atParserHandlerBoardRestart(uAtClientHandle_t client, void *arg)
{
    if (xplrAtParserTryLock(false)) {
        atParserCallbackWrapper(&parser, atParserCallbackBoardRestart, NULL);
    } else {
        atParserReturnErrorBusy(XPLR_ATPARSER_SUBSYSTEM_ALL);
    }
}

static void atParserCallbackBoardRestart(uAtClientHandle_t client, void *arg)
{
    xplr_at_parser_data_t *data = &parser.data;

    data->restartSignal = true;
    atParserReturnOk();
    xplrAtParserUnlock();
}

xplr_at_parser_error_t xplrAtParserFaultReset(xplr_at_parser_subsystem_type_t type)
{
    xplr_at_parser_error_t error;

    switch (type) {
        case XPLR_ATPARSER_SUBSYSTEM_ALL:
            parser.faults.value = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_SUBSYSTEM_WIFI:
            parser.faults.fault.wifi = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_SUBSYSTEM_CELL:
            parser.faults.fault.cell = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_SUBSYSTEM_TS:
            parser.faults.fault.thingstream = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_SUBSYSTEM_NTRIP:
            parser.faults.fault.ntrip = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_SUBSYSTEM_GNSS:
            parser.faults.fault.gnss = 0;
            error = XPLR_AT_PARSER_OK;
            break;
        default:
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Invalid fault type");
            error = XPLR_AT_PARSER_ERROR;
    }

    return error;
}

xplr_at_parser_error_t xplrAtParserInternalDriverFaultReset(
    xplr_at_parser_internal_driver_fault_type_t type)
{
    xplr_at_parser_error_t error;

    switch (type) {
        case XPLR_ATPARSER_INTERNAL_FAULT_ALL:
            parser.internalFaults.value = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_INTERNAL_FAULT_UART:
            parser.internalFaults.fault.uart = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_INTERNAL_FAULT_CALLBACK:
            parser.internalFaults.fault.callback = 0;
            error = XPLR_AT_PARSER_OK;
            break;

        case XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE:
            parser.internalFaults.fault.semaphore = 0;
            error = XPLR_AT_PARSER_OK;
            break;
        default:
            //! BUG: results to spinlock if enabled
            //XPLRATPARSER_CONSOLE(E, "Invalid fault type");
            error = XPLR_AT_PARSER_ERROR;
    }

    return error;
}

void xplrAtParserSetSubsystemStatus(xplr_at_parser_subsystem_type_t subsystem,
                                    xplr_at_parser_status_type_t newStatus)
{
    xplr_at_parser_data_t *data = &parser.data;

    switch (subsystem) {
        case XPLR_ATPARSER_SUBSYSTEM_ALL:
            data->status.wifi = newStatus;
            data->status.cell = newStatus;
            data->status.ts = newStatus;
            data->status.ntrip = newStatus;
            data->status.gnss = newStatus;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_WIFI:
            data->status.wifi = newStatus;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_CELL:
            data->status.cell = newStatus;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_TS:
            data->status.ts = newStatus;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_NTRIP:
            data->status.ntrip = newStatus;
            break;
        case XPLR_ATPARSER_SUBSYSTEM_GNSS:
            data->status.gnss = newStatus;
            break;
        default:
            XPLRATPARSER_CONSOLE(E, "Invalid subsystem type");
    }
}

xplr_at_parser_error_t setDeviceModeBusyStatus(bool isDeviceModeBusy)
{
    xplr_at_parser_error_t error;
    int32_t xSemaphoreReturn;

    xSemaphoreReturn = xSemaphoreTake(deviceModeBusySemaphore, portMAX_DELAY);
    if (xSemaphoreReturn == pdTRUE) {
        deviceModeBusy = isDeviceModeBusy;
        xSemaphoreReturn = xSemaphoreGive(deviceModeBusySemaphore);
        if (xSemaphoreReturn != pdTRUE) {
            error = XPLR_AT_PARSER_ERROR;
        } else {
            error = XPLR_AT_PARSER_OK;
        }
    } else {
        error = XPLR_AT_PARSER_ERROR;
    }

    return error;
}