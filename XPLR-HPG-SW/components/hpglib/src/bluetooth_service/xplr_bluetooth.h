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

#ifndef _XPLR_BLUETOOTH_H_
#define _XPLR_BLUETOOTH_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "xplr_bluetooth_types.h"
#include "./../../common/xplr_common.h"
#include "freertos/semphr.h"
#ifdef __cplusplus
extern "C" {
#endif

#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE
#define BLE_SEND_MTU 128
// Necessary defines to format the BLE characteristic UUID
#define BYTE0(x) ((x)&0xFF)
#define BYTE1(x) (((x) >> 8) & 0xFF)
#define BYTE2(x) (((x) >> 16) & 0xFF)
#define BYTE3(x) (((x) >> 24) & 0xFF)
#define BYTE4(x) (((x) >> 32) & 0xFF)
#define BYTE5(x) (((x) >> 40) & 0xFF)
#define UUID128_CONST(a32, b16, c16, d16, e48) \
    BLE_UUID128_INIT( \
                      BYTE0(e48), BYTE1(e48), BYTE2(e48), BYTE3(e48), BYTE4(e48), BYTE5(e48), \
                      BYTE0(d16), BYTE1(d16), BYTE0(c16), BYTE1(c16), BYTE0(b16), \
                      BYTE1(b16), BYTE0(a32), BYTE1(a32), BYTE2(a32), BYTE3(a32), \
                    )

#define XPLRBLUETOOTH_BLE_CHARS_NORDIC                  (2)
#define XPLRBLUETOOTH_BLE_CHARS_CUSTOM                  (3)
#define XPLRBLUETOOTH_BLE_CHARS                         (XPLRBLUETOOTH_BLE_NORDIC)
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialise the HPGLib Bluetooth client
 *        Configuration struct should be correctly populated before using this function
 *
 * @param  client                                   Pointer to Bluetooth client struct
 * @param  xplrBluetoothSemaphore                   Semaphore need by bluetooth module
 * @param  xplrBluetoothDeviceMessageBuffer         Connected device RX buffer
 *                                                  (must be atleast XPLRBLUETOOTH_NUMOF_DEVICES*XPLRBLUETOOTH_MAX_MSG_SIZE bytes)
 * @param  xplrBluetoothDeviceMessageBufferSize     Size of xplrBluetoothDeviceMessageBuffer
 *
 * @return XPLR_BLUETOOTH_OK on success, XPLR_BLUETOOTH_ERROR otherwise.
 */
xplrBluetooth_error_t xplrBluetoothInit(xplrBluetooth_client_t *client,
                                        SemaphoreHandle_t xplrBluetoothSemaphore,
                                        char *xplrBluetoothDeviceMessageBuffer,
                                        size_t xplrBluetoothDeviceMessageBufferSize);

/**
 * @brief De-initialise the HPGLib Bluetooth client
 *
 *
 * @return XPLR_BLUETOOTH_OK on success, XPLR_BLUETOOTH_ERROR otherwise.
 */
xplrBluetooth_error_t xplrBluetoothDeInit(void);

/**
 * @brief Disconnect all connected Bluetooth/BLE devices
 *        Should be run before de-initialising the HPGLib Bluetooth client
 *
 *
 */
void xplrBluetoothDisconnectAllDevices(void);

/**
 * @brief Disconnect connected Bluetooth/BLE devices
 *
 * @param  dvc     pointer to xplrBluetooth_connected_device_t struct
 *
 */
void xplrBluetoothDisconnectDevice(xplrBluetooth_connected_device_t *dvc);

/**
 * @brief Get the current state of the HPGLib Bluetooth client FSM
 *
 *
 * @return XPLR_BLUETOOTH_OK on success, XPLR_BLUETOOTH_ERROR otherwise.
 */
xplrBluetooth_conn_state_t xplrBluetoothGetState(void);

/**
 * @brief Read incoming message from a connected Bluetooth/BLE device
 *
 * @param  dvc     pointer to xplrBluetooth_connected_device_t struct
 *
 * @return size of incoming message in bytes
 */
int32_t xplrBluetoothRead(xplrBluetooth_connected_device_t *dvc);

/**
 * @brief Read the first incoming message in the queue (from any connected Bluetooth/BLE device)
 *
 * @param  dvc     pointer to xplrBluetooth_connected_device_t struct
 *
 * @return size of incoming message in bytes
 */
int32_t xplrBluetoothReadFirstAvailableMsg(xplrBluetooth_connected_device_t **dvc);

/**
 * @brief Write a message to a connected Bluetooth/BLE device
 *
 * @param  dvc                      pointer to xplrBluetooth_connected_device_t struct
 * @param  msg                      Pointer to buffer which stores the message
 * @param  msgSize                  Size of the message
 *
 * @return XPLR_BLUETOOTH_OK on success, XPLR_BLUETOOTH_ERROR otherwise.
 */
xplrBluetooth_error_t xplrBluetoothWrite(xplrBluetooth_connected_device_t *dvc,
                                         char *msg,
                                         size_t msgSize);

/**
 * @brief Update diagnostic information on a connected device struct and print them in the console
 *
 * @param  dvc     pointer to xplrBluetooth_connected_device_t struct
 *
 */
void xplrBluetoothPrintDiagnostics(xplrBluetooth_connected_device_t *dvc);

/**
 * @brief Print a table of the devices currently connected
 *
 * @return pointer to the connected device array
 */
xplrBluetooth_connected_device_t *xplrBluetoothPrintConnectedDevices(void);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrBluetoothInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops logging of the module.
 *
 * @return  ESP_OK on success, ESP_FAIL otherwise.
*/
esp_err_t xplrBluetoothStopLogModule(void);

#ifdef __cplusplus
}
#endif

#endif

#endif

// End of file