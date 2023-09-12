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
#if (1 == XPLRHELPERS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRLOCATION_LOG_ACTIVE))
#define XPLRHELPERS_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrCommonHelpers", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRHELPERS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRHELPERS_CONSOLE(tag, message, ...)  esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrCommonHelpers", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrCommonHelpers", __FUNCTION__, __LINE__, ## __VA_ARGS__);\
    XPLRLOG(&locationLog,buff2Log);
#elif ((0 == XPLRHELPERS_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRHELPERS_CONSOLE(tag, message, ...) \
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), "[(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrCommonHelpers", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    XPLRLOG(&locationLog,buff2Log)
#else
#define XPLRHELPERS_CONSOLE(message, ...) do{} while(0)
#endif

#define XPLRHELPERS_XTRA_DEBUG 0

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERN VARIABLES
 * -------------------------------------------------------------- */
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
xplrLog_t locationLog = {0};
char buff2Log[XPLRLOG_BUFFER_SIZE_SMALL] = {0};
#endif
/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

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
    esp_err_t ret;
    int32_t intRet;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
    if (!locationLog.logEnable) {
        xplrLog_error_t err = xplrLogInit(&locationLog, XPLR_LOG_DEVICE_INFO, "/location.log", 100,
                                          XPLR_SIZE_MB);
        if (err == XPLR_LOG_OK) {
            locationLog.logEnable = true;
        } else {
            locationLog.logEnable = false;
        }
    }
#endif

    intRet = uPortInit();
    if (intRet != 0) {
        XPLRHELPERS_CONSOLE(E, "ubxlib init failed with error code [%d]!", intRet);
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        intRet = uPortI2cInit();
        if (intRet != 0) {
            XPLRHELPERS_CONSOLE(E, "ubxlib I2C port init failed with error code [%d]!", intRet);
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        intRet = uDeviceInit();
        if (intRet != 0) {
            XPLRHELPERS_CONSOLE(E, "ubxlib device init failed with error code [%d]!", intRet);
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        XPLRHELPERS_CONSOLE(D, "ubxlib init ok!");
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcDeviceOpen(xplrLocationDevConf_t *dvcConf, uDeviceHandle_t *dvcHandler)
{
    esp_err_t ret;
    int32_t intRet = -1;
    uint8_t progCnt = 0;
    uint64_t lastActionTime = MICROTOSEC(esp_timer_get_time());
    uint64_t nowTime = lastActionTime;

    if (dvcConf == NULL) {
        XPLRHELPERS_CONSOLE(E, "dvcConf pointer is NULL!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        XPLRHELPERS_CONSOLE(D, "Trying to open device.");
        while (((nowTime - lastActionTime) <= XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT) && (intRet != 0)) {
            nowTime = MICROTOSEC(esp_timer_get_time());
            intRet = uDeviceOpen(&dvcConf->dvcConfig, dvcHandler);

            vTaskDelay(pdMS_TO_TICKS(100));
            progCnt++;
            /**
             * Roughly print every second so the user knows routine is not stuck
             */
            if (progCnt >= 20) {
                XPLRHELPERS_CONSOLE(D,
                                    "Trying to open device - elapsed time: %llu out of %u seconds",
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
                intRet = uNetworkInterfaceUp(*dvcHandler, U_NETWORK_TYPE_GNSS, &dvcConf->dvcNetwork);
                if (intRet == 0) {
                    XPLRHELPERS_CONSOLE(I, "ubxlib device opened!");
                    ret = ESP_OK;
                } else {
                    XPLRHELPERS_CONSOLE(E, "ubxlib interface open failed with error code [%d]", intRet);
                    XPLRHELPERS_CONSOLE(E, "Trying to close device!");

                    xplrHlprLocSrvcDeviceClose(dvcHandler);
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

    return ret;
}

esp_err_t xplrHlprLocSrvcDeviceOpenNonBlocking(xplrLocationDevConf_t *dvcConf,
                                               uDeviceHandle_t *dvcHandler)
{
    esp_err_t ret;
    int32_t intRet;

    if (dvcConf == NULL) {
        XPLRHELPERS_CONSOLE(E, "dvcConf pointer is NULL!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if ((ret == ESP_OK) && (dvcHandler == NULL)) {
        XPLRHELPERS_CONSOLE(E, "dvcHandler pointer is NULL!");
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        intRet = uDeviceOpen(&dvcConf->dvcConfig, dvcHandler);

        if (intRet == 0) {
            XPLRHELPERS_CONSOLE(I, "ubxlib device opened!");
            intRet = uNetworkInterfaceUp(*dvcHandler, U_NETWORK_TYPE_GNSS, &dvcConf->dvcNetwork);
            if (intRet == 0) {
                XPLRHELPERS_CONSOLE(I, "Network interface opened!");
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
            }
        } else {
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcDeviceClose(uDeviceHandle_t *dvcHandler)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uDeviceClose(*dvcHandler, false);
    if (intRet == 0) {
        XPLRHELPERS_CONSOLE(D, "ubxlib device closed!");
        ret = ESP_OK;
    } else {
        XPLRHELPERS_CONSOLE(E, "ubxlib device close failed with error code [%d]", intRet);
        ret = ESP_FAIL;
    }

    return ret;
}

uDeviceHandle_t *xplrHlprLocSrvcGetHandler(uDeviceHandle_t *dvcHandler)
{
    return dvcHandler;
}

esp_err_t xplrHlprLocSrvcUbxlibDeinit(void)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uDeviceDeinit();
    if (intRet == 0) {
        uPortI2cDeinit();
        uPortDeinit();

        XPLRHELPERS_CONSOLE(D, "ubxlib deinit ok!");
        ret = ESP_OK;
    } else {
        XPLRHELPERS_CONSOLE(E, "ubxlib device deinit failed with error code [%d]!", intRet);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcOptionSingleValSet(uDeviceHandle_t *dvcHandler,
                                            uint32_t keyId,
                                            uint64_t value,
                                            uGnssCfgValTransaction_t transaction,
                                            uint32_t layer)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uGnssCfgValSet(*dvcHandler, keyId, value, transaction, layer);
    if (intRet == 0) {
        XPLRHELPERS_CONSOLE(D, "Set configuration value.");
        ret = ESP_OK;
    } else if (intRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRHELPERS_CONSOLE(W, "SingleValSet timed out!");
        ret = ESP_ERR_TIMEOUT;
    } else {
        XPLRHELPERS_CONSOLE(E, "SingleValSet error code [%d]!", intRet);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcOptionMultiValSet(uDeviceHandle_t *dvcHandler,
                                           const uGnssCfgVal_t *list,
                                           size_t numValues,
                                           uGnssCfgValTransaction_t transaction,
                                           uint32_t layer)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uGnssCfgValSetList(*dvcHandler, list, numValues, transaction, layer);
    if (intRet == 0) {
        XPLRHELPERS_CONSOLE(D, "Set multiple configuration values.");
        ret = ESP_OK;
    } else if (intRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRHELPERS_CONSOLE(W, "MultiValSet timed out!");
        ret = ESP_ERR_TIMEOUT;
    } else {
        XPLRHELPERS_CONSOLE(E, "MultiValSet error code [%d]!", intRet);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcOptionSingleValGet(uDeviceHandle_t *dvcHandler,
                                            uint32_t keyId,
                                            void *value,
                                            size_t size,
                                            uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uGnssCfgValGet(*dvcHandler, keyId, value, size, layer);
    if (intRet == 0) {
#if (1 == XPLRHELPERS_XTRA_DEBUG)
        XPLRHELPERS_CONSOLE(D, "Got configuration value.");
#endif
        ret = ESP_OK;
    } else if (intRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRHELPERS_CONSOLE(W, "SingleValGet timed out!");
        ret = ESP_ERR_TIMEOUT;
    } else {
        XPLRHELPERS_CONSOLE(E, "SingleValGet error code [%d]!", intRet);
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcOptionMultiValGet(uDeviceHandle_t *dvcHandler,
                                           const uint32_t *keyIdList,
                                           size_t numKeyIds,
                                           uGnssCfgVal_t **list,
                                           uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uGnssCfgValGetListAlloc(*dvcHandler, keyIdList, numKeyIds, list, layer);

    if (intRet == 0) {
        XPLRHELPERS_CONSOLE(E, "MultiValGet error code [%d]!", intRet);
        ret = ESP_FAIL;
    } else if (intRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRHELPERS_CONSOLE(W, "MultiValGet timed out!");
        ret = ESP_ERR_TIMEOUT;
    } else {
        XPLRHELPERS_CONSOLE(D, "Got multiple configuration values.");
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcGetDeviceInfo(xplrLocationDevConf_t *dvcConf,
                                       uDeviceHandle_t dvcHandler,
                                       xplrLocDvcInfo_t *dvcInfo)
{
    esp_err_t ret;
    int32_t intRet;

    dvcInfo->i2cAddress = dvcConf->dvcConfig.deviceCfg.cfgGnss.i2cAddress;
    dvcInfo->i2cPort    = dvcConf->dvcConfig.transportCfg.cfgI2c.i2c;
    dvcInfo->pinSda     = dvcConf->dvcConfig.transportCfg.cfgI2c.pinSda;
    dvcInfo->pinScl     = dvcConf->dvcConfig.transportCfg.cfgI2c.pinScl;

    intRet = uGnssInfoGetVersions(dvcHandler, &dvcInfo->ver);
    if (intRet != 0) {
        XPLRHELPERS_CONSOLE(E, "Getting version failed with error code [%d]!", intRet);
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        intRet = uGnssInfoGetIdStr(dvcHandler, (char *)dvcInfo->id, sizeof(dvcInfo->id));
        if (intRet < 0) {
            XPLRHELPERS_CONSOLE(E, "Getting ID failed with error code [%d]!", intRet);
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        XPLRHELPERS_CONSOLE(I, "Got device info.");
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcPrintDeviceInfo(xplrLocDvcInfo_t *dvcInfo)
{
    esp_err_t ret;

#if (1 == XPLRHELPERS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char idBuff[16];
    int intRet;

    intRet = snprintf(idBuff,
                      16,
                      "%02x%02x%02x%02x%02x",
                      dvcInfo->id[0],
                      dvcInfo->id[1],
                      dvcInfo->id[2],
                      dvcInfo->id[3],
                      dvcInfo->id[4]);
    if (intRet < 0) {
        XPLRHELPERS_CONSOLE(D, "Failed to write ID to buffer!");
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        printf("========= Device Info =========\n");
        printf("Module variant: %s\nModule version: %s\nHardware version: %s\nRom: %s\nFirmware: %s\nProtocol: %s\nID: %s\n",
               dvcInfo->ver.mod,
               dvcInfo->ver.ver,
               dvcInfo->ver.hw,
               dvcInfo->ver.rom,
               dvcInfo->ver.fw,
               dvcInfo->ver.prot,
               idBuff);
        printf("-------------------------------\n");
        printf("I2C Port: %d\nI2C Address: 0x%2x\nI2C SDA pin: %d\nI2C SCL pin: %d\n",
               dvcInfo->i2cPort,
               dvcInfo->i2cAddress,
               dvcInfo->pinSda,
               dvcInfo->pinScl);
        printf("===============================\n");
    }
#else
    ret = ESP_OK;
#endif

    return ret;
}

int32_t xplrHlprLocSrvcSendUbxFormattedCommand(uDeviceHandle_t *dvcHandler,
                                               const char *buffer,
                                               size_t size)
{
    int32_t ret;

    ret = uGnssMsgSend(*dvcHandler, buffer, size);
    if (ret < 0) {
        XPLRHELPERS_CONSOLE(E, "Failed to send message with error code [%d]!", ret);
    } else if (ret != size) {
        XPLRHELPERS_CONSOLE(E, "Failed to send message send size [%d] mismatch [%d]!", ret, size);
    } else {
        XPLRHELPERS_CONSOLE(D, "Sent UBX data [%d] bytes.", ret);
    }

    return ret;
}

esp_err_t xplrHlprLocSrvcSendRtcmFormattedCommand(uDeviceHandle_t *dvcHandler,
                                                  const char *buffer,
                                                  size_t size)
{
    esp_err_t ret;
    int32_t intRet;

    intRet = uGnssMsgSend(*dvcHandler, buffer, size);
    if (intRet < 0) {
        XPLRHELPERS_CONSOLE(E, "Failed to send message with error code [%d]!", intRet);
        ret = ESP_FAIL;
    } else if (intRet != size) {
        XPLRHELPERS_CONSOLE(E, "Failed to send message send size [%d] mismatch [%d]!", intRet, size);
        ret = ESP_FAIL;
    } else {
        XPLRHELPERS_CONSOLE(D, "Sent RTCM data [%d] bytes.", intRet);
        ret = ESP_OK;
    }

    return ret;
}

bool xplrHlprLocSrvcCheckDvcProfileValidity(uint8_t dvcProfile, uint8_t maxDevLim)
{
    bool ret;

    if (dvcProfile > maxDevLim) {
        XPLRHELPERS_CONSOLE(E, "Device profile out of bounds! Max allowed [%d]", maxDevLim);
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */