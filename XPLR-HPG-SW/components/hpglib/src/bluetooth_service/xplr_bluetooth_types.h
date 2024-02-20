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
#include "./../../xplr_hpglib_cfg.h"

#if XPLRBLUETOOTH_MODE != XPLRBLUETOOTH_MODE_OFF

#ifndef _XPLR_BLUETOOTH_TYPES_H_
#define _XPLR_BLUETOOTH_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "xplr_log.h"
#include "freertos/ringbuf.h"
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
#include <esp_bt_defs.h>
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
#include <host/ble_gap.h>
#else
#error "hpglib Bluetooth module not configured correctly"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

typedef enum {
    XPLR_BLUETOOTH_ERROR = -1,
    XPLR_BLUETOOTH_OK
} xplrBluetooth_error_t;

typedef enum {
    XPLR_BLUETOOTH_MODE_LOW_ENERGY = -1,
    XPLR_BLUETOOTH_MODE_CLASSIC
} xplrBluetooth_mode_t;

typedef enum {
    XPLR_BLUETOOTH_CONN_STATE_ERROR = -3,
    XPLR_BLUETOOTH_CONN_STATE_BUSY,
    XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL,
    XPLR_BLUETOOTH_CONN_STATE_READY,
    XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE,
    XPLR_BLUETOOTH_CONN_STATE_CONNECTED
} xplrBluetooth_conn_state_t;

typedef struct xplrBluetooth_config_type {
    char                            deviceName[64];     /*< Bluetooth Classic/BLE server name */
    RingbufHandle_t                 ringBuffer;          /*< Ring buffer handle */
    StaticRingbuffer_t              staticBufHandle;    /*< Static ring buffer handle */
} xplrBluetooth_config_t;

typedef struct xplrBluetooth_diagnostics_type {
    xplrBluetooth_conn_state_t      state;              /*< State of connected device */
    int8_t                          rssi;               /*< RSSI of connected device */
} xplrBluetooth_diagnostics_t;

// *INDENT-OFF*
typedef struct xplrBluetooth_connected_device_type {
    uint32_t                        handle;             /*< Connected device handle (used to read to and write from the device) */
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
    esp_bd_addr_t                   address;            /*< Connected device address (used to identify the device)*/
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
    ble_addr_t                      address;            /*< Connected device address (used to identify the device) */
#else
#error "hpglib Bluetooth module not configured correctly"
#endif
    xplrBluetooth_diagnostics_t     diagnostics;        /*< Diagnostics struct */
    bool                            msgAvailable;       /*< True if message is available from this connected device */
    bool                            connected;          /*< True if device is connected (data invalid if this flag is false) */
    char                            *msg;               /*< Device RX message buffer */
} xplrBluetooth_connected_device_t;

typedef struct xplrBluetooth_client_type {
    xplrBluetooth_connected_device_t    devices[XPLRBLUETOOTH_NUMOF_DEVICES];   /*< Array containing the currently connected devices*/
    xplrBluetooth_config_t              configuration;                          /*< Configuration struct */
    xplrBluetooth_conn_state_t          state;                                  /*< State of the HPGLib Bluetooth client FSM */
    uint8_t                             buffer[XPLRBLUETOOTH_RX_BUFFER_SIZE];   /*< Static memory allocation for ring buffer*/
} xplrBluetooth_client_t;
// *INDENT-ON*
#ifdef __cplusplus
}
#endif
#endif

#endif


// End of file