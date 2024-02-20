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

#ifndef XPLR_WIFI_NTRIP_CLIENT_TYPES_H_
#define XPLR_WIFI_NTRIP_CLIENT_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include <stdbool.h>
#include "./../../../components/hpglib/src/ntripClientCommon/xplr_ntrip_client_types.h"
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

/** NTRIP client struct. */
typedef struct xplrWifi_ntrip_client_type {
    xplr_ntrip_config_t                 *config;            /*< Ntrip configuration data*/
    bool                                config_set;         /*< safety check for ntrip init function*/
    bool                                credentials_set;    /*< safety check for ntrip init function*/
    int                                 socket;             /*< socket number*/
    bool                                socketIsValid;      /*< sanity check to prevent unhandled panics*/
    uint32_t                            ggaInterval;        /*< timekeeping to send GGA back to caster*/
    uint32_t                            timeout;            /*< timekeeping to go to error state*/
    xplr_ntrip_state_t                  state;              /*< state variable indicating the currect state of the NTRIP client*/
    xplr_ntrip_error_t                  error;              /*< detailed error struct*/
} xplrWifi_ntrip_client_t;
/*INDENT-ON*/

#ifdef __cplusplus
}
#endif
#endif // XPLR_WIFI_NTRIP_CLIENT_TYPES_H_

// End of file