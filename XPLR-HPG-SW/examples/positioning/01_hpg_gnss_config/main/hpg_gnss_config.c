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
 * An example for MQTT connection to Thingstream (U-blox broker).
 *
 * In the current example U-blox XPLR-HPG-2 kit with host NINA-W106 (esp32 based),
 * is initialized using boards component,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream and subscribes to PointPerfect correction data topic, using hpg_mqtt component.
 *
 *  Thingstream MQTT address, device ID and topic to subscribe are defined into hpg_mqtt.h of hpg_mqtt component.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "u_cfg_app_platform_specific.h"
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
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/**
 * If paths not found in VScode: 
 *      press keys --> <ctrl+shift+p> 
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define  APP_SERIAL_DEBUG_ENABLED 1U /* used to print debug messages in console. Set to 0 for disabling */
#if defined (APP_SERIAL_DEBUG_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)   esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * These values are given as a starting point
 * to show the functionality of xplrLbandSetFrequency
 * These value could/will change in time so the best way to
 * get the correct frequency values is to use MQTT
 * You can refer to example positioning/02_hpg_gnss_lband_correction
 * on how to do so
 */
#define APP_LBAND_FREQUENCY_EU     1545260000
#define APP_LBAND_FREQUENCY_US     1556290000

/**
 * I2C addresses for location devices
 */
#define XPLR_GNSS_I2C_ADDR  0x42
#define XPLR_LBAND_I2C_ADDR 0x43

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
/**
 * GNSS configuration.
 * This is an example for a GNSS ZED-F9 module.
 * The same structure can be used with an LBAND NEO-D9S module.
 * Depending on your device and board you might have to change these values.
 */
static xplrGnssDeviceCfg_t gnssCfg = {
    .dvcSettings =  {
        .deviceType = U_DEVICE_TYPE_GNSS,
        .deviceCfg = {
            .cfgGnss = {
                .moduleType = 1,        /**< Module type, 1 for M9 devices */
                .pinEnablePower = -1,   /**< -1 if unused */
                .pinDataReady = -1,     /**< -1 if unused */
                .i2cAddress = XPLR_GNSS_I2C_ADDR      /**< I2C address value */
            },
        },
        .transportType = U_DEVICE_TRANSPORT_TYPE_I2C,
        .transportCfg = {
            .cfgI2c = {
                .i2c = 0,       /**< ESP-IDF port number for I2C */
                .pinSda = BOARD_IO_I2C_PERIPHERALS_SDA,   /**< SDA pin value */
                .pinScl = BOARD_IO_I2C_PERIPHERALS_SCL,   /**< SCL pin value */
                .clockHertz = 400000    /**< Clock value in Hertz */
            },
        },
    },

    .dvcNetwork = {
        .type = U_NETWORK_TYPE_GNSS,            /**< Network type */
        .moduleType = U_GNSS_MODULE_TYPE_M9,    /**< Module type family */
        .devicePinPwr = -1,                     /**< -1 if power pin is not used */
        .devicePinDataReady = -1                /**< -1 if data ready pin is not used */
    }
};

/**
 * Data storages for reading options values
 */
static uint8_t  dataU8;
static uint16_t dataU16;
static uint32_t frequency;

/**
 * Stores data from multi get configurations
 * REMEMBER TO DEALLOCATE THIS POINTER AFTER YOU ARE DONE
 */
uGnssCfgVal_t *pReply;

/**
 * Some values to set and read.
 * Essentially we are enabling High precision GNSS data
 * and output of their message into the I2C bus.
 */
static const uGnssCfgVal_t pGnssOpts[] = {
    {U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1, 1}
};

/**
 * A struct with key values we want to read back
 * We are reading the same values as the ones we setup above
 */
const uint32_t pKeyVals[] = {
    U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L,
    U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1,
};

esp_err_t espRet;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t appInitAll(void);
static esp_err_t appPrintDeviceInfos(void);
static esp_err_t appCloseAllDevices(void);
static void appHaltExecution(void);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */

void app_main(void)
{
    espRet = appInitAll();
    if (espRet != ESP_OK) {
        appHaltExecution();
    }
    APP_CONSOLE(I,"All inits OK!");

    espRet = appPrintDeviceInfos();
    if (espRet != ESP_OK) {
        appHaltExecution();
    }
    APP_CONSOLE(I,"All infos OK!");

    /**
     * Sets the frequency for LBAND
     * Calls xplrLbandOptionSingleValSet internally
     */
    espRet = xplrLbandSetFrequency(0, APP_LBAND_FREQUENCY_EU);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to set LBAND frequency!");
        appHaltExecution();
    }

    /**
     * Reads stored frequency in LBAND frequency
     * Calls xplrLbandOptionSingleValGet internally
     */
    frequency = xplrLbandGetFrequency(0);
    if (frequency == 0) {
        APP_CONSOLE(W, "Frequency is not set");
    } else {
        APP_CONSOLE(I, "Stored frequency: %d Hz", frequency);
    }

    /**
     * Setting multiple values at once as a list.
     * By checking the declaration of pGnssOpts we can 
     * see how to setup a list of (key,val) pairs for 
     * the multival set function.
     */
    espRet = xplrGnssOptionMultiValSet(0, 
                                       pGnssOpts, 
                                       ELEMENTCNT(pGnssOpts),
                                       U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to set list val!");
        appHaltExecution();
    }

    /**
     * There's a list of available layers to choose from
     * You can choose where from/to you can read/write values.
     * Be careful: you can write values to U_GNSS_CFG_VAL_LAYER_DEFAULT
     * 
     * U_GNSS_CFG_VAL_LAYER_NONE    --> Store nowhere
     * U_GNSS_CFG_VAL_LAYER_RAM     --> the currently active value, stored non-persistently in RAM
     * U_GNSS_CFG_VAL_LAYER_BBRAM   --> the value stored in battery-backed RAM.
     * U_GNSS_CFG_VAL_LAYER_FLASH   --> the value stored in external configuration flash connected to the GNSS chip
     * U_GNSS_CFG_VAL_LAYER_DEFAULT --> default value stored on EEPROM (persists)
     */

    /**
     * NOTE: data length is denoted by the suffix of the key name e.g.
     *      U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L
     * Has a size of 1 byte hence the use of an uint8_t
     * It's strongly recommended to read the comment at
     * xplr_gnss_types.h or check the integration manual of your module
     * to check what is the correct size of variables
     */
    espRet = xplrGnssOptionSingleValGet(0, U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, &dataU8, sizeof(uint8_t), U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to read data!");
        appHaltExecution();
    }
    APP_CONSOLE(I,"Read one value -- U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val %d", dataU8);

    /**
     * Change the key value to something else
     * Setting a single value
     */
    espRet = xplrGnssOptionSingleValSet(0, U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, 0, U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to read data!");
        appHaltExecution();
    }

    espRet = xplrGnssOptionSingleValGet(0, U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, &dataU8, sizeof(uint8_t), U_GNSS_CFG_VAL_LAYER_DEFAULT);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to read data!");
        appHaltExecution();
    }
    APP_CONSOLE(I,"Read one value -- Default U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val %d", dataU8);

    /**
     * Here U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2
     * has a size of 2 bytes hence the use of an uint16_t
     * It's strongly recommended to read the comment at
     * xplr_gnss_types.h or check the integration manual of your module
     * to check what is the correct size of variables
     */
    espRet = xplrLbandOptionSingleValGet(0, U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2, &dataU16, sizeof(uint16_t), U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to read single option!");
        appHaltExecution();
    }
    APP_CONSOLE(I,"Read one value -- U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2: val %d", dataU8);

    /**
     * Reading multiple values
     * We need to provide a list with the key we want to read as in pKeyVals
     * Next we need to provide a pointer to store the results as in pReply. 
     * Care must be taken here for 2 reasons:
     * - First we need to pass a double pointer, hence the &pReply
     * - IMPORTANT: this is a dynamically allocated pointer. After we are done
     *              using the pointer we must free the allocated space --> line 284
     */
    espRet = xplrGnssOptionMultiValGet(0, pKeyVals, ELEMENTCNT(pKeyVals), &pReply, U_GNSS_CFG_VAL_LAYER_RAM);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to read multiple options with error code [%d]!", espRet);
        appHaltExecution();
    }
    
    /**
     * Printing the values that we passed with the list pKeyVals
     */
    APP_CONSOLE(I,"KEY: U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L | val: %llu", pReply[0].value);
    APP_CONSOLE(I,"KEY: U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1 | val: %llu", pReply[1].value);

    /**
     * CAUTION
     * You MUST free your pointer after you are done using it
     */
    free(pReply);

    espRet = appCloseAllDevices();
    if (espRet != ESP_OK) {
        appHaltExecution();
    }
    APP_CONSOLE(I,"All devices stopped!");

    APP_CONSOLE(I,"ALL DONE");
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * Makes all required initializations
 */
static esp_err_t appInitAll(void)
{
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Board init failed!");
        return espRet;
    }

    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"UbxLib init failed!");
        return espRet;
    }

    APP_CONSOLE(I,"Waiting for GNSS device to come online!");
    espRet = xplrGnssStartDevice(0, &gnssCfg);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"GNSS device config failed!");
        return espRet;
    }

    APP_CONSOLE(I,"Waiting for LBAND device to come online!");
    espRet = xplrLbandStartDeviceDefaultSettings(0, 
                                                 XPLR_LBAND_I2C_ADDR, 
                                                 xplrGnssGetHandler(0));
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"LBAND device config failed!");
        return espRet;
    }

    return ESP_OK;
}

/**
 * Prints some info for the initialized devices
 */
static esp_err_t appPrintDeviceInfos(void)
{
    espRet = xplrGnssPrintDeviceInfo(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to print GNSS device info!");
        return espRet;
    }

    espRet = xplrLbandPrintDeviceInfo(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to print LBAND device info!");
        return espRet;
    }

    return ESP_OK;
}

/**
 * Closes/deInits all devices and UbxLib
 */
esp_err_t appCloseAllDevices(void)
{

    espRet = xplrLbandStopDevice(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to close LBAND device!");
        return ESP_FAIL;
    }

    espRet = xplrGnssStopDevice(0);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to close GNSS device!");
        return ESP_FAIL;
    }

    espRet = xplrGnssUbxlibDeinit();
     if (espRet != ESP_OK) {
        APP_CONSOLE(E,"Failed to deInit UbxLib!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * On error halt execution
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}