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
#include "./../../../components/ubxlib/ubxlib.h"
#include "./../../../components/hpglib/xplr_hpglib_cfg.h"
#include "xplr_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */
/*INDENT-OFF*/

typedef enum {
    XPLR_CELL_NTRIP_ERROR = -1,    /**< process returned with errors. */
    XPLR_CELL_NTRIP_OK,            /**< indicates success of returning process. */
} xplrCell_ntrip_error_t;

typedef enum {
    XPLR_CELL_NTRIP_STATE_ERROR = -5,
    XPLR_CELL_NTRIP_STATE_BUSY,
    XPLR_CELL_NTRIP_STATE_CONNECTION_RESET,
    XPLR_CELL_NTRIP_STATE_CORRECTION_DATA_AVAILABLE,
    XPLR_CELL_NTRIP_STATE_REQUEST_GGA,
    XPLR_CELL_NTRIP_STATE_READY,
} xplrCell_ntrip_state_t;                                /*< enum indicating current state of NTRIP client*/

typedef enum {
    XPLR_CELL_NTRIP_UKNOWN_ERROR = -9,
    XPLR_CELL_NTRIP_BUSY_ERROR,
    XPLR_CELL_NTRIP_CONNECTION_RESET_ERROR,
    XPLR_CELL_NTRIP_BUFFER_TOO_SMALL_ERROR,
    XPLR_CELL_NTRIP_NO_GGA_TIMEOUT_ERROR,
    XPLR_CELL_NTRIP_CORR_DATA_TIMEOUT_ERROR,
    XPLR_CELL_NTRIP_SOCKET_ERROR,
    XPLR_CELL_NTRIP_UNABLE_TO_CREATE_TASK_ERROR,
    XPLR_CELL_NTRIP_SEMAPHORE_ERROR,
    XPLR_CELL_NTRIP_NO_ERROR,
} xplrCell_ntrip_detailed_error_t;                      /*< enum indicating specific error encountered by the NTRIP client */

typedef struct xplrCell_ntrip_config_type {
    char                    host[64];           /*< NTRIP Caster address (domain or IP address)*/
    uint16_t                port;               /*< Caster's port (usually 2101)*/
    char                    mountpoint[128];    /*< Mountpoint on the caster from which to request data*/
    bool                    ggaNecessary;       /*< set to true if caster requires client to send a periodic GGA message*/
} xplrCell_ntrip_config_t;

typedef struct xplrCell_ntrip_transfer_type {
    char                    corrData[XPLRCELL_NTRIP_RECEIVE_DATA_SIZE];     /*< buffer containing correction data*/
    uint32_t                corrDataSize;                                   /*< size of correction data in corrData buffer*/
    char                    ggaMsg[256];                                    /*< buffer for the APP to place GGA when requested by the NTRIP client*/
} xplrCell_ntrip_transfer_t;

typedef struct xplrCell_ntrip_credentials_type {
    bool                    useAuth;            /*< check for correct configuration in ntrip init function*/
    char                    username[32];       /*< your username to connect to the NTRIP Caster*/
    char                    password[32];       /*< your password to connect to the NTRIP Caster*/
    char                    userAgent[64];      /*< your device's User-Agent on the NTRIP Caster*/
} xplrCell_ntrip_credentials_t;

/** NTRIP client struct. */
typedef struct xplrCell_ntrip_client_type {
    xplrCell_ntrip_config_t             config;             /*< xplr NTRIP configuration struct*/
    bool                                config_set;         /*< safety check for ntrip init function*/
    xplrCell_ntrip_credentials_t        credentials;        /*< xplr NTRIP credentials struct*/
    bool                                credentials_set;    /*< safety check for ntrip init function*/
    uint32_t                            socket;             /*< socket number*/
    bool                                socketIsValid;      /*< sanity check to prevent unhandled panics*/
    xplrCell_ntrip_transfer_t           transfer;           /*< transfer struct */
    uint8_t                             cellDvcProfile;     /*< cellular module device profile ID*/
    uint32_t                            ggaInterval;        /*< timekeeping to send GGA back to caster*/
    uint32_t                            timeout;            /*< timekeeping to go to error state*/
    xplrLog_t                           *logCfg;            /*< pointer to the log struct of the module*/
    xplrCell_ntrip_state_t              state;              /*< state variable indicating the currect state of the NTRIP client*/
    xplrCell_ntrip_error_t              error;              /*< detailed error struct*/
} xplrCell_ntrip_client_t;
/*INDENT-ON*/

#ifdef __cplusplus
}
#endif
#endif // XPLR_NTRIP_CLIENT_TYPES_H_

// End of file