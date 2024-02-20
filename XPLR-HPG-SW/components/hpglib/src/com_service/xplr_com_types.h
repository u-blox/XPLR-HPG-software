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

#ifndef _XPLR_COM_TYPES_H_
#define _XPLR_COM_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"
#include "xplr_log.h"

/** @file
 * @brief This header file defines the types used in communication service API,
 * Types include status, state, config enums and structs that are exposed to the user
 * providing an easy to use and configurable communication library.
 * The API builds on top of ubxlib, implementing some high level logic that can be used
 * in common IoT scenarios.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLRCOM_CELL_RAT_SIZE        3U
#define XPLRCOM_CELL_REBOOT_WAIT_MS  5000U

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to hpgCom module. */
typedef enum {
    XPLR_COM_ERROR = -1,    /**< process returned with errors. */
    XPLR_COM_OK,            /**< indicates success of returning process. */
    XPLR_COM_BUSY           /**< returning process currently busy. */
} xplrCom_error_t;

/** States describing the cellular connection process. */
typedef enum {
    XPLR_COM_CELL_CONNECT_TIMEOUT = -2,
    XPLR_COM_CELL_CONNECT_ERROR,
    XPLR_COM_CELL_CONNECT_OK = 0,
    XPLR_COM_CELL_CONNECT_INIT,
    XPLR_COM_CELL_CONNECT_OPENDEVICE,
    XPLR_COM_CELL_CONNECT_SET_MNO,
    XPLR_COM_CELL_CONNECT_SET_RAT,
    XPLR_COM_CELL_CONNECT_SET_BANDS,
    XPLR_COM_CELL_CONNECT_CHECK_READY,
    XPLR_COM_CELL_CONNECT,
    XPLR_COM_CELL_CONNECTED
} xplrCom_cell_connect_t;

/*INDENT-OFF*/

/** Cellular configuration struct for setting up deviceSettings.
 * Struct to be provided by the user via xplrComCellInit().
*/
typedef struct xplrCom_cell_config_type {
    uDeviceCfgCell_t     *hwSettings;                           /**< pointer to cellular module hw settings. */
    uDeviceCfgUart_t     *comSettings;                          /**< pointer to peripheral configuration pins. */
    uNetworkCfgCell_t    *netSettings;                          /**< pointer to cellular module network settings. */
    int8_t               profileIndex;                          /**< holds profile index that current module is stored. */
    int32_t              mno;                                   /**< MNO of current profile. */
    uCellNetRat_t        ratList[XPLRCOM_CELL_RAT_SIZE];        /**< struct array to RAT list. */
    uint64_t             bandList[XPLRCOM_CELL_RAT_SIZE * 2];   /**< array containing bandmask values
                                                                    of given ratList. Bandmask pairs
                                                                    are in sync to ratList values.
                                                                    That is bandmask[0],[1] are assigned to
                                                                    ratList[0] and so on.
                                                                    Bandmask is only configured when the
                                                                    corresponding RAT is set to CAT-M1 or
                                                                    NB1. */
} xplrCom_cell_config_t;

/** Cellular network information struct.
 * To retrieve it xplrComCellNetworkInfo() needs to called.
*/
typedef struct xplrCom_cell_netInfo_type {
    char                networkOperator[32];            /**< Network operator name. */
    char                ip[U_CELL_NET_IP_ADDRESS_SIZE]; /**< IP acquired from network carrier. */
    char                apn[64];                        /**< APN of network carrier. */
    char                rat[32];                        /**< RAT used to register. */
    int32_t             Mcc;                            /**< MCC of network carrier. */
    int32_t             Mnc;                            /**< MNC of network carrier. */
    bool                registered;                     /**< Bool denoting registration status. */
    char                status[32];                     /**< Actual network status of the module.
                                                             Can be any status as described in uCellNetStatus_t */
} xplrCom_cell_netInfo_t;

/*INDENT-ON*/

#ifdef __cplusplus
}
#endif
#endif // _XPLR_COM_TYPES_H_

// End of file