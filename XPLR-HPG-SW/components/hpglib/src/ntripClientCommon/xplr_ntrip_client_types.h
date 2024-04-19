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

#ifndef XPLR_NTRIP_CLIENT_TYPES_H_
#define XPLR_NTRIP_CLIENT_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include <stdbool.h>
#include "./../../../components/hpglib/xplr_hpglib_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLR_NTRIP_HOST_LENGTH (128U)
#define XPLR_NTRIP_USERAGENT_LENGTH (64U)
#define XPLR_NTRIP_MOUNTPOINT_LENGTH (128U)
#define XPLR_NTRIP_CREDENTIALS_LENGTH (64U)

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */
/*INDENT-OFF*/

typedef enum {
    XPLR_NTRIP_ERROR = -1,    /**< process returned with errors. */
    XPLR_NTRIP_OK,            /**< indicates success of returning process. */
} xplr_ntrip_error_t;

typedef enum {
    XPLR_NTRIP_STATE_ERROR = -5,
    XPLR_NTRIP_STATE_BUSY,
    XPLR_NTRIP_STATE_CONNECTION_RESET,
    XPLR_NTRIP_STATE_CORRECTION_DATA_AVAILABLE,
    XPLR_NTRIP_STATE_REQUEST_GGA,
    XPLR_NTRIP_STATE_READY,
} xplr_ntrip_state_t;                       /*< enum indicating current state of NTRIP client*/

typedef enum {
    XPLR_NTRIP_UKNOWN_ERROR = -9,
    XPLR_NTRIP_BUSY_ERROR,
    XPLR_NTRIP_CONNECTION_RESET_ERROR,
    XPLR_NTRIP_BUFFER_TOO_SMALL_ERROR,
    XPLR_NTRIP_NO_GGA_TIMEOUT_ERROR,
    XPLR_NTRIP_CORR_DATA_TIMEOUT_ERROR,
    XPLR_NTRIP_SOCKET_ERROR,
    XPLR_NTRIP_UNABLE_TO_CREATE_TASK_ERROR,
    XPLR_NTRIP_SEMAPHORE_ERROR,
    XPLR_NTRIP_NO_ERROR,
} xplr_ntrip_detailed_error_t;              /*< enum indicating specific error encountered by the NTRIP client */

typedef struct xplr_ntrip_server_config_type {
    char                    host[XPLR_NTRIP_HOST_LENGTH];                   /*< NTRIP Caster address (domain or IP address)*/
    uint16_t                port;                                           /*< Caster's port (usually 2101)*/
    char                    mountpoint[XPLR_NTRIP_MOUNTPOINT_LENGTH];       /*< Mountpoint on the caster from which to request data*/
    bool                    ggaNecessary;                                   /*< set to true if caster requires client to send a periodic GGA message*/
} xplr_ntrip_server_config_t;

typedef struct xplr_ntrip_transfer_type {
    char                    corrData[XPLRNTRIP_RECEIVE_DATA_SIZE];     /*< buffer containing correction data*/
    uint32_t                corrDataSize;                                   /*< size of correction data in corrData buffer*/
} xplr_ntrip_transfer_t;

typedef struct xplr_ntrip_credentials_type {
    bool                    useAuth;                                        /*< check for correct configuration in ntrip init function*/
    char                    username[XPLR_NTRIP_CREDENTIALS_LENGTH];        /*< your username to connect to the NTRIP Caster*/
    char                    password[XPLR_NTRIP_CREDENTIALS_LENGTH];        /*< your password to connect to the NTRIP Caster*/
    char                    userAgent[XPLR_NTRIP_USERAGENT_LENGTH];         /*< your device's User-Agent on the NTRIP Caster*/
} xplr_ntrip_credentials_t;

typedef struct xplr_ntrip_config_type {
    xplr_ntrip_server_config_t     server;             /*< xplr NTRIP configuration struct*/
    xplr_ntrip_credentials_t       credentials;        /*< xplr NTRIP configuration struct*/
    xplr_ntrip_transfer_t          transfer;           /*< transfer struct */
} xplr_ntrip_config_t;

/*INDENT-ON*/

#ifdef __cplusplus
}
#endif
#endif // XPLR_NTRIP_CLIENT_TYPES_H_

// End of file