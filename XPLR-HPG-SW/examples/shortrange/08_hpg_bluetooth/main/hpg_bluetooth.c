/*
 * Copyright 2022 u-blox Ltd
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/bluetooth_service/xplr_bluetooth.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define  APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define  APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
#define  APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_PRINT_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Bluetooth devices print interval
 */
#define APP_DEVICES_PRINT_INTERVAL 10

/**
 * The size of the allocated Bluetooth buffer
 */
#define APP_BT_BUFFER_SIZE XPLRBLUETOOTH_MAX_MSG_SIZE * XPLRBLUETOOTH_NUMOF_DEVICES

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog           : 1;
        uint8_t bluetoothLog     : 1;
    } singleLogOpts;
    uint8_t value;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          bluetoothLogIndex;
} appLog_t;

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

xplrBluetooth_client_t xplrBtClient;
SemaphoreHandle_t btSemaphore;
uint16_t timeNow;
char xplrBluetoothMessageBuffer[APP_BT_BUFFER_SIZE];
esp_err_t espRet;
static uint64_t timePrevDevicesPrint;

/**
 * Static log configuration struct and variables
 */

static appLog_t appLogCfg = {
    .logOptions.value = ~0, // All modules selected to log
    .appLogIndex = -1,
    .bluetoothLogIndex = -1
};

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
#endif
static void appInitBoard(void);
static void appInitBt(void);
static void appPrintConnectedDevices(uint8_t periodSecs);
static void appHaltExecution(void);

void app_main(void)
{
    esp_err_t ret;
    int readLen;
    xplrBluetooth_connected_device_t *device = NULL;

#if (APP_SD_LOGGING_ENABLED == 1)
    ret = appInitLogging();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif
    appInitBoard();
    APP_CONSOLE(I, "XPLR-HPG Bluetooth Echo example");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    appInitBt();

    while (true) {
        switch (xplrBluetoothGetState()) {
            case XPLR_BLUETOOTH_CONN_STATE_CONNECTED:
                // In this state there is a device conencted to your XPLR-HPG board
                // you can send data to the device using the xplrBluetoothWrite function and adressing the device using it connectedDevice struct.
                // If there is a message from the device the state changes to XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE.
                appPrintConnectedDevices(APP_DEVICES_PRINT_INTERVAL);
                break;
            case XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL:
                // In this state you need to ingest the message(s) that is/are queued up to free up space on the asynchronous RX buffer
                // When ingesting messages in this state, the state will change initially to XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE and after all the queued messages
                // are ingested the state will change to XPLR_BLUETOOTH_CONN_STATE_CONNECTED
                APP_CONSOLE(D, "RX buffer is full, reading all uread messages...");
                do {
                    readLen = xplrBluetoothReadFirstAvailableMsg(&device);
                    if (readLen > 0) {
                        APP_CONSOLE(I, "Received message: [%s] from client: [%d]", device->msg, device->handle);
                        APP_CONSOLE(D, "Echoing message back to sender");
                        xplrBluetoothWrite(device, device->msg, readLen);
                    }
                    vTaskDelay(pdMS_TO_TICKS(150));
                } while (xplrBluetoothGetState() == XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE);
                break;
            case XPLR_BLUETOOTH_CONN_STATE_MSG_AVAILABLE:
                // In this state there is a message queued up from a device, you can read this message using xplrBluetoothReadFirstAvailableMsg or
                // xplrBluetoothRead depending on your application.
                // If the message(s) fill up the asynchronous RX buffer the state changes to XPLR_BLUETOOTH_CONN_STATE_RX_BUFFER_FULL, indicating that any
                // new messages received while in this state will be discarded

                // Uncomment this line if you instead want ot use the first connectedDevice in the array
                // readLen = xplrBluetoothRead(&xplrBtClient.devices[0]);
                readLen = xplrBluetoothReadFirstAvailableMsg(&device);
                if (readLen > 0) {
                    APP_CONSOLE(I,
                                "Received message: [%s] from client: [%d]",
                                xplrBtClient.devices[0].msg,
                                xplrBtClient.devices[0].handle);
                    APP_CONSOLE(D, "Echoing message back to sender");
                    xplrBluetoothWrite(&xplrBtClient.devices[0], xplrBtClient.devices[0].msg, readLen);
                }
                break;
            case XPLR_BLUETOOTH_CONN_STATE_READY:
                // After successful initialization the state goes to XPLR_BLUETOOTH_CONN_STATE_READY
                // In this state the board is waiting for a client device (e.g. Android phone) to initialize the conenction
                // after the device has been paired and connected successfully the state changes to XPLR_BLUETOOTH_CONN_STATE_CONNECTED
                APP_CONSOLE(D, "Bluetooth ready to connect...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                break;
            case XPLR_BLUETOOTH_CONN_STATE_BUSY:
                APP_CONSOLE(D, "Bluetooth client busy...");
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case XPLR_BLUETOOTH_CONN_STATE_ERROR:
                APP_CONSOLE(E, "Bluetooth client encountered an error");
                appHaltExecution();
                break;
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/**
 * Initialize logging to the SD card
*/
#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;


    /* Configure the SD card */
    sdErr = xplrSdConfigDefaults();
    if (sdErr != XPLR_SD_OK) {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = ESP_FAIL;
    } else {
        /* Create the card detect task */
        sdErr = xplrSdStartCardDetectTask();
        /* A time window so that the card gets detected*/
        vTaskDelay(pdMS_TO_TICKS(50));
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Failed to start the card detect task");
            ret = ESP_FAIL;
        } else {
            /* Initialize the SD card */
            sdErr = xplrSdInit();
            if (sdErr != XPLR_SD_OK) {
                APP_CONSOLE(E, "Failed to initialize the SD card");
                ret = ESP_FAIL;
            } else {
                APP_CONSOLE(D, "SD card initialized");
                ret = ESP_OK;
            }
        }
    }


    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                "main_app.log",
                                                XPLRLOG_FILE_SIZE_INTERVAL,
                                                XPLRLOG_NEW_FILE_ON_BOOT);
            if (appLogCfg.appLogIndex >= 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.bluetoothLog == 1) {
            appLogCfg.bluetoothLogIndex = xplrBluetoothInitLogModule(NULL);
            if (appLogCfg.bluetoothLogIndex >= 0) {
                APP_CONSOLE(D, "Bluetooth service logging instance initialized");
            }
        }
    }

    return ret;
}
#endif

/*
 * Initialize XPLR-HPG kit using its board file
 */
void appInitBoard(void)
{
    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    }
}

void appInitBt(void)
{
    btSemaphore = xSemaphoreCreateMutex();
    strcpy(xplrBtClient.configuration.deviceName, CONFIG_XPLR_BLUETOOTH_DEVICE_NAME);
    xplrBluetoothInit(&xplrBtClient,
                      btSemaphore,
                      xplrBluetoothMessageBuffer,
                      APP_BT_BUFFER_SIZE);
}

/**
 * Prints connected devices according to period
 */
void appPrintConnectedDevices(uint8_t periodSecs)
{

    if (MICROTOSEC(esp_timer_get_time()) - timePrevDevicesPrint >= periodSecs) {
        xplrBluetoothPrintConnectedDevices();
        timePrevDevicesPrint = MICROTOSEC(esp_timer_get_time());
    }

}

/**
 * A dummy function to pause on error
 */
void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}