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

/** @file
 * @brief This header file defines the wifi http ztp API,
 * includes functions perform an HTTPS POST request and get
 * the necessary settings for an MQTT connection to Thingstream's services.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/**
 * Contains ZTP payload in a form of a JSON string
 */
typedef struct xplrZtpData_type {
    char *payload;           /**< the payload/JSON itself */
    uint16_t payloadLength;  /**< length of payload/JSON. */
    HttpStatus_Code httpReturnCode;  /**< HTTP/S return code */
} xplrZtpData_t;

/**
 * Contains specific device data for post body
 */
typedef struct xplrZtpDevicePostData_type {
    char *dvcToken; /**< Thingstream device token. */
    char *dvcName;  /**< some device name, must not be empty. */
} xplrZtpDevicePostData_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Performs an HTTP(S) request to fetch ZTP data.
 * 
 * @param rootCert     root certificate for HTTPS request.
 *                     This must be provided since the request requires SSL
 *                     authentication during HTTPS.
 * @param url          url to perform POST request to.
 * @param devPostData  POST body data.
 * @param ztpData      a xplrZtpData_type struct that contains payload and http return code.
 * @return             zero on success or negative error code
 *                     on failure.
 */
esp_err_t xplrZtpGetPayload(const char *rootCert,
                            const char *url,
                            xplrZtpDevicePostData_t *dvcPostData, 
                            xplrZtpData_t *ztpData);

/**
 * @brief Returns device ID based on MAC.
 *
 * @param deviceID  device ID value.
 * @param maxLen    max buffer length.
 * @param macType   mac type either WiFi or Ethernet.
 * @return          zero on success or negative error code
 *                  on failure.
 */
esp_err_t xplrZtpGetDeviceID(char *deviceID, uint8_t maxLen, esp_mac_type_t macType);

/**
 * @brief Returns a post body for ZTP.
 * Can be used as is to get a string and either be used
 * in ZTP with WiFI or cell.
 * 
 * @param res          result POST body.
 * @param maxLen       max buffer length.
 * @param devPostData  POST body data.
 * @return             zero on success or negative error code
 *                     on failure.
 */
esp_err_t xplrZtpGetPostBody(char *res, uint32_t maxLen, xplrZtpDevicePostData_t *devPostData);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_ZTP_H_ */

// End of file