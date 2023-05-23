/*
 * Copyright 2019-2022 u-blox
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

#include <stdint.h>
#include "string.h"
#include "esp_task_wdt.h"
#include "u_cfg_app_platform_specific.h"
#include "xplr_gnss.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRGNSS_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrGnss", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRGNSS_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Set this option to 1 if you want extra debug messages
 */
#define XPLR_GNSS_XTRA_DEBUG 0

/**
 * Used to translate fix types to strings for printing
 * IF YOU EDIT CALCULATE SIZE MANUALLY
 * THIS IS DONE TO SAVE CYLES FOR strlen() checks
 */
#define XPLR_GNSS_LOCFIX_INVALID_STR        "NO FIX"            // 6
#define XPLR_GNSS_LOCFIX_2D3D_STR           "3D"                // 2
#define XPLR_GNSS_LOCFIX_DGNSS_STR          "DGNSS"             // 1 +  5
#define XPLR_GNSS_LOCFIX_FIXED_RTK_STR      "RTK-FIXED"         // 1 +  9 (either fixed of float)
#define XPLR_GNSS_LOCFIX_FLOAT_RTK_STR      "RTK-FLOAT"         // 1 +  9 (either fixed of float)
#define XPLR_GNSS_LOCFIX_DEAD_RECKONING_STR "DEAD RECKONING"    // 1 + 14
// 2 + 6 + 10 + 15  ---> 33 chars + 1
#define XPLR_GNSS_LOCFIX_STR_MAX_LEN        34

/**
 * Check Ubx Interface definitions p.41 for headers 
 * and p.98 for UBX-NAV-HPPOSLLH
 * F9-HPS-1.21_InterfaceDescription_UBX-21019746
 * Header  + 2
 * Class   + 1
 * ID      + 1
 * Length  + 2
 * Payload +36
 * CRC     + 2
 * -----------
 * Ttl Byt  44
*/
#define XPLR_GNSS_HPPOSLLH_LEN  44

/**
 * Check Ubx Interface definitions p.41 for headers 
 * and p.104 for UBX-NAV-PVT
 * F9-HPS-1.21_InterfaceDescription_UBX-21019746
 * Header  + 2
 * Class   + 1
 * ID      + 1
 * Length  + 2
 * Payload +92
 * CRC     + 2
 * -----------
 * Ttl Byt  44
*/
#define XPLR_GNSS_NAVPVT_LEN  100

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/**
 * In order to find the size of each key we have to look at the end of the name.
 * According to that value we can find the size:
 *
 * -----+------------------------------------------------------------
 * CODE | DESCRIPTION
 * -----+------------------------------------------------------------
 * U1   | unsigned 8-bit integer 1 0…28-1 1
 * -----+------------------------------------------------------------
 * I1   | signed 8-bit integer, two's complement 1 -27…27-1 1
 * -----+------------------------------------------------------------
 * X1   | 8-bit bitfield 1 n/a n/a
 * -----+------------------------------------------------------------
 * U2   | unsigned little-endian 16-bit integer 2 0…216-1 1
 * -----+------------------------------------------------------------
 * I2   | signed little-endian 16-bit integer, two's complement 2 -215…215-1 1
 * -----+------------------------------------------------------------
 * X2   | 16-bit little-endian bitfield 2 n/a n/a
 * -----+------------------------------------------------------------
 * U4   | unsigned little-endian 32-bit integer 4 0…232-1 1
 * -----+------------------------------------------------------------
 * I4   | signed little-endian 32-bit integer, two's complement 4 -231…231-1 1
 * -----+------------------------------------------------------------
 * X4   | 32-bit little-endian bitfield 4 n
 * -----+------------------------------------------------------------
 * R4   | IEEE 754 single (32-bit) precision 4 -2127…2127 ~ value·2-24
 * -----+------------------------------------------------------------
 * R8   | IEEE 754 double (64-bit) precision 8 -21023…21023 ~ value·2-53
 * -----+------------------------------------------------------------
 * CH   | ASCII / ISO 8859-1 char (8-bit) 1 n/a n/a
 * -----+------------------------------------------------------------
 * U:n  | unsigned bitfield value of n bits width var. variable variable
 * -----+------------------------------------------------------------
 * I:n  | signed (two's complement) bitfield value of n bits width var. variable variable
 * -----+------------------------------------------------------------
 * S:n  | signed bitfield value of n bits width, in sign (most significant bit) and magnitude (remaining bits) notation var. variable variable
 * -----+------------------------------------------------------------
 *
 * Example of value
 *      U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1 <--- ending
 * has an ending of "U1" that means
 * that it is an 8bit integer according to the table
 *
 * Taken from the following docs:
 * https://content.u-blox.com/sites/default/files/documents/u-blox-F9-HPG-1.32_InterfaceDescription_UBX-22008968.pdf
 */
static const uGnssCfgVal_t pGnssSetting[] = {
    {U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_SFCORE_USE_SF_L, 0}
};

/**
 * Struct that contains extra info needed only for
 * specific device type: GNSS
 */
typedef struct xplrGnssXtra_type {
    bool                 msgDataRefreshed;   /**< check if any type of location metrics was changed */
    bool                 msgLocationDataAvailable ; /**< check if GNSS message is available for reading */
    int32_t              ahGeolocation;  /**< ubxlib async handler for Geolocation */
    int32_t              ahAccuracy;     /**< ubxlib async handler for Accuracy */
    int32_t              ahLocFixType;   /**< ubxlib async handler for Location Fix Type */
    xplrGnssLocation_t   locData;        /**< location info */
} xplrGnssDevXtra_t;

/**
 * Settings and data struct for GNSS devices
 */
typedef struct xplrGnss_type {
    xplrGnssDevBase_t dvcBase;
    xplrGnssDevXtra_t dvcXtra;
} xplrGnss_t;

/**
 * Message ID for UBX_NAV_PVT
 * Geolocation reading
 */
static const uGnssMessageId_t msgIdNavPvt = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x0107    /**< ubx protocol command class(c)/id(i) 0xccii */
};

/**
 * Message ID for HPPOSLLH
 * Accuracy reading
 */
static const uGnssMessageId_t msgIdHpposllh = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x0114    /**< ubx protocol command class(c)/id(i) 0xccii */
};

/**
 * Message ID for GNGGA
 * Fix type reading
 */
static const uGnssMessageId_t msgIdFixType = {
    .type = U_GNSS_PROTOCOL_NMEA,
    .id.pNmea = "GNGGA\0"
};

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static xplrGnss_t gnssDvcs[XPLRGNSS_NUMOF_DEVICES] = {NULL};
static const char *gLocationUrlPart = "https://maps.google.com/?q=";
static int32_t ubxRet;
static esp_err_t espRet;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t xplrGnssPrivateSetDeviceConfig(uint8_t dvcProfile, uDeviceCfg_t *deviceSettings);
static esp_err_t xplrGnssPrivateSetNetworkConfig(uint8_t dvcProfile, uNetworkCfgGnss_t *deviceNetwork);
static esp_err_t xplrGnssPrivateConfigAllDefault(uint8_t dvcProfile, uint8_t i2cAddress);
static esp_err_t xplrGnssPrivateDeviceOpen(uint8_t dvcProfile);
static esp_err_t xplrGnssPrivateDeviceClose(uint8_t dvcProfile);
static esp_err_t xplrGnssPrivateGetLocFixType(uint8_t dvcProfile, char *pBuff);
static esp_err_t xplrGnssPrivateFixTypeToString(xplrGnssLocation_t *locData, char *pBuffer);
static int32_t   xplrGnssPrivateAsyncStopper(uint8_t dvcProfile, int32_t handler);
static esp_err_t xplrGnssPrivateGeolocationParser(uint8_t dvcProfile, char *pBuffer);
static esp_err_t xplrGnssPrivateAccuracyParser(uint8_t dvcProfile, char *pBuffer);

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void xplrGnssGeolocationCB(uDeviceHandle_t gnssHandle,
                                  const uGnssMessageId_t *pMessageId,
                                  int32_t errorCodeOrLength,
                                  void *pCallbackParam);
static void xplrGnssFixTypeCB(uDeviceHandle_t gnssHandle,
                              const uGnssMessageId_t *pMessageId,
                              int32_t errorCodeOrLength,
                              void *pCallbackParam);
static void xplrGnssAccuracyCB(uDeviceHandle_t gnssHandle,
                               const uGnssMessageId_t *pMessageId,
                               int32_t errorCodeOrLength,
                               void *pCallbackParam);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

esp_err_t xplrGnssUbxlibInit(void)
{
    return xplrHelpersUbxlibInit();
}

esp_err_t xplrGnssStartDeviceDefaultSettings(uint8_t dvcProfile, uint8_t i2cAddress)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgLocationDataAvailable  = false;

    gnssDvcs[dvcProfile].dvcXtra.ahGeolocation = -1;
    gnssDvcs[dvcProfile].dvcXtra.ahAccuracy = -1;
    gnssDvcs[dvcProfile].dvcXtra.ahLocFixType = -1;

    espRet = xplrGnssPrivateConfigAllDefault(dvcProfile, i2cAddress);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssPrivateDeviceOpen(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssOptionMultiValSet(dvcProfile, 
                                       pGnssSetting, 
                                       ELEMENTCNT(pGnssSetting),
                                       U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssStartAllAsyncGets(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return ESP_OK;
}

esp_err_t xplrGnssStartDevice(uint8_t dvcProfile, xplrGnssDeviceCfg_t *dvcCfg)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgLocationDataAvailable  = false;

    gnssDvcs[dvcProfile].dvcXtra.ahGeolocation = -1;
    gnssDvcs[dvcProfile].dvcXtra.ahAccuracy = -1;
    gnssDvcs[dvcProfile].dvcXtra.ahLocFixType = -1;

    espRet = xplrGnssPrivateSetDeviceConfig(dvcProfile, &dvcCfg->dvcSettings);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssPrivateSetNetworkConfig(dvcProfile, &dvcCfg->dvcNetwork);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssPrivateDeviceOpen(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssOptionMultiValSet(dvcProfile, 
                                       pGnssSetting, 
                                       ELEMENTCNT(pGnssSetting),
                                       U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssStartAllAsyncGets(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return ESP_OK;
}

esp_err_t xplrGnssStopDevice(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrGnssStopAllAsyncGets(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    espRet = xplrGnssPrivateDeviceClose(dvcProfile);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return ESP_OK;
}

esp_err_t xplrGnssUbxlibDeinit(void)
{
    return xplrHlprLocSrvcUbxlibDeinit();
}

uDeviceHandle_t *xplrGnssGetHandler(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return NULL;
    }

    return xplrHlprLocSrvcGetHandler(&gnssDvcs[dvcProfile].dvcBase);
}

esp_err_t xplrGnssOptionSingleValSet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     uint64_t value,
                                     uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionSingleValSet(&gnssDvcs[dvcProfile].dvcBase,
                                             keyId,
                                             value,
                                             U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                             layer);
}

esp_err_t xplrGnssOptionMultiValSet(uint8_t dvcProfile,
                                    const uGnssCfgVal_t *pList,
                                    size_t numValues,
                                    uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionMultiValSet(&gnssDvcs[dvcProfile].dvcBase,
                                            pList,
                                            numValues,
                                            U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                            layer);
}

esp_err_t xplrGnssOptionSingleValGet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     void *pValue,
                                     size_t size,
                                     uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionSingleValGet(&gnssDvcs[dvcProfile].dvcBase,
                                             keyId,
                                             pValue,
                                             size,
                                             layer);
}

esp_err_t xplrGnssOptionMultiValGet(uint8_t dvcProfile,
                                    const uint32_t *pKeyIdList,
                                    size_t numKeyIds,
                                    uGnssCfgVal_t **pList,
                                    uGnssCfgValLayer_t layer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcOptionMultiValGet(&gnssDvcs[dvcProfile].dvcBase,
                                            pKeyIdList,
                                            numKeyIds,
                                            pList,
                                            layer);
}

esp_err_t xplrGnssSetCorrectionDataSource(uint8_t dvcProfile, xplrGnssCorrDataSrc_t src)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (src == XPLR_GNSS_CORRECTION_FROM_IP || src == XPLR_GNSS_CORRECTION_FROM_LBAND) {
        return xplrGnssOptionSingleValSet(dvcProfile,
                                          U_GNSS_CFG_VAL_KEY_ID_SPARTN_USE_SOURCE_E1,
                                          src,
                                          U_GNSS_CFG_VAL_LAYER_RAM);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid correction data source [%d]!", src);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrGnssSendDecryptionKeys(uint8_t dvcProfile, const char *pBuffer, size_t size)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    ubxRet = xplrGnssSendFormattedCommand(dvcProfile, pBuffer, size);
    if (ubxRet < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrGnssSendCorrectionData(uint8_t dvcProfile, const char *pBuffer, size_t size)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    ubxRet = xplrGnssSendFormattedCommand(dvcProfile, pBuffer, size);
    if (ubxRet < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

int32_t xplrGnssSendFormattedCommand(uint8_t dvcProfile, const char *pBuffer, size_t size)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSendUbxFormattedCommand(&gnssDvcs[dvcProfile].dvcBase.dHandler,
                                                  pBuffer,
                                                  size);
}

esp_err_t xplrGnssStartAllAsyncGets(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetGeolocationAsyncStart(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetFixTypeAsyncStart(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetAccuracyAsyncStart(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    XPLRGNSS_CONSOLE(I, "Started all async getters.");
    return ESP_OK;
}

esp_err_t xplrGnssStopAllAsyncGets(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetGeolocationAsyncStop(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetFixTypeAsyncStop(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetAccuracyAsyncStop(dvcProfile);
    if (espRet != ESP_OK) {
        return ESP_FAIL;
    }

    XPLRGNSS_CONSOLE(I, "Stopped all async getters.");
    return ESP_OK;
}

bool xplrGnssHasMessage(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return false;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.msgLocationDataAvailable  && gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed) {
        return true;
    }

    return false;
}

esp_err_t xplrGnssConsumeMessage(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = false;

    return ESP_OK;
}

esp_err_t xplrGnssGetGmapsLocation(uint8_t dvcProfile, char *gmapsLocationRes, uint16_t maxLen)
{
    int writeLen;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    writeLen = snprintf(gmapsLocationRes, 
                        maxLen, 
                        "%s%f,%f", 
                        gLocationUrlPart,
                        gnssDvcs[dvcProfile].dvcXtra.locData.location.latitudeX1e7  * (1e-7),
                        gnssDvcs[dvcProfile].dvcXtra.locData.location.longitudeX1e7 * (1e-7));

    if (writeLen < 0) {
        XPLRGNSS_CONSOLE(E, "Getting GMaps location failed with error code[%d]!", writeLen);
        return ESP_FAIL;
    } else if (writeLen == 0) {
        XPLRGNSS_CONSOLE(E, "Getting GMpas location failed!");
        XPLRGNSS_CONSOLE(E, "Nothing was written in the buffer");
        return ESP_FAIL;
    } else if (writeLen >= maxLen) {
        XPLRGNSS_CONSOLE(E, "Getting GMaps location failed!");
        XPLRGNSS_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
        return ESP_FAIL;
    } else {
        //XPLRGNSS_CONSOLE(D, "Got GMaps location successfully.");
    }

    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;

    return ESP_OK;
}

esp_err_t xplrGnssPrintGmapsLocation(int8_t dvcProfile)
{
#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char gmapsLocationRes[64];
    bool hasMessage;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    hasMessage = xplrGnssHasMessage(dvcProfile);
    if (hasMessage) {
        espRet = xplrGnssGetGmapsLocation(dvcProfile, gmapsLocationRes, ELEMENTCNT(gmapsLocationRes));
        if (espRet != ESP_OK) {
            XPLRGNSS_CONSOLE(E, "Error printing GMaps location!");
            return ESP_FAIL;
        }

        XPLRGNSS_CONSOLE(I, "Printing GMapsLocation!");
        printf("%s\n", gmapsLocationRes);
    }
#endif

    return ESP_OK;
}

esp_err_t xplrGnssGetGeolocation(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahGeolocation >= 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Get Geolocation async is already running! Do not run blocking version when async is running.");
        return ESP_OK;
    }

    ubxRet = uLocationGet(gnssDvcs[dvcProfile].dvcBase.dHandler,
                          U_LOCATION_TYPE_GNSS,
                          NULL,
                          NULL,
                          &gnssDvcs[dvcProfile].dvcXtra.locData.location,
                          NULL);

     if (ubxRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRGNSS_CONSOLE(W, "Getting location timed out!");
        return ESP_OK;
     }
        
    if (ubxRet != U_ERROR_COMMON_SUCCESS) {
        XPLRGNSS_CONSOLE(E, "Failed getting location with error code [%d]!", ubxRet);
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;

    XPLRGNSS_CONSOLE(I, "Got location struct.");
    return ESP_OK;
}

esp_err_t xplrGnssGetGeolocationAsyncStart(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahGeolocation >= 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Gnss Get Geolocation async is already running!");
        return ESP_OK;
    }

    gnssDvcs[dvcProfile].dvcXtra.ahGeolocation = uGnssMsgReceiveStart(gnssDvcs[dvcProfile].dvcBase.dHandler,
                                                                      &msgIdNavPvt,
                                                                      xplrGnssGeolocationCB,
                                                                      NULL);
    if (gnssDvcs[dvcProfile].dvcXtra.ahGeolocation < 0) {
        XPLRGNSS_CONSOLE(E, "Gnss Get Geolocation async failed to start with error code [%d]",
                         gnssDvcs[dvcProfile].dvcXtra.ahGeolocation);
        return ESP_FAIL;
    }

    XPLRGNSS_CONSOLE(I, "Started Gnss Get Geolocation async.");
    return ESP_OK;
}

esp_err_t xplrGnssGetGeolocationAsyncStop(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahGeolocation < 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Gnss Get Geolocation async is not running. Nothing to do.");
        return ESP_OK;
    }

    XPLRGNSS_CONSOLE(I, "Trying to stop Gnss Get Geolocation async.");
    ubxRet = xplrGnssPrivateAsyncStopper(dvcProfile, gnssDvcs[dvcProfile].dvcXtra.ahGeolocation);

    if (ubxRet == 0) {
        gnssDvcs[dvcProfile].dvcXtra.ahGeolocation = -1;
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrGnssGetDeviceInfo(uint8_t dvcProfile, xplrGnssDevInfo_t *pDevInfo)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }
    
    return xplrHlprLocSrvcGetDeviceInfo(&gnssDvcs[dvcProfile].dvcBase, pDevInfo);
}

esp_err_t xplrGnssPrintDeviceInfo(uint8_t dvcProfile)
{
    xplrGnssDevInfo_t pDevInfo;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    espRet = xplrGnssGetDeviceInfo(dvcProfile, &pDevInfo);
    if (espRet != ESP_OK) {
        return espRet;
    }

    return xplrHlprLocSrvcPrintDeviceInfo(&pDevInfo);
}

esp_err_t xplrGnssGetLocation(uint8_t dvcProfile, xplrGnssLocation_t *pLocData)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    memcpy(pLocData, &gnssDvcs[dvcProfile].dvcXtra.locData, sizeof(xplrGnssLocation_t));

    return ESP_OK;
}

esp_err_t xplrGnssPrintLocation(uint8_t dvcProfile)
{
#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    xplrGnssLocation_t pLocData;
    uint8_t timeParseRet;
    char locFixTypeStr[XPLR_GNSS_LOCFIX_STR_MAX_LEN];
    char timeToHuman[32];
    bool hasMessage;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    hasMessage = xplrGnssHasMessage(dvcProfile);
    if (hasMessage) {
        espRet = xplrGnssGetLocation(dvcProfile, &pLocData);
        if (espRet != ESP_OK) {
            return espRet;
        }

        espRet = xplrGnssPrivateFixTypeToString(&pLocData, locFixTypeStr);

        XPLRGNSS_CONSOLE(I, "Printing location info.");

        printf("======== Location Info ========\n");
        printf("Location type: %d\n", pLocData.location.type);
        printf("Location fix type: %s\n", locFixTypeStr);
        printf("Location latitude: %f (raw: %d)\n", 
               pLocData.location.latitudeX1e7  * (1e-7),
               pLocData.location.latitudeX1e7);
        printf("Location longitude: %f (raw: %d)\n", 
               pLocData.location.longitudeX1e7 * (1e-7),
               pLocData.location.longitudeX1e7);

        if (pLocData.location.altitudeMillimetres != INT_MIN) {
            printf("Location altitude: %f (m) | %d (mm)\n", 
                   pLocData.location.altitudeMillimetres * (1e-3),
                   pLocData.location.altitudeMillimetres);
        } else {
            printf("Location altitude: N/A\n");
        }

        if (pLocData.location.radiusMillimetres != -1) {
            printf("Location radius: %f (m) | %d (mm)\n", 
                   pLocData.location.radiusMillimetres * (1e-3),
                   pLocData.location.radiusMillimetres);
        } else {
            printf("Location radius: N/A\n");
        }

        if (pLocData.location.speedMillimetresPerSecond != INT_MIN) {
            printf("Speed: %f (km/h) | %f (m/s) | %d (mm/s)\n",
                   pLocData.location.speedMillimetresPerSecond * (1e-6) * 3600,
                   pLocData.location.speedMillimetresPerSecond * (1e-3),
                   pLocData.location.speedMillimetresPerSecond);
        } else {
            printf("Location radius: N/A\n");
        }

        printf("Estimated horizontal accuracy: %.4f (m) | %.2f (mm)\n",
               pLocData.accuracy.horizontal * (1e-4),
               pLocData.accuracy.horizontal * (1e-1));

        printf("Estimated vertical accuracy: %.4f (m) | %.2f (mm)\n",
               pLocData.accuracy.vertical * (1e-4),
               pLocData.accuracy.vertical * (1e-1));

        if (pLocData.location.svs != -1) {
            printf("Satellite number: %d\n", pLocData.location.svs);
        } else {
            printf("Satellite number: N/A\n");
        }

        if (pLocData.location.timeUtc != -1) {
            timeParseRet = timestampToTime(pLocData.location.timeUtc, 
                                           timeToHuman, 
                                           ELEMENTCNT(timeToHuman));
            if (timeParseRet == ESP_OK) {
                printf("Time UTC: %s\n", timeToHuman);
            } else {
                printf("Time UTC: Error Parsing Time\n");
            }

            timeParseRet = timestampToDate(pLocData.location.timeUtc, 
                                           timeToHuman, 
                                           ELEMENTCNT(timeToHuman));
            if (timeParseRet == ESP_OK) {
                printf("Date UTC: %s\n", timeToHuman);
            } else {
                printf("Date UTC: Error Parsing Time\n");
            }

            timeParseRet = timestampToDateTime(pLocData.location.timeUtc, 
                                               timeToHuman, 
                                               ELEMENTCNT(timeToHuman));
            if (timeParseRet == ESP_OK) {
                printf("Calendar Time UTC: %s\n", timeToHuman);
            } else {
                printf("Calendar Time UTC: Error Parsing Time\n");
            }
        } else {
            printf("Time UTC: N/A\n");
        }

        printf("===============================\n");
    }
#endif

    return ESP_OK;
}

esp_err_t xplrGnssGetFixTypeAsyncStart(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahLocFixType >= 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Get Fix Type async is already running!");
        return ESP_OK;
    }

    gnssDvcs[dvcProfile].dvcXtra.ahLocFixType = uGnssMsgReceiveStart(gnssDvcs[dvcProfile].dvcBase.dHandler,
                                                                     &msgIdFixType,
                                                                     xplrGnssFixTypeCB,
                                                                     NULL);
    if (gnssDvcs[dvcProfile].dvcXtra.ahLocFixType < 0) {
        XPLRGNSS_CONSOLE(E, 
                         "Gnss Get Fix Type async failed to start with error code [%d]",
                         gnssDvcs[dvcProfile].dvcXtra.ahLocFixType);
        return ESP_FAIL;
    }

    XPLRGNSS_CONSOLE(I, "Started Gnss Get Fix Type async.");
    return ESP_OK;
}

esp_err_t xplrGnssGetFixTypeAsyncStop(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahLocFixType < 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Gnss Get Fix Type async is not running. Nothing to do.");
        return ESP_OK;
    }

    XPLRGNSS_CONSOLE(I, "Trying to stop Gnss Get Fix Type async.");

    ubxRet = xplrGnssPrivateAsyncStopper(dvcProfile, gnssDvcs[dvcProfile].dvcXtra.ahLocFixType);

    if (ubxRet == 0) {
        gnssDvcs[dvcProfile].dvcXtra.ahLocFixType = -1;
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t xplrGnssGetFixType(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahLocFixType < 0) {
        XPLRGNSS_CONSOLE(W, "Looks like Gnss Get Fix Type async is running! Do not run blocking version when async is running.");
        return ESP_OK;
    }

    /**
     * Setting up double pointer buffer
     */
    char buffer[128];
    char *pBuffer   = buffer;
    char **ppBuffer = &pBuffer;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    ubxRet = uGnssMsgReceive(gnssDvcs[dvcProfile].dvcBase.dHandler,
                             &msgIdFixType,
                             ppBuffer,
                             ELEMENTCNT(buffer),
                             XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS,
                             NULL);

    if (ubxRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRGNSS_CONSOLE(W, "Receiving Fix type message timed out!");
        return ESP_OK;
    }

    if (ubxRet < 0) {
        XPLRGNSS_CONSOLE(E, "Receiving Fix type message failed!");
        return ESP_FAIL;
    }

    espRet = xplrGnssPrivateGetLocFixType(dvcProfile, pBuffer);
    if (espRet != ESP_OK) {
        XPLRGNSS_CONSOLE(E, "Parsing Fix type failed!");
        return espRet;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;

    return ESP_OK;
}

esp_err_t xplrGnssGetAccuracy(uint8_t dvcProfile)
{
    char buffer[XPLR_GNSS_HPPOSLLH_LEN];
    char *pBuffer   = buffer;
    char **ppBuffer = &pBuffer;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahAccuracy < 0) {
        XPLRGNSS_CONSOLE(W, "Looks like Gnss Get Accuracy async is running! Do not run blocking version when async is running.");
        return ESP_OK;
    }

    /**
     * wait XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS for response
     */
    ubxRet = uGnssMsgReceive(gnssDvcs[dvcProfile].dvcBase.dHandler, 
                             &msgIdHpposllh, 
                             ppBuffer,
                             ELEMENTCNT(buffer),
                             XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS,
                             NULL);

    if (ubxRet == U_ERROR_COMMON_TIMEOUT) {
        XPLRGNSS_CONSOLE(W, "Receiving accuracy data timed out!");
        return ESP_OK;
    }

    if (ubxRet < 0) {
        XPLRGNSS_CONSOLE(E, "Error getting accuracy data!");
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;

    return xplrGnssPrivateAccuracyParser(dvcProfile, buffer);
}

esp_err_t xplrGnssGetAccuracyAsyncStart(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahAccuracy >= 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Gnss Get Accuracy async is already running!");
        return ESP_OK;
    }

    gnssDvcs[dvcProfile].dvcXtra.ahAccuracy = uGnssMsgReceiveStart(gnssDvcs[dvcProfile].dvcBase.dHandler,
                                                                   &msgIdHpposllh,
                                                                   xplrGnssAccuracyCB,
                                                                   NULL);
    if (gnssDvcs[dvcProfile].dvcXtra.ahAccuracy < 0) {
        XPLRGNSS_CONSOLE(E, "Gnss Get Accuracy async failed to start with error code [%d]",
                         gnssDvcs[dvcProfile].dvcXtra.ahAccuracy);
        return ESP_FAIL;
    }

    XPLRGNSS_CONSOLE(I, "Started Gnss Get Accuracy async.");
    return ESP_OK;
}

esp_err_t xplrGnssGetAccuracyAsyncStop(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (gnssDvcs[dvcProfile].dvcXtra.ahAccuracy < 0) {
        XPLRGNSS_CONSOLE(I, "Looks like Gnss Get Accuracy async is not running. Nothing to do.");
        return ESP_OK;
    }

    XPLRGNSS_CONSOLE(I, "Trying to stop Gnss Get Accuracy async.");
    ubxRet = xplrGnssPrivateAsyncStopper(dvcProfile, gnssDvcs[dvcProfile].dvcXtra.ahAccuracy);

    if (ubxRet == 0) {
        gnssDvcs[dvcProfile].dvcXtra.ahAccuracy = -1;
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * Private function to set device config.
 */
static esp_err_t xplrGnssPrivateSetDeviceConfig(uint8_t dvcProfile, uDeviceCfg_t *deviceSettings)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSetDeviceConfig(&gnssDvcs[dvcProfile].dvcBase, deviceSettings);
}

/**
 * Private function to set network config.
 */
static esp_err_t xplrGnssPrivateSetNetworkConfig(uint8_t dvcProfile, uNetworkCfgGnss_t *deviceNetwork)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcSetNetworkConfig(&gnssDvcs[dvcProfile].dvcBase, deviceNetwork);
}

/**
 * Configs all default settings.
 */
static esp_err_t xplrGnssPrivateConfigAllDefault(uint8_t dvcProfile, uint8_t i2cAddress)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcConfigAllDefault(&gnssDvcs[dvcProfile].dvcBase, i2cAddress);
}

/**
 * Opens communication with a device.
 */
static esp_err_t xplrGnssPrivateDeviceOpen(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcDeviceOpen(&gnssDvcs[dvcProfile].dvcBase);
}

/**
 * Closes communication with a device
 */
static esp_err_t xplrGnssPrivateDeviceClose(uint8_t dvcProfile)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    return xplrHlprLocSrvcDeviceClose(&gnssDvcs[dvcProfile].dvcBase);
}

/**
 * Writes the location fix type to a string buffer
 */
static esp_err_t xplrGnssPrivateFixTypeToString(xplrGnssLocation_t *pLocData, char *pBuffer)
{
    pBuffer[0] = 0;

    if (pLocData->locFixType == XPLR_GNSS_LOCFIX_INVALID) {
        strcpy(pBuffer, XPLR_GNSS_LOCFIX_INVALID_STR);
        return ESP_OK;
    }

    if (pLocData->locFixType >= XPLR_GNSS_LOCFIX_2D3D) {
        strcat(pBuffer, XPLR_GNSS_LOCFIX_2D3D_STR);
    }

    if (pLocData->locFixType >= XPLR_GNSS_LOCFIX_DGNSS) {
        strcat(pBuffer, "\\");
        strcat(pBuffer, XPLR_GNSS_LOCFIX_DGNSS_STR);
    }

    if ((pLocData->locFixType >= XPLR_GNSS_LOCFIX_FLOAT_RTK) &&
        (pLocData->locFixType != XPLR_GNSS_LOCFIX_FIXED_RTK)) {
        strcat(pBuffer, "\\");
        strcat(pBuffer, XPLR_GNSS_LOCFIX_FLOAT_RTK_STR);
    }

    if ((pLocData->locFixType >= XPLR_GNSS_LOCFIX_FIXED_RTK) &&
        (pLocData->locFixType != XPLR_GNSS_LOCFIX_FLOAT_RTK)) {
        strcat(pBuffer, "\\");
        strcat(pBuffer, XPLR_GNSS_LOCFIX_FIXED_RTK_STR);
    }

    if (pLocData->locFixType == XPLR_GNSS_LOCFIX_DEAD_RECKONING) {
        strcpy(pBuffer, XPLR_GNSS_LOCFIX_DEAD_RECKONING_STR);
    }

    return ESP_OK;
}

/**
 * Parses the appropriate NMEA message (GNGGA) and tries to extract 
 * fix type
 */
static esp_err_t xplrGnssPrivateGetLocFixType(uint8_t dvcProfile, char *pBuffer)
{
    uint8_t strIdx;
    uint8_t commaCnt = 0;
    static uint8_t noFixCnt;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    /**
     * We are parsing the following message:
     *      $GNGGA,185115.00,3758.82530,N,02339.41564,E,1,12,0.54,64.8,M,33.1,M,,*7E
     * 7th part is what we are looking for -------------^
     */

    if (strlen(pBuffer) < 1) {
        return ESP_OK;
    }

    /**
     * Try to find 6th comma and stop
     */
    for (strIdx = 0; strIdx < strlen(pBuffer); strIdx++) {
        if (pBuffer[strIdx] == ',') {
            commaCnt++;
        }

        if (commaCnt == 6) {
            break;
        }
    }

    if (commaCnt < 6) {
        XPLRGNSS_CONSOLE(W, "Could not reach the 7th segment for the GNGGA message!");
        return ESP_FAIL;
    }

    /**
     * Check if next character is another comma or terminator
     */
    if (pBuffer[strIdx + 1] == 0 || pBuffer[strIdx + 2] == 0) {
        XPLRGNSS_CONSOLE(W, "Seems like GNGGA is terminating early.");
        return ESP_FAIL;
    } else if (pBuffer[strIdx + 1] == ',') {
        if (noFixCnt == 10) {
            gnssDvcs[dvcProfile].dvcXtra.locData.locFixType = XPLR_GNSS_LOCFIX_INVALID;
#if 1 == XPLR_GNSS_XTRA_DEBUG
            XPLRGNSS_CONSOLE(W, "Seems like location fix type has not been parsed for the last 10 messages!");
#endif
        }

        if (noFixCnt < 10) {
            noFixCnt++;
        }
        return ESP_OK;
    } else if (pBuffer[strIdx + 2] != ',') {
        XPLRGNSS_CONSOLE(W, "Seems like location fix type is not a single char!");
        return ESP_OK;
    } else  if (pBuffer[strIdx + 1] < '0' || pBuffer[strIdx + 1] > '6' || pBuffer[strIdx + 1] == '3') {
        XPLRGNSS_CONSOLE(W, "Seems like location fix type is not a valid char [%c]!", pBuffer[strIdx + 1]);
        return ESP_FAIL;
    }

    noFixCnt = 0;
    gnssDvcs[dvcProfile].dvcXtra.locData.locFixType = pBuffer[strIdx + 1] - '0';

    switch (gnssDvcs[dvcProfile].dvcXtra.locData.locFixType) {
        case XPLR_GNSS_LOCFIX_2D3D:
        case XPLR_GNSS_LOCFIX_DGNSS:
        case XPLR_GNSS_LOCFIX_FIXED_RTK:
        case XPLR_GNSS_LOCFIX_FLOAT_RTK:
        case XPLR_GNSS_LOCFIX_DEAD_RECKONING:
        case XPLR_GNSS_LOCFIX_INVALID:
            espRet = ESP_OK;
            break;

        default:
            espRet = ESP_FAIL;
            break;
    }

    return espRet;
}

static int32_t xplrGnssPrivateAsyncStopper(uint8_t dvcProfile, int32_t handler)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    ubxRet = uGnssMsgReceiveStop(gnssDvcs[dvcProfile].dvcBase.dHandler, handler);
                                 
    if (ubxRet < 0) {
        XPLRGNSS_CONSOLE(E, "Failed to stop async function with error code [%d]!", ubxRet);
        return ubxRet;
    }

    XPLRGNSS_CONSOLE(I, "Successfully stoped async function.");
    return ubxRet;
}

static esp_err_t xplrGnssPrivateAccuracyParser(uint8_t dvcProfile, char *pBuffer)
{
    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if ((pBuffer == NULL) || ((pBuffer + 34) == NULL) || ((pBuffer + 38) == NULL)) {
        XPLRGNSS_CONSOLE(E, "Null pointer detected!");
        return ESP_FAIL;
    }

    gnssDvcs[dvcProfile].dvcXtra.locData.accuracy.horizontal = 0;
    gnssDvcs[dvcProfile].dvcXtra.locData.accuracy.vertical   = 0;

    /**
     * For HPPOSLLH
     * 28 + 6 U4 (32bit unsigned) hAcc 0.1 mm Horizontal accuracy estimate
     * 32 + 6 U4 (32bit unsigned) vAcc 0.1 mm Vertical   accuracy estimate
     * ==========================================
     * + 6 for offset (Header 2 + Class 1 + ID 1 + Length 2)
     */
    gnssDvcs[dvcProfile].dvcXtra.locData.accuracy.horizontal = (uint32_t) uUbxProtocolUint32Decode(pBuffer + 34);
    gnssDvcs[dvcProfile].dvcXtra.locData.accuracy.vertical   = (uint32_t) uUbxProtocolUint32Decode(pBuffer + 38);

    return ESP_OK;
}

static esp_err_t xplrGnssPrivateGeolocationParser(uint8_t dvcProfile, char *pBuffer)
{
    int32_t months;
    int32_t year;
    int64_t t = -1;

    if (!xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES)) {
        return ESP_FAIL;
    }

    if (pBuffer == NULL) {
        XPLRGNSS_CONSOLE(E, "Null pointer detected!");
        return ESP_FAIL;
    }

    switch(gnssDvcs[dvcProfile].dvcBase.dConfig.deviceType){
        case U_DEVICE_TYPE_GNSS:
            gnssDvcs[dvcProfile].dvcXtra.locData.location.type = U_LOCATION_TYPE_GNSS;
            break;
        default:
            gnssDvcs[dvcProfile].dvcXtra.locData.location.type = U_LOCATION_TYPE_NONE;
            break;
    }

    /**
     * Time conversion taken from ubxlib
     */
    if ((pBuffer[17] & 0x03) == 0x03) {
            /**
             * Time and date are valid; we don't indicate
             * success based on this but we report it anyway
             * if it is valid
             */
            t = 0;
            
            /**
             * Year is 1999-2099, so need to adjust to get year since 1970
             */
            year = ((int32_t) uUbxProtocolUint16Decode(pBuffer + 10) - 1999) + 29;

            /**
             * Month (1 to 12), so take away 1 to make it zero-based
             */
            months = pBuffer[12] - 1;
            months += year * 12;

            // Work out the number of seconds due to the year/month count
            t += uTimeMonthsToSecondsUtc(months);
            // Day (1 to 31)
            t += ((int32_t) pBuffer[13] - 1) * 3600 * 24;
            // Hour (0 to 23)
            t += ((int32_t) pBuffer[14]) * 3600;
            // Minute (0 to 59)
            t += ((int32_t) pBuffer[15]) * 60;
            // Second (0 to 60)
            t += pBuffer[16];
    }
    
    gnssDvcs[dvcProfile].dvcXtra.locData.location.timeUtc = t;

    if (pBuffer[27] & 0x01) {
        gnssDvcs[dvcProfile].dvcXtra.locData.location.svs = (int32_t) pBuffer[29];
        gnssDvcs[dvcProfile].dvcXtra.locData.location.longitudeX1e7 = (int32_t) uUbxProtocolUint32Decode(pBuffer + 30);
        gnssDvcs[dvcProfile].dvcXtra.locData.location.latitudeX1e7 = (int32_t) uUbxProtocolUint32Decode(pBuffer + 34);

        if (pBuffer[26] == 0x03) {
            gnssDvcs[dvcProfile].dvcXtra.locData.location.altitudeMillimetres = (int32_t) uUbxProtocolUint32Decode(pBuffer + 42);
        } else {
            gnssDvcs[dvcProfile].dvcXtra.locData.location.altitudeMillimetres = INT_MIN;
        }

        gnssDvcs[dvcProfile].dvcXtra.locData.location.radiusMillimetres = (int32_t) uUbxProtocolUint32Decode(pBuffer + 46);
        gnssDvcs[dvcProfile].dvcXtra.locData.location.speedMillimetresPerSecond = (int32_t) uUbxProtocolUint32Decode(pBuffer + 66);

        gnssDvcs[dvcProfile].dvcXtra.msgLocationDataAvailable  = true;
    } else {
        gnssDvcs[dvcProfile].dvcXtra.msgLocationDataAvailable  = false;
    }

    return ESP_OK;
}

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void xplrGnssGeolocationCB(uDeviceHandle_t gnssHandle,
                                  const uGnssMessageId_t *pMessageId,
                                  int32_t errorCodeOrLength,
                                  void *pCallbackParam)
{
    char pBuffer[XPLR_GNSS_NAVPVT_LEN];
    int cbRead = 0;
    uint8_t dvcProfile;

    /**
     * We do not have access to the device index that made the callback, hence
     * we are using device handler value to determine which one was that made the call
    */

    for (dvcProfile = 0; dvcProfile < XPLRGNSS_NUMOF_DEVICES; dvcProfile++) {
        if (errorCodeOrLength == XPLR_GNSS_NAVPVT_LEN) {
            cbRead = uGnssMsgReceiveCallbackRead(gnssHandle, pBuffer, errorCodeOrLength);

            espRet = xplrGnssPrivateGeolocationParser(dvcProfile, pBuffer);

            if (espRet != ESP_OK) {
                XPLRGNSS_CONSOLE(W, "Could not parse geolocation!");
            } else {
                gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;
            }
        } else {
            XPLRGNSS_CONSOLE(W, 
                            "Geolocation length read mismatch: read [%d] bytes - message must be size [%d]!",
                             cbRead, 
                             XPLR_GNSS_HPPOSLLH_LEN);
        }
    }
}

static void xplrGnssFixTypeCB(uDeviceHandle_t gnssHandle,
                              const uGnssMessageId_t *pMessageId,
                              int32_t errorCodeOrLength,
                              void *pCallbackParam)
{
    char pBuffer[128];
    int cbRead = 0;
    uint8_t dvcProfile;

    /**
     * We do not have access to the device index that made the callback, hence
     * we are using device handler value to determine which one was that made the call
    */
    if (errorCodeOrLength > 0) {
        for (dvcProfile = 0; dvcProfile < XPLRGNSS_NUMOF_DEVICES; dvcProfile++) {
            if (errorCodeOrLength < ELEMENTCNT(pBuffer)) {
                cbRead = uGnssMsgReceiveCallbackRead(gnssHandle, pBuffer, errorCodeOrLength);
                /**
                * Terminate string and get rid of trailing '/n'
                */
                pBuffer[cbRead] = 0;
                espRet = xplrGnssPrivateGetLocFixType(dvcProfile, pBuffer);

                if (espRet != ESP_OK) {
                    XPLRGNSS_CONSOLE(W, "Error getting Location Fix Type!");
                } else {
                    gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;
                }
            } else {
                XPLRGNSS_CONSOLE(W, 
                                 "Fix type read buffer overflow: read [%d] bytes - max buffer size [%d]!",
                                 cbRead, ELEMENTCNT(pBuffer));
            }
        }
    }
}

static void xplrGnssAccuracyCB(uDeviceHandle_t gnssHandle,
                              const uGnssMessageId_t *pMessageId,
                              int32_t errorCodeOrLength,
                              void *pCallbackParam)
{
    char pBuffer[XPLR_GNSS_HPPOSLLH_LEN];
    int cbRead = 0;
    uint8_t dvcProfile;

    /**
     * We do not have access to the device index that made the callback, hence
     * we are using device handler value to determine which one was that made the call
    */
    for (dvcProfile = 0; dvcProfile < XPLRGNSS_NUMOF_DEVICES; dvcProfile++) {
        if (errorCodeOrLength == XPLR_GNSS_HPPOSLLH_LEN) {
            cbRead = uGnssMsgReceiveCallbackRead(gnssHandle, pBuffer, errorCodeOrLength);

            espRet = xplrGnssPrivateAccuracyParser(dvcProfile, pBuffer);

            if (espRet != ESP_OK) {
                XPLRGNSS_CONSOLE(W, "Could not parse accuracy!");
            }
            else
            {
                gnssDvcs[dvcProfile].dvcXtra.msgDataRefreshed = true;
            }
        } else {
            XPLRGNSS_CONSOLE(W, 
                            "Accuracy length read mismatch: read [%d] bytes - message must be size [%d]!",
                             cbRead, 
                             XPLR_GNSS_HPPOSLLH_LEN);
        }
    }
}