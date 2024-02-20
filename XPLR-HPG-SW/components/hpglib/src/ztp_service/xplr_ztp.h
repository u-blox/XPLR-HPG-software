/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _XPLR_ZTP_H_
#define _XPLR_ZTP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../hpglib/xplr_hpglib_cfg.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "xplr_thingstream.h"
#include "xplr_com.h"
#include "xplr_log.h"

/** @file
 * @brief This header file defines the wifi and cellular http ztp API,
 * includes functions perform an HTTPS POST request and get
 * the necessary settings for an MQTT connection to Thingstream's services.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application header data type
 */
#define HEADER_DATA_TYPE "application/json"

/*
 * POST Header and Data types
 */
#define HTTP_POST_HEADER_TYPE_CONTENT       "Content-Type"
#define HTTP_POST_HEADER_TYPE_DATA_CONTENT  HEADER_DATA_TYPE
#define HTTP_POST_HEADER_TYPE_JSON          "Accept"
#define HTTP_POST_HEADER_TYPE_DATA_JSON     HEADER_DATA_TYPE

/**
 * Timeout for HTTP POST
*/
#define XPLR_ZTP_HTTP_TIMEOUT_MS            (5U * 1000U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/**
 * Contains ZTP payload in a form of a JSON string
 */
typedef struct xplrZtpData_type {
    char *payload;                  /**< the payload/JSON itself */
    uint16_t payloadLength;         /**< length of payload/JSON. */
    HttpStatus_Code httpReturnCode; /**< HTTP/S return code */
} xplrZtpData_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Function that handles the ZTP with Thingstream and fetches the required
 *        credentials to be able to connect and subscribe to Thingstream. Specific
 *        for WiFi.
 *
 * @param thingstream  Thingstream configuration struct pointer. Must be initialized.
 * @param ztpData      a xplrZtpData_type struct that contains payload and http return code.
 * @return             zero on success or negative error code
 *                     on failure.
 */
esp_err_t xplrZtpGetPayloadWifi(xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData);

/**
 * @brief Function that handles the ZTP with Thingstream and fetches the required
 *        credentials to be able to connect and subscribe to Thingstream. Specific
 *        for cell.
 *
 * @param rootCaName   root certificate name for HTTPS request.
 *                     This must be provided since the request requires SSL
 *                     authentication during HTTPS.
 * @param thingstream  Thingstream configuration struct pointer. Must be initialized.
 * @param ztpData      a xplrZtpData_type struct that contains payload and http return code.
 * @param cellConfig   struct containing the cellular configuration variables after
 *                     initialization
 * @return ESP_OK on success or ESP_FAIL on failure
*/
esp_err_t xplrZtpGetPayloadCell(const char *rootCaName,
                                xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData,
                                xplrCom_cell_config_t *cellConfig);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrZtpInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops the logging of the http cell module
 *
 * @return  XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
*/
esp_err_t xplrZtpStopLogModule(void);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_ZTP_H_ */

// End of file