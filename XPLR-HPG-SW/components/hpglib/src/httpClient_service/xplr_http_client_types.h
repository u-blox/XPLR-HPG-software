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

#ifndef XPLR_HTTP_CLIENT_TYPES_H_
#define XPLR_HTTP_CLIENT_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"
#include "./../../../../components/hpglib/src/nvs_service/xplr_nvs.h"
#include "./../../../../components/hpglib/src/log_service/xplr_log.h"

/** @file
 * @brief This header file defines the types used in mqtt client service API,
 * Types include status, state, config enums and structs that are exposed to the user
 * providing an easy to use and configurable mqtt client library.
 * The API builds on top of ubxlib, implementing some high level logic that can be used
 * in common IoT scenarios.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to xplrHttp module. */
typedef enum {
    XPLR_CELL_HTTP_ERROR = -1,    /**< process returned with errors. */
    XPLR_CELL_HTTP_OK,            /**< indicates success of returning process. */
    XPLR_CELL_HTTP_BUSY           /**< returning process currently busy. */
} xplrCell_http_error_t;

/** Certification methods for logging in the http(s) webserver. */
typedef enum {
    XPLR_CELL_HTTP_CERT_METHOD_NONE = 0,    /**< register to a http server. */
    XPLR_CELL_HTTP_CERT_METHOD_PSWD,        /**< register to a https server using a username and password. */
    XPLR_CELL_HTTP_CERT_METHOD_ROOTCA,      /**< register to a https server using a rootCA. */
    XPLR_CELL_HTTP_CERT_METHOD_TLS,         /**< register to a https server using rootCA and user certificate. */
    XPLR_CELL_HTTP_CERT_METHOD_TLS_KEY,     /**< register to a https server using rootCA, user certificate and user key. */
    XPLR_CELL_HTTP_CERT_METHOD_TLS_KEY_PSWD /**< register to a https server using rootCA, user certificate and password protected key. */
} xplrCell_http_cert_method_t;

/** HTTP configuration struct for setting up deviceSettings.
 * Struct to be provided by the user via xplrCellHttpConnect().
*/
// *INDENT-OFF*
typedef struct xplrCell_http_config_type {
    const char                      *serverAddress; /**< HTTP(S) Server Address. */
    int32_t                         timeoutSeconds; /**< request timeout for blocking functions. */
    bool                            async;          /**< configure async mode (ubxlib callbacks). */
    bool                            errorOnBusy;    /**< Non-blocking functions return busy until timeout. */
    xplrCell_http_cert_method_t     registerMethod; /**< registration method to use. */
} xplrCell_http_config_t;
// *INDENT-ON*

/** Broker credentials configuration struct.
 * Struct to be provided by the user via xplrCellMqttInit().
*/
typedef struct xplrCell_http_credentials_type {
    const char          *name;          /**< Server name. */
    const char          *user;          /**< User name to use when connecting to webserver. */
    const char          *password;      /**< Password to use when connecting to webserver. */
    const char          *token;         /**< Device ID / Token to use. */
    const char          *rootCa;        /**< Root Certificate to use. */
    const char          *rootCaName;     /**< Root Certificate name to use. */
    const char          *rootCaHash;
    const char          *cert;          /**< Certificate to use. */
    const char          *certName;      /**< Certificate name to use. */
    const char          *certHash;
    const char          *key;           /**< Key to use. */
    const char          *keyName;       /**< Key name to use. */
    const char          *keyPassword;   /**< Key password to use. */
    const char          *keyHash;
} xplrCell_http_credentials_t;

/** States describing the cellular HTTP client process. */
typedef enum {
    XPLR_CELL_HTTP_CLIENT_FSM_TIMEOUT = -3,
    XPLR_CELL_HTTP_CLIENT_FSM_ERROR,
    XPLR_CELL_HTTP_CLIENT_FSM_BUSY,
    XPLR_CELL_HTTP_CLIENT_FSM_CONNECT = 0,
    XPLR_CELL_HTTP_CLIENT_FSM_REQUEST,
    XPLR_CELL_HTTP_CLIENT_FSM_RESPONSE,
    XPLR_CELL_HTTP_CLIENT_FSM_READY
} xplrCell_http_client_fsm_t;

typedef struct xplrCell_http_dataTransfer_type {
    char            *path;
    char            contentType[U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES];
    char            *buffer;
    size_t          bufferSizeOut;
    size_t          bufferSizeIn;
} xplrCell_http_dataTransfer_t;

// *INDENT-OFF*
typedef struct xplrCell_http_session_type {
    uint32_t                        rspSize;
    int32_t                         error;          /**< indicates if an error ocurred during a request . */
    bool                            rspAvailable;   /**< indicates if a response is available to read. */
    int32_t                         statusCode;
    int32_t                         returnCode;
    bool                            requestPending; /**< indicates if a http request is pending for response. */
    xplrCell_http_dataTransfer_t    data;
} xplrCell_http_session_t;
// *INDENT-ON*

/** HTTP NVS struct.
 * contains data to be stored in NVS under namespace <id>
*/
typedef struct xplrCell_http_nvs_type {
    xplrNvs_t   nvs;            /**< nvs module to handle operations */
    char        id[16];         /**< nvs namespace */
    char        md5RootCa[33];  /**< sha-md5 hash of rootCa stored */
} xplrCell_http_nvs_t;

/** HTTP client struct. */
typedef struct xplrCell_http_client_type {
    int8_t                          id;
    xplrCell_http_nvs_t             storage;      /**< storage module for provisioning settings */
    xplrCell_http_config_t          settings;
    xplrCell_http_credentials_t     credentials;
    xplrCell_http_session_t         *session;     /**< Pointer to current http session. */
    xplrCell_http_client_fsm_t      fsm[2];       /**< HTTP fsm array.
                                                       element 0 holds most current state.
                                                       d element 1 holds previous state */
    bool                            msgAvailable; /**< indicates if a message is available to read. */
    uHttpClientResponseCallback_t   *responseCb;  /**< function pointer to msg received callback. */
    xplrLog_t                       *logCfg;      /**< pointer to the log struct configuration. */
} xplrCell_http_client_t;

#ifdef __cplusplus
}
#endif
#endif // XPLR_HTTP_CLIENT_TYPES_H_

// End of file