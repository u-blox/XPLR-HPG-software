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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */
#include "./../../xplr_hpglib_cfg.h"

#if XPLRBLUETOOTH_MODE != XPLRBLUETOOTH_MODE_OFF

#include "xplr_bluetooth.h"
#include "xplr_bluetooth_types.h"
#include <string.h>
#include "freertos/ringbuf.h"

#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#elif XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#else
#error "Wrong XPLR_BLUETOOTH mode"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#if (1 == XPLRBLUETOOTH_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRBLUETOOTH_LOG_ACTIVE))
#define XPLRBLUETOOTH_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgBluetooth", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRBLUETOOTH_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRBLUETOOTH_LOG_ACTIVE)
#define XPLRBLUETOOTH_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgBluetooth", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRBLUETOOTH_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRBLUETOOTH_LOG_ACTIVE)
#define XPLRBLUETOOTH_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgBluetooth", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRBLUETOOTH_CONSOLE(message, ...) do{} while(0)
#endif

#define XPLR_BLUETOOTH_MAX_DELAY    (TickType_t)pdMS_TO_TICKS(100)

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static int8_t logIndex = -1;
xplrBluetooth_client_t *btClient;
SemaphoreHandle_t btSemaphore;

#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE
#if XPLRBLUETOOTH_BLE_CHARS == XPLRBLUETOOTH_BLE_NORDIC
// BLE UUIDs for Nordic UART Service (NUS)
static const ble_uuid128_t xplrBleServiceUuid = UUID128_CONST(0x6E400001,
                                                              0xB5A3,
                                                              0xF393,
                                                              0xE0A9,
                                                              0xE50E24DCCA9E);
static const ble_uuid128_t xplrBleCharRxUuid = UUID128_CONST(0x6E400002,
                                                             0xB5A3,
                                                             0xF393,
                                                             0xE0A9,
                                                             0xE50E24DCCA9E);
static const ble_uuid128_t xplrBleCharTxUuid = UUID128_CONST(0x6E400003,
                                                             0xB5A3,
                                                             0xF393,
                                                             0xE0A9,
                                                             0xE50E24DCCA9E);
#elif XPLRBLUETOOTH_BLE_CHARS == XPLRBLUETOOTH_BLE_CHARS_CUSTOM
// BLE UUIDs for custom Service
static const ble_uuid128_t xplrBleServiceUuid = UUID128_CONST(0x00000000,
                                                              0x0000,
                                                              0x0000,
                                                              0x0000,
                                                              0x000000000000);
static const ble_uuid128_t xplrBleCharRxUuid = UUID128_CONST(0x00000000,
                                                             0x0000,
                                                             0x0000,
                                                             0x0000,
                                                             0x000000000000);
static const ble_uuid128_t xplrBleCharTxUuid = UUID128_CONST(0x00000000,
                                                             0x0000,
                                                             0x0000,
                                                             0x0000,
                                                             0x000000000000);
#endif
// BLE address used for advertising
static uint8_t bleAddr;
// BLE attribute handle for notifying
static uint16_t bleNotifyCharAttrHandle;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
static void btInitDeviceTable(void);
static void btDeInitDeviceTable(void);
static xplrBluetooth_error_t btInitDeviceBuffers(char *xplrBluetoothDeviceMessageBuffer,
                                                 size_t xplrBluetoothDeviceMessageBufferSize);
static void btRemoveDevice(uint32_t handle);
static uint8_t btGetNumOfConnectedDevices(void);
static void btSetAvailableMessage(uint32_t handle);
static int8_t btDeviceHandleToIndex(uint32_t handle);
static xplrBluetooth_connected_device_t *btDeviceHandleToDeviceStructHelper(uint32_t handle);
static void btCacheMsg(uint32_t handle, uint8_t *incomingMsg, uint32_t incomingMsgLen);
static void btPlaceMessageBackInBufferHelper(uint8_t items, uint8_t *writeBackBuffer);
static int32_t btRead(xplrBluetooth_connected_device_t **dvc, bool readFirstAvailableMsg);
static xplrBluetooth_connected_device_t *btReadFirstAvailableMsgHelper(uint8_t *message,
                                                                       size_t messageSize,
                                                                       int32_t *size,
                                                                       bool *correctMsgRead);
static void btUpdateStateHelper(UBaseType_t *itemsRemainingInBuffer);
#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC
static xplrBluetooth_error_t btClassicInit(void);
static xplrBluetooth_error_t btClassicControllerInit(void);
static xplrBluetooth_error_t btClassicBluedroidInit(void);
static xplrBluetooth_error_t btClassicCbInit(void);
static xplrBluetooth_error_t btClassicDeInit(void);
static void btClassicDisconnectDevice(xplrBluetooth_connected_device_t *dvc);
static xplrBluetooth_error_t btClassicWrite(xplrBluetooth_connected_device_t *dvc,
                                            char *msg,
                                            uint32_t msgSize);
static void btClassicGAPcb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void BtClassicSPPcb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static void btClassicDiagnostics(uint32_t handle, bool disconnected);
static void btClassicAddDevice(uint32_t handle, esp_bd_addr_t address);
static int8_t btClassicDeviceAddressToIndex(esp_bd_addr_t address);
#elif XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE
static xplrBluetooth_error_t bleInit(void);
static xplrBluetooth_error_t bleDeInit(void);
static void bleDisconnectDevice(xplrBluetooth_connected_device_t *dvc);
static xplrBluetooth_error_t bleWrite(xplrBluetooth_connected_device_t *dvc, char *msg,
                                      uint32_t msgSize);
static int bleReceiveMsgCb(uint16_t conn_handle,
                           uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt,
                           void *arg);
static int bleDummyCb(uint16_t conn_handle,
                      uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt,
                      void *arg);
static int bleGapEventCb(struct ble_gap_event *event, void *arg);
static void bleAppOnSyncCb(void);
static void bleAppAdvertise(void);
static void bleTask(void *param);
static void bleAddDevice(uint32_t handle, ble_addr_t address);
static void bleDiagnostics(uint32_t handle, bool disconnected);
#else
#error "Wrong XPLR_BLUETOOTH mode"
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */
xplrBluetooth_error_t xplrBluetoothInit(xplrBluetooth_client_t *client,
                                        SemaphoreHandle_t xplrBluetoothSemaphore,
                                        char *xplrBluetoothDeviceMessageBuffer,
                                        size_t xplrBluetoothDeviceMessageBufferSize)
{
    xplrBluetooth_error_t ret;

    btClient = client;
    btSemaphore = xplrBluetoothSemaphore;
    btClient->configuration.ringBuffer = xRingbufferCreateStatic(XPLRBLUETOOTH_RX_BUFFER_SIZE,
                                                                 RINGBUF_TYPE_NOSPLIT,
                                                                 btClient->buffer,
                                                                 &(btClient->configuration.staticBufHandle));
    if (btClient->configuration.ringBuffer == NULL) {
        XPLRBLUETOOTH_CONSOLE(E, "Failed to create ring buffer");
        ret = XPLR_BLUETOOTH_ERROR;
    } else {
        btInitDeviceTable();
        ret = btInitDeviceBuffers(xplrBluetoothDeviceMessageBuffer, xplrBluetoothDeviceMessageBufferSize);
        if (ret != XPLR_BLUETOOTH_ERROR) {
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
            ret = btClassicInit();
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
            ret = bleInit();
#else
#error "hpglib Bluetooth module not configured correctly"
            ret = XPLR_BLUETOOTH_ERROR;
#endif
        } else {
            // Do nothing
        }
    }

    return ret;
}

xplrBluetooth_error_t xplrBluetoothDeInit(void)
{
    xplrBluetooth_error_t ret;
    BaseType_t semaphoreRet;

    xplrBluetoothDisconnectAllDevices();

#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
    ret = btClassicDeInit();
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
    ret = bleDeInit();
#else
#error "hpglib Bluetooth module not configured correctly"
    ret = XPLR_BLUETOOTH_ERROR;
#endif
    btDeInitDeviceTable();
    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {

        vRingbufferDelete(btClient->configuration.ringBuffer);
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }

    return ret;
}

void xplrBluetoothDisconnectAllDevices(void)
{
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            if (btClient->devices[i].connected) {
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
                btClassicDisconnectDevice(&btClient->devices[i]);
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
                bleDisconnectDevice(&btClient->devices[i]);
#else
#error "hpglib Bluetooth module not configured correctly"
#endif
            } else {
                // No need to disconnect
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        //  Do nothing
    }
}

void xplrBluetoothDisconnectDevice(xplrBluetooth_connected_device_t *dvc)
{
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
        btClassicDisconnectDevice(dvc);
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
        bleDisconnectDevice(dvc);
#else
#error "hpglib Bluetooth module not configured correctly"
#endif
        xSemaphoreGive(btSemaphore);
    } else {
        //  Do nothing
    }
}

xplrBluetooth_conn_state_t xplrBluetoothGetState(void)
{
    xplrBluetooth_conn_state_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        ret = btClient->state;
        xSemaphoreGive(btSemaphore);
    } else {
        // BUSY state is only returned here and it signals that some other task
        // is accessing the btClient struct
        ret = XPLR_BLUETOOTH_CONN_STATE_BUSY;
    }

    return ret;
}

int32_t xplrBluetoothRead(xplrBluetooth_connected_device_t *dvc)
{
    int32_t ret = btRead(&dvc, false);

    return ret;
}

int32_t xplrBluetoothReadFirstAvailableMsg(xplrBluetooth_connected_device_t **dvc)
{
    int32_t ret = btRead(dvc, true);

    return ret;
}

xplrBluetooth_error_t xplrBluetoothWrite(xplrBluetooth_connected_device_t *dvc,
                                         char *msg,
                                         size_t msgSize)
{
    xplrBluetooth_error_t ret;

#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
    ret = btClassicWrite(dvc, msg, msgSize);
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
    ret = bleWrite(dvc, msg, msgSize);
#else
#error "hpglib Bluetooth module not configured correctly"
    ret = XPLR_BLUETOOTH_ERROR;
#endif

    return ret;
}

void xplrBluetoothPrintDiagnostics(xplrBluetooth_connected_device_t *dvc)
{
#if (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC)
    btClassicDiagnostics(dvc->handle, false);
#elif (XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE)
    bleDiagnostics(dvc->handle, false);
#else
#error "hpglib Bluetooth module not configured correctly"
    ret = XPLR_BLUETOOTH_ERROR;
#endif
}

xplrBluetooth_connected_device_t *xplrBluetoothPrintConnectedDevices(void)
{
    uint8_t devices, i;

    devices = btGetNumOfConnectedDevices();
    XPLRBLUETOOTH_CONSOLE(I, "There are %d connected devices", devices);

    for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
        if (btClient->devices[i].connected == true) {
            xplrBluetoothPrintDiagnostics(&btClient->devices[i]);
        }
    }

    return btClient->devices;
}

int8_t xplrBluetoothInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_BLUETOOTH_DEFAULT_FILENAME,
                                   XPLRLOG_FILE_SIZE_INTERVAL,
                                   XPLRLOG_NEW_FILE_ON_BOOT);
        } else {
            /* logCfg contains the instance settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   logCfg->filename,
                                   logCfg->sizeInterval,
                                   logCfg->erasePrev);
        }
        ret = logIndex;
    } else {
        /* logIndex is positive so logging has been initialized before */
        logErr = xplrLogEnable(logIndex);
        if (logErr != XPLR_LOG_OK) {
            ret = -1;
        } else {
            ret = logIndex;
        }
    }

    return ret;
}

esp_err_t xplrBluetoothStopLogModule(void)
{
    esp_err_t ret;
    xplrLog_error_t logErr;

    logErr = xplrLogDisable(logIndex);
    if (logErr != XPLR_LOG_OK) {
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    return ret;
}
/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */
void btInitDeviceTable(void)
{
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            btClient->devices[i].handle = -999;
            btClient->devices[i].connected = false;
            btClient->devices[i].msgAvailable = false;
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}

void btDeInitDeviceTable(void)
{
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            btClient->devices[i].handle = -999;
            btClient->devices[i].connected = false;
            btClient->devices[i].msgAvailable = false;
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}


xplrBluetooth_error_t btInitDeviceBuffers(char *xplrBluetoothDeviceMessageBuffer,
                                          size_t xplrBluetoothDeviceMessageBufferSize)
{

    xplrBluetooth_error_t ret;
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        if (xplrBluetoothDeviceMessageBufferSize < XPLRBLUETOOTH_MAX_MSG_SIZE *
            XPLRBLUETOOTH_NUMOF_DEVICES) {
            XPLRBLUETOOTH_CONSOLE(E, "Insufficient device buffer");
            ret = XPLR_BLUETOOTH_ERROR;
        } else {
            for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
                btClient->devices[i].msg = &(xplrBluetoothDeviceMessageBuffer[i * XPLRBLUETOOTH_MAX_MSG_SIZE]);
            }
            ret = XPLR_BLUETOOTH_OK;
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
        ret = XPLR_BLUETOOTH_ERROR;
    }

    return ret;
}

void btRemoveDevice(uint32_t handle)
{
    uint8_t index, connectedDevices;
    BaseType_t semaphoreRet;

    // Find the device's index in the table
    index = btDeviceHandleToIndex(handle);
    // Mark device as disconnected
    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        btClient->devices[index].connected = false;
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
    // Store the number of connected devices after the disconnection
    connectedDevices = btGetNumOfConnectedDevices();

    // If there is no connected device change the client's state
    if (!connectedDevices) {
        semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
        if (semaphoreRet == pdTRUE) {
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_READY;
            xSemaphoreGive(btSemaphore);
        } else {
            XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
        }
    } else {
        // Do nothing
    }
}

uint8_t btGetNumOfConnectedDevices(void)
{
    uint8_t i;
    uint8_t counter = 0;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            if (btClient->devices[i].connected) {
                counter++;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        // Couldn't get semaphore
    }

    return counter;
}

void btSetAvailableMessage(uint32_t handle)
{
    uint8_t i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            // Find the corresponding slot
            if (btClient->devices[i].handle == handle && btClient->devices[i].connected) {
                // If the device exists and is connected
                btClient->devices[i].msgAvailable = true;
                break;
            }
        }
        btClient->state = XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE;
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(E, "Couldn't get semaphore");
    }
}

int8_t btDeviceHandleToIndex(uint32_t handle)
{
    uint8_t i;
    int8_t ret = -1;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            // Find the corresponding slot
            if (btClient->devices[i].handle == handle && btClient->devices[i].connected) {
                // If the device exists and is connected
                ret = i;
                break;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }

    if (ret == -1) {
        XPLRBLUETOOTH_CONSOLE(E, "Cannot find device");
    } else {
        // Do nothing
    }

    return ret;
}

xplrBluetooth_connected_device_t *btDeviceHandleToDeviceStructHelper(uint32_t handle)
{
    xplrBluetooth_connected_device_t *ret = &(btClient->devices[btDeviceHandleToIndex(handle)]);

    return ret;
}

void btCacheMsg(uint32_t handle, uint8_t *message, size_t messageSize)
{
    BaseType_t bufRet, semaphoreRet;
    if (messageSize <= XPLRBLUETOOTH_MAX_MSG_SIZE) {
        uint8_t buf[4 + messageSize];

        // The first 4 bytes of every message are the device handle from which the message was received
        memcpy(buf, &handle, 4);
        memcpy(&buf[4], message, messageSize);

        semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
        if (semaphoreRet == pdTRUE) {
            bufRet = xRingbufferSendFromISR(btClient->configuration.ringBuffer, buf, 4 + messageSize, NULL);
            if (bufRet != pdTRUE) {
                XPLRBLUETOOTH_CONSOLE(E, "Buffer full cannot store message");
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL;
            } else {
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE;
            }
            xSemaphoreGive(btSemaphore);
        } else {
            XPLRBLUETOOTH_CONSOLE(E, "Couldn't get semaphore");
        }
    } else {
        XPLRBLUETOOTH_CONSOLE(E, "Message larger than configured max message size, discarding...");
    }
}

void btPlaceMessageBackInBufferHelper(uint8_t items, uint8_t *writeBackBuffer)
{
    uint8_t remainingItems = items;
    size_t messageSize;
    uint8_t *message = NULL;
    BaseType_t bufRet;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        while (remainingItems > 0) {
            message = xRingbufferReceiveFromISR(btClient->configuration.ringBuffer, &messageSize);
            memcpy(writeBackBuffer, message, messageSize);
            bufRet = xRingbufferSendFromISR(btClient->configuration.ringBuffer, writeBackBuffer, messageSize,
                                            NULL);
            if (bufRet != pdTRUE) {
                XPLRBLUETOOTH_CONSOLE(E, "Buffer full cannot store message");
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL;
            } else {
                remainingItems--;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}

int32_t btRead(xplrBluetooth_connected_device_t **dvc,
               bool readFirstAvailableMsg)
{
    int32_t ret, handleBuf, initialItemsRemainingInBuffer;
    size_t messageSize;
    uint8_t *message = NULL;
    UBaseType_t itemsRemainingInBuffer;
    BaseType_t semaphoreRet, bufRet;
    uint8_t writeBackBuffer[4 + XPLRBLUETOOTH_MAX_MSG_SIZE];
    bool correctMsgRead = false;
    uint8_t repCounter = 0;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        vRingbufferGetInfo(btClient->configuration.ringBuffer,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           &itemsRemainingInBuffer);
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
    initialItemsRemainingInBuffer = itemsRemainingInBuffer;
    if (itemsRemainingInBuffer != 0) {
        if (readFirstAvailableMsg) {
            // Read the first available message in the buffer (from any device)
            *dvc = btReadFirstAvailableMsgHelper(message,
                                                 messageSize,
                                                 &ret,
                                                 &correctMsgRead);
        } else {
            do {
                // Keep reading messages until you find the first from the requested device
                repCounter++;
                semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
                if (semaphoreRet == pdTRUE) {
                    message = xRingbufferReceiveFromISR(btClient->configuration.ringBuffer, &messageSize);
                    xSemaphoreGive(btSemaphore);
                } else {
                    XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
                }
                memcpy(&handleBuf, message, 4);
                semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
                if (semaphoreRet == pdTRUE) {
                    // If the device handle is the same as requested in the function call
                    if ((*dvc)->handle == handleBuf) {
                        // Keep the message in the device buffer
                        memset((*dvc)->msg, '\0', XPLRBLUETOOTH_MAX_MSG_SIZE);
                        memcpy((*dvc)->msg, &message[4], messageSize - 4);
                        vRingbufferReturnItemFromISR(btClient->configuration.ringBuffer, message, NULL);
                        xSemaphoreGive(btSemaphore);
                    } else {
                        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
                    }
                    ret = messageSize - 4;
                    correctMsgRead = true;
                    itemsRemainingInBuffer--;
                } else {
                    // If the read message is from a different device then return it to the buffer
                    memcpy(writeBackBuffer, message, messageSize);
                    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
                    if (semaphoreRet == pdTRUE) {
                        vRingbufferReturnItemFromISR(btClient->configuration.ringBuffer, message, NULL);
                        bufRet = xRingbufferSendFromISR(btClient->configuration.ringBuffer, writeBackBuffer, messageSize,
                                                        NULL);
                        if (bufRet != pdTRUE) {
                            XPLRBLUETOOTH_CONSOLE(E, "Buffer full cannot store message");
                            btClient->state = XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL;
                        } else {
                            itemsRemainingInBuffer--;
                        }
                        xSemaphoreGive(btSemaphore);
                    } else {
                        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
                    }
                    ret = -1;
                }
                if (initialItemsRemainingInBuffer == repCounter && !correctMsgRead) {
                    // Checked all the messages in the buffer and didn't find one from the requested device
                    XPLRBLUETOOTH_CONSOLE(W, "No message from requested device");
                    ret = 0;
                    break;
                }
            } while (!correctMsgRead);
            // Arrange the messages in the buffer in the correct order (if needed)
            if (repCounter == 1 && correctMsgRead) {
                // Do nothing
            } else if (repCounter != 1 && correctMsgRead) {
                // Place all the remaining messages in the right order
                btPlaceMessageBackInBufferHelper(itemsRemainingInBuffer, writeBackBuffer);
            }
        }
    } else {
        // No message to read
        XPLRBLUETOOTH_CONSOLE(I, "No message to read");
        ret = 0;
    }
    btUpdateStateHelper(&itemsRemainingInBuffer);

    return ret;
}

xplrBluetooth_connected_device_t *btReadFirstAvailableMsgHelper(uint8_t *message,
                                                                size_t messageSize,
                                                                int32_t *size,
                                                                bool *correctMsgRead)
{
    xplrBluetooth_connected_device_t *ret;
    int32_t handle;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {

        message = xRingbufferReceiveFromISR(btClient->configuration.ringBuffer, &messageSize);
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
    // Keep the pointer to the device struct
    memcpy(&handle, message, 4);
    ret = btDeviceHandleToDeviceStructHelper(handle);
    // Keep the message in the user provided buffer
    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        memset(ret->msg, '\0', XPLRBLUETOOTH_MAX_MSG_SIZE);
        memcpy(ret->msg, &message[4], messageSize - 4);
        vRingbufferReturnItemFromISR(btClient->configuration.ringBuffer, message, NULL);
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
    *size = messageSize - 4;
    *correctMsgRead = true;

    return ret;
}

void btUpdateStateHelper(UBaseType_t *itemsRemainingInBuffer)
{
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        vRingbufferGetInfo(btClient->configuration.ringBuffer,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           itemsRemainingInBuffer);
        if (*itemsRemainingInBuffer) {
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE;
        } else {
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_CONNECTED;
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}

#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC

xplrBluetooth_error_t btClassicInit(void)
{
    xplrBluetooth_error_t ret;

    ret = btClassicControllerInit();

    if (ret == XPLR_BLUETOOTH_OK) {
        ret = btClassicBluedroidInit();
    } else {
        btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
    }

    if (ret == XPLR_BLUETOOTH_OK) {
        ret = btClassicCbInit();
        if (ret == XPLR_BLUETOOTH_OK) {
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_READY;
        } else {
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
        }
    } else {
        btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
    }

    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    const uint8_t *bda = esp_bt_dev_get_address();
    XPLRBLUETOOTH_CONSOLE(I,
                          "xplr-hpg address:[%02x:%02x:%02x:%02x:%02x:%02x]",
                          bda[0],
                          bda[1],
                          bda[2],
                          bda[3],
                          bda[4],
                          bda[5]);
    return ret;
}

xplrBluetooth_error_t btClassicControllerInit(void)
{
    esp_err_t espRet;
    xplrBluetooth_error_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    espRet = esp_bt_controller_init(&bt_cfg);

    if (espRet != ESP_OK) {
        XPLRBLUETOOTH_CONSOLE(E, "initialize controller failed: %s", esp_err_to_name(espRet));
        ret = XPLR_BLUETOOTH_ERROR;
    } else {
        espRet = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
        if (espRet != ESP_OK) {
            XPLRBLUETOOTH_CONSOLE(E, "enable controller failed: %s", esp_err_to_name(espRet));
            ret = XPLR_BLUETOOTH_ERROR;
        } else {
            ret = XPLR_BLUETOOTH_OK;
        }
    }
    return ret;
}

xplrBluetooth_error_t btClassicBluedroidInit(void)
{
    esp_err_t espRet;
    xplrBluetooth_error_t ret;

    espRet = esp_bluedroid_init();

    if (espRet != ESP_OK) {
        XPLRBLUETOOTH_CONSOLE(E, "initialize bluedroid failed: %s", esp_err_to_name(espRet));
        ret = XPLR_BLUETOOTH_ERROR;
    } else {
        espRet = esp_bluedroid_enable();
        if (espRet != ESP_OK) {
            XPLRBLUETOOTH_CONSOLE(E, "enable bluedroid failed: %s", esp_err_to_name(espRet));
            ret = XPLR_BLUETOOTH_ERROR;
        } else {
            ret = XPLR_BLUETOOTH_OK;
        }
    }
    return ret;
}

xplrBluetooth_error_t btClassicCbInit(void)
{
    esp_err_t espRet;
    xplrBluetooth_error_t ret;

    espRet = esp_spp_register_callback(BtClassicSPPcb);

    if (espRet != ESP_OK) {
        XPLRBLUETOOTH_CONSOLE(E, "spp register failed: %s", esp_err_to_name(espRet));
        ret = XPLR_BLUETOOTH_ERROR;
    } else {
        espRet = esp_bt_gap_register_callback(btClassicGAPcb);
        if (espRet != ESP_OK) {
            XPLRBLUETOOTH_CONSOLE(E, "gap register failed: %s", esp_err_to_name(espRet));
            ret = XPLR_BLUETOOTH_ERROR;
        } else {
            espRet = esp_spp_init(ESP_SPP_MODE_CB);
            if (espRet != ESP_OK) {
                XPLRBLUETOOTH_CONSOLE(E, "spp init failed: %s", esp_err_to_name(espRet));
                ret = XPLR_BLUETOOTH_ERROR;
            } else {
                ret = XPLR_BLUETOOTH_OK;
            }
        }
    }
    return ret;
}

xplrBluetooth_error_t btClassicDeInit(void)
{
    esp_err_t espRet[6];
    int i;
    xplrBluetooth_error_t ret;

    esp_bt_controller_deinit();

    espRet[0] = esp_spp_stop_srv();
    espRet[1] = esp_spp_deinit();
    espRet[2] = esp_bluedroid_disable();
    espRet[3] = esp_bluedroid_deinit();
    espRet[4] = esp_bt_controller_disable();
    espRet[5] = esp_bt_controller_deinit();

    for (i = 0; i < 6; i++) {
        if (espRet[i] != ESP_OK) {
            ret = XPLR_BLUETOOTH_ERROR;
            break;
        }
    }

    return ret;
}

void btClassicDisconnectDevice(xplrBluetooth_connected_device_t *dvc)
{
    esp_err_t espRet;

    espRet = esp_spp_disconnect(dvc->handle);
    if (espRet != ESP_OK) {
        XPLRBLUETOOTH_CONSOLE(E,
                              "Couldn't disconnect device with handle: [%d]",
                              dvc->handle);
    } else {
        // Do nothing
    }
}

xplrBluetooth_error_t btClassicWrite(xplrBluetooth_connected_device_t *dvc,
                                     char *msg,
                                     size_t msgSize)
{
    xplrBluetooth_error_t ret;
    esp_err_t espRet;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        if (msgSize == 0) {
            ret = XPLR_BLUETOOTH_ERROR;
            XPLRBLUETOOTH_CONSOLE(E, "msgSize is 0");
        } else {
            espRet = esp_spp_write(dvc->handle, msgSize, (uint8_t *)msg);
            if (espRet == ESP_OK) {
                ret = XPLR_BLUETOOTH_OK;
            } else {
                XPLRBLUETOOTH_CONSOLE(E, "esp_spp_write fail");
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
                ret = XPLR_BLUETOOTH_ERROR;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        // Couldn't get semaphore returning error
        ret = XPLR_BLUETOOTH_ERROR;
    }

    return ret;
}

void btClassicDiagnostics(uint32_t handle, bool disconnected)
{
    esp_err_t espErr;
    int8_t index;
    BaseType_t semaphoreRet;

    index = btDeviceHandleToIndex(handle);
    if (index == -1) {
        // Cannot find device not printing anything btDeviceHandleToIndex will print error
    } else {
        semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
        if (semaphoreRet == pdTRUE) {
            if (disconnected) {
                // Don't try to get RSSI since device already disconnected, previous stored RSSI will be printed
            } else {
                espErr = esp_bt_gap_read_rssi_delta(btClient->devices[index].address);
                if (espErr != ESP_OK) {
                    XPLRBLUETOOTH_CONSOLE(E, "cannot get RSSI");
                } else {
                    // Do nothing
                }
            }
            XPLRBLUETOOTH_CONSOLE(I,
                                  "Device handle: %d | Device address: %02x:%02x:%02x:%02x:%02x:%02x | RSSI: %d dBm",
                                  handle,
                                  btClient->devices[index].address[0],
                                  btClient->devices[index].address[1],
                                  btClient->devices[index].address[2],
                                  btClient->devices[index].address[3],
                                  btClient->devices[index].address[4],
                                  btClient->devices[index].address[5],
                                  btClient->devices[index].diagnostics.rssi);

            xSemaphoreGive(btSemaphore);
        } else {
            XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
        }
    }
}


void btClassicAddDevice(uint32_t handle, esp_bd_addr_t address)
{
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            // Find the first free slot
            if (!btClient->devices[i].connected) {
                // Use this slot for the new connected device
                btClient->devices[i].handle = handle;
                btClient->devices[i].connected = true;
                btClient->devices[i].msgAvailable = false;
                memcpy(btClient->devices[i].address, address, ESP_BD_ADDR_LEN * sizeof(uint8_t));
                break;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}

int8_t btClassicDeviceAddressToIndex(esp_bd_addr_t address)
{
    uint8_t i, x;
    uint8_t check = 0;
    int8_t ret = -1;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            // Find the corresponding slot
            for (x = 0; x < 6; x++) {
                if (btClient->devices[i].address[x] == address[x]) {
                    check++;
                } else {
                    // Do nothing
                }
            }
            if (check == 6) {
                ret = i;
                break;
            } else {
                check = 0;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }

    if (ret == -1) {
        XPLRBLUETOOTH_CONSOLE(E, "Cannot find device");
    } else {
        // Do nothing
    }

    return ret;
}

#elif XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE

xplrBluetooth_error_t bleInit()
{
    xplrBluetooth_error_t ret;
    esp_err_t espRet;
    int intRet;
    static const struct ble_gatt_chr_def bleGattCharDefVar[] = {
        {
            .uuid = (ble_uuid_t *) &xplrBleCharRxUuid,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            .access_cb = bleReceiveMsgCb
        },
        {
            .uuid = (ble_uuid_t *) &xplrBleCharTxUuid,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &bleNotifyCharAttrHandle,
            .access_cb = bleDummyCb
        },
        {0},
    };
    static const struct ble_gatt_svc_def bleGattSvcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &xplrBleServiceUuid.u,
            .characteristics = bleGattCharDefVar
        },
        {0}
    };

    espRet = esp_nimble_hci_and_controller_init();
    if (espRet != ESP_OK) {
        XPLRBLUETOOTH_CONSOLE(E, "esp_nimble_hci_and_controller_init() failed with error: %d", espRet);
        ret = XPLR_BLUETOOTH_ERROR;
        btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
    } else {
        nimble_port_init();

        // Initialize the NimBLE Host configuration
        // Bluetooth device name for advertisement
        intRet = ble_svc_gap_device_name_set(btClient->configuration.deviceName);
        if (!intRet) {
            ble_svc_gap_init();
            ble_svc_gatt_init();
            intRet = ble_gatts_count_cfg(bleGattSvcs);
            if (!intRet) {
                intRet = ble_gatts_add_svcs(bleGattSvcs);
                if (!intRet) {
                    ble_hs_cfg.sync_cb = bleAppOnSyncCb;

                    // Create NimBLE thread
                    nimble_port_freertos_init(bleTask);
                    ret = XPLR_BLUETOOTH_OK;
                    btClient->state = XPLR_BLUETOOTH_CONN_STATE_READY;
                } else {
                    ret = XPLR_BLUETOOTH_ERROR;
                    btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
                }
            } else {
                ret = XPLR_BLUETOOTH_ERROR;
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
            }
        } else {
            ret = XPLR_BLUETOOTH_ERROR;
            btClient->state = XPLR_BLUETOOTH_CONN_STATE_ERROR;
        }
    }

    return ret;
}

xplrBluetooth_error_t bleDeInit(void)
{
    esp_err_t espRet;
    int intRet;
    xplrBluetooth_error_t ret;

    intRet = nimble_port_stop();
    if (!intRet) {
        espRet = esp_nimble_hci_and_controller_deinit();
        if (espRet != ESP_OK) {
            XPLRBLUETOOTH_CONSOLE(E, "esp_nimble_hci_and_controller_deinit() failed with error: %d", espRet);
            ret = XPLR_BLUETOOTH_ERROR;
        } else {
            ret = XPLR_BLUETOOTH_OK;
        }
    } else {
        ret = XPLR_BLUETOOTH_ERROR;
    }

    return ret;
}

void bleDisconnectDevice(xplrBluetooth_connected_device_t *dvc)
{
    int intRet;

    intRet = ble_gap_terminate((uint16_t) dvc->handle, BLE_ERR_RD_CONN_TERM_PWROFF);
    if (intRet != 0) {
        XPLRBLUETOOTH_CONSOLE(E,
                              "Couldn't disconnect device with handle: [%d] err %d",
                              dvc->handle, intRet);
    } else {
        //  Do nothing
    }
}

xplrBluetooth_error_t bleWrite(xplrBluetooth_connected_device_t *dvc,
                               char *msg,
                               size_t msgSize)
{
    xplrBluetooth_error_t ret;
    int err, i;
    bool abort = false;
    struct os_mbuf *memoryBuffer;
    int errCount = 0;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        if (msgSize == 0) {
            ret = XPLR_BLUETOOTH_ERROR;
            XPLRBLUETOOTH_CONSOLE(E, "msgSize is 0");
        } else {
            // Split the message into chucks and send
            for (i = 0; i < msgSize; i += BLE_SEND_MTU) {
                while (1) {
                    if (abort) {
                        break;
                    }
                    if ((BLE_SEND_MTU) < (msgSize - i)) {
                        memoryBuffer = ble_hs_mbuf_from_flat(&msg[i], (BLE_SEND_MTU));
                    } else {
                        memoryBuffer = ble_hs_mbuf_from_flat(&msg[i], (msgSize - i));
                    }
                    err = ble_gattc_notify_custom(dvc->handle, bleNotifyCharAttrHandle, memoryBuffer);
                    // If send fails wait for a while and retry
                    if (err == BLE_HS_ENOMEM && errCount++ < 10) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    } else if (err) {
                        XPLRBLUETOOTH_CONSOLE(E, "couldn't send message");
                        abort = true;
                    } else {
                        // If message chunk is sent successfully exit the while loop
                        break;
                    }
                }
            }
            if (i >= msgSize) {
                ret = XPLR_BLUETOOTH_OK;
            } else {
                ret = XPLR_BLUETOOTH_ERROR;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        // Couldn't get semaphore returning error
        ret = XPLR_BLUETOOTH_ERROR;
    }

    return ret;
}

void bleAddDevice(uint32_t handle, ble_addr_t address)
{
    int i;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        for (i = 0; i < XPLRBLUETOOTH_NUMOF_DEVICES; i++) {
            // Find the first free slot
            if (!btClient->devices[i].connected) {
                // Use this slot for the new connected device
                btClient->devices[i].handle = handle;
                btClient->devices[i].connected = true;
                btClient->devices[i].msgAvailable = false;
                memcpy(&(btClient->devices[i].address), &address, 6 * sizeof(uint8_t));
                break;
            }
        }
        xSemaphoreGive(btSemaphore);
    } else {
        XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
    }
}

// Advertising for BLE, when scanning for devices you should see the device_name you gave to your XPLR-HPG board
// when initialising the BT client
void bleAppAdvertise(void)
{
    struct ble_hs_adv_fields fields, fieldsExt;
    char nameShort[6];
    const char *name = ble_svc_gap_device_name();
    struct ble_gap_adv_params advParams;
    int err;

    memset(&fields, 0x00, sizeof(fields));
    memset(&fieldsExt, 0x00, sizeof(fieldsExt));
    memset(&advParams, 0x00, sizeof(advParams));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    memset(nameShort, '\0', sizeof(nameShort));
    strncpy(nameShort, name, sizeof(nameShort) - 1);
    fields.name = (uint8_t *)nameShort;
    fields.name_len = strlen(nameShort);
    if (strlen(name) <= sizeof(nameShort) - 1) {
        fields.name_is_complete = 1;
    } else {
        fields.name_is_complete = 0;
    }

    fields.uuids128_is_complete = 1;
    fields.uuids128 = &xplrBleServiceUuid;
    fields.num_uuids128 = 1;

    err = ble_gap_adv_set_fields(&fields);
    if (err) {
        XPLRBLUETOOTH_CONSOLE(E, "ble_gap_adv_set_fields, err %d", err);
    }

    fieldsExt.flags = fields.flags;
    fieldsExt.name = (uint8_t *)name;
    fieldsExt.name_len = strlen(name);
    fieldsExt.name_is_complete = 1;
    err = ble_gap_adv_rsp_set_fields(&fieldsExt);
    if (err) {
        XPLRBLUETOOTH_CONSOLE(E,
                              "ble_gap_adv_rsp_set_fields fieldsExt, name might be too long, err %d",
                              err);
    }

    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;

    err = ble_gap_adv_start(bleAddr, NULL, BLE_HS_FOREVER, &advParams, bleGapEventCb, NULL);
    if (err == 2) {
        // Do nothing
    } else if (err) {
        XPLRBLUETOOTH_CONSOLE(E, "Advertising start failed: err %d", err);
    } else {
        // Do nothing
    }
}

void bleTask(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed.
    nimble_port_freertos_deinit();
}

void bleDiagnostics(uint32_t handle, bool disconnected)
{
    int intRet, index, i;
    BaseType_t semaphoreRet;

    index = btDeviceHandleToIndex(handle);
    if (index == -1) {
        // Cannot find device not printing anything btDeviceHandleToIndex will print error
    } else {
        semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
        if (semaphoreRet == pdTRUE) {
            if (disconnected) {
                // Don't try to get RSSI since device already disconnected, previous stored RSSI will be printed
            } else {
                for (i = 0; i < 5; i++) {
                    intRet = ble_gap_conn_rssi((uint16_t)(btClient->devices[index].handle),
                                               &(btClient->devices[index].diagnostics.rssi));
                    if (intRet != 0) {
                        XPLRBLUETOOTH_CONSOLE(E, "cannot get RSSI");
                    } else {
                        // Do nothing
                    }
                    vTaskDelay(pdMS_TO_TICKS(25));
                }

            }
            XPLRBLUETOOTH_CONSOLE(I,
                                  "Device handle: %d | Device address: %02x:%02x:%02x:%02x:%02x:%02x | RSSI: %d dBm",
                                  handle,
                                  btClient->devices[index].address.val[0],
                                  btClient->devices[index].address.val[1],
                                  btClient->devices[index].address.val[2],
                                  btClient->devices[index].address.val[3],
                                  btClient->devices[index].address.val[4],
                                  btClient->devices[index].address.val[5],
                                  btClient->devices[index].diagnostics.rssi);
            xSemaphoreGive(btSemaphore);
        } else {
            XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
        }
    }
}

#else
#error "Wrong XPLR_BLUETOOTH mode"
#endif
/* -------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */
#if XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BT_CLASSIC

void BtClassicSPPcb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
    esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;
    BaseType_t semaphoreRet;

    switch (event) {
        case ESP_SPP_INIT_EVT:
            if (param->init.status == ESP_SPP_SUCCESS) {
                esp_spp_start_srv(sec_mask, role_slave, 0, btClient->configuration.deviceName);
            } else {
                XPLRBLUETOOTH_CONSOLE(E, "ESP_SPP_INIT_EVT status:%d", param->init.status);
            }
            break;
        case ESP_SPP_CLOSE_EVT:
            XPLRBLUETOOTH_CONSOLE(I, "Device disconnected");
            btClassicDiagnostics(param->close.handle, true);
            // Remove device from connected device list
            btRemoveDevice(param->close.handle);
            break;
        case ESP_SPP_START_EVT:
            if (param->start.status == ESP_SPP_SUCCESS) {
                esp_bt_dev_set_device_name(btClient->configuration.deviceName);
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            } else {
                XPLRBLUETOOTH_CONSOLE(E, "ESP_SPP_START_EVT status:%d", param->start.status);
            }
            break;
        case ESP_SPP_DATA_IND_EVT:
            // Received SPP data
            btCacheMsg(param->data_ind.handle, param->data_ind.data, param->data_ind.len);
            btSetAvailableMessage(param->data_ind.handle);
            break;
        case ESP_SPP_CONG_EVT:
            XPLRBLUETOOTH_CONSOLE(W, "ESP_SPP_CONG_EVT");
            break;
        case ESP_SPP_SRV_OPEN_EVT:
            XPLRBLUETOOTH_CONSOLE(I, "Device connected");
            // Add new connected device to list of connected devices
            btClassicAddDevice(param->srv_open.handle, param->srv_open.rem_bda);
            btClassicDiagnostics(param->srv_open.handle, false);
            semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
            if (semaphoreRet == pdTRUE) {
                btClient->state = XPLR_BLUETOOTH_CONN_STATE_CONNECTED;
                xSemaphoreGive(btSemaphore);
            } else {
                XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
            }
            break;
        default:
            break;
    }

}

void btClassicGAPcb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    esp_bt_pin_code_t pin_code;
    uint8_t index;
    BaseType_t semaphoreRet;

    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                XPLRBLUETOOTH_CONSOLE(D,
                                      "authentication success: %s bda:[%02x:%02x:%02x:%02x:%02x:%02x]",
                                      param->auth_cmpl.device_name,
                                      param->auth_cmpl.bda[0],
                                      param->auth_cmpl.bda[1],
                                      param->auth_cmpl.bda[2],
                                      param->auth_cmpl.bda[3],
                                      param->auth_cmpl.bda[4],
                                      param->auth_cmpl.bda[5]);
            } else {
                XPLRBLUETOOTH_CONSOLE(E, "authentication failed, status:%d", param->auth_cmpl.stat);
            }
            break;
        case ESP_BT_GAP_PIN_REQ_EVT:
            XPLRBLUETOOTH_CONSOLE(I, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
            if (param->pin_req.min_16_digit) {
                XPLRBLUETOOTH_CONSOLE(I, "Input pin code: 0000 0000 0000 0000");
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
            } else {
                XPLRBLUETOOTH_CONSOLE(I, "Input pin code: 1234");
                pin_code[0] = '1';
                pin_code[1] = '2';
                pin_code[2] = '3';
                pin_code[3] = '4';
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            }
            break;
#if (CONFIG_BT_SSP_ENABLED == true)
        case ESP_BT_GAP_CFM_REQ_EVT:
            XPLRBLUETOOTH_CONSOLE(I,
                                  "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d",
                                  param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            XPLRBLUETOOTH_CONSOLE(I, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
            break;
#endif
        case ESP_BT_GAP_READ_RSSI_DELTA_EVT:
            // Save RSSI to diagnostics struct
            index = btClassicDeviceAddressToIndex(param->read_rssi_delta.bda);
            semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
            if (semaphoreRet == pdTRUE) {
                btClient->devices[index].diagnostics.rssi = param->read_rssi_delta.rssi_delta;
                xSemaphoreGive(btSemaphore);
            } else {
                XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
            }
            break;
        default: {
            break;
        }
    }
}

#elif XPLRBLUETOOTH_MODE == XPLRBLUETOOTH_MODE_BLE

int bleGapEventCb(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int intRet;
    BaseType_t semaphoreRet;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                XPLRBLUETOOTH_CONSOLE(I, "Device connected");
                semaphoreRet = xSemaphoreTake(btSemaphore, XPLR_BLUETOOTH_MAX_DELAY);
                if (semaphoreRet == pdTRUE) {
                    btClient->state = XPLR_BLUETOOTH_CONN_STATE_CONNECTED;
                    xSemaphoreGive(btSemaphore);
                } else {
                    XPLRBLUETOOTH_CONSOLE(W, "Couldn't get semaphore");
                }
                intRet = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (intRet != 0) {
                    XPLRBLUETOOTH_CONSOLE(E, "ble_gap_conn_find error");
                } else {
                    // Do nothing
                }
                bleAddDevice(event->connect.conn_handle, desc.peer_id_addr);
                bleDiagnostics(event->connect.conn_handle, false);
            } else {
                // Do nothing
            }
            bleAppAdvertise();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            XPLRBLUETOOTH_CONSOLE(I, "Device disconnected");
            bleDiagnostics(event->disconnect.conn.conn_handle, true);
            btRemoveDevice(event->disconnect.conn.conn_handle);
            break;
        default:
            break;
    }

    return 0;
}

void bleAppOnSyncCb(void)
{
    int ret = ble_hs_id_infer_auto(0, &bleAddr);
    if (ret != 0) {
        XPLRBLUETOOTH_CONSOLE(E, "Error ble_hs_id_infer_auto: %d", ret);
    }
    bleAppAdvertise();
}

int bleReceiveMsgCb(uint16_t conn_handle,
                    uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt,
                    void *arg)
{
    btCacheMsg(conn_handle, ctxt->om->om_data, ctxt->om->om_len);
    btSetAvailableMessage(conn_handle);
    // int return just for compatibility with the callback type
    // errors are handled by xplr ble client
    return 0;
}

int bleDummyCb(uint16_t conn_handle,
               uint16_t attr_handle,
               struct ble_gatt_access_ctxt *ctxt,
               void *arg)
{
    return 0;
}

#else
#error "Wrong XPLR_BLUETOOTH mode"
#endif

#endif
// End of file
