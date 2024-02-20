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

#ifndef XPLR_CELL_NTRIP_CLIENT_H_
#define XPLR_CELL_NTRIP_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "xplr_cell_ntrip_client_types.h"
#include "xplr_common.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/** @file
 * @brief This header file defines the NTRIP client API,
 * including configuration settings, authentication settings,
 * of corresponding modules and high level functions to be used by the application.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */


/**
 * @brief Init client used to connect to the NTRIP caster.
 *        If successful, creates main NTRIP task
 *        Client configuration and credentials must be valid at this point.
 *
 * @param  client           client handle
 * @param  ntripSemaphore   semaphore handle used by NTRIP client (needs to be created in application)
 *
 * @return XPLR_NTRIP_OK on success, XPLR_NTRIP_ERROR otherwise.
 */
xplr_ntrip_error_t xplrCellNtripInit(xplrCell_ntrip_client_t *client,
                                     SemaphoreHandle_t ntripSemaphore);

/**
 * @brief Provide a GGA NMEA message to the NTRIP client.
 *        Use this function when xplrCellNtripGetClientState returns XPLR_NTRIP_STATE_REQUEST_GGA
 *
 * @param  client           client handle
 * @param  buffer           buffer containing GGA message
 * @param  ggaSize          size of GGA message
 *
 * @return XPLR_NTRIP_OK on success, XPLR_NTRIP_ERROR otherwise.
 */
xplr_ntrip_error_t xplrCellNtripSendGGA(xplrCell_ntrip_client_t *client,
                                        char *buffer,
                                        uint32_t ggaSize);

/**
 * @brief Get correction data from the NTRIP client buffer
 *        Use this function after xplrCellNtripGetClientState returns XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE
 *
 * @param  client           client handle
 * @param  buffer           buffer to hoold correction data
 * @param  bufferSize       size of buffer
 * @param  corrDataSize     pointer to uint32_t, size of correction data in buffer
 *
 * @return XPLR_NTRIP_OK on success, XPLR_NTRIP_ERROR otherwise.
 */
xplr_ntrip_error_t xplrCellNtripGetCorrectionData(xplrCell_ntrip_client_t *client,
                                                  char *buffer,
                                                  uint32_t bufferSize,
                                                  uint32_t *corrDataSize);

/**
 * @brief Used in the APP to retrieve NTRIP client FSM state
 *
 *
 * @param  client   client handle
 *
 * @return xplrCell_ntrip_status_t enum indicating the status of the NTRIP client
 */
xplr_ntrip_state_t xplrCellNtripGetClientState(xplrCell_ntrip_client_t *client);

/**
 * @brief Used in the APP to retrieve a detailed enum of the last error
 *        that occured in the NTRIP client
 *
 *
 * @param  client   client handle
 *
 * @return xplr_ntrip_detailed_error_t enum according to error
 */
xplr_ntrip_detailed_error_t xplrCellNtripGetDetailedError(xplrCell_ntrip_client_t *client);

/**
 * @brief Set connection configuration
 *
 * @param  client           client handle
 * @param  config           pointer to NTRIP configuration data structure
 * @param  host             NTRIP caster host
 * @param  port             NTRIP caster port
 * @param  mountpoint       the mountpoit you want to get data from
 * @param  cellDvcProfile   cellular module hpglib device profile
 * @param  ggaNecessary     flag indicates if caster requires periodic GGA message
 */
void xplrCellNtripSetConfig(xplrCell_ntrip_client_t *client,
                            xplr_ntrip_config_t *config,
                            const char *host,
                            uint16_t port,
                            const char *mountpoint,
                            uint8_t cellDvcProfile,
                            bool ggaNecessary);

/**
 * @brief Set authentication credentials for NTRIP caster
 *
 * @param  client           client handle
 * @param  use_password     if set to false, username and password argument will be ignored
 * @param  username
 * @param  password
 * @param  userAgent        your User-Agent on the caster (many casters don't support spaces on User-Agents)
 *
 */
void xplrCellNtripSetCredentials(xplrCell_ntrip_client_t *client,
                                 bool useAuth,
                                 const char *username,
                                 const char *password,
                                 const char *userAgent);

/**
 * @brief DeInit, invalidate configuration and credentials
 *
 * @param  client           client handle
 *
 * @return XPLR_NTRIP_OK on success, XPLR_NTRIP_ERROR otherwise
 */
xplr_ntrip_error_t xplrCellNtripDeInit(xplrCell_ntrip_client_t *client);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrCellNtripInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops the logging of the http cell module
 *
 * @return  XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
*/
esp_err_t xplrCellNtripStopLogModule(void);

#ifdef __cplusplus
}
#endif
#endif // XPLR_CELL_NTRIP_CLIENT_H_

// End of file
