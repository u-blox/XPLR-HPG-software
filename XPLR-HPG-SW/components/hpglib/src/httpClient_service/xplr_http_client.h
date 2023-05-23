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

#ifndef XPLR_HTTP_CLIENT_H_
#define XPLR_HTTP_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "xplr_http_client_types.h"

/** @file
 * @brief This header file defines the http client API,
 * including configuration settings, security settings,
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
 * @brief Connect to a HTTP(S) server.
 *        Client settings must be valid at this point.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to perform the connection.
 * @param  client       pointer to HTTP client struct. Provided by the user.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpConnect(int8_t dvcProfile,
                                          int8_t clientId,
                                          xplrCell_http_client_t *client);

/**
 * @brief De-initialize HTTP API.
 *        Must be called after running xplrCellHttpConnect().
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to de-initialize.
 */
void xplrCellHttpDeInit(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Disconnect HTTP client from current server.
 *        Checks if client is currently connected to a server and disconnects.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to disconnect from server.
 */
void xplrCellHttpDisconnect(int8_t dvcProfile, int8_t clientId);

/**
 * @brief Save RootCA certificate to module's memory.
 *        Saves given certificate in module's memory by deleting it if it exists and (re)writing it.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to save rootCa.
 * @param  md5          buffer to store the md5 hash. Use NULL to discard.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpCertificateSaveRootCA(int8_t dvcProfile,
                                                        int8_t clientId,
                                                        char *md5);

/**
 * @brief Check if RootCA certificate is stored to module's memory.
 *        Checks if a given certificate is stored in module's memory providing a name and
 *        optionally a md5 hash code.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to check rootCa.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpCertificateCheckRootCA(int8_t dvcProfile,
                                                         int8_t clientId);

/**
 * @brief Erase rootCa from  modules' memory.
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to erase rootCa.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpCertificateEraseRootCA(int8_t dvcProfile,
                                                         int8_t clientId);

/**
 * @brief HTTP POST Request.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to perform post request.
 * @param  data         pointer to dataTransfer instance.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpPostRequest(int8_t dvcProfile,
                                              int8_t clientId,
                                              xplrCell_http_dataTransfer_t *data);

/**
 * @brief HTTP Get Request.
 *
 *
 * @param  dvcProfile   hpgLib device id.
 * @param  clientId     HTTP client index to perform get request.
 * @param  data         pointer to dataTransfer instance.
 * @return XPLR_CELL_HTTP_OK on success, XPLR_CELL_HTTP_ERROR otherwise.
 */
xplrCell_http_error_t xplrCellHttpGetRequest(int8_t dvcProfile,
                                             int8_t clientId,
                                             xplrCell_http_dataTransfer_t *data);

#ifdef __cplusplus
}
#endif
#endif // XPLR_HTTP_CLIENT_H_

// End of file
