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

#ifndef _XPLR_COM_H_
#define _XPLR_COM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "xplr_com_types.h"
#include "xplr_common.h"

/** @file
 * @brief This header file defines the general communication service API,
 * including com profile configuration, initialization and deinitialization
 * of corresponding modules and high level functions to be used by the application.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize ubxlib platform specific layers and device API.
 *
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrUbxlibInit(void);

/**
 * @brief De-initialize ubxlib platform specific layers and device API.
 */
void xplrUbxlibDeInit(void);

/**
 * @brief De-initialize all com devices.
 *
 * @param  dvcProfile device profile id to de-init.
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComDeInit(void);

/**
 * @brief Initialize cell API using user settings.
 *
 * @param  cfg pointer to cellular configuration struct. Need to be filled by the user.
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellInit(xplrCom_cell_config_t *cfg);

/**
 * @brief De-initialize cell API using user settings.
 *
 * @param  dvcProfile device profile id to de-init.
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellDeInit(int8_t dvcProfile);

/**
 * @brief Get ubxlib device handler.
 *
 * @param  dvcProfile device profile id to retrieve ubxlib device handler.
 * @return ubxlib device handler (uDeviceHandle_t).
 */
uDeviceHandle_t xplrComGetDeviceHandler(int8_t dvcProfile);

/**
 * @brief Function that sets a greeting message to the LARA module
 * (a message that is sent by the module when powered on).
 * This is useful to be able to get alerted in case of a random or
 * unexpected reboot of the module.
 *
 * @param dvcProfile        device profile to set the greeting message.
 * @param pStr              the message to be set (cannot be greater than U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES, excluding the NULL-terminator).
 * @param pCallback         the callback; use NULL to remove a previous callback.
 * @param pCallbackParam    user parameter which will be passed to pCallback as its second parameter; may be NULL.
 * @return                  XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
*/
xplrCom_error_t xplrComSetGreetingMessage(int8_t dvcProfile,
                                          const char *pStr,
                                          void (*pCallback)(uDeviceHandle_t, void *),
                                          void *pCallbackParam);

/**
 * @brief Function that performs a full reboot to the module. Network gets disconnected and after the operation,
 *        the module is ready for use.
 *
 * @param dvcProfile        device profile to perform the reboot.
 * @return                  XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
*/
xplrCom_error_t xplrComPowerResetHard(int8_t dvcProfile);

/**
 * @brief Function that checks if a reboot of the module is controlled (was performed by xplrComPowerResetHard).
 *
 * @param dvcProfile        device profile to check for controlled reboot.
 * @return                  true if the reboot was controlled, false if it was an unexpected reboot.
*/
bool xplrComIsRstControlled(int8_t dvcProfile);

/**
 * @brief FSM handling the device connection to the network.
 *        xplrComCellInit() has to be called before running the FSM.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellFsmConnect(int8_t dvcProfile);

/**
 * @brief Reset xplrComCellFsmConnect() to its init state.
 * Function can only be used when cell module is in one of the following states
 * * CELL_FSM_CONNECT_ERROR
 * * CELL_FSM_CONNECT_TIMEOUT
 * * CELL_FSM_CONNECT_OK
 *
 * When reseting from erroneous state the module will perform a cold boot reset (POR)
 * meaning that it will power of, reconfigure the API and power back on the device.
 * Reseting from CELL_FSM_CONNECT_OK will force the device to soft reset by closing the device
 * on the ubxlib side, opening it back on and reset the xplrComCellFsmConnect() FSM to init state.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellFsmConnectReset(int8_t dvcProfile);

/**
 * @brief Get current state of xplrComCellFsmConnect().
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t.
 * @return current state of xplrComCellFsmConnect().
 */
xplrCom_cell_connect_t xplrComCellFsmConnectGetState(int8_t dvcProfile);

/**
 * @brief Perform network scan.
 * Scan results are stored in scanBuff. Make sure to provide a big enough
 * buffer, typically 512b.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @param  scanBuff   pointer to store scan results.
 * @return number of networks found or negative error code.
 */
int16_t xplrComCellNetworkScan(int8_t dvcProfile, char *scanBuff);

/**
 * @brief Get current network info.
 * Function should be called after registering to the cellular carrier.
 * It provides some general info with network details such as operator name, ip acquired,
 * apn etc.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @param  info   structure holding current network info.
 */
void xplrComCellNetworkInfo(int8_t dvcProfile, xplrCom_cell_netInfo_t *info);

/**
 * @brief Power down cellular device.
 * To resume call xplrComCellPowerResume().
 * Make sure that a pwr pin is connected to the cellular module
 * or you might end up locked out from the device.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellPowerDown(int8_t dvcProfile);

/**
 * @brief Resume power to cellular device.
 * Resumes power to the device and resets ubxlib and hpglib.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
void xplrComCellPowerResume(int8_t dvcProfile);

/**
 * @brief Retrieve device info.
 *
 * @param  dvcProfile device profile id. Stored in xplrCom_cell_config_t
 * @param  model buffer holding device model. Must be at least 32 bytes long
 * @param  fw buffer holding device fw. Must be at least 32 bytes long
 * @param  imei buffer holding device imei. Must be at least 32 bytes long
 * @return XPLR_COM_OK on success, XPLR_COM_ERROR otherwise.
 */
xplrCom_error_t xplrComCellGetDeviceInfo(int8_t dvcProfile, char *model, char *fw, char *imei);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrComCellInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops logging of the module.
 *
 * @return  ESP_OK on success, ESP_FAIL otherwise.
*/
esp_err_t xplrComCellStopLogModule(void);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_COM_H_

// End of file