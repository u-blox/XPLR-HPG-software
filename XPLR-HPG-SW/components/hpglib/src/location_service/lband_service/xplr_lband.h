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

#ifndef _XPLR_LBAND_H_
#define _XPLR_LBAND_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "./../location_service_helpers/xplr_location_helpers.h"
#include "xplr_lband_types.h"

/** @file
 * @brief This header file defines the general LBAND API,
 * such as initialization and de-initialization of modules, 
 * settings routines and sending correction data to GNSS modules.
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
#define XPLR_LBAND_FUNCTIONS_TIMEOUTS_MS XPLR_HLPRLOCSRVC_FUNCTIONS_TIMEOUTS_MS

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initializes ubxLib.
 * 
 * @return  zero on success or negative error code on
 *          failure.
 */
esp_err_t xplrLbandUbxlibInit(void);

/**
 * @brief Deinitialize ubxLib.
 * 
 * @return  zero on success or negative error code on
 *          failure.
 */
esp_err_t xplrLbandUbxlibDeinit(void);

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
uDeviceHandle_t *xplrLbandGetHandler(uint8_t dvcProfile);

/**
 * @brief Starts an LBAND device based on some default settings.
 * 
 * @param dvcProfile   an integer number denoting the device profile/index.
 * @param i2cAddress   I2C address of the LBAND device.
 * @param destHandler  GNSS destination device handler to push data to.
 * @return             zero on success or negative error code on
 *                     failure.
 */
esp_err_t xplrLbandStartDeviceDefaultSettings(uint8_t dvcProfile, 
                                              uint8_t i2cAddress,
                                              uDeviceHandle_t *destHandler);

/**
 * @brief Starts an LBAND device with the provided settings in a form of a struct.
 * 
 * @param dvcProfile   an integer number denoting the device profile/index.
 * @param dvcCfg       device configuration.
 * @param destHandler  GNSS destination device handler to push data to.
 * @return             zero on success or negative error code on
 *                     failure.
 */
esp_err_t xplrLbandStartDevice(uint8_t dvcProfile, 
                               xplrLbandDeviceCfg_t *dvcCfg, 
                               uDeviceHandle_t *destHandler);

/**
 * @brief Stops an LBAND device.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandStopDevice(uint8_t dvcProfile);

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
esp_err_t xplrLbandOptionSingleValSet(uint8_t dvcProfile,
                                      uint32_t keyId,
                                      uint64_t value,
                                      uint32_t layer);

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
esp_err_t xplrLbandOptionMultiValSet(uint8_t dvcProfile,
                                     const uGnssCfgVal_t *pList,
                                     size_t numValues,
                                     uint32_t layers);

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
esp_err_t xplrLbandOptionSingleValGet(uint8_t dvcProfile,
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
esp_err_t xplrLbandOptionMultiValGet(uint8_t dvcProfile,
                                     const uint32_t *pKeyIdList,
                                     size_t numKeyIds,
                                     uGnssCfgVal_t **pList,
                                     uGnssCfgValLayer_t layer);

/**
 * @brief Sets the frequency for correction data channel.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param frequency   LBAND data channel frequency. Taken usually from MQTT
 *                    or can be manually passed if the values are known.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandSetFrequency(uint8_t dvcProfile, uint32_t frequency);

/**
 * @brief Set frequency for LBAND directly from the received MQTT message
 * depending on current region
 * 
 * @param dvcProfile   an integer number denoting the device profile/index.
 * @param mqttPayload  MQTT payload as received from the client.
 * @param region       geographic region we are currently in.
 * @return             zero on success or negative error code on
 *                     failure.
 */
esp_err_t xplrLbandSetFrequencyFromMqtt(uint8_t dvcProfile, 
                                        char *mqttPayload, 
                                        xplrLbandRegion region);

/**
 * @brief Reads configured frequency from LBAND
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
uint32_t xplrLbandGetFrequency(uint8_t dvcProfile);

/**
 * @brief 
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param pBuffer     a buffer containing a ubx formatted command.
 * @param size        buffer size.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t xplrLbandSendFormattedCommand(uint8_t dvcProfile, const char *pBuffer, size_t size);

/**
 * @brief Start an async function to send data to a GNSS device using its handler.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param dHandler    a device handler belonging to the GNSS device
 *                    we want to push correction data to.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandSendCorrectionDataAsyncStart(uint8_t dvcProfile, uDeviceHandle_t *dHandler);

/**
 * @brief Stops async function for sending data to GNSS device.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandSendCorrectionDataAsyncStop(uint8_t dvcProfile);

/**
 * @brief Collects device information and stores them to a struct.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param devInfo     a struct containing data regarding the device.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandGetDeviceInfo(uint8_t dvcProfile, xplrGnssDevInfo_t *devInfo);

/**
 * @brief Prints location data.
 * 
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            zero on success or negative error code on
 *                    failure.
 */
esp_err_t xplrLbandPrintDeviceInfo(uint8_t dvcProfile);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_LBAND_H_

// End of file