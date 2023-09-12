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
 * @return  ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *          ESP_FAIL on failure.
 */
esp_err_t xplrGnssUbxlibInit(void);

/**
 * @brief Starts a GNSS device with the provided settings in a form of a struct.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param dvcCfg      a configuration of the GNSS device
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure
 */
esp_err_t xplrGnssStartDevice(uint8_t dvcProfile, xplrGnssDeviceCfg_t *dvcCfg);

/**
 * @brief Executes FSM for GNSS module
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            XPLR_GNSS_OK on success, XPLR_GNSS_BUSY on long term
 *                    executions and XPLR_GNSS_ERROR on failure
 */
xplrGnssError_t xplrGnssFsm(uint8_t dvcProfile);

/**
 * @brief Stops a GNSS device.
 * Device will terminate in an unconfigured state.
 * You can start the device again with xplrGnssStartDevice
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure
 */
esp_err_t xplrGnssStopDevice(uint8_t dvcProfile);

/**
 * @brief Deinitialize ubxLib.
 * Be careful when deinitializing ubxlib since there might
 * be other modules that use it.
 *
 * @return  ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *          ESP_FAIL on failure.
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
 * @return            uDeviceHandle_t pointer on success otherwise NULL.
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
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
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
 * @param list        a list of key-value pairs to modify.
 * @param numValues   size of list, i.e. number of keys in list.
 * @param layer       device memory layer to store value at.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrGnssOptionMultiValSet(uint8_t dvcProfile,
                                    const uGnssCfgVal_t *list,
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
 * @param value       a pointer to store the value at. This must be of a type
 *                    suitable (large enough) to hold the key's value type.
 *                    This can be inferred by checking the suffix of the key
 *                    name: e.g. U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1
 *                    needs one byte (U1) to store its value so an uint8_t is sufficient.
 *                    We can then pass to size --> sizeof(uint8_t). There's a note
 *                    in xplr_gnss_types.h where you can find more types.
 * @param size        size of the value type we want to store.
 * @param layer       device memory layer to read values from.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrGnssOptionSingleValGet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     void *value,
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
 * @param keyIdList   a list containing key ids.
 * @param numKeyIds   size of list, i.e. number of keys in keyIdList.
 * @param list        a list that will contain the keys and its store values.
 *                    Needs to be a double pointer to a uGnssCfgVal_t list.
 *                    Remember to deallocate your list pointer after you are done with it.
 * @param layer       device memory layer to read values from.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrGnssOptionMultiValGet(uint8_t dvcProfile,
                                    const uint32_t *keyIdList,
                                    size_t numKeyIds,
                                    uGnssCfgVal_t **list,
                                    uGnssCfgValLayer_t layer);

/**
 * @brief Collects device information and stores them to a struct.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param devInfo     a struct containing data regarding the device.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetDeviceInfo(uint8_t dvcProfile, xplrLocDvcInfo_t *devInfo);

/**
 * @brief Prints device info.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintDeviceInfo(uint8_t dvcProfile);

/**
 * @brief Selects/switches correction data source: LBAND or IP-MQTT
 * and stores that data to the GNSS configuration struct. That setting
 * will be used every time during device startup.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param src         source of correction data.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_ERR_TIMEOUT on timeout, ESP_FAIL on failure.
 */
esp_err_t xplrGnssSetCorrectionDataSource(uint8_t dvcProfile, xplrGnssCorrDataSrc_t src);

/**
 * @brief Enables Dead Reckoning function.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssEnableDeadReckoning(uint8_t dvcProfile);

/**
 * @brief Disables Dead Reckoning.
 * If this cause a change in the Dead Reckoning state
 * the GNSS module will restart.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssDisableDeadReckoning(uint8_t dvcProfile);

/**
 * @brief Shows if Dead Reckoning is enabled
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            true if enabled otherwise false.
 */
bool xplrGnssIsDrEnabled(uint8_t dvcProfile);

/**
 * @brief Shows if Dead Reckoning calibration is done
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            true if calibration is done otherwise false.
 */
bool xplrGnssIsDrCalibrated(uint8_t dvcProfile);

/**
 * @brief Send decryption keys (in UBX format) to the GNSS module and
 * stores them to the GNSS configuration struct. If populated the keys
 * can be used when the device restarts without the need to resend from
 * e.g. MQTT.
 * Keys will be refreshed every time this function gets called.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssSendDecryptionKeys(uint8_t dvcProfile, const char *buffer, size_t size);

/**
 * @brief Send correction data (SPARTN messages in ubx format) to the GNSS module.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssSendCorrectionData(uint8_t dvcProfile, const char *buffer, size_t size);

/**
 * @brief Sends generic ubx formatted command.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
int32_t xplrGnssSendFormattedCommand(uint8_t dvcProfile, const char *buffer, size_t size);

/**
 * @brief Send correction data (RTCM) to the GNSS module.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssSendRtcmCorrectionData(uint8_t dvcProfile, const char *buffer, size_t size);

/**
 * @brief Sends RTCM formatted command.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a buffer containing data to send.
 * @param size        size of the buffer.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssSendRtcmFormattedCommand(uint8_t dvcProfile, const char *buffer, size_t size);

/**
 * @brief Starts all available async data getters for the GNSS module.
 * Used when initializing a device.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssStartAllAsyncs(uint8_t dvcProfile);

/**
 * @brief Stops all available async data getters for the GNSS module.
 * Used when terminating a device.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssStopAllAsyncs(uint8_t dvcProfile);

/**
 * @brief Starts async parser for NMEA/STRING type messages.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssNmeaMessagesAsyncStart(uint8_t dvcProfile);

/**
 * @brief Stops async parser for NMEA/STRING type messages.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssNmeaMessagesAsyncStop(uint8_t dvcProfile);

/**
 * @brief Starts async parser for UBX/BINARY type messages.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssUbxMessagesAsyncStart(uint8_t dvcProfile);

/**
 * @brief Stops async parser for UBX/BINARY type messages
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssUbxMessagesAsyncStop(uint8_t dvcProfile);

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
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssConsumeMessage(uint8_t dvcProfile);

/**
 * @brief Deletes the stored calibration angles (yaw, pitch and roll)
 * saved on NVS after a successful AUTO calibration.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssNvsDeleteCalibrations(uint8_t dvcProfile);

/**
 * @brief Returns a string in GMaps format that can be used to view your location online.
 * Useful for checking location while using console output.
 *
 * @param dvcProfile        an integer number denoting the device profile/index.
 * @param gmapsLocationRes  buffer to store resulting url for GMaps.
 * @param maxSize           maximum buffer size.
 * @return                  ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                          ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetGmapsLocation(uint8_t dvcProfile, char *gmapsLocationRes, uint16_t maxSize);

/**
 * @brief Prints GMaps formatted location
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintGmapsLocation(uint8_t dvcProfile);

/**
 * @brief Returns a struct containing location data.
 * This is needed because new location data is always stored internally
 * in the device profile, especially when async getters are running and data
 * gets refreshed automatically.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param locData     a pointer to a struct tos tore location data.
 * @return            ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                    ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetLocationData(uint8_t dvcProfile, xplrGnssLocation_t *locData);

/**
 * @brief Prints location data struct (xplrGnssLocation_t)
 * Use xplrGnssGetLocationData to location data.
 *
 * @param locData  struct containing location data information.
 * @return         ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                 ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintLocationData(xplrGnssLocation_t *locData);

/**
 * @brief Used by Dead Reckoning. Returns a struct containing IMU alignment info such as:
 * calibration mode, calibration status, calibrated angles (yaw, pitch and roll).
 *
 * @param dvcProfile        an integer number denoting the device profile/index.
 * @param imuAlignmentInfo  struct containing calibration data information.
 * @return                  ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                          ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetImuAlignmentInfo(uint8_t dvcProfile,
                                      xplrGnssImuAlignmentInfo_t *imuAlignmentInfo);

/**
 * @brief Used by Dead Reckoning. Prints IMU alignment information.
 *
 * @param imuAlignmentInfo  an integer number denoting the device profile/index.
 * @return                  ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                          ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintImuAlignmentInfo(xplrGnssImuAlignmentInfo_t *imuAlignmentInfo);

/**
 * @brief Used by Dead Reckoning. Gets IMU alignment status.
 *
 * @param dvcProfile       an integer number denoting the device profile/index.
 * @param imuFusionStatus  struct containing calibration status.
 * @return                 ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                         ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetImuAlignmentStatus(uint8_t dvcProfile,
                                        xplrGnssImuFusionStatus_t *imuFusionStatus);

/**
 * @brief Used by Dead Reckoning. Prints IMU alignment status.
 *
 * @param imuFusionStatus  struct containing calibration status.
 * @return                 ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                         ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintImuAlignmentStatus(xplrGnssImuFusionStatus_t *imuFusionStatus);

/**
 * @brief Used by Dead Reckoning. Gets IMU sensors measurements
 * (vehicle dynamics).
 *
 * @param dvcProfile          an integer number denoting the device profile/index.
 * @param imuVehicleDynamics  struct containing sensors measurements (vehicle dynamics).
 * @return                    ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                            ESP_FAIL on failure.
 */
esp_err_t xplrGnssGetImuVehicleDynamics(uint8_t dvcProfile,
                                        xplrGnssImuVehDynMeas_t *imuVehicleDynamics);

/**
 * @brief Used by Dead Reckoning. Prints IMU sensors measurements
 * (vehicle dynamics).
 *
 * @param imuVehicleDynamics  struct containing sensors measurements (vehicle dynamics).
 * @return                    ESP_OK on success, ESP_INVALID_ARG on invalid parameters,
 *                            ESP_FAIL on failure.
 */
esp_err_t xplrGnssPrintImuVehicleDynamics(xplrGnssImuVehDynMeas_t *imuVehicleDynamics);

/**
 * @brief Gets current FSM state.
 *
 * @param dvcProfile  struct containing sensors measurements (vehicle dynamics).
 * @return            current FSM state.
 */
xplrGnssStates_t xplrGnssGetCurrentState(uint8_t dvcProfile);

/**
 * @brief Gets previous FSM state.
 *
 * @param dvcProfile  struct containing sensors measurements (vehicle dynamics).
 * @return            previous FSM state.
 */
xplrGnssStates_t xplrGnssGetPreviousState(uint8_t dvcProfile);

/**
 * @brief Gets a GGA message from the GNSS module.
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @param buffer      a pointer to a pointer to a buffer in which the message will be placed, cannot be NULL.
 * @param size        the amount of storage at *buffer, zero if buffer points to NULL.
 * @return            ubxlib error code
 */
uErrorCode_t xplrGnssGetGgaMessage(uint8_t dvcProfile, char **buffer, size_t size);

/**
 * @brief Function that initializes the GNSS message logging task
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success otherwise ESP_FAIL on failure.
*/
esp_err_t xplrGnssAsyncLogInit(uint8_t dvcProfile);

/**
 * @brief Function that de-initializes the GNSS message logging task
 *
 * @param dvcProfile  an integer number denoting the device profile/index.
 * @return            ESP_OK on success otherwise ESP_FAIL on failure.
*/
esp_err_t xplrGnssAsyncLogDeInit(uint8_t dvcProfile);

/**
 * @brief Function that halts the logging of the gnss module's selected submodule
 *
 * @param module  the submodule for which the logging will be halted.
 * @return true if succeeded to halt the module or false otherwise.
*/
bool xplrGnssHaltLogModule(xplrGnssLogModule_t module);

/**
 * @brief Function that starts the logging of the gnss module's selected submodule
 * 
 * @param module  the submodule for which the logging will be started/resumed.
 * @return        true if the module has been enabled or false in error
*/
bool xplrGnssStartLogModule(xplrGnssLogModule_t module);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_GNSS_H_

// End of file