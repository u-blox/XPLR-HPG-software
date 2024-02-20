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

/*
 * An example for board initialization and info printing
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig by choosing the appropriate board from the menu,
 * uses boards component to initialize the devkit and display information
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_task_wdt.h"

#if defined(CONFIG_BOARD_XPLR_HPG2_C214)
#define XPLR_BOARD_SELECTED_IS_C214
#elif defined(CONFIG_BOARD_XPLR_HPG1_C213)
#define XPLR_BOARD_SELECTED_IS_C213
#elif defined(CONFIG_BOARD_MAZGCH_HPG_SOLUTION)
#define XPLR_BOARD_SELECTED_IS_MAZGCH
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/*
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR          (0x42)

/*
 * LBAND I2C address in hex
 */
#define APP_LBAND_I2C_ADDR         (0x43)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * Location modules configs
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static xplrLbandDeviceCfg_t dvcLbandConfig;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;

/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;

/*
 * Simple char buffer to print info
 */
char buff_to_print[64] = {0};
char cellModel[64] = {0};
char cellFw[64] = {0};
char cellImei[64] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
static void configCellSettings(xplrCom_cell_config_t *cfg);

void app_main(void)
{
    esp_err_t ret;
    xplrCom_error_t xplrComErr;
    /*
     * Initialize HPG-XPLR-HPG kit using its board file
     */
    xplrBoardInit();

    /*
     * Check that board has been initialized
     */
    if (xplrBoardIsInit()) {
        printf("XPLR-HPG kit has already initialized. \n");
        XPLR_CI_CONSOLE(9901, "OK");
    } else {
        printf("XPLR-HPG kit has not been initialized. \n");
        XPLR_CI_CONSOLE(9901, "ERROR");
    }

    /*
     * Config GNSS and LBAND modules

     */
    ret = xplrGnssUbxlibInit();
    if (ret != ESP_OK) {
        XPLR_CI_CONSOLE(9902, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9902, "OK");
    }

    printf("Waiting for GNSS device to come online!\n");
    appConfigGnssSettings(&dvcGnssConfig);
    ret = xplrGnssStartDevice(0, &dvcGnssConfig);
    if (ret != ESP_OK) {
        printf("GNSS device config failed!\n");
        XPLR_CI_CONSOLE(9903, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9903, "OK");
    }

    gnssState = xplrGnssGetCurrentState(0);
    while (gnssState != XPLR_GNSS_STATE_DEVICE_READY) {
        xplrGnssFsm(0);
        gnssState = xplrGnssGetCurrentState(0);
    }

    printf("Waiting for LBAND device to come online!\n");
    appConfigLbandSettings(&dvcLbandConfig);
    ret = xplrLbandStartDevice(0, &dvcLbandConfig);
    if (ret != ESP_OK) {
        printf("LBAND device config failed!\n");
        XPLR_CI_CONSOLE(9904, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9904, "OK");
    }

    /*
     * Config Cell module
     */
    configCellSettings(&cellConfig); /* Setup configuration parameters for hpg com */
    xplrComCellInit(&cellConfig); /* Initialize hpg com */
    xplrComErr = xplrComCellFsmConnect(cellConfig.profileIndex);
    if (xplrComErr != XPLR_COM_OK) {
        XPLR_CI_CONSOLE(9905, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9905, "OK");
    }

    /*
     * Print board info
     */
    printf("\nXPLR-HPG kit Info\n");
    xplrBoardGetInfo(XPLR_BOARD_INFO_NAME, buff_to_print);
    printf("Board Info Name: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_VERSION, buff_to_print);
    printf("Board Info HW Version: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_VENDOR, buff_to_print);
    printf("Board Info Vendor: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_URL, buff_to_print);
    printf("Board Info Url: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_MCU, buff_to_print);
    printf("Board Info MCU: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_FLASH_SIZE, buff_to_print);
    printf("Board Info Flash Size: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_RAM_SIZE, buff_to_print);
    printf("Board Info RAM Size: %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrBoardGetInfo(XPLR_BOARD_INFO_RAM_USER_SIZE, buff_to_print);
    printf("Board Info RAM Size (user): %s \n", buff_to_print);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    xplrGetDeviceMac((uint8_t *)buff_to_print);
    printf("Board WiFi MAC Address: %02X:%02X:%02X:%02X:%02X:%02X \n",
           buff_to_print[0],
           buff_to_print[1],
           buff_to_print[2],
           buff_to_print[3],
           buff_to_print[4],
           buff_to_print[5]);
    memset(buff_to_print, 0x00, strlen(buff_to_print));

    if (xplrGnssPrintDeviceInfo(0) != ESP_OK) {
        XPLR_CI_CONSOLE(9906, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9906, "OK");
    }

    if (xplrLbandPrintDeviceInfo(0) != ESP_OK) {
        XPLR_CI_CONSOLE(9907, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9907, "OK");
    }

    if (xplrComCellGetDeviceInfo(cellConfig.profileIndex, cellModel, cellFw, cellImei) != XPLR_COM_OK) {
        XPLR_CI_CONSOLE(9908, "ERROR");
    } else {
        XPLR_CI_CONSOLE(9908, "OK");
    }

    printf("Cell Info:\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Model: %s \n", cellModel);
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Fw: %s \n", cellFw);
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("IMEI: %s \n", cellImei);

    xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);

#if 0 /* re-enable after upgrading to v4.4 */
    /* Print chip information form idf core */
    printf("Board Info (extended):\n");
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%uMB %s flash\n", flash_size / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
#endif

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * Populates gnss settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
{
    gnssCfg->hw.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    gnssCfg->hw.dvcType = (xplrLocDeviceType_t)CONFIG_GNSS_MODULE;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_GNSS_I2C_ADDR;
    gnssCfg->hw.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    gnssCfg->hw.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    gnssCfg->hw.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    gnssCfg->hw.dvcNetwork.devicePinPwr = -1;
    gnssCfg->hw.dvcNetwork.devicePinDataReady = -1;

    // Disable DR for this example
    gnssCfg->dr.enable = 0;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_LBAND;
}

/**
 * Populates lband settings
 */
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg)
{
    /**
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case an lband
    * module connected via I2C
    */
    lbandCfg->hwConf.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_LBAND_I2C_ADDR;
    lbandCfg->hwConf.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    lbandCfg->hwConf.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    lbandCfg->hwConf.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    lbandCfg->hwConf.dvcNetwork.devicePinPwr = -1;
    lbandCfg->hwConf.dvcNetwork.devicePinDataReady = -1;

    lbandCfg->destHandler = NULL;

    lbandCfg->corrDataConf.freq = 0;
    lbandCfg->corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
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
    //cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN; /* configured using kconfig */
    cfg->netSettings->timeoutSeconds = 240; /* Connection timeout in seconds */
    cfg->mno = 90;

    cfg->ratList[0] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[1] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[2] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    cfg->bandList[0] = 0;
    cfg->bandList[1] = 0;
    cfg->bandList[2] = 0;
    cfg->bandList[3] = 0;
    cfg->bandList[4] = 0;
    cfg->bandList[5] = 0;
}
