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

#ifndef _XPLR_HPGLIB_CFG_H_
#define _XPLR_HPGLIB_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdbool.h>
#include "esp_log.h"


/** @file
 * @brief This header file defines the general configuration of API modules.
 * To be included by all hpglib components.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
/**
 * Board selection.
 * use KConfig --> Board options to select your board
 */

#if defined(CONFIG_BOARD_XPLR_HPG2_C214)
#define XPLR_BOARD_SELECTED_IS_C214
#elif defined(CONFIG_BOARD_XPLR_HPG1_C213)
#define XPLR_BOARD_SELECTED_IS_C213
#elif defined(CONFIG_BOARD_MAZGCH_HPG_SOLUTION)
#define XPLR_BOARD_SELECTED_IS_MAZGCH
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/**
 * Enable debug log output to serial for hpglib modules.
 */
#define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    (1U)
#define XPLR_HPGLIB_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"

/**
 * Enable logging to SD card for hpglib modules.
*/
#define XPLR_HPGLIB_LOG_ENABLED    1U

/**
 * Select in which modules to activate debug serial output.
 */
#define XPLR_BOARD_DEBUG_ACTIVE                        (1U)
#define XPLRCOM_DEBUG_ACTIVE                           (1U)
#define XPLRCELL_MQTT_DEBUG_ACTIVE                     (1U)
#define XPLRCELL_HTTP_DEBUG_ACTIVE                     (1U)
#define XPLRNVS_DEBUG_ACTIVE                           (1U)
#define XPLRTHINGSTREAM_DEBUG_ACTIVE                   (1U)
#define XPLRHELPERS_DEBUG_ACTIVE                       (1U)
#define XPLRGNSS_DEBUG_ACTIVE                          (1U)
#define XPLRLBAND_DEBUG_ACTIVE                         (1U)
#define XPLRZTP_DEBUG_ACTIVE                           (1U)
#define XPLRWIFISTARTER_DEBUG_ACTIVE                   (1U)
#define XPLRWIFIDNS_DEBUG_ACTIVE                       (1U)
#define XPLRWIFIWEBSERVER_DEBUG_ACTIVE                 (1U)
#define XPLRMQTTWIFI_DEBUG_ACTIVE                      (1U)
#define XPLRLOG_DEBUG_ACTIVE                           (0U)         /*< These debug messages are off by default. Please read the readme of log_service before enabling them*/
#define XPLRSD_DEBUG_ACTIVE                            (0U)         /*< These debug messages are off by default. Please read the readme of log_service before enabling them*/
#define XPLRCELL_NTRIP_DEBUG_ACTIVE                    (1U)
#define XPLRWIFI_NTRIP_DEBUG_ACTIVE                    (1U)


/**
 * Select in which modules to activate the logging in the SD card
*/
#define XPLRGNSS_LOG_ACTIVE                            (1U)
#define XPLRLBAND_LOG_ACTIVE                           (1U)
#define XPLRCOM_LOG_ACTIVE                             (1U)
#define XPLRCELL_HTTP_LOG_ACTIVE                       (1U)
#define XPLRCELL_MQTT_LOG_ACTIVE                       (1U)
#define XPLRLOCATION_LOG_ACTIVE                        (1U)
#define XPLRNVS_LOG_ACTIVE                             (1U)
#define XPLR_THINGSTREAM_LOG_ACTIVE                    (1U)
#define XPLRWIFISTARTER_LOG_ACTIVE                     (1U)
#define XPLRWIFIWEBSERVER_LOG_ACTIVE                   (1U)
#define XPLRMQTTWIFI_LOG_ACTIVE                        (1U)
#define XPLRZTP_LOG_ACTIVE                             (1U)
#define XPLRWIFI_NTRIP_LOG_ACTIVE                      (1U)
#define XPLRCELL_NTRIP_LOG_ACTIVE                      (1U)

/**
 * Configure hpg module settings
 */
#define XPLRCOM_NUMOF_DEVICES                          (1U)
#define XPLRCELL_MQTT_NUMOF_CLIENTS                    (1U)
#define XPLRGNSS_NUMOF_DEVICES                         (1U)
#define XPLRLBAND_NUMOF_DEVICES                        (1U)
#define XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_NAME           (64U)
#define XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_PAYLOAD        (10U * 1024U)
#define XPLRZTP_PAYLOAD_SIZE_MAX                       (6U * 1024U)
#if (XPLRCELL_MQTT_NUMOF_CLIENTS > 1)
#error "Only one (1) MQTT client is currently supported from ubxlib."
#endif
#define XPLRCELL_NTRIP_RECEIVE_DATA_SIZE (2U * 1024U)
#define XPLRCELL_NTRIP_GGA_INTERVAL_S (20)
#define XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE (2U * 1024U)
#define XPLRWIFI_NTRIP_GGA_INTERVAL_S (20)

/**
 * Sizes for the logging buffers, used to format the logging messages
 * Normally, the small size buffer should be enough, however, when logging
 * large strings such as certificates or json payloads large buffer size
 * might be needed.
*/
#define XPLRLOG_BUFFER_SIZE_SMALL                      (512U)
#define XPLRLOG_BUFFER_SIZE_LARGE                      (5U * 1024U)

/**
 * Macro definition to "surpress" any compiler warning message regarding "unused variables".
 * If a variable is not used (eg. because serial debug is deactivated) then it needs to be
 * declared using the UNUSED_PARAM() macro.
 */
#define UNUSED_PARAM(x)                     x  __attribute__((unused))

#ifdef __cplusplus
}
#endif
#endif // _XPLR_HPGLIB_CFG_H_

// End of file