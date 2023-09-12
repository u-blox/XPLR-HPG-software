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
#include "esp_task_wdt.h"
#include "xplr_com.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRCOM_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRCOM_LOG_ACTIVE))
#define XPLRCOM_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgCom", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRCOM_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
#define XPLRCOM_CONSOLE(tag, message, ...)  esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgCom", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "hpgCom", __FUNCTION__, __LINE__, ## __VA_ARGS__);\
    XPLRLOG(&cellLog,buff2Log);
#elif ((0 == XPLRCOM_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
#define XPLRCOM_CONSOLE(tag, message, ...)\
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), "[(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "hpgCom", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    XPLRLOG(&cellLog,buff2Log);
#else
#define XPLRCOM_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

typedef struct xplrCom_type {
    uDeviceHandle_t                 handler;        /**< ubxlib device handler */
    uDeviceCfg_t                    deviceSettings; /**< ubxlib device settings */
    uNetworkType_t                  deviceNetwork;  /**< ubxlib device network type */
    xplrCom_cell_config_t          *cellSettings;   /**< cell module settings */
    xplrCom_cell_connect_t          cellFsm[2];     /**< cell fsm array.
                                                         element 0 holds most current state.
                                                         element 1 holds previous state */
    int8_t                          retries;
} xplrCom_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

const char *const ratStr[] = {"unknown or not used",
                              "GSM/GPRS/EGPRS",
                              "GSM Compact",
                              "UTRAN",
                              "EGPRS",
                              "HSDPA",
                              "HSUPA",
                              "HSDPA/HSUPA",
                              "LTE",
                              "EC GSM",
                              "CAT-M1",
                              "NB1"
                             };

const char *const netStatStr[] = {"unknown",
                                  "not registered",
                                  "registered home",
                                  "searching",
                                  "registration denied",
                                  "out of coverage",
                                  "registered - roaming",
                                  "registered sms only - home",
                                  "registered sms only - roaming",
                                  "emergency only",
                                  "registered no csfb - home",
                                  "registered no csfb - roaming",
                                  "temporary network barring"
                                 };

xplrCom_t comDevices[XPLRCOM_NUMOF_DEVICES] = {NULL};
xplrCom_cell_netInfo_t currentNetInfo;
xplrCom_cell_connect_t cellFsm = XPLR_COM_CELL_CONNECT_ERROR;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
static xplrLog_t cellLog;
static char buff2Log[XPLRLOG_BUFFER_SIZE_SMALL];
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static int8_t          dvcGetFirstFreeSlot(void);
static xplrCom_error_t dvcRemoveSlot(int8_t index);
static xplrCom_error_t dvcRemoveAllSlots(void);
static xplrCom_error_t cellSetConfig(xplrCom_cell_config_t *cfg);
static xplrCom_error_t cellDvcCheckReady(int8_t index);
static xplrCom_error_t cellDvcRebootNeeded(int8_t index);
static xplrCom_error_t cellDvcReset(int8_t index, bool por);
static xplrCom_error_t cellDvcOpen(int8_t index);
static xplrCom_error_t cellDvcSetMno(int8_t index);
static xplrCom_error_t cellDvcSetRat(int8_t index);
static xplrCom_error_t cellDvcSetBands(int8_t index);
static xplrCom_error_t cellDvcRegister(int8_t index);
static void            cellDvcGetNetworkInfo(int8_t index, xplrCom_cell_netInfo_t *info);

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

bool cbCellWait(void (*ptr));

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCom_error_t xplrUbxlibInit(void)
{
    xplrCom_error_t ret;
    int32_t ubxlibRes;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
    xplrLog_error_t err;
    err = xplrLogInit(&cellLog, XPLR_LOG_DEVICE_INFO, "/cell.log", 100, XPLR_SIZE_MB);
    if (err == XPLR_LOG_OK) {
        cellLog.logEnable = true;
    } else {
        cellLog.logEnable = false;
    }
#endif

    ubxlibRes = uPortInit();

    if (ubxlibRes == 0) {
        XPLRCOM_CONSOLE(D, "ubxlib init ok");
        ubxlibRes = uDeviceInit();
        if (ubxlibRes == 0) {
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(D, "ubxlib dvc init ok");
        } else {
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(E, "error initializing dvc (%d)", ubxlibRes);
        }
    } else {
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(E, "error initializing ubxlib (%d)", ubxlibRes);
    }

    return ret;
}

void xplrUbxlibDeInit(void)
{
    uPortDeinit();
    XPLRCOM_CONSOLE(D, "ubxlib de-init ok");
}

xplrCom_error_t xplrComDeInit(void)
{
    xplrCom_error_t ret;

    ret = dvcRemoveAllSlots();
    if (ret == XPLR_COM_OK) {
        XPLRCOM_CONSOLE(D, "com service de-init ok");
    } else {
        XPLRCOM_CONSOLE(E, "error removing com service");
    }

    return ret;
}

xplrCom_error_t xplrComCellInit(xplrCom_cell_config_t *cfg)
{
    xplrCom_error_t ret;
    int8_t profileIndex;

    /* check for empty profile*/
    profileIndex = dvcGetFirstFreeSlot();
    if (profileIndex != -1) {
        cfg->profileIndex = profileIndex;
        ret = cellSetConfig(cfg);
        if (ret == XPLR_COM_OK) {
            XPLRCOM_CONSOLE(D, "ok, module settings configured");
        } else {
            XPLRCOM_CONSOLE(E, "error, cell settings could not be initialized");
        }
    } else {
        ret = XPLR_COM_ERROR;
    }

    return ret;
}

xplrCom_error_t xplrComCellDeInit(int8_t dvcProfile)
{
    xplrCom_error_t ret;

    ret = dvcRemoveSlot(dvcProfile);
    if (ret == XPLR_COM_OK) {
        XPLRCOM_CONSOLE(D, "removed cell dvc from slot %d", dvcProfile);
    } else {
        XPLRCOM_CONSOLE(E, "error removing cell dvc %d", dvcProfile);
    }

    return ret;
}

uDeviceHandle_t xplrComGetDeviceHandler(int8_t dvcProfile)
{
    return comDevices[dvcProfile].handler;
}

xplrCom_error_t xplrComCellFsmConnect(int8_t dvcProfile)
{
    xplrCom_error_t ret;
    xplrCom_cell_connect_t *fsm = comDevices[dvcProfile].cellFsm;

    switch (fsm[0]) {
        case XPLR_COM_CELL_CONNECT_INIT:
        case XPLR_COM_CELL_CONNECT_OPENDEVICE:
            fsm[1] = fsm[0]; /* hold current state */
            ret = cellDvcOpen(dvcProfile); /* open device */
            if (ret == XPLR_COM_OK) {
                fsm[0] = XPLR_COM_CELL_CONNECT_SET_MNO; /* dvc open, continue with MNO */
                XPLRCOM_CONSOLE(D, "open ok, configuring MNO");
            } else {
                fsm[0] = XPLR_COM_CELL_CONNECT_ERROR; /* failed to open, go to error state */
                XPLRCOM_CONSOLE(E, "open failed with code: %d", ret);
            }
            break;
        case XPLR_COM_CELL_CONNECT_SET_MNO:
            fsm[1] = fsm[0]; /* hold current state */
            /* not all modules are capable to change MNO profile, handle it */
            ret = cellDvcSetMno(dvcProfile); /* check if MNO needs to be changed */
            if (ret == XPLR_COM_OK) {
                fsm[0] = XPLR_COM_CELL_CONNECT_CHECK_READY; /* MNO ok or could not be changed, check dvc rdy */
                XPLRCOM_CONSOLE(D, "MNO ok or cannot be changed, checking device");
            } else {
                fsm[0] = XPLR_COM_CELL_CONNECT_ERROR; /* failed to set or read MNO, go to error state */
                XPLRCOM_CONSOLE(E, "MNO Set failed with code: %d", ret);
            }
            break;
        case XPLR_COM_CELL_CONNECT_SET_RAT:
            fsm[1] = fsm[0]; /* hold current state */
            ret = cellDvcSetRat(dvcProfile); /* update dvc RAT if needed */
            if (ret == XPLR_COM_OK) {
                fsm[0] = XPLR_COM_CELL_CONNECT_CHECK_READY; /* RAT updated, check dvc rdy */
                XPLRCOM_CONSOLE(D, "RAT ok, checking device");
            } else {
                fsm[0] = XPLR_COM_CELL_CONNECT_ERROR; /* failed to set / read RAT list, go to error state */
                XPLRCOM_CONSOLE(E, "RAT Set failed with code: %d", ret);
            }
            break;
        case XPLR_COM_CELL_CONNECT_SET_BANDS:
            fsm[1] = fsm[0]; /* hold current state */
            ret = cellDvcSetBands(dvcProfile); /* update dvc bands if needed */
            if (ret == XPLR_COM_OK) {
                fsm[0] = XPLR_COM_CELL_CONNECT_CHECK_READY; /* Bands updated, check dvc rdy */
                XPLRCOM_CONSOLE(D, "bands ok, checking device");
            } else {
                fsm[0] = XPLR_COM_CELL_CONNECT_ERROR; /* failed to set / read band list, go to error state */
                XPLRCOM_CONSOLE(E,  "bands Set failed with code: %d", ret);
            }
            break;
        case XPLR_COM_CELL_CONNECT:
            fsm[1] = fsm[0]; /* hold current state */
            ret = cellDvcRegister(dvcProfile); /* register dvc to cellular carrier */
            if (ret == XPLR_COM_OK) {
                fsm[0] = XPLR_COM_CELL_CONNECT_OK; /* dvc connected to network, switch to connected */
                XPLRCOM_CONSOLE(D, "dvc interface up, switching to connected");
            } else {
                fsm[0] = XPLR_COM_CELL_CONNECT_ERROR; /* failed to set / read band list, go to error state */
                XPLRCOM_CONSOLE(E, "dvc register failed with code: %d", ret);
            }
            break;
        case XPLR_COM_CELL_CONNECT_CHECK_READY:
            /* performing certain configuration actions may require a reboot from the module.
             * rebooting phase may take several seconds thus it is suggested to check if device is ready
             * by calling the cellDvcCheckReady() function. */
            ret = cellDvcCheckReady(dvcProfile);
            if (ret == XPLR_COM_OK) {
                if (fsm[1] == XPLR_COM_CELL_CONNECT_SET_MNO) { /* dvc rdy after mno set reboot */
                    fsm[1] = fsm[0]; /* hold current state */
                    fsm[0] = XPLR_COM_CELL_CONNECT_SET_RAT;
                    XPLRCOM_CONSOLE(D, "dvc rdy, setting RAT(s)...");
                } else if (fsm[1] == XPLR_COM_CELL_CONNECT_SET_RAT) { /* dvc rdy after rat set reboot */
                    fsm[1] = fsm[0]; /* hold current state */
                    fsm[0] = XPLR_COM_CELL_CONNECT_SET_BANDS;
                    XPLRCOM_CONSOLE(D, "dvc rdy, setting Band(s)...");
                } else if (fsm[1] == XPLR_COM_CELL_CONNECT_SET_BANDS) { /* dvc rdy after bands set reboot */
                    fsm[1] = fsm[0]; /* hold current state */
                    fsm[0] = XPLR_COM_CELL_CONNECT;
                    XPLRCOM_CONSOLE(D, "dvc rdy, scanning networks...");
                } else {
                    /* ok... we should not be here. actually you should never be here.
                     * just in case, try to recover by running again previous state */
                    fsm[0] = fsm[1];
                    XPLRCOM_CONSOLE(E,
                                    "dvc rdy after unknown conditions, running previous state: %d",
                                    (int32_t)fsm[1]);
                }
            } else { /* dvc busy, retry */
                // TODO: add max retries
                XPLRCOM_CONSOLE(W, "dvc busy, check again: %d", ret);
                ret = XPLR_COM_OK; /* mask return value from busy to ok*/
            }
            break;
        case XPLR_COM_CELL_CONNECT_OK:
            cellDvcGetNetworkInfo(dvcProfile, &currentNetInfo);
            fsm[1] = fsm[0]; /* hold current state */
            fsm[0] = XPLR_COM_CELL_CONNECTED;
            XPLRCOM_CONSOLE(I, "dvc connected!");
            ret = XPLR_COM_OK;
            break;
        case XPLR_COM_CELL_CONNECTED:
            ret = XPLR_COM_OK;
            break;
        case XPLR_COM_CELL_CONNECT_TIMEOUT:
        case XPLR_COM_CELL_CONNECT_ERROR:
            if (fsm[0] != fsm[1]) { /* print error msg once*/
                fsm[1] = fsm[0]; /* hold current state */
                XPLRCOM_CONSOLE(E, "dvc %d in error!", dvcProfile);
                cellDvcGetNetworkInfo(dvcProfile, &currentNetInfo);
            }
            ret = XPLR_COM_ERROR;
            break;

        default:
            ret = XPLR_COM_ERROR;
            break;
    }

    return ret;
}

xplrCom_error_t xplrComCellFsmConnectReset(int8_t dvcProfile)
{
    xplrCom_error_t ret;
    xplrCom_cell_connect_t *fsm = comDevices[dvcProfile].cellFsm;

    if ((fsm[0] == XPLR_COM_CELL_CONNECT_ERROR) || (fsm[0] == XPLR_COM_CELL_CONNECT_TIMEOUT)) {
        /* reseting from erroneous state, perform POR */
        ret = cellDvcReset(dvcProfile, true);
        if (ret == XPLR_COM_OK) {
            /* dvc in reset, reset connect fsm */
            fsm[0] = XPLR_COM_CELL_CONNECT_INIT;
            fsm[1] = fsm[0];
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(I, "dvc reset, ok");
        } else {
            fsm[0] = XPLR_COM_CELL_CONNECT_ERROR;
            fsm[1] = fsm[0];
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(E, "error reseting dvc");
        }
    } else if ((fsm[0] == XPLR_COM_CELL_CONNECT_OK) || (fsm[0] == XPLR_COM_CELL_CONNECTED)) {
        /* reseting from ok state, soft reset dvc */
        ret = cellDvcReset(dvcProfile, false);
        if (ret == XPLR_COM_OK) {
            /* dvc in reset, reset connect fsm */
            fsm[0] = XPLR_COM_CELL_CONNECT_INIT;
            fsm[1] = fsm[0];
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(I, "dvc soft reset, ok");
        } else {
            fsm[0] = XPLR_COM_CELL_CONNECT_ERROR;
            fsm[1] = fsm[0];
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(E, "error soft reseting dvc");
        }
    } else {
        /* calling function from unsupported state */
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(W, "warning, trying to reset from state [%d]", fsm[0]);
    }

    return ret;
}

xplrCom_cell_connect_t xplrComCellFsmConnectGetState(int8_t dvcProfile)
{
    return comDevices[dvcProfile].cellFsm[0];
}

void xplrComCellNetworkInfo(int8_t dvcProfile, xplrCom_cell_netInfo_t *info)
{
    cellDvcGetNetworkInfo(dvcProfile, &currentNetInfo);
    memcpy(info, &currentNetInfo, sizeof(xplrCom_cell_netInfo_t));
}

int16_t xplrComCellNetworkScan(int8_t dvcProfile, char *scanBuff)
{
    int32_t ubxlibRet;
    int8_t found;
    uDeviceHandle_t dvcHandler = comDevices[dvcProfile].handler;
    char buff[64] = {0};
    int16_t ret;

    found = uCellNetScanGetFirst(dvcHandler, NULL, 0, buff, NULL, NULL);
    XPLRCOM_CONSOLE(D, "Networks found: %d\n", found);
    XPLRCOM_CONSOLE(D, "### %d: network: %s\n", found, buff);
    strcat(scanBuff, buff);
    ret = found;
    for (int8_t i = 0; i < found; i++) {
        ubxlibRet = uCellNetScanGetNext(dvcHandler, NULL, 0, buff, NULL);
        strcat(scanBuff, buff);
        XPLRCOM_CONSOLE(D, "### %d: network: %s\n", ubxlibRet, buff);
        if (ubxlibRet < 0) {
            uCellNetScanGetLast(dvcHandler);
            break;
        }
    }

    return ret;
}

xplrCom_error_t xplrComCellPowerDown(int8_t dvcProfile)
{
    int32_t ubxlibRet;
    uDeviceHandle_t dvcHandler = comDevices[dvcProfile].handler;
    xplrCom_error_t ret;

    ubxlibRet = uDeviceClose(dvcHandler, true);
    if (ubxlibRet == 0) {
        ret = XPLR_COM_OK;
        XPLRCOM_CONSOLE(D, "dvc powered down, ok");
    } else {
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(E, "error (%d) powering down dvc", ubxlibRet);
    }

    return ret;
}

void xplrComCellPowerResume(int8_t dvcProfile)
{
    xplrCom_cell_connect_t *fsm = comDevices[dvcProfile].cellFsm;

    /* power down can be triggered from any state of xplrComCellFsmConnect.
     * resuming can be achieved by hot reseting the fsm state of xplrComCellFsmConnect(). */
    fsm[0] = XPLR_COM_CELL_CONNECT_INIT;
    XPLRCOM_CONSOLE(D, "resuming power to device...");
}

xplrCom_error_t xplrComCellGetDeviceInfo(int8_t dvcProfile, char *model, char *fw, char *imei)
{
    uDeviceHandle_t dvcHandler = comDevices[dvcProfile].handler;
    int64_t ubxlibRet[3];
    xplrCom_error_t ret;

    ubxlibRet[0] = uCellInfoGetModelStr(dvcHandler, model, 32);
    ubxlibRet[1] = uCellInfoGetFirmwareVersionStr(dvcHandler, fw, 32);
    ubxlibRet[2] = uCellInfoGetImei(dvcHandler, imei);

    for (int i = 0; i < 3; i++) {
        if (ubxlibRet[i] < 0) {
            ret = XPLR_COM_ERROR;
            break;
        }
        ret = XPLR_COM_OK;
    }

    return ret;
}

bool xplrComHaltLogModule(int8_t dvcProfile)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
    if(comDevices[dvcProfile].cellSettings->logCfg != NULL) {
        comDevices[dvcProfile].cellSettings->logCfg->logEnable = false;
        ret = true;
    } else {
        ret = false;
        /* log module is not initialized thus do nothing*/
    }
#else
    ret = false;
#endif
    return ret;
}

bool xplrComStartLogModule(int8_t dvcProfile)
{
    bool ret;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
    xplrLog_error_t err;
    if(comDevices[dvcProfile].cellSettings->logCfg != NULL) {
        comDevices[dvcProfile].cellSettings->logCfg->logEnable = true;
        ret = true;
    } else {
        /* log module is not initialized thus initialize it*/
        err = xplrLogInit(&cellLog, XPLR_LOG_DEVICE_INFO, "/cell.log", 100, XPLR_SIZE_MB);
        if (err == XPLR_LOG_OK) {
            cellLog.logEnable = true;
        } else {
            cellLog.logEnable = false;
        }
        comDevices[dvcProfile].cellSettings->logCfg = &cellLog;
        ret = cellLog.logEnable;  
    }
#else
    ret = false;
#endif
    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

int8_t dvcGetFirstFreeSlot(void)
{
    int8_t ret = -1;

    for (int8_t i = 0; i < XPLRCOM_NUMOF_DEVICES; i++) {
        if (&comDevices[i] != NULL) {
            ret = i;
            break;
        }
    }

    XPLRCOM_CONSOLE(D, "ret index: %d", ret);
    return ret;
}

xplrCom_error_t dvcRemoveSlot(int8_t index)
{
    xplrCom_error_t ret;

    if ((index < XPLRCOM_NUMOF_DEVICES) && (&comDevices[index] != NULL)) {
        memset(&comDevices[index], (int)NULL, sizeof(xplrCom_t));
        ret = XPLR_COM_OK;
        XPLRCOM_CONSOLE(D, "slot %d removed", index);
    } else {
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(E, "failed to remove slot %d", index);
    }

    return ret;
}

xplrCom_error_t dvcRemoveAllSlots(void)
{
    for (int8_t i = 0; i < XPLRCOM_NUMOF_DEVICES; i++) {
        memset(&comDevices[i], (int)NULL, sizeof(xplrCom_t));
        XPLRCOM_CONSOLE(D, "slot %d removed", i);
    }

    return XPLR_COM_OK;
}

xplrCom_error_t cellSetConfig(xplrCom_cell_config_t *cfg)
{
    xplrCom_error_t ret = XPLR_COM_ERROR;
    int8_t dvcProfile = cfg->profileIndex;

    /* check config null */
    if (cfg != NULL) {
        if (dvcProfile >= 0) {
            /* clear device handler*/
            comDevices[dvcProfile].handler = NULL;
            /* copy user settings into dvc profile */
            uDeviceCfgCell_t *cellHw = &comDevices[dvcProfile].deviceSettings.deviceCfg.cfgCell;
            uDeviceCfgUart_t *cellCom =  &comDevices[dvcProfile].deviceSettings.transportCfg.cfgUart;
            memcpy(cellHw, cfg->hwSettings, sizeof(uDeviceCfgCell_t));
            memcpy(cellCom, cfg->comSettings, sizeof(uDeviceCfgUart_t));
            comDevices[dvcProfile].cellSettings = cfg;
            comDevices[dvcProfile].cellFsm[0] = XPLR_COM_CELL_CONNECT_INIT;
            comDevices[dvcProfile].cellFsm[1] = XPLR_COM_CELL_CONNECT_INIT;
            /* config rest of settings in device config*/
            comDevices[dvcProfile].deviceSettings.deviceType = U_DEVICE_TYPE_CELL;
            comDevices[dvcProfile].deviceNetwork = U_NETWORK_TYPE_CELL;
            comDevices[dvcProfile].deviceSettings.transportType = U_DEVICE_TRANSPORT_TYPE_UART;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCOM_LOG_ACTIVE)
            /* point to the local log struct*/
            cfg->logCfg = &cellLog;
#endif
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(D, "ok: %d", ret);
        } else {
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(W, "Device profile list full: %d", dvcProfile);
        }
    } else {
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(E, "Error, config null: %d", ret);
    }

    return ret;
}

xplrCom_error_t cellDvcCheckReady(int8_t index)
{
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    xplrCom_error_t ret;

    if (uCellPwrIsAlive(dvcHandler)) {
        ret = XPLR_COM_OK;
        XPLRCOM_CONSOLE(D, "dvc ready");
    } else {
        ret = XPLR_COM_BUSY;
        XPLRCOM_CONSOLE(W, "dvc busy");
    }

    return ret;
}

xplrCom_error_t cellDvcRebootNeeded(int8_t index)
{
    int32_t ubxlibRet;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    xplrCom_error_t ret;

    if (uCellPwrRebootIsRequired(&dvcHandler)) {
        ubxlibRet = uCellPwrReboot(&dvcHandler, cbCellWait);
        if (ubxlibRet == 0) {
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(D, "dvc rebooted ok");
        } else {
            ret = XPLR_COM_ERROR;
        }
    } else {
        ret = XPLR_COM_OK;
        XPLRCOM_CONSOLE(D, "no need to reset");
    }

    return ret;
}

xplrCom_error_t cellDvcReset(int8_t index, bool por)
{
    int32_t ubxlibRet;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    xplrCom_cell_config_t  *cellCfg = comDevices[index].cellSettings;
    xplrCom_error_t ret;

    if (por) {
        ubxlibRet = uDeviceClose(dvcHandler, true); /* dvc close and power off */
        if (ubxlibRet == 0) {
            XPLRCOM_CONSOLE(D, "dvc powered down, ok");
            vTaskDelay(XPLRCOM_CELL_REBOOT_WAIT_MS); /* allow some time for device to power down */
            ubxlibRet = xplrComCellInit(cellCfg); /* re-init cell dvc */
            if (ubxlibRet == XPLR_COM_OK) {
                ret = XPLR_COM_OK;
                XPLRCOM_CONSOLE(D, "dvc re-init ok");
            } else {
                ret = XPLR_COM_ERROR;
                XPLRCOM_CONSOLE(E, "error initializing cell dvc");
            }
        } else {
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(E, "error powering down dvc");
        }
    } else { /* soft reset */
        ubxlibRet = uDeviceClose(dvcHandler, false); /* dvc close without powering off */
        if (ubxlibRet == 0) {
            ret = XPLR_COM_OK;
            XPLRCOM_CONSOLE(D, "dvc closed ok");
        } else {
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(E, "error closing dvc");
        }
    }

    return ret;
}

xplrCom_error_t cellDvcOpen(int8_t index)
{
    int32_t ubxlibRet;
    uDeviceHandle_t *dvcHandler = &comDevices[index].handler;
    uDeviceCfg_t  *dvcSettings = &comDevices[index].deviceSettings;
    xplrCom_error_t ret;

    ubxlibRet = uDeviceOpen(dvcSettings, dvcHandler);
    if (ubxlibRet == 0) {
        ret = XPLR_COM_OK;
        XPLRCOM_CONSOLE(D, "ok");
    } else {
        ret = XPLR_COM_ERROR;
        XPLRCOM_CONSOLE(E, "error, with code %d: ", ubxlibRet);
    }

    return ret;
}

xplrCom_error_t cellDvcSetMno(int8_t index)
{
    int32_t storedMno;
    const int32_t configMno = comDevices[index].cellSettings->mno;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    xplrCom_error_t ret;

    XPLRCOM_CONSOLE(D, "MNO in config: %d", configMno);
    storedMno = uCellCfgGetMnoProfile(dvcHandler);
    if (storedMno != configMno) {
        /* MNO stored in cellular module different than in config. Try changing it. */
        XPLRCOM_CONSOLE(W,
                        "Module's MNO: %d differs from config: %d",
                        storedMno,
                        configMno);
        if (storedMno > 0) {
            ret = uCellCfgSetMnoProfile(dvcHandler, configMno); /* set MNO in module */
            if (ret == XPLR_COM_OK) {
                XPLRCOM_CONSOLE(D, "MNO changed to: %d", configMno);
            } else {
                ret = XPLR_COM_ERROR;
                XPLRCOM_CONSOLE(W, "MNO cannot be set in this module");
            }
        } else {
            ret = XPLR_COM_ERROR;
            XPLRCOM_CONSOLE(W, "error with code: %d", storedMno);
        }
    } else {
        /* MNOs match, nothing to do */
        ret = XPLR_COM_OK;
    }

    /* reboot module if required */
    ret = cellDvcRebootNeeded(index);

    return ret;
}

xplrCom_error_t cellDvcSetRat(int8_t index)
{
    int32_t ubxlibRet;
    int8_t errors = 0;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    uCellNetRat_t *ratList = comDevices[index].cellSettings->ratList;
    uCellNetRat_t storedRatList[XPLRCOM_CELL_RAT_SIZE];
    xplrCom_error_t ret;

    /* Get RAT list stored in module and update it (if needed) with the one in config*/
    for (int8_t i = 0; i < XPLRCOM_CELL_RAT_SIZE; i++) { /* iterate through dvc RAT list */
        storedRatList[i] = uCellCfgGetRat(dvcHandler, i);
        if (storedRatList[i] >= 0) { /* ok, we have something in dvc list */
            XPLRCOM_CONSOLE(D, "RAT[%d] is %s.\n", i, ratStr[storedRatList[i]]);
            if (((ratList[i] > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) || (i > 0)) && /* content is RAT */
                (ratList[i] != storedRatList[i])) { /* but not matching user config */
                XPLRCOM_CONSOLE(D, "setting RAT[%d] to %s...\n", i, ratStr[ratList[i]]);
                ubxlibRet = uCellCfgSetRatRank(dvcHandler, ratList[i], i); /* set RAT to dvc from config */
                if (ubxlibRet != 0) {
                    errors++;
                    XPLRCOM_CONSOLE(E, "error setting RAT[%d] to %s. \n", i, ratStr[ratList[i]]);
                    /* something went wrong, continue with rest of list */
                } else {
                    ret = XPLR_COM_OK;
                    XPLRCOM_CONSOLE(D, "setting RAT[%d] to %s...\n", i, ratStr[ratList[i]]);
                }
            }
        } else {
            errors++;
            XPLRCOM_CONSOLE(E, "error reading RAT slot [%d]:[%d]. \n", i, storedRatList[i]);
            /* something went wrong, continue with rest of list */
        }
    }

    /* lets check for errors */
    if (errors > 0) {
        XPLRCOM_CONSOLE(E, "there were errors (%d) writing RAT list.\n", errors);
        ret = XPLR_COM_ERROR;
    } else {
        ret = XPLR_COM_OK;
    }

    return ret;
}

xplrCom_error_t cellDvcSetBands(int8_t index)
{
    int32_t ubxlibRet;
    int8_t errors = 0;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    uCellNetRat_t *ratList = comDevices[index].cellSettings->ratList;
    uint64_t *bandList = comDevices[index].cellSettings->bandList;
    uint64_t activeBandMask[2];
    xplrCom_error_t ret;

    for (int8_t i = 0; i < XPLRCOM_CELL_RAT_SIZE; i++) { /* iterate through dvc Band list */
        /* check if selected RAT is CAT-M1 or NB1 */
        if ((ratList[i] == U_CELL_NET_RAT_CATM1) || (ratList[i] == U_CELL_NET_RAT_NB1)) {
            /* CAT-M1 or NB1 found, read current bandmask info */
            ubxlibRet = uCellCfgGetBandMask(dvcHandler, ratList[i], &activeBandMask[0], &activeBandMask[1]);
            if (ubxlibRet == 0) {
                XPLRCOM_CONSOLE(D,
                                "band mask for RAT %s is 0x%08x%08x %08x%08x.\n",
                                ratStr[ratList[i]],
                                (uint32_t)(activeBandMask[1] >> 32),
                                (uint32_t) activeBandMask[1],
                                (uint32_t)(activeBandMask[0] >> 32),
                                (uint32_t) activeBandMask[0]);
                /* check if stored bandmask differs from config */
                if ((activeBandMask[0] != bandList[(i * 2)]) || (activeBandMask[1] != bandList[(i * 2) + 1])) {
                    /* bandmasks are different, update dvc  */
                    XPLRCOM_CONSOLE(D,
                                    "setting band mask for RAT %s to 0x%08x%08x %08x%08x...\n",
                                    ratStr[ratList[i]],
                                    (uint32_t)(bandList[(i * 2) + 1] >> 32),
                                    (uint32_t)(bandList[(i * 2) + 1]),
                                    (uint32_t)(bandList[(i * 2)] >> 32),
                                    (uint32_t)(bandList[(i * 2)]));
                    ubxlibRet = uCellCfgSetBandMask(dvcHandler, ratList[i], bandList[(i * 2)], bandList[(i * 2) + 1]);
                    if (ubxlibRet != 0) {
                        XPLRCOM_CONSOLE(E, "unable to change band mask for RAT %s, it is"
                                        " likely your module does not support one of those"
                                        " bands.\n", ratStr[ratList[i]]);
                        errors++;
                    }
                }
            } else {
                errors++;
                XPLRCOM_CONSOLE(E, "failed to get band info for RAT %s.", ratStr[ratList[i]]);
            }
        }
    }

    /* check for errors */
    if (errors > 0) {
        ret = XPLR_COM_ERROR;
    } else {
        ret = XPLR_COM_OK;
    }

    return ret;
}

xplrCom_error_t cellDvcRegister(int8_t index)
{
    int32_t ubxlibRet;
    uDeviceHandle_t dvcHandler = comDevices[index].handler;
    uNetworkType_t netType = comDevices[index].deviceNetwork;
    uNetworkCfgCell_t *netConfig = comDevices[index].cellSettings->netSettings;
    xplrCom_error_t ret;

    XPLRCOM_CONSOLE(D, "Bringing up the network...\n");
    ubxlibRet = uNetworkInterfaceUp(dvcHandler, netType, netConfig);
    if (ubxlibRet == 0) {
        XPLRCOM_CONSOLE(I, "Network is up!\n");
        ret = XPLR_COM_OK;
    } else {
        XPLRCOM_CONSOLE(E, "Unable to bring up the network!\n");
        ret = ubxlibRet;
    }

    return ret;
}

void cellDvcGetNetworkInfo(int8_t index, xplrCom_cell_netInfo_t *info)
{
    uDeviceHandle_t dvcHandler = comDevices[index].handler;

    uCellNetGetOperatorStr(dvcHandler, info->operator, 32);
    memcpy(info->rat,
           ratStr[uCellNetGetActiveRat(dvcHandler)],
           strlen(ratStr[uCellNetGetActiveRat(dvcHandler)])
          );
    info->registered = uCellNetIsRegistered(dvcHandler);
    memcpy(info->status,
           netStatStr[uCellNetGetNetworkStatus(dvcHandler, U_CELL_NET_REG_DOMAIN_PS)],
           strlen(netStatStr[uCellNetGetNetworkStatus(dvcHandler, U_CELL_NET_REG_DOMAIN_PS)])
          );
    uCellNetGetIpAddressStr(dvcHandler, info->ip);
    uCellNetGetMccMnc(dvcHandler, &info->Mcc, &info->Mnc);

    XPLRCOM_CONSOLE(D, "cell network settings:");
    XPLRCOM_CONSOLE(D, "operator: %s", info->operator);
    XPLRCOM_CONSOLE(D, "ip: %s", info->ip);
    XPLRCOM_CONSOLE(D, "registered: %d", info->registered);
    XPLRCOM_CONSOLE(D, "RAT: %s", info->rat);
    XPLRCOM_CONSOLE(D, "status: %s", info->status);
    XPLRCOM_CONSOLE(D, "Mcc: %d", info->Mcc);
    XPLRCOM_CONSOLE(D, "Mnc: %d", info->Mnc);
}

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

bool cbCellWait(void (*ptr))
{
    XPLRCOM_CONSOLE(W, "cell wait callback fired");
    esp_task_wdt_reset();
    return true;
}