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
#include "cJSON.h"
#include "xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRLBAND_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRLBAND_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrLband", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRLBAND_CONSOLE(message, ...) do{} while(0)
#endif

#define XPLRLBAND_FREQ_REGION_EU    "eu"
#define XPLRLBAND_FREQ_REGION_US    "us"

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/**
 * You should not change these given values in any case or your LBAND
 * module will not function properly.
 */
static const uGnssCfgVal_t pLbandSettings[] = {
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
typedef struct xplrGnssDevXtra_t {
    bool    msgAvailable;  /**< check if message is available for reading */
    int32_t ahCorrData;    /**< ubxlib async handler */
} xplrGnssDevXtra_t;

/**
 * Setting struct for LBAND devices
 */
typedef struct xplrLband_type {
    xplrGnssDevBase_t dvcBase;
    xplrGnssDevXtra_t dvcXtra;
} xplrLband_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char *freqRegions[] = {
    XPLRLBAND_FREQ_REGION_EU,
    XPLRLBAND_FREQ_REGION_US
};

static xplrLband_t lbandDvcs[XPLRLBAND_NUMOF_DEVICES] = {NULL};
static int32_t ubxRet;
static int32_t espRet;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t xplrLbandPrivateDeviceOpen(uint8_t dvcProfile);
static esp_err_t xplrLbandPrivateDeviceClose(uint8_t dvcProfile);
static esp_err_t xplrLbandPrivateConfigAllDefault(uint8_t dvcProfile, uint8_t i2cAddress);
static esp_err_t xplrLbandPrivateSetDeviceConfig(uint8_t dvcProfile, uDeviceCfg_t *deviceSettings);
static esp_err_t xplrLbandPrivateSetNetworkConfig(uint8_t dvcProfile, uNetworkCfgGnss_t *deviceNetwork);
static int32_t   xplrLbandPrivateAsyncStopper(uint8_t dvcProfile, int32_t handler);
static esp_err_t xplrLbandParseFrequencyFromMqtt(uint8_t dvcProfile,
                                                 char *mqttPayload,
                                                 xplrLbandRegion region,
                                                 uint32_t *parsedFrequency);

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void xplrLbandMessageReceivedCB(uDeviceHandle_t gnssHandle,
                                       const uGnssMessageId_t *pMessageId,
                                       int32_t errorCodeOrLength,
                                       void *pCallbackParam);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

esp_err_t xplrLbandUbxlibInit(void)
{
    return xplrHelpersUbxlibInit();
}

esp_err_t xplrLbandUbxlibDeinit(void)
{
    return xplrHlprLocSrvcUbxlibDeinit();
}

esp_err_t xplrLbandStartDeviceDefaultSettings(uint8_t dvcProfile, 
                                              uint8_t i2cAddress,
                                              uDeviceHandle_t *destHandler)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    lbandDvcs[dvcProfile].dvcXtra.ahCorrData = -1;

    espRet = xplrLbandPrivateConfigAllDefault(dvcProfile, i2cAddress);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandPrivateDeviceOpen(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandOptionMultiValSet(dvcProfile, 
                                        pLbandSettings, 
                                        ELEMENTCNT(pLbandSettings),
                                        U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        return espRet;
    }

    if (destHandler != NULL) {
        espRet = xplrLbandSendCorrectionDataAsyncStart(dvcProfile, destHandler);
        if (espRet != ESP_OK) {
            return espRet;
        }
    }

    return ESP_OK;
}

esp_err_t xplrLbandStartDevice(uint8_t dvcProfile, 
                               xplrLbandDeviceCfg_t *dvcCfg,
                               uDeviceHandle_t *destHandler)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    lbandDvcs[dvcProfile].dvcXtra.ahCorrData = -1;

    espRet = xplrLbandPrivateSetDeviceConfig(dvcProfile, &dvcCfg->dvcSettings);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandPrivateSetNetworkConfig(dvcProfile, &dvcCfg->dvcNetwork);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandPrivateDeviceOpen(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandOptionMultiValSet(dvcProfile, 
                                        pLbandSettings, 
                                        ELEMENTCNT(pLbandSettings),
                                        U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        return espRet;
    }

    if (destHandler != NULL) {
        espRet = xplrLbandSendCorrectionDataAsyncStart(dvcProfile, destHandler);
        if (espRet != ESP_OK) {
            return espRet;
        }
    }

    return ESP_OK;
}

esp_err_t xplrLbandStopDevice(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrLbandSendCorrectionDataAsyncStop(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrLbandPrivateDeviceClose(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return ESP_OK;
}

uDeviceHandle_t *xplrLbandGetHandler(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return NULL;
    }

    return &lbandDvcs[dvcProfile].dvcBase.dHandler;
}

esp_err_t xplrLbandOptionSingleValSet(uint8_t dvcProfile,
                                      uint32_t keyId,
                                      uint64_t value,
                                      uint32_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionSingleValSet(&lbandDvcs[dvcProfile].dvcBase,
                                             keyId,
                                             value,
                                             U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                             layer);
}

esp_err_t xplrLbandOptionMultiValSet(uint8_t dvcProfile,
                                     const uGnssCfgVal_t *pList,
                                     size_t numValues,
                                     uint32_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionMultiValSet(&lbandDvcs[dvcProfile].dvcBase,
                                            pList,
                                            numValues,
                                            U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                            layer);
}

esp_err_t xplrLbandOptionSingleValGet(uint8_t dvcProfile,
                                      uint32_t keyId,
                                      void *pValue,
                                      size_t size,
                                      uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionSingleValGet(&lbandDvcs[dvcProfile].dvcBase,
                                             keyId,
                                             pValue,
                                             size,
                                             layer);
}

esp_err_t xplrLbandOptionMultiValGet(uint8_t dvcProfile,
                                     const uint32_t *pKeyIdList,
                                     size_t numKeyIds,
                                     uGnssCfgVal_t **pList,
                                     uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionMultiValGet(&lbandDvcs[dvcProfile].dvcBase,
                                            pKeyIdList,
                                            numKeyIds,
                                            pList,
                                            layer);
}

esp_err_t xplrLbandSetFrequency(uint8_t dvcProfile, uint32_t frequency)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrLbandOptionSingleValSet(dvcProfile,
                                         U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4, 
                                         frequency,
                                         U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return ESP_OK;
}

esp_err_t xplrLbandSetFrequencyFromMqtt(uint8_t dvcProfile, char *mqttPayload, xplrLbandRegion region)
{
    esp_err_t ret = ESP_OK;
    uint32_t frequency = 0;
    
    ret = xplrLbandParseFrequencyFromMqtt(dvcProfile, mqttPayload, region, &frequency);

    if (ret != ESP_OK || frequency == 0) {
        XPLRLBAND_CONSOLE(E, "Could not parse frequency!");
        ret = ESP_FAIL;
    } else {
        ret = xplrLbandSetFrequency(dvcProfile, frequency);
        if (ret == ESP_OK) {
            XPLRLBAND_CONSOLE(D, 
                              "Set LBAND location: %s frequency: %d Hz successfully!", 
                              freqRegions[region], 
                              frequency);
        } else {
            XPLRLBAND_CONSOLE(E, "Could not set LBAND location: %s frequency: %d Hz!", frequency);
        }
    }

    return ret;
}

uint32_t xplrLbandGetFrequency(uint8_t dvcProfile)
{
    uint32_t ret = 0;
    esp_err_t espRet = ESP_OK;

    espRet = xplrLbandOptionSingleValGet(dvcProfile,
                                         U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4,
                                         &ret,
                                         sizeof(ret),
                                         U_GNSS_CFG_VAL_LAYER_RAM);

    if (espRet != ESP_OK) {
        XPLRLBAND_CONSOLE(E, "Could not read frequency from LBAND module!");
        ret = 0;
    }

    return ret;
}

int32_t xplrLbandSendFormattedCommand(uint8_t dvcProfile, const char *pBuffer, size_t size)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSendUbxFormattedCommand(&lbandDvcs[dvcProfile].dvcBase.dHandler,
                                                  pBuffer,
                                                  size);
}

esp_err_t xplrLbandSendCorrectionDataAsyncStart(uint8_t dvcProfile, uDeviceHandle_t *destHandler)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (lbandDvcs[dvcProfile].dvcXtra.ahCorrData >= 0) {
        XPLRLBAND_CONSOLE(I, "Looks like LBAND Send Correction Data async is already running!");
        return ESP_OK;
    }

    lbandDvcs[dvcProfile].dvcXtra.ahCorrData = uGnssMsgReceiveStart(lbandDvcs[dvcProfile].dvcBase.dHandler,
                                                                    &messageIdLBand,
                                                                    xplrLbandMessageReceivedCB,
                                                                    destHandler);
    if (lbandDvcs[dvcProfile].dvcXtra.ahCorrData < 0) {
        XPLRLBAND_CONSOLE(E, 
                          "LBAND Send Correction Data async failed to start with error code [%d]",
                          lbandDvcs[dvcProfile].dvcXtra.ahCorrData);
        return ESP_FAIL;
    }

    XPLRLBAND_CONSOLE(D, "Started LBAND Send Correction Data async.");
    return ESP_OK;
}

esp_err_t xplrLbandSendCorrectionDataAsyncStop(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    XPLRLBAND_CONSOLE(I, "Trying to stop LBAND Send Correction Data async.");
    ubxRet = xplrLbandPrivateAsyncStopper(dvcProfile, lbandDvcs[dvcProfile].dvcXtra.ahCorrData);

    if (ubxRet == 0) {
        lbandDvcs[dvcProfile].dvcXtra.ahCorrData = -1;
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrLbandGetDeviceInfo(uint8_t dvcProfile, xplrGnssDevInfo_t *devInfo)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcGetDeviceInfo(&lbandDvcs[dvcProfile].dvcBase, devInfo);
}

esp_err_t xplrLbandPrintDeviceInfo(uint8_t dvcProfile)
{
    xplrGnssDevInfo_t pDevInfo;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrLbandGetDeviceInfo(dvcProfile, &pDevInfo);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return xplrHlprLocSrvcPrintDeviceInfo(&pDevInfo);
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static esp_err_t xplrLbandPrivateDeviceOpen(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcDeviceOpen(&lbandDvcs[dvcProfile].dvcBase);
}

static esp_err_t xplrLbandPrivateDeviceClose(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcDeviceClose(&lbandDvcs[dvcProfile].dvcBase);
}

esp_err_t xplrLbandPrivateConfigAllDefault(uint8_t dvcProfile, uint8_t i2cAddress)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcConfigAllDefault(&lbandDvcs[dvcProfile].dvcBase, i2cAddress);
}

esp_err_t xplrLbandPrivateSetDeviceConfig(uint8_t dvcProfile, uDeviceCfg_t *deviceSettings)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSetDeviceConfig(&lbandDvcs[dvcProfile].dvcBase, deviceSettings);
}

esp_err_t xplrLbandPrivateSetNetworkConfig(uint8_t dvcProfile, uNetworkCfgGnss_t *deviceNetwork)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSetNetworkConfig(&lbandDvcs[dvcProfile].dvcBase, deviceNetwork);
}

static int32_t xplrLbandPrivateAsyncStopper(uint8_t dvcProfile, int32_t handler)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    ubxRet = uGnssMsgReceiveStop(lbandDvcs[dvcProfile].dvcBase.dHandler, handler);
                                 
    if (ubxRet < 0) {
        XPLRLBAND_CONSOLE(E, "Failed to stop async function with error code [%d]!", ubxRet);
        return ubxRet;
    }

    XPLRLBAND_CONSOLE(I, "Successfully stoped async function.");
    return ubxRet;
}

static esp_err_t xplrLbandParseFrequencyFromMqtt(uint8_t dvcProfile,
                                                 char *mqttPayload,
                                                 xplrLbandRegion region,
                                                 uint32_t *parsedFrequency)
{
    esp_err_t ret = ESP_OK;
    cJSON *json, *freqs, *jregion, *current, *frequency;
    double tmpfreq = 0;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRLBAND_NUMOF_DEVICES)) {
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        json = cJSON_Parse(mqttPayload);

        if (cJSON_HasObjectItem(json, "frequencies")){
            freqs = cJSON_GetObjectItem(json, "frequencies");
            if (cJSON_HasObjectItem(freqs, freqRegions[region])) {
                jregion = cJSON_GetObjectItem(freqs, freqRegions[region]);
                if (cJSON_HasObjectItem(jregion, "current")) {
                    current = cJSON_GetObjectItem(jregion, "current");
                    if (cJSON_HasObjectItem(current, "value")) {
                        frequency = cJSON_GetObjectItem(current, "value");
                        sscanf(cJSON_GetStringValue(frequency), "%lf", &tmpfreq);
                        *parsedFrequency = (uint32_t)((1e+6) * tmpfreq);
                    } else {
                        XPLRLBAND_CONSOLE(E, "Theres no frequency \"value\" object.");
                        *parsedFrequency = 0;
                        ret = ESP_FAIL;
                    }
                } else {
                    XPLRLBAND_CONSOLE(E, "Theres no \"current\" object.");
                    *parsedFrequency = 0;
                    ret = ESP_FAIL;
                }
            } else {
                XPLRLBAND_CONSOLE(E, "Theres no \"%s\" location object.", freqRegions[region]);
                *parsedFrequency = 0;
                ret = ESP_FAIL;
            }
        } else {
            XPLRLBAND_CONSOLE(E, "Theres no \"frequencies\" object.");
            *parsedFrequency = 0;
            ret = ESP_FAIL;
        }

        cJSON_Delete(json);
    }
    
    return ESP_OK;
}

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void xplrLbandMessageReceivedCB(uDeviceHandle_t gnssHandle,
                                       const uGnssMessageId_t *pMessageId,
                                       int32_t errorCodeOrLength,
                                       void *pCallbackParam)
{
    /**
     * Standard is 536 bytes.
     * We allocate  32 bytes extra for future proofing if needed
     */
    char pBuffer[568];
    int32_t intRet;

    if ((errorCodeOrLength > 0) && (errorCodeOrLength <= ELEMENTCNT(pBuffer))) {
        int32_t lbandCbRead = uGnssMsgReceiveCallbackRead(gnssHandle, pBuffer, errorCodeOrLength);

        intRet = xplrHlprLocSrvcSendUbxFormattedCommand((uDeviceHandle_t *)pCallbackParam, 
                                                        pBuffer,
                                                        lbandCbRead);

        if (intRet < 0 || intRet != lbandCbRead) {
            XPLRLBAND_CONSOLE(E,
                                "Error sending LBAND correction data to LBAND, size mismatch: was [%d] bytes | sent [%d] bytes!",
                                intRet, 
                                lbandCbRead);
        } else {
            XPLRLBAND_CONSOLE(I,
                                "Sent LBAND correction data size [%d]",
                                lbandCbRead, 
                                intRet);
        }
    } else {
        XPLRLBAND_CONSOLE(W,
                          "Message received [%d bytes] which is invalid! Length must be between [1] and [%d] bytes!",
                          errorCodeOrLength, ELEMENTCNT(pBuffer));
    }
}
