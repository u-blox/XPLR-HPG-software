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

#ifndef _XPLR_HELPERS_H_
#define _XPLR_HELPERS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "xplr_location_helpers_types.h"

/** @file
 * @brief This header file defines the general location modules API,
 * such as initializations of devices, communication initializations,
 * command and settings pushing to modules.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Define the timeout in milliseconds
 * for which the blocking functions shall wait before failing.
 * Can be overwritten.
 */
#define XPLR_HLPRLOCSRVC_FUNCTIONS_TIMEOUTS_MS 5000

/**
 * Define the timeout in seconds which we should wait
 * until the GNSS/LBAND modules come online.
 * You can increase this timeout if it's not sufficient for
 * your modules
 */
#define XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT 20

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @note functions are not meant to be sued directly.
 * They are more of a grouping of similar functions used by location services
 * such as Lband and Gnss.
 * It is strongly recommended to use the functions implemented inside their
 * respective libraries.
 */

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHelpersUbxlibInit(void);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcDeviceOpen(xplrGnssDevBase_t *dvcBase);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcDeviceClose(xplrGnssDevBase_t *dvcBase);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
uDeviceHandle_t *xplrHlprLocSrvcGetHandler(xplrGnssDevBase_t *dvcBase);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcUbxlibDeinit(void);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcConfigAllDefault(xplrGnssDevBase_t *dvcBase, uint8_t i2cAddress);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcDeviceConfigDefault(xplrGnssDevBase_t *dvcBase, uint8_t i2cAddress);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcNetworkConfigDefault(xplrGnssDevBase_t *dvcBase);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcSetDeviceConfig(xplrGnssDevBase_t *dvcBase, uDeviceCfg_t *sdConfig);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcSetNetworkConfig(xplrGnssDevBase_t *dvcBase, uNetworkCfgGnss_t *sdNetwork);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcOptionSingleValSet(xplrGnssDevBase_t *dvcBase,
                                            uint32_t keyId,
                                            uint64_t value,
                                            uGnssCfgValTransaction_t transaction,
                                            uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcOptionMultiValSet(xplrGnssDevBase_t *dvcBase,
                                           const uGnssCfgVal_t *pList,
                                           size_t numValues,
                                           uGnssCfgValTransaction_t transaction,
                                           uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcOptionSingleValGet(xplrGnssDevBase_t *dvcBase,
                                            uint32_t keyId,
                                            void *pValue,
                                            size_t size,
                                            uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcOptionMultiValGet(xplrGnssDevBase_t *dvcBase,
                                           const uint32_t *pKeyIdList,
                                           size_t numKeyIds,
                                           uGnssCfgVal_t **pList,
                                           uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcGetDeviceInfo(xplrGnssDevBase_t *dvcBase, xplrGnssDevInfo_t *dInfo);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
esp_err_t xplrHlprLocSrvcPrintDeviceInfo(xplrGnssDevInfo_t *dInfo);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
int32_t xplrHlprLocSrvcSendUbxFormattedCommand(uDeviceHandle_t *dHandler,
                                               const char *pBuffer,
                                               size_t size);

/**
 * @brief Do not use this function directly.
 * Only meant for internal/private use by location components.
 */
bool xplrHlprLocSrvcCheckDvcProfileValidity(uint8_t dvcProfile, uint8_t maxDevLim);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_HELPERS_H_

// End of file