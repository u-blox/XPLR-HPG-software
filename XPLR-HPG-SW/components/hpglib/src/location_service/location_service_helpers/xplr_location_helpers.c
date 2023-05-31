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

#include <stdlib.h>
#include "string.h"
#include "esp_task_wdt.h"
#include "u_cfg_app_platform_specific.h"
#include "xplr_location_helpers.h"
#include "./../../../../../components/hpglib/src/common/xplr_common.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board has been selected in xplr_hpglib_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRHELPERS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRHELPERS_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrCommonHelpers", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRHELPERS_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static int32_t ubxRet;
static esp_err_t espRet;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

esp_err_t xplrHelpersUbxlibInit(void)
{
    ubxRet = uPortInit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib init failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    ubxRet = uPortI2cInit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib I2C port init failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    ubxRet = uDeviceInit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib device init failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "ubxlib init ok!");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcDeviceOpen(xplrGnssDevBase_t *dvcBase)
{
    esp_err_t ret = ESP_OK;
    int32_t intRet = -1;
    uint8_t progCnt = 0;
    uint64_t lastActionTime = MICROTOSEC(esp_timer_get_time());
    uint64_t nowTime = lastActionTime;

    if (dvcBase == NULL) {
        XPLRHELPERS_CONSOLE(E, "Device pointer is NULL!");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        XPLRHELPERS_CONSOLE(D, "Trying to open device.");
        while (((nowTime - lastActionTime) <= XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT) && (intRet != 0)) {
            nowTime = MICROTOSEC(esp_timer_get_time());
            intRet = uDeviceOpen(&dvcBase->dConfig, &dvcBase->dHandler);

            vTaskDelay(pdMS_TO_TICKS(50));
            progCnt++;
            /**
             * Roughly print every second so the user knows routine is not stuck
             */
            if (progCnt >= 20) {
                XPLRHELPERS_CONSOLE(D, 
                                    "Trying to open device - elapsed time: %llu out of %llu seconds",
                                    nowTime - lastActionTime,
                                    XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT);
                progCnt = 0;
            }
        }

        if (((nowTime - lastActionTime) > XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT)) {
            XPLRHELPERS_CONSOLE(E, 
                                "ubxlib device open failed - timeout: [%llu] seconds | ubxlib error code [%d]", 
                                nowTime - lastActionTime, 
                                intRet);
            ret = ESP_ERR_TIMEOUT;
        } else {
            if (intRet == 0) {
                intRet = uNetworkInterfaceUp(dvcBase->dHandler, U_NETWORK_TYPE_GNSS, &dvcBase->dNetwork);
                if (intRet == 0) {
                    XPLRHELPERS_CONSOLE(I, "ubxlib device opened!");
                    ret = ESP_OK;
                } else {
                    XPLRHELPERS_CONSOLE(E, "ubxlib interface open failed with error code [%d]", intRet);
                    XPLRHELPERS_CONSOLE(E, "Trying to close device!");

                    xplrHlprLocSrvcDeviceClose(dvcBase);
                    /**
                     * we do not check return for xplrHlprLocSrvcDeviceClose
                     * as long as we are here ret should be ESP_FAIL
                     */
                    ret = ESP_FAIL;
                }
            } else {
                ret = ESP_FAIL;
            }
        }
    }

    XPLRHELPERS_CONSOLE(D, "ubxlib device opened!");
    return ret;
}

esp_err_t xplrHlprLocSrvcDeviceClose(xplrGnssDevBase_t *dvcBase)
{
    ubxRet = uDeviceClose(dvcBase->dHandler, false);
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib device close failed with error code [%d]", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(I, "ubxlib device closed!");
    return ESP_OK;
}

uDeviceHandle_t *xplrHlprLocSrvcGetHandler(xplrGnssDevBase_t *dvcBase)
{
    return &dvcBase->dHandler;
}

esp_err_t xplrHlprLocSrvcUbxlibDeinit(void)
{
    ubxRet = uDeviceDeinit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib device deinit failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    uPortI2cDeinit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib I2C deinit failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    uPortDeinit();
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib device deinit failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(I, "ubxlib deinit ok!");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcConfigAllDefault(xplrGnssDevBase_t *dvcBase, uint8_t i2cAddress)
{
    espRet = xplrHlprLocSrvcDeviceConfigDefault(dvcBase, i2cAddress);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    espRet = xplrHlprLocSrvcNetworkConfigDefault(dvcBase);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "All default configs set.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcDeviceConfigDefault(xplrGnssDevBase_t *dvcBase, uint8_t i2cAddress)
{
    dvcBase->dConfig.deviceType = U_DEVICE_TYPE_GNSS;

    dvcBase->dConfig.deviceCfg.cfgGnss.moduleType = U_GNSS_MODULE_TYPE_M9;
    dvcBase->dConfig.deviceCfg.cfgGnss.pinEnablePower = -1;
    dvcBase->dConfig.deviceCfg.cfgGnss.pinDataReady = -1;
    dvcBase->dConfig.deviceCfg.cfgGnss.i2cAddress = i2cAddress;

    dvcBase->dConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;

    dvcBase->dConfig.transportCfg.cfgI2c.i2c = 0;
    dvcBase->dConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    dvcBase->dConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    dvcBase->dConfig.transportCfg.cfgI2c.clockHertz = 400000;

    XPLRHELPERS_CONSOLE(D, "Device config set.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcNetworkConfigDefault(xplrGnssDevBase_t *dvcBase)
{
    dvcBase->dNetwork.type = U_NETWORK_TYPE_GNSS;
    dvcBase->dNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    dvcBase->dNetwork.devicePinPwr = -1;
    dvcBase->dNetwork.devicePinDataReady = -1;

    XPLRHELPERS_CONSOLE(D, "Network config set.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcSetDeviceConfig(xplrGnssDevBase_t *dvcBase, uDeviceCfg_t *sdConfig)
{
    if (sdConfig == NULL || dvcBase == NULL) {
        XPLRHELPERS_CONSOLE(E, "One of the 2 structs is NULL!");
        return ESP_FAIL;
    }

    memcpy(&dvcBase->dConfig, sdConfig, sizeof(uDeviceCfg_t));

    XPLRHELPERS_CONSOLE(D, "Device config set.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcSetNetworkConfig(xplrGnssDevBase_t *dvcBase, uNetworkCfgGnss_t *sdNetwork)
{
    if (sdNetwork == NULL || dvcBase == NULL) {
        XPLRHELPERS_CONSOLE(E, "One of the 2 structs is NULL!");
        return ESP_FAIL;
    }

    memcpy(&dvcBase->dNetwork, sdNetwork, sizeof(uNetworkCfgGnss_t));

    XPLRHELPERS_CONSOLE(D, "Network config set.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcOptionSingleValSet(xplrGnssDevBase_t *dvcBase,
                                            uint32_t keyId,
                                            uint64_t value,
                                            uGnssCfgValTransaction_t transaction,
                                            uGnssCfgValLayer_t layer)
{
    ubxRet = uGnssCfgValSet(dvcBase->dHandler, keyId, value, transaction, layer);
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(D, "SingleValSet error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Set configuration value.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcOptionMultiValSet(xplrGnssDevBase_t *dvcBase,
                                           const uGnssCfgVal_t *pList,
                                           size_t numValues,
                                           uGnssCfgValTransaction_t transaction,
                                           uGnssCfgValLayer_t layer)
{
    ubxRet = uGnssCfgValSetList(dvcBase->dHandler, pList, numValues, transaction, layer);
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(D, "MultiValSet error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Set multiple configuration values.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcOptionSingleValGet(xplrGnssDevBase_t *dvcBase,
                                            uint32_t keyId,
                                            void *pValue,
                                            size_t size,
                                            uGnssCfgValLayer_t layer)
{
    ubxRet = uGnssCfgValGet(dvcBase->dHandler, keyId, pValue, size, layer);
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "SingleValGet error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Got configuration value.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcOptionMultiValGet(xplrGnssDevBase_t *dvcBase,
                                           const uint32_t *pKeyIdList,
                                           size_t numKeyIds,
                                           uGnssCfgVal_t **pList,
                                           uGnssCfgValLayer_t layer)
{
    ubxRet = uGnssCfgValGetListAlloc(dvcBase->dHandler, pKeyIdList, numKeyIds, pList, layer);
    if (ubxRet < 0) {
        XPLRHELPERS_CONSOLE(E, "MultiValGet error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Got multiple configuration values.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcGetDeviceInfo(xplrGnssDevBase_t *dvcBase,
                                       xplrGnssDevInfo_t *dInfo)
{
    dInfo->i2cAddress = dvcBase->dConfig.deviceCfg.cfgGnss.i2cAddress;
    dInfo->i2cPort    = dvcBase->dConfig.transportCfg.cfgI2c.i2c;
    dInfo->pinSda     = dvcBase->dConfig.transportCfg.cfgI2c.pinSda;
    dInfo->pinScl     = dvcBase->dConfig.transportCfg.cfgI2c.pinScl;

    ubxRet = uGnssInfoGetVersions(dvcBase->dHandler, &dInfo->pVer);
    if (ubxRet != 0) {
        XPLRHELPERS_CONSOLE(E, "Getting version failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    ubxRet = uGnssInfoGetIdStr(dvcBase->dHandler, (char *)dInfo->id, sizeof(dInfo->id));
    if (ubxRet < 0) {
        XPLRHELPERS_CONSOLE(E, "Getting ID failed with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Got device info.");
    return ESP_OK;
}

esp_err_t xplrHlprLocSrvcPrintDeviceInfo(xplrGnssDevInfo_t *dInfo)
{
#if (1 == XPLRHELPERS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char idBuff[16];
    int snpRet;

    snpRet = snprintf(idBuff, 16, "%02x%02x%02x%02x%02x", dInfo->id[0],
                      dInfo->id[1],
                      dInfo->id[2],
                      dInfo->id[3],
                      dInfo->id[4]);
    if (snpRet < 0) {
        XPLRHELPERS_CONSOLE(D, "Failed to write ID to buffer!");
        return ESP_FAIL;
    }

    printf("========= Device Info =========\n");
    printf("Lband version: %s\nHardware: %s\nRom: %s\nFirmware: %s\nProtocol: %s\nModel: %s\nID: %s\n",
           dInfo->pVer.ver,
           dInfo->pVer.hw,
           dInfo->pVer.rom,
           dInfo->pVer.fw,
           dInfo->pVer.prot,
           dInfo->pVer.mod,
           idBuff);
    printf("-------------------------------\n");
    printf("I2C Port: %d\nI2C Address: 0x%2x\nI2C SDA pin: %d\nI2C SCL pin: %d\n", dInfo->i2cPort,
           dInfo->i2cAddress,
           dInfo->pinSda,
           dInfo->pinScl);
    printf("===============================\n");
#endif

    return ESP_OK;
}

int32_t xplrHlprLocSrvcSendUbxFormattedCommand(uDeviceHandle_t *dHandler,
                                               const char *pBuffer,
                                               size_t size)
{
    ubxRet = uGnssMsgSend(*dHandler, pBuffer, size);
    if (ubxRet < 0) {
        XPLRHELPERS_CONSOLE(E, "Failed to send message!", ubxRet);
        return ESP_FAIL;
    }

    XPLRHELPERS_CONSOLE(D, "Sent UBX formatted command [%d] bytes.", ubxRet);
    return ubxRet;
}

bool xplrHlprLocSrvcCheckDvcProfileValidity(uint8_t dvcProfile, uint8_t maxDevLim)
{
    if (dvcProfile > maxDevLim) {
        XPLRHELPERS_CONSOLE(E, "Device profile out of bounds! Max allowed [%d]", maxDevLim);
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */