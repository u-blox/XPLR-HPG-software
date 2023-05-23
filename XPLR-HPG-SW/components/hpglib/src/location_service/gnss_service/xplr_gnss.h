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

#ifndef _XPLR_GNSS_H_
#define _XPLR_GNSS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"
#include "./../../xplr_hpglib_cfg.h"
#include "./../location_service_helpers/xplr_location_helpers.h"
#include "xplr_gnss_types.h"

/** @file
 * @brief This header file defines the general GNSS API,
 * such as initialization and de-initialization of modules, 
 * settings routines, location data parsing and displaying.
 * The API builds on top of ubxlib, implementing some high level logic that can be used
 * in common IoT scenarios.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Define the timeout in milliseconds
 * for which the blocking functions shall wait before failing
 * Can be overwritten
 */
#define XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS XPLR_HLPRLOCSRVC_FUNCTIONS_TIMEOUTS_MS

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initializes ubxLib.
 * 
 * @return  zero on success or negative error code on
 *          failure.
 */
esp_err_t xplrGnssUbxlibInit(void);

/**
 * @brief Starts a GNSS device based on some default settings.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param i2cAddress  GNSS I2C address.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssStartDeviceDefaultSettings(uint8_t dvcProfile, uint8_t i2cAddress);

/**
 * @brief Starts a GNSS device with the provided settings in a form of a struct.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param dvcCfg      a configuration of the GNSS device
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssStartDevice(uint8_t dvcProfile, xplrGnssDeviceCfg_t *dvcCfg);

/**
 * @brief Stops a GNSS device.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssStopDevice(uint8_t dvcProfile);

/**
 * @brief Deinitialize ubxLib.
 * 
 * @return  zero on success or negative error code on
 *          failure.
 */
esp_err_t xplrGnssUbxlibDeinit(void);

/**
 * @brief Returns the internal handler of a device.
 * Only to be used with specific functions that require the
 * device handler, otherwise to be left alone.
 * Do not modify/use the handler in any other way other then passing it
 * as an argument to required functions.
 * 
 * @param dvcProfile  a ubxLib device handler.
 * @return            zero on success or negative error code on
 *                    failure.
 */
uDeviceHandle_t *xplrGnssGetHandler(uint8_t dvcProfile);

/**
 * @brief Sets a single device option/config value.
 * Refer to your device/module manual for more info
 * on available options/configs that can be set such as:
 * keyId, value and layer.
 * For implemented options/configs names you can check 
 * u_gnss_cfg_val_key.h under ubxlib/gnss/api.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param keyId       a key id, essentially a name for the desired option/setting.
 * @param value       value to set the option/setting at.
 * @param layer       device memory layer to store value at.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssOptionSingleValSet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     uint64_t value,
                                     uGnssCfgValLayer_t layer);

/**
 * @brief Sets multiple device option/config values.
 * Refer to your device/module manual for more info
 * on available options/configs that can be set such as:
 * keyId, value and layer.
 * For implemented options/configs names you can check 
 * u_gnss_cfg_val_key.h under ubxlib/gnss/api.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pList       a list of key-value pairs to modify.
 * @param numValues   size of list, i.e. number of keys in pList.
 * @param layer       device memory layer to store value at.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssOptionMultiValSet(uint8_t dvcProfile,
                                    const uGnssCfgVal_t *pList,
                                    size_t numValues,
                                    uGnssCfgValLayer_t layer);

/**
 * @brief Returns the configured value for a certain option/config key.
 * Refer to your device/module manual for more info
 * on available options/configs that can be set such as:
 * keyId, value and layer.
 * For implemented options/configs names you can check 
 * u_gnss_cfg_val_key.h under ubxlib/gnss/api.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param keyId       a key id, essentially a name for the desired option/setting.
 * @param pValue      a pointer to store the value at. This must be of a type 
 *                    suitable (large enough) to hold the key's value type.
 *                    This can be inferred by checking the suffix of the key
 *                    name: e.g. U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1
 *                    needs one byte (U1) to store its value so an uint8_t is sufficient.
 *                    We can then pass to size --> sizeof(uint8_t). There's a note
 *                    in xplr_gnss_types.h where you can find more types.
 * @param size        size of the value type we want to store.
 * @param layer       device memory layer to read values from.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssOptionSingleValGet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     void *pValue,
                                     size_t size,
                                     uGnssCfgValLayer_t layer);

/**
 * @brief Returns the configured value for a list of option/config keys.
 * Refer to your device/module manual for more info
 * on available options/configs that can be set such as:
 * keyId, value and layer.
 * For implemented options/configs names you can check 
 * u_gnss_cfg_val_key.h under ubxlib/gnss/api.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pKeyIdList  a list containing key ids.
 * @param numKeyIds   size of list, i.e. number of keys in pKeyIdList.
 * @param pList       a list that will contain the keys and its store values.
 *                    Needs to be a double pointer to a uGnssCfgVal_t list. 
 *                    Remember to deallocate your pList pointer after you are done with it.
 * @param layer       device memory layer to read values from.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssOptionMultiValGet(uint8_t dvcProfile,
                                    const uint32_t *pKeyIdList,
                                    size_t numKeyIds,
                                    uGnssCfgVal_t **pList,
                                    uGnssCfgValLayer_t layer);

/**
 * @brief Collects device information and stores them to a struct.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param devInfo     a struct containing data regarding the device.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetDeviceInfo(uint8_t dvcProfile, xplrGnssDevInfo_t *devInfo);

/**
 * @brief Prints device info.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssPrintDeviceInfo(uint8_t dvcProfile);

/**
 * @brief Selects/switches correction data source: LBAND or MQTT.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param src         source of correction data.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssSetCorrectionDataSource(uint8_t dvcProfile, xplrGnssCorrDataSrc_t src);

/**
 * @brief Send decryption keys (in ubx format) to the GNSS module.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pBuffer     a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssSendDecryptionKeys(uint8_t dvcProfile, const char *pBuffer, size_t size);

/**
 * @brief Send correction data (SPARTN messages in ubx format) to the GNSS module.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pBuffer     a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssSendCorrectionData(uint8_t dvcProfile, const char *pBuffer, size_t size);

/**
 * @brief Sends generic ubx formatted command.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pBuffer     a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t xplrGnssSendFormattedCommand(uint8_t dvcProfile, const char *pBuffer, size_t size);

/**
 * @brief Starts all available async data getters for the GNSS module.
 * Used when initializing a device.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssStartAllAsyncGets(uint8_t dvcProfile);

/**
 * @brief Stops all available async data getters for the GNSS module.
 * Used when terminating a device.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssStopAllAsyncGets(uint8_t dvcProfile);

/**
 * @brief Checks if there's an available data change in order
 * to display location information.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            true when there's data to be displayed otherwise false.
 */
bool xplrGnssHasMessage(uint8_t dvcProfile);

/**
 * @brief Consumes the available message by resetting the boolean flag.
 * Used after we are done processing the location data.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssConsumeMessage(uint8_t dvcProfile);

/**
 * @brief Gets and stores location data to internal device profile.
 * This function has a timeout builtin.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetGeolocation(uint8_t dvcProfile);

/**
 * @brief Gets and stores location data to internal device profile.
 * This is an async function.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetGeolocationAsyncStart(uint8_t dvcProfile);

/**
 * @brief Stops the execution of get location async.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetGeolocationAsyncStop(uint8_t dvcProfile);

/**
 * @brief Gets GNSS location fix type.
 * This function has a timeout builtin.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetFixType(uint8_t dvcProfile);

/**
 * @brief Gets GNSS location fix type.
 * This is an async function.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetFixTypeAsyncStart(uint8_t dvcProfile);

/**
 * @brief Stops the execution of get fix type location async.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetFixTypeAsyncStop(uint8_t dvcProfile);

/**
 * @brief Gets location accuracy.
 * This function has a timeout builtin.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetAccuracy(uint8_t dvcProfile);

/**
 * @brief Gets GNSS accuracy estimate.
 * This is an async function.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetAccuracyAsyncStart(uint8_t dvcProfile);

/**
 * @brief Stops the execution of get location accuracy async.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetAccuracyAsyncStop(uint8_t dvcProfile);

/**
 * @brief Returns a string in GMaps format that can be used to view your location online.
 * Useful for checking location while using console output.
 * 
 * @param dvcProfile        an integer number denoting the device profile/index.
 * @param gmapsLocationRes  buffer to store resulting url for GMaps.
 * @param maxSize           maximum buffer size.
 * @return                  zero on success or negative error code on
 *                          failure.
 */
esp_err_t xplrGnssGetGmapsLocation(uint8_t dvcProfile, char *gmapsLocationRes, uint16_t maxSize);

/**
 * @brief Prints GMaps formatted location
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssPrintGmapsLocation(int8_t dvcProfile);

/**
 * @brief Returns a struct containing location data.
 * This is needed because new location data is always stored internally
 * in the device profile, especially when async getters are running and data
 * gets refreshed automatically.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pLocData    a pointer to a struct tos tore location data.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssGetLocation(uint8_t dvcProfile, xplrGnssLocation_t *pLocData);

/**
 * @brief Prints location data.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrGnssPrintLocation(uint8_t dvcProfile);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_GNSS_H_

// End of file