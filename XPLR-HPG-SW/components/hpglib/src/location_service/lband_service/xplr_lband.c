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
#include "freertos/semphr.h"
#include "cJSON.h"
#include "xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRLBAND_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRLOCATION_LOG_ACTIVE))
#define XPLRLBAND_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgLband", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRLBAND_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRLBAND_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgLband", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRLBAND_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRLBAND_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgLband", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRLBAND_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/*INDENT-OFF*/

/**
 * You should not change these given values in any case or your LBAND
 * module will not function properly.
 */
static const uGnssCfgVal_t lbandSettings[] = {
    {0x10b10016, 0},
    {0x30b10015, 0x6959},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_I2C_U1, 1}
};

/**
 * Message ID for UBX-RXM-PMP
 * SPARTN correction data
 */
static const uGnssMessageId_t messageIdLBand = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x0272    /**< ubx protocol command class(c)/id(i) 0xccii */
};

/**
 * Struct that contains extra info needed only for
 * specific device type: LBAND
 */
typedef struct xplrLbandAsyncIds_type {
    int32_t ahCorrData;    /**< ubxlib async handler */
} xplrLbandAsyncIds_t;

typedef struct xplrLbandRunContext_type {
    uDeviceHandle_t dvcHandler;     /**< ubxlib device handler */
    xplrLbandAsyncIds_t asyncIds;   /**< async id handers */
} xplrLbandRunContext_t;

/**
 * Setting struct for LBAND devices
 */
typedef struct xplrLband_type {
    xplrLbandDeviceCfg_t *dvcCfg;
    xplrLbandRunContext_t options;
} xplrLband_t;
/*INDENT-ON*/

static SemaphoreHandle_t xSemaphore = NULL;

/**< flag showing if data is forwarded to the GNSS module */
static bool hasFrwdCorrMsg = false;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char *freqRegions[] = {
    "eu",
    "us"
};

static xplrLband_t lbandDvcs[XPLRLBAND_NUMOF_DEVICES] = {NULL};
static int8_t logIndex = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t lbandDeviceOpen(uint8_t dvcProfile);
static esp_err_t lbandDeviceClose(uint8_t dvcProfile);
static int32_t   lbandAsyncStopper(uint8_t dvcProfile, int32_t handler);
static esp_err_t lbandSetFreqFromPrm(uint8_t dvcProfile, uint32_t freq);
static esp_err_t lbandSetFreqFromCfg(uint8_t dvcProfile);
static esp_err_t lbandParseFrequencyFromMqtt(uint8_t dvcProfile,
                                             char *mqttPayload);
static bool lbandIsDvcProfileValid(uint8_t dvcProfile);

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void xplrLbandMessageReceivedCB(uDeviceHandle_t gnssHandle,
                                       const uGnssMessageId_t *messageId,
                                       int32_t errorCodeOrLength,
                                       void *callbackParam);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

esp_err_t xplrLbandUbxlibInit(void)
{
    esp_err_t ret;
    ret = xplrHelpersUbxlibInit();
    return ret;
}

esp_err_t xplrLbandUbxlibDeinit(void)
{
    esp_err_t ret;
    ret = xplrHlprLocSrvcUbxlibDeinit();
    return ret;
}

esp_err_t xplrLbandStartDevice(uint8_t dvcProfile,
                               xplrLbandDeviceCfg_t *dvcCfg)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if ((ret == ESP_OK) && (dvcCfg == NULL)) {
        XPLRLBAND_CONSOLE(E, "dvcCfg pointer is NULL!");
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        lbandDvcs[dvcProfile].options.asyncIds.ahCorrData = -1;
        lbandDvcs[dvcProfile].dvcCfg = dvcCfg;

        ret = lbandDeviceOpen(dvcProfile);

        if (ret == ESP_OK) {
            ret = xplrLbandOptionMultiValSet(dvcProfile,
                                             lbandSettings,
                                             ELEMENTCNT(lbandSettings),
                                             U_GNSS_CFG_VAL_LAYER_RAM);
            if (ret == ESP_OK) {
                if (lbandDvcs[dvcProfile].dvcCfg->destHandler != NULL) {
                    XPLRLBAND_CONSOLE(D, "GNSS destination handler found in config. Starting async sender.");
                    ret = xplrLbandSendCorrectionDataAsyncStart(dvcProfile);
                } else {
                    XPLRLBAND_CONSOLE(D, "GNSS destination handler is not set. Skipping async sender start.");
                }

                if ((ret == ESP_OK) &&
                    (lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq != 0)) {
                    XPLRLBAND_CONSOLE(D, "GNSS destination handler found in config. Starting async sender.");
                    ret = lbandSetFreqFromCfg(dvcProfile);
                }

                if (ret == ESP_OK) {
                    XPLRLBAND_CONSOLE(D, "LBAND module started successfully.");
                } else {
                    XPLRLBAND_CONSOLE(E, "Failed to start LBAND module!");
                }
            } else {
                XPLRLBAND_CONSOLE(E, "Failed to set LBAND options!");
            }
        } else {
            XPLRLBAND_CONSOLE(E, "Failed to open LBAND module!");
        }
    }

    return ret;
}

esp_err_t xplrLbandStopDevice(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrLbandSendCorrectionDataAsyncStop(dvcProfile);
        if (ret == ESP_OK) {
            ret = lbandDeviceClose(dvcProfile);
            if (ret == ESP_OK) {
                XPLRLBAND_CONSOLE(D, "Successfully stoped LBAND module!");
            } else {
                XPLRLBAND_CONSOLE(E, "Failed to close LBAND module!");
            }
        } else {
            XPLRLBAND_CONSOLE(E, "Failed to stop async data sender!");
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

uDeviceHandle_t *xplrLbandGetHandler(uint8_t dvcProfile)
{
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        return NULL;
    }

    return &lbandDvcs[dvcProfile].options.dvcHandler;
}

esp_err_t xplrLbandSetDestGnssHandler(uint8_t dvcProfile,
                                      uDeviceHandle_t *destHandler)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = ESP_OK;
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    if ((ret == ESP_OK) && (destHandler == NULL)) {
        XPLRLBAND_CONSOLE(E, "destHandler pointer is NULL! Cannot set GNSS destination handler.");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        lbandDvcs[dvcProfile].dvcCfg->destHandler = destHandler;
        XPLRLBAND_CONSOLE(D, "Successfully set GNSS device handler.");
        XPLRLBAND_CONSOLE(D, "Stored GNSS device handler in config.");
    }

    return ret;
}

esp_err_t xplrLbandOptionSingleValSet(uint8_t dvcProfile,
                                      uint32_t keyId,
                                      uint64_t value,
                                      uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrHlprLocSrvcOptionSingleValSet(&lbandDvcs[dvcProfile].options.dvcHandler,
                                                keyId,
                                                value,
                                                U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                layer);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandOptionMultiValSet(uint8_t dvcProfile,
                                     const uGnssCfgVal_t *list,
                                     size_t numValues,
                                     uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrHlprLocSrvcOptionMultiValSet(&lbandDvcs[dvcProfile].options.dvcHandler,
                                               list,
                                               numValues,
                                               U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                               layer);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandOptionSingleValGet(uint8_t dvcProfile,
                                      uint32_t keyId,
                                      void *value,
                                      size_t size,
                                      uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrHlprLocSrvcOptionSingleValGet(&lbandDvcs[dvcProfile].options.dvcHandler,
                                                keyId,
                                                value,
                                                size,
                                                layer);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandOptionMultiValGet(uint8_t dvcProfile,
                                     const uint32_t *keyIdList,
                                     size_t numKeyIds,
                                     uGnssCfgVal_t **list,
                                     uGnssCfgValLayer_t layer)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrHlprLocSrvcOptionMultiValGet(&lbandDvcs[dvcProfile].options.dvcHandler,
                                               keyIdList,
                                               numKeyIds,
                                               list,
                                               layer);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandSetFrequency(uint8_t dvcProfile, uint32_t frequency)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = lbandSetFreqFromPrm(dvcProfile, frequency);
        if (ret == ESP_OK) {
            lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = frequency;
            XPLRLBAND_CONSOLE(D, "Stored frequency into LBAND config!");
        } else {
            XPLRLBAND_CONSOLE(E, "Could net set LBAND frequency!");
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandSetFrequencyFromMqtt(uint8_t dvcProfile,
                                        char *mqttPayload,
                                        xplrLbandRegion_t freqRegion)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if ((ret == ESP_OK) && (mqttPayload == NULL)) {
        XPLRLBAND_CONSOLE(E, "mqttPayload pointer is NULL!");
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region = freqRegion;

        ret = lbandParseFrequencyFromMqtt(dvcProfile, mqttPayload);

        if (ret != ESP_OK || lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq == 0) {
            XPLRLBAND_CONSOLE(E, "Could not parse frequency!");
            ret = ESP_FAIL;
        } else {
            ret = lbandSetFreqFromCfg(dvcProfile);
            if (ret == ESP_OK) {
                XPLRLBAND_CONSOLE(D,
                                  "Set LBAND location: %s frequency: %d Hz successfully!",
                                  freqRegions[lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region],
                                  lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq);
            } else {
                XPLRLBAND_CONSOLE(E,
                                  "Could not set LBAND location: %s frequency: %d Hz!",
                                  freqRegions[lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region],
                                  lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq);
            }
        }
    }

    return ret;
}

uint32_t xplrLbandGetFrequency(uint8_t dvcProfile)
{
    esp_err_t espRet;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);
    uint32_t ret = 0;

    if (boolRet) {
        espRet = xplrLbandOptionSingleValGet(dvcProfile,
                                             U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4,
                                             &ret,
                                             sizeof(ret),
                                             U_GNSS_CFG_VAL_LAYER_RAM);

        if (espRet != ESP_OK) {
            XPLRLBAND_CONSOLE(E, "Could not read frequency from LBAND module!");
            ret = 0;
        }
    }

    return ret;
}

esp_err_t xplrLbandSendFormattedCommand(uint8_t dvcProfile, const char *buffer, size_t size)
{
    esp_err_t ret;
    int32_t intRet;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        intRet = xplrHlprLocSrvcSendUbxFormattedCommand(&lbandDvcs[dvcProfile].options.dvcHandler,
                                                        buffer,
                                                        size);
        if (intRet < 1) {
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrLbandSendCorrectionDataAsyncStart(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        if (xSemaphore == NULL) {
            xSemaphore = xSemaphoreCreateMutex();
        }
        if (lbandDvcs[dvcProfile].dvcCfg->destHandler != NULL) {
            if (lbandDvcs[dvcProfile].options.asyncIds.ahCorrData >= 0) {
                XPLRLBAND_CONSOLE(D, "Looks like LBAND Send Correction Data async is already running!");
            } else {
                lbandDvcs[dvcProfile].options.asyncIds.ahCorrData = uGnssMsgReceiveStart(
                                                                        lbandDvcs[dvcProfile].options.dvcHandler,
                                                                        &messageIdLBand,
                                                                        xplrLbandMessageReceivedCB,
                                                                        lbandDvcs[dvcProfile].dvcCfg->destHandler);
                if (lbandDvcs[dvcProfile].options.asyncIds.ahCorrData < 0) {
                    XPLRLBAND_CONSOLE(E,
                                      "LBAND Send Correction Data async failed to start with error code [%d]",
                                      lbandDvcs[dvcProfile].options.asyncIds.ahCorrData);
                    lbandDvcs[dvcProfile].options.asyncIds.ahCorrData = -1;
                    ret = ESP_FAIL;
                } else {
                    XPLRLBAND_CONSOLE(D, "Started LBAND Send Correction Data async.");
                }
            }
        } else {
            XPLRLBAND_CONSOLE(W,
                              "Gnss destination handler is not initialized [NULL]. Cannot start async sender.");
            ret = ESP_OK;
        }
    }

    return ret;
}

esp_err_t xplrLbandSendCorrectionDataAsyncStop(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);
    int32_t intRet;

    if (boolRet) {
        XPLRLBAND_CONSOLE(I, "Trying to stop LBAND Send Correction Data async.");
        if (lbandDvcs[dvcProfile].options.asyncIds.ahCorrData < 0) {
            XPLRLBAND_CONSOLE(I, "Looks like Correction data async sender is not running. Nothing to do.");
            ret = ESP_OK;
        } else {
            intRet = lbandAsyncStopper(dvcProfile, lbandDvcs[dvcProfile].options.asyncIds.ahCorrData);

            if (intRet == 0) {
                if (xSemaphore != NULL) {
                    vSemaphoreDelete(xSemaphore);
                    xSemaphore = NULL;
                }
                lbandDvcs[dvcProfile].options.asyncIds.ahCorrData = -1;
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
            }
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

bool xplrLbandIsSendCorrectionDataAsyncRunning(uint8_t dvcProfile)
{
    bool ret;

    if (lbandDvcs[dvcProfile].options.asyncIds.ahCorrData != -1) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

esp_err_t xplrLbandGetDeviceInfo(uint8_t dvcProfile, xplrLocDvcInfo_t *dvcInfo)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if ((ret == ESP_OK) && (dvcInfo == NULL)) {
        XPLRLBAND_CONSOLE(E, "dvcInfo pointer is NULL");
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        ret = xplrHlprLocSrvcGetDeviceInfo(&lbandDvcs[dvcProfile].dvcCfg->hwConf,
                                           lbandDvcs[dvcProfile].options.dvcHandler,
                                           dvcInfo);
    }

    return ret;
}

esp_err_t xplrLbandPrintDeviceInfo(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = lbandIsDvcProfileValid(dvcProfile);
    xplrLocDvcInfo_t dvcInfo;

    if (boolRet) {
        ret = xplrLbandGetDeviceInfo(dvcProfile, &dvcInfo);
        if (ret == ESP_OK) {
            ret = xplrHlprLocSrvcPrintDeviceInfo(&dvcInfo);
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

bool xplrLbandHasFrwdMessage(void)
{
    bool ret;

    if (xSemaphore == NULL) {
        ret = false;
    } else {
        if (xSemaphoreTake(xSemaphore, XPLR_LBAND_SEMAPHORE_TIMEOUT) == pdTRUE) {
            ret = hasFrwdCorrMsg;
            hasFrwdCorrMsg = false;
            xSemaphoreGive(xSemaphore);
        } else {
            ret = false;
        }
    }

    return ret;
}

/**
 * Checks if input device profile is valid
 */
static bool lbandIsDvcProfileValid(uint8_t dvcProfile)
{
    bool ret;
    ret = xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES);
    return ret;
}

int8_t xplrLbandInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_LBAND_INFO_DEFAULT_FILENAME,
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

esp_err_t xplrLbandStopLogModule(void)
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

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static esp_err_t lbandDeviceOpen(uint8_t dvcProfile)
{
    esp_err_t ret;
    ret = xplrHlprLocSrvcDeviceOpen(&lbandDvcs[dvcProfile].dvcCfg->hwConf,
                                    &lbandDvcs[dvcProfile].options.dvcHandler);
    return ret;
}

static esp_err_t lbandDeviceClose(uint8_t dvcProfile)
{
    esp_err_t ret;
    ret = xplrHlprLocSrvcDeviceClose(&lbandDvcs[dvcProfile].options.dvcHandler);
    return ret;
}

static int32_t lbandAsyncStopper(uint8_t dvcProfile, int32_t handler)
{
    int32_t intRet;
    intRet = uGnssMsgReceiveStop(lbandDvcs[dvcProfile].options.dvcHandler, handler);
    if (intRet < 0) {
        XPLRLBAND_CONSOLE(E, "Failed to stop async function with error code [%d]!", intRet);
    } else {
        XPLRLBAND_CONSOLE(D, "Successfully stoped async function.");
    }
    return intRet;
}

static esp_err_t lbandSetFreqFromPrm(uint8_t dvcProfile, uint32_t freq)
{
    esp_err_t ret;

    ret = xplrLbandOptionSingleValSet(dvcProfile,
                                      U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4,
                                      (uint64_t)freq,
                                      U_GNSS_CFG_VAL_LAYER_RAM);

    return ret;
}

static esp_err_t lbandSetFreqFromCfg(uint8_t dvcProfile)
{
    esp_err_t ret;

    ret = xplrLbandOptionSingleValSet(dvcProfile,
                                      U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4,
                                      (uint64_t)lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq,
                                      U_GNSS_CFG_VAL_LAYER_RAM);

    return ret;
}

static esp_err_t lbandParseFrequencyFromMqtt(uint8_t dvcProfile,
                                             char *mqttPayload)
{
    esp_err_t ret;
    cJSON *json, *freqs, *jregion, *current, *frequency;
    double tmpfreq = 0;

    if (mqttPayload == NULL) {
        XPLRLBAND_CONSOLE(E, "mqttPayload pointer is NULL");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        json = cJSON_Parse(mqttPayload);

        if (cJSON_HasObjectItem(json, "frequencies")) {
            freqs = cJSON_GetObjectItem(json, "frequencies");
            if (cJSON_HasObjectItem(freqs, freqRegions[lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region])) {
                jregion = cJSON_GetObjectItem(freqs,
                                              freqRegions[lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region]);
                if (cJSON_HasObjectItem(jregion, "current")) {
                    current = cJSON_GetObjectItem(jregion, "current");
                    if (cJSON_HasObjectItem(current, "value")) {
                        frequency = cJSON_GetObjectItem(current, "value");
                        sscanf(cJSON_GetStringValue(frequency), "%lf", &tmpfreq);
                        lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = (uint32_t)((1e+6) * tmpfreq);
                    } else {
                        XPLRLBAND_CONSOLE(E, "Theres no frequency \"value\" object.");
                        lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = 0;
                        ret = ESP_FAIL;
                    }
                } else {
                    XPLRLBAND_CONSOLE(E, "Theres no \"current\" object.");
                    lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = 0;
                    ret = ESP_FAIL;
                }
            } else {
                XPLRLBAND_CONSOLE(E,
                                  "Theres no \"%s\" location object.",
                                  freqRegions[lbandDvcs[dvcProfile].dvcCfg->corrDataConf.region]);
                lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = 0;
                ret = ESP_FAIL;
            }
        } else {
            XPLRLBAND_CONSOLE(E, "Theres no \"frequencies\" object.");
            lbandDvcs[dvcProfile].dvcCfg->corrDataConf.freq = 0;
            ret = ESP_FAIL;
        }

        cJSON_Delete(json);
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void xplrLbandMessageReceivedCB(uDeviceHandle_t gnssHandle,
                                       const uGnssMessageId_t *messageId,
                                       int32_t errorCodeOrLength,
                                       void *callbackParam)
{
    /**
     * Standard is 536 bytes.
     * We allocate  32 bytes extra for future proofing if needed
     */
    char buffer[568];
    int32_t intRet;
    int32_t lbandCbRead;
    static bool correctionDataSentInitial = true;

    if ((errorCodeOrLength > 0) && (errorCodeOrLength <= ELEMENTCNT(buffer))) {
        lbandCbRead = uGnssMsgReceiveCallbackRead(gnssHandle, buffer, errorCodeOrLength);

        intRet = xplrHlprLocSrvcSendUbxFormattedCommand((uDeviceHandle_t *)callbackParam,
                                                        buffer,
                                                        lbandCbRead);

        if (intRet < 0 || intRet != lbandCbRead) {
            XPLRLBAND_CONSOLE(E,
                              "Error sending LBAND correction data to LBAND, size mismatch: was [%d] bytes | sent [%d] bytes!",
                              intRet,
                              lbandCbRead);
            XPLR_CI_CONSOLE(11, "ERROR");
        } else {
            if (xSemaphoreTake(xSemaphore, XPLR_LBAND_SEMAPHORE_TIMEOUT) == pdTRUE) {
                hasFrwdCorrMsg = true;
                xSemaphoreGive(xSemaphore);
            }
            XPLRLBAND_CONSOLE(D,
                              "Sent LBAND correction data size [%d]",
                              intRet);
            if (correctionDataSentInitial) {
                XPLR_CI_CONSOLE(11, "OK");
                correctionDataSentInitial = false;
            }
        }
    } else {
        XPLRLBAND_CONSOLE(W,
                          "Message received [%d bytes] which is invalid! Length must be between [1] and [%d] bytes!",
                          errorCodeOrLength, ELEMENTCNT(buffer));
        XPLR_CI_CONSOLE(11, "ERROR");
    }
}
