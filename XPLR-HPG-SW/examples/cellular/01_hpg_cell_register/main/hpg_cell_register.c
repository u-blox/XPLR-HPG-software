/*
 * Copyright 2023 u-blox
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

/* An example for demonstration of the configuration of the LARA R6 cellular module το register to a network provider.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * registers to a network provider using the xplr_com component
 * and then shuts down
 *
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "./../../../components/ubxlib/ubxlib.h"
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
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "xplr_log.h"
#include "xplr_common.h"

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */


/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 0 for disabling */
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0) XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)\
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Logging configuration macros
*/
#if (1 == APP_SD_LOGGING_ENABLED)
static char appLogFilename[] =
    "/APPLOG.TXT";               /**< Follow the same format if changing the filename*/
static char errorLogFilename[] =
    "/ERRORLOG.TXT";           /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize =
    100;                        /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType =
    XPLR_SIZE_MB;    /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* variable holding current state of xplrComCellFsmConnect() */
static xplrCom_error_t hpgComRes = XPLR_COM_ERROR;

/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;

/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;

#if (1 == APP_SD_LOGGING_ENABLED)
/* static log configuration struct*/
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_SMALL];
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* configures cell settings. Needs to be called once, before calling the xplrComCellFsmConnect() function*/
static void configCellSettings(xplrCom_cell_config_t *cfg);
static void appInitLog(void);
#if (1 == APP_SD_LOGGING_ENABLED)
static void appDeInitLog(void);
#endif

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{

    /* Initialize logging first to catch any potential errors*/
    appInitLog();

    APP_CONSOLE(I, "XPLR-HPG kit Demo: CELL Register\n");

    /* Initialize XPLR-HPG kit using its board file */
    xplrBoardInit();

    bool appFinished = false;

    hpgComRes = xplrUbxlibInit(); /* Initialise the ubxlib APIs we will need */
    if (hpgComRes == XPLR_COM_OK) {
        configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
        hpgComRes = xplrComCellInit(&cellConfig); /* Initialize hpg com */
    } else {
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", hpgComRes);
    }

    while (1) {
        /* xplrComCellFsmConnect() needs to be polled in order to keep hpg com service running */
        hpgComRes = xplrComCellFsmConnect(cellConfig.profileIndex);

        /* calling xplrComCellFsmConnectGetState() will return the latest state of xplrComCellFsmConnect().
         * we can use this state to update our application accordingly. */
        if (!appFinished) {
            switch (xplrComCellFsmConnectGetState(cellConfig.profileIndex)) {
                case XPLR_COM_CELL_CONNECTED:
                    APP_CONSOLE(I, "Cell module is Online.");
                    appFinished = true;
                    APP_CONSOLE(I, "App finished.");
                    /* quick blink 5 times*/
                    for (int i = 0; i < 5; i++) {
                        xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                        vTaskDelay(250 / portTICK_PERIOD_MS);
                    }
                    xplrBoardSetLed(XPLR_BOARD_LED_ON);
                    break;
                case XPLR_COM_CELL_CONNECT_TIMEOUT:
                case XPLR_COM_CELL_CONNECT_ERROR:
                    APP_CONSOLE(W, "Cell module is Offline.");
                    appFinished = true;
                    APP_CONSOLE(E, "App finished with errors.");
                    /* slow blink 5 times*/
                    for (int i = 0; i < 5; i++) {
                        xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                    xplrBoardSetLed(XPLR_BOARD_LED_ON);
                    break;

                default:
                    appFinished = false;
                    break;
            }
        }
#if (1 == APP_SD_LOGGING_ENABLED)
        if (appFinished && appLog.logEnable && errorLog.logEnable) {
            xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
            appDeInitLog();
        }
#endif
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void configCellSettings(xplrCom_cell_config_t *cfg)
{
    /* Config hardware pins connected to cellular module */
    cfg->hwSettings = &cellHwConfig;
    cfg->comSettings = &cellComConfig;
    cfg->netSettings = &netConfig;

    /*
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case a cellular
    * module connected via UART
    */

    cfg->hwSettings->moduleType = U_CELL_MODULE_TYPE_LARA_R6;
    cfg->hwSettings->pSimPinCode = NULL;
    cfg->hwSettings->pinEnablePower = -1;
    cfg->hwSettings->pinPwrOn = BOARD_IO_LTE_PWR_ON;

    cfg->hwSettings->pinVInt = BOARD_IO_LTE_ON_nSENSE;
    cfg->hwSettings->pinDtrPowerSaving = -1;

    cfg->comSettings->uart = 1;
    cfg->comSettings->baudRate = U_CELL_UART_BAUD_RATE;
    cfg->comSettings->pinTxd = BOARD_IO_UART_LTE_TX;
    cfg->comSettings->pinRxd = BOARD_IO_UART_LTE_RX;
    cfg->comSettings->pinCts = BOARD_IO_UART_LTE_CTS;
    cfg->comSettings->pinRts = BOARD_IO_UART_LTE_RTS;

    cfg->netSettings->type = U_NETWORK_TYPE_CELL;
    cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN; /* configured using kconfig */
    cfg->netSettings->timeoutSeconds = 240; /* Connection timeout in seconds */
    cfg->mno = 100;

    cfg->ratList[0] = U_CELL_NET_RAT_LTE;
    cfg->ratList[1] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[2] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    cfg->bandList[0] = 0;
    cfg->bandList[1] = 0;
    cfg->bandList[2] = 0;
    cfg->bandList[3] = 0;
    cfg->bandList[4] = 0;
    cfg->bandList[5] = 0;
}

static void appInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLog_error_t err = xplrLogInit(&errorLog, XPLR_LOG_DEVICE_ERROR, errorLogFilename,
                                      logFileMaxSize, logFileMaxSizeType);
    if (err == XPLR_LOG_OK) {
        errorLog.logEnable = true;
        err = xplrLogInit(&appLog, XPLR_LOG_DEVICE_INFO, appLogFilename, logFileMaxSize,
                          logFileMaxSizeType);
    }
    if (err == XPLR_LOG_OK) {
        appLog.logEnable = true;
    } else {
        APP_CONSOLE(E, "Error initializing logging...");
    }
#endif
}

#if (1 == APP_SD_LOGGING_ENABLED)
static void appDeInitLog(void)
{
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
}
#endif