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
#include "xplr_log.h"

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
#define XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT 40

/* ----------------------------------------------------------------
 * EXTERN VARIABLES
 * -------------------------------------------------------------- */
extern xplrLog_t locationLog;
extern char buff2Log[512];

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
 * Used to initialize ubxlib.
 *
 * @return  ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHelpersUbxlibInit(void);

/**
 * @brief Do not use this function directly.
 * Tries to open a device communication in
 * XPLR_HLPRLOCSRVC_DEVICE_ONLINE_TIMEOUT seconds.
 * This function will be deprecated on future releases.
 *
 * @param dvcConf     device configuration.
 * @param dvcHandler  device handler.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcDeviceOpen(xplrLocationDevConf_t *dvcConf,
                                    uDeviceHandle_t *dvcHandler);

/**
 * @brief Do not use this function directly.
 * Tries to open a device communication on a single try,
 * if it fails it does not retry automatically.

 *
 * @param dvcConf     device configuration.
 * @param dvcHandler  device handler.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcDeviceOpenNonBlocking(xplrLocationDevConf_t *dvcConf,
                                               uDeviceHandle_t *dvcHandler);

/**
 * @brief Do not use this function directly.
 * Closes open device.
 *
 * @param dvcHandler  device handler.
 * @return            ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcDeviceClose(uDeviceHandle_t *dvcHandler);

/**
 * @brief Do not use this function directly.
 * Returns a device handler pointer.
 *
 * @param dvcHandler  device handler.
 * @return            device handler pointer on success otherwise NULL.
 */
uDeviceHandle_t *xplrHlprLocSrvcGetHandler(uDeviceHandle_t *dvcHandler);

/**
 * @brief Do not use this function directly.
 * Deinitialize ubxlib.
 *
 * @return  ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcUbxlibDeinit(void);

/**
 * @brief Do not use this function directly.
 * Sets a single config value into the module.
 *
 * @param dvcHandler   device handler.
 * @param keyId        config key id.
 * @param value        value to be set for said config key id.
 * @param transaction  transaction type.
 * @param layer        layer for the value to be set into, essentially
 *                     declares in which memory vals will be stored.
 * @return             ESP_OK on success, ESP_ERR_TIMEOUT on timeout,
 *                     ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcOptionSingleValSet(uDeviceHandle_t *dvcHandler,
                                            uint32_t keyId,
                                            uint64_t value,
                                            uGnssCfgValTransaction_t transaction,
                                            uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Sets multiple config values in one go.
 *
 * @param dvcHandler   device handler.
 * @param list         a list of tuples (key id, val) to be set.
 * @param numValues    number/length of list to be set.
 * @param transaction  transaction type.
 * @param layer        layer for the value to be set into, essentially
 *                     declares in which memory vals will be stored.
 * @return             ESP_OK on success, ESP_ERR_TIMEOUT on timeout,
 *                     ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcOptionMultiValSet(uDeviceHandle_t *dvcHandler,
                                           const uGnssCfgVal_t *list,
                                           size_t numValues,
                                           uGnssCfgValTransaction_t transaction,
                                           uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Gets a single config value as stored on the module.
 *
 * @param dvcHandler  device handler.
 * @param keyId       config key id.
 * @param value       stores value fetched from module.
 * @param size        size of value.
 * @param layer       layer from which to get value from, essentially
 *                    declares from which memory the value will be read.
 * @return            ESP_OK on success, ESP_ERR_TIMEOUT on timeout,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcOptionSingleValGet(uDeviceHandle_t *dvcHandler,
                                            uint32_t keyId,
                                            void *value,
                                            size_t size,
                                            uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Gets multiple config values as stored on the module in
 * the form of a list.
 *
 * @param dvcHandler  device handler.
 * @param keyIdList   a list of key ids.
 * @param numKeyIds   number/length of list to be set.
 * @param list        a list of tuples (key id, val) to store values.
 * @param layer       layer from which to get value from, essentially
 *                    declares from which memory the value will be read.
 * @return            ESP_OK on success, ESP_ERR_TIMEOUT on timeout,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcOptionMultiValGet(uDeviceHandle_t *dvcHandler,
                                           const uint32_t *keyIdList,
                                           size_t numKeyIds,
                                           uGnssCfgVal_t **list,
                                           uGnssCfgValLayer_t layer);

/**
 * @brief Do not use this function directly.
 * Gets device information such as module type,
 * firmware version, hardware version, I2C info, etc.
 *
 * @param dvcConf     device configuration.
 * @param dvcHandler  device handler.
 * @param dvcInfo     stored device info.
 * @return            ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcGetDeviceInfo(xplrLocationDevConf_t *dvcConf,
                                       uDeviceHandle_t dvcHandler,
                                       xplrLocDvcInfo_t *dvcInfo);

/**
 * @brief Do not use this function directly.
 * Prints device info such module type,
 * firmware version, hardware version, I2C info, etc.
 *
 * @param dvcInfo  storage for device info.
 * @return         ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcPrintDeviceInfo(xplrLocDvcInfo_t *dvcInfo);

/**
 * @brief Do not use this function directly.
 * Sends a ubx formatted command.
 * Function checks internally if sent size matches
 * desired size (size param).
 *
 * @param dvcHandler  device handler.
 * @param buffer      buffer of ubx formatted command to send.
 * @param size        size of buffer to send.
 * @return            size of bytes sent on success otherwise
 *                    negative value on error
 */
int32_t xplrHlprLocSrvcSendUbxFormattedCommand(uDeviceHandle_t *dvcHandler,
                                               const char *buffer,
                                               size_t size);

/**
 * @brief Do not use this function directly.
 * Sends RTCM (NTRIP) correction data to GNSS module.
 *
 * @param dvcHandler  device handler.
 * @param buffer      buffer containing RTCM correction data.
 * @param size        size of buffer to send.
 * @return            ESP_OK on success otherwise ESP_FAIL on failure.
 */
esp_err_t xplrHlprLocSrvcSendRtcmFormattedCommand(uDeviceHandle_t *dvcHandler,
                                                  const char *buffer,
                                                  size_t size);

/**
 * @brief Do not use this function directly.
 * Checks if profile id is inside permited limits of
 * maximum allowed devices.
 *
 * @param dvcProfile  a device profile id.
 * @param maxDevLim   maximum allowed devices according to
 *                    configuration (check xplr_hpglib_cfg.h)
 * @return            true if device profile is inside limits otherwise false
 */
bool xplrHlprLocSrvcCheckDvcProfileValidity(uint8_t dvcProfile, uint8_t maxDevLim);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_HELPERS_H_

// End of file