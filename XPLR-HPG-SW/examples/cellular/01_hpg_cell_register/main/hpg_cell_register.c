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

#define APP_PRINT_IMU_DATA         0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_PRINT_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/*
 * Simple macros used to set buffer size in bytes
 */
#define KIB                         (1024U)
#define APP_JSON_PAYLOAD_BUF_SIZE   ((6U) * (KIB))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog          : 1;
        uint8_t comLog          : 1;
    } singleLogOpts;
    uint8_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          comLogIndex;
} appLog_t;

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

static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0, // All modules selected to log
    .appLogIndex = -1,
    .comLogIndex = -1
};

static char configData[APP_JSON_PAYLOAD_BUF_SIZE];
/* The name of the configuration file */
static char configFilename[] = "xplr_config.json";
/* Application configuration options */
xplr_cfg_t appOptions;
/* Flag indicating the board is setup by the SD config file */
static bool isConfiguredFromFile = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* configures cell settings. Needs to be called once, before calling the xplrComCellFsmConnect() function*/
static void configCellSettings(xplrCom_cell_config_t *cfg);
#if (1 == APP_SD_LOGGING_ENABLED)
static esp_err_t appInitLogging(void);
static void appDeInitLogging(void);
#endif
static esp_err_t appFetchConfigFromFile(void);
static void appApplyConfigFromFile(void);
static esp_err_t appInitSd(void);
static void appHaltExecution(void);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{
    esp_err_t espErr;

    APP_CONSOLE(I, "XPLR-HPG kit Demo: CELL Register\n");

    /* Initialize XPLR-HPG kit using its board file */
    xplrBoardInit();

    espErr = appFetchConfigFromFile();
    if (espErr == ESP_OK) {
        appApplyConfigFromFile();
    } else {
        APP_CONSOLE(D, "No configuration file found, running on Kconfig configuration");
    }

#if (APP_SD_LOGGING_ENABLED == 1)
    espErr = appInitLogging();
    if (espErr != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif

    bool appFinished = false;

    hpgComRes = xplrUbxlibInit(); /* Initialise the ubxlib APIs we will need */
    if (hpgComRes == XPLR_COM_OK) {
        XPLR_CI_CONSOLE(2101, "OK");
        configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
        hpgComRes = xplrComCellInit(&cellConfig); /* Initialize hpg com */
    } else {
        XPLR_CI_CONSOLE(2101, "ERROR");
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
                    XPLR_CI_CONSOLE(2102, "OK");
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
                    XPLR_CI_CONSOLE(2102, "ERROR");
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
        } else {
            xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
#if (1 == APP_SD_LOGGING_ENABLED)
            appDeInitLogging();
#endif
            appHaltExecution();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplr_cfg_logInstance_t *instance = NULL;

    /* Initialize the SD card */
    if (!xplrSdIsCardInit()) {
        ret = appInitSd();
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.appLogIndex];
                appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                    instance->filename,
                                                    instance->sizeInterval,
                                                    instance->erasePrev);
                instance = NULL;
            } else {
                appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                    "main_app.log",
                                                    XPLRLOG_FILE_SIZE_INTERVAL,
                                                    XPLRLOG_NEW_FILE_ON_BOOT);
            }

            if (appLogCfg.appLogIndex >= 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.comLog == 1) {
            if (isConfiguredFromFile) {
                instance = &appOptions.logCfg.instance[appLogCfg.comLogIndex];
                appLogCfg.comLogIndex = xplrComCellInitLogModule(instance);
                instance = NULL;
            } else {
                appLogCfg.comLogIndex = xplrComCellInitLogModule(NULL);
            }

            if (appLogCfg.comLogIndex >= 0) {
                APP_CONSOLE(D, "COM Cell logging instance initialized");
            }
        }
    }

    return ret;
}

static void appDeInitLogging(void)
{
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr = XPLR_SD_ERROR;

    logErr = xplrLogDisableAll();
    if (logErr != XPLR_LOG_OK) {
        APP_CONSOLE(E, "Error disabling logging");
    } else {
        logErr = xplrLogDeInitAll();
        if (logErr != XPLR_LOG_OK) {
            APP_CONSOLE(E, "Error de-initializing logging");
        } else {
            // Do nothing
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdStopCardDetectTask();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error stopping the the SD card detect task");
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdDeInit();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error de-initializing the SD card");
        }
    } else {
        //Do nothing
    }

    if (logErr == XPLR_LOG_OK && sdErr == XPLR_SD_OK) {
        APP_CONSOLE(I, "Logging service de-initialized successfully");
    }
}
#endif

/**
 * Fetch configuration options from SD card (if existent),
 * otherwise, keep Kconfig values
*/
static esp_err_t appFetchConfigFromFile(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;
    xplr_board_error_t boardErr = xplrBoardDetectSd();

    if (boardErr == XPLR_BOARD_ERROR_OK) {
        ret = appInitSd();
        if (ret == ESP_OK) {
            memset(configData, 0, APP_JSON_PAYLOAD_BUF_SIZE);
            sdErr = xplrSdReadFileString(configFilename, configData, APP_JSON_PAYLOAD_BUF_SIZE);
            if (sdErr == XPLR_SD_OK) {
                ret = xplrParseConfigSettings(configData, &appOptions);
                if (ret == ESP_OK) {
                    APP_CONSOLE(I, "Successfully parsed application and module configuration");
                } else {
                    APP_CONSOLE(E, "Failed to parse application and module configuration from <%s>", configFilename);
                }
            } else {
                APP_CONSOLE(E, "Unable to get configuration from the SD card");
                ret = ESP_FAIL;
            }
        } else {
            // Do nothing
        }
    } else {
        APP_CONSOLE(D, "SD is not mounted. Keeping Kconfig configuration");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * Apply configuration from file
*/
static void appApplyConfigFromFile(void)
{
    xplr_cfg_logInstance_t *instance = NULL;
    /* Applying the options that are relevant to the example */
    /* Logging Settings */
    appLogCfg.logOptions.allLogOpts = 0;
    for (uint8_t i = 0; i < appOptions.logCfg.numOfInstances; i++) {
        instance = &appOptions.logCfg.instance[i];
        if (strstr(instance->description, "Application") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.appLog = 1;
                appLogCfg.appLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else if (strstr(instance->description, "COM Cell") != NULL) {
            if (instance->enable) {
                appLogCfg.logOptions.singleLogOpts.comLog = 1;
                appLogCfg.comLogIndex = i;
            } else {
                // Do nothing module not enabled
            }
        } else {
            // Do nothing module not used in example
        }
    }
    /* Options from SD config file applied */
    isConfiguredFromFile = true;
}

/**
 * Initialize SD card
*/
static esp_err_t appInitSd(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;

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

    return ret;
}

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
    if (isConfiguredFromFile) {
        cfg->netSettings->pApn = appOptions.cellCfg.apn;

    } else {
        cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN; /* configured using kconfig */
    }
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

static void appHaltExecution(void)
{
    APP_CONSOLE(W, "App finished halting execution...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
