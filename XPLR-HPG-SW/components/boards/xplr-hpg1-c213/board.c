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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "./../../hpglib/xplr_hpglib_cfg.h"
#include "board.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLR_BOARD_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLR_BOARD_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrBoard", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLR_BOARD_CONSOLE(message, ...) do{} while(0)
#endif

#define BOARD_CHECK(a, str, ret) if(!(a)) { \
        XPLR_BOARD_CONSOLE(E,"%s", str); \
        return (ret); \
    }

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

typedef enum board_gpio_config_type {
    BOARD_GPIO_CONFIG_INVALID = -1,
    BOARD_GPIO_CONFIG_LTE,
    BOARD_GPIO_CONFIG_LEDS,
    BOARD_GPIO_CONFIG_BTNS,
    BOARD_GPIO_CONFIG_SD_CD
} board_gpio_config_t;

typedef struct board_details_type {
    const char *name;
    const char *version;
    const char *vendor;
    const char *url;
    const char *mcu;
    const char *flash;
    const char *ram;
    const char *ram_user;
} board_details_t;

static bool board_is_init = false;
board_details_t board_info = {
    .name = BOARD_NAME,
    .version = BOARD_VERSION,
    .vendor = BOARD_VENDOR,
    .url = BOARD_URL,
    .mcu = BOARD_MCU_NAME,
    .flash = BOARD_MCU_FLASH_SIZE,
    .ram = BOARD_MCU_RAM_SIZE,
    .ram_user = BOARD_MCU_RAM_USER_SIZE
};

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplr_board_error_t board_config_default_gpios(board_gpio_config_t gpio_id);
static xplr_board_error_t board_deconfig_default_gpios(board_gpio_config_t gpio_id);
static void board_recover_I2c_bus(void);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplr_board_error_t xplrBoardInit(void)
{
    esp_err_t err[3];
    xplr_board_error_t ret;

    err[0] = board_config_default_gpios(BOARD_GPIO_CONFIG_LEDS);
    err[1] = board_config_default_gpios(BOARD_GPIO_CONFIG_LTE);
    err[2] = board_config_default_gpios(BOARD_GPIO_CONFIG_SD_CD);
    board_recover_I2c_bus();
    for (int i = 0; i < 3; i++) {
        if (err[i] != ESP_OK) {
            ret = XPLR_BOARD_ERROR;
            break;
        } else {
            ret = XPLR_BOARD_ERROR_OK;
        }
    }
    board_is_init = true;
    XPLR_BOARD_CONSOLE(D, "Board init Done");

    return ret;
}

xplr_board_error_t xplrBoardDeinit(void)
{
    esp_err_t err[2];
    xplr_board_error_t ret;

    err[0] = board_deconfig_default_gpios(BOARD_GPIO_CONFIG_LEDS);
    err[1] = board_deconfig_default_gpios(BOARD_GPIO_CONFIG_LTE);
    for (int i = 0; i < 2; i++) {
        if (err[i] != ESP_OK) {
            ret = XPLR_BOARD_ERROR;
            break;
        } else {
            ret = XPLR_BOARD_ERROR_OK;
        }
    }
    board_is_init = false;
    XPLR_BOARD_CONSOLE(D, "Board de-init Done");

    return ret;
}

bool xplrBoardIsInit(void)
{
    return board_is_init;
}

void xplrBoardGetInfo(xplr_board_info_t id, char *info)
{

    switch (id) {
        case XPLR_BOARD_INFO_NAME:
            memcpy(info, board_info.name, strlen(board_info.name));
            break;
        case XPLR_BOARD_INFO_VERSION:
            memcpy(info, board_info.version, strlen(board_info.version));
            break;
        case XPLR_BOARD_INFO_VENDOR:
            memcpy(info, board_info.vendor, strlen(board_info.vendor));
            break;
        case XPLR_BOARD_INFO_URL:
            memcpy(info, board_info.url, strlen(board_info.url));
            break;
        case XPLR_BOARD_INFO_MCU:
            memcpy(info, board_info.mcu, strlen(board_info.mcu));
            break;
        case XPLR_BOARD_INFO_FLASH_SIZE:
            memcpy(info, board_info.flash, strlen(board_info.flash));
            break;
        case XPLR_BOARD_INFO_RAM_SIZE:
            memcpy(info, board_info.ram, strlen(board_info.ram));
            break;
        case XPLR_BOARD_INFO_RAM_USER_SIZE:
            memcpy(info, board_info.ram_user, strlen(board_info.ram_user));
            break;
        default:
            break;
    }
}

xplr_board_error_t xplrBoardSetPower(xplr_board_peripheral_id_t id, bool on)
{
    esp_err_t err[3];
    xplr_board_error_t ret;

    switch (id) {
        case XPLR_PERIPHERAL_LTE_ID:
            if (!on) {
                // send power off pulse (>3100ms)
                err[0] = gpio_set_level(BOARD_IO_LTE_PWR_ON, 1);
                vTaskDelay(1600 / portTICK_PERIOD_MS);
                err[1] = gpio_set_level(BOARD_IO_LTE_PWR_ON, 0);
                for (int i = 0; i < 2; i++) {
                    if (err[i] != ESP_OK) {
                        ret = XPLR_BOARD_ERROR;
                        break;
                    } else {
                        ret = XPLR_BOARD_ERROR_OK;
                    }
                }
                BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LTE power off seq failed", ret);
            } else {
                // send power on pulse (>150ms)
                err[0] = gpio_set_level(BOARD_IO_LTE_PWR_ON, 1);
                vTaskDelay(155 / portTICK_PERIOD_MS);
                err[1] = gpio_set_level(BOARD_IO_LTE_PWR_ON, 0);
                for (int i = 0; i < 2; i++) {
                    if (err[i] != ESP_OK) {
                        ret = XPLR_BOARD_ERROR;
                        break;
                    } else {
                        ret = XPLR_BOARD_ERROR_OK;
                    }
                }
                BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LTE power on seq failed", ret);
            }
            break;

        default:
            ret = XPLR_BOARD_ERROR;
            break;
    }

    return ret;
}

xplr_board_error_t xplrBoardSetLed(xplr_board_led_mode_t mode)
{
    esp_err_t err;
    xplr_board_error_t ret;
    static int lastState = 1;

    switch (mode) {
        case XPLR_BOARD_LED_OFF:
            err = gpio_set_level(BOARD_IO_LED, 0);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LED Off failed", ret);
            break;
        case XPLR_BOARD_LED_ON:
            err = gpio_set_level(BOARD_IO_LED, 1);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LED On failed", ret);
            break;
        case XPLR_BOARD_LED_TOGGLE:
            lastState = !lastState;
            err = gpio_set_level(BOARD_IO_LED, lastState);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LED Toggle failed", ret);
            break;

        default:
            ret = XPLR_BOARD_ERROR;
            break;
    }

    return ret;
}

xplr_board_error_t xplrBoardDetectSd(void)
{
    xplr_board_error_t ret;
    /*Check if Card Detect Pin is Low */
    gpio_set_direction(BOARD_IO_SPI_SD_nCS, GPIO_MODE_INPUT_OUTPUT);
    int lvl = gpio_get_level(BOARD_IO_SPI_SD_nCS);
    if (lvl == 0) {
        gpio_set_level(BOARD_IO_SPI_SD_nCS, 1);
        lvl = gpio_get_level(BOARD_IO_SPI_SD_nCS);
        if (lvl == 0) {
            ret = XPLR_BOARD_ERROR;
        } else {
            ret = XPLR_BOARD_ERROR_OK;
        }
        gpio_set_level(BOARD_IO_SPI_SD_nCS, 0);
    } else {
        ret = XPLR_BOARD_ERROR_OK;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static xplr_board_error_t board_config_default_gpios(board_gpio_config_t gpio_id)
{
    esp_err_t err;
    esp_err_t ret;
    gpio_config_t io_conf = {};

    switch (gpio_id) {
        case BOARD_GPIO_CONFIG_LEDS:
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = ((1ULL << BOARD_IO_LED_RED) |
                                    (1ULL << BOARD_IO_LED_GREEN) |
                                    (1ULL << BOARD_IO_LED_BLUE));
            io_conf.pull_down_en = 0;
            io_conf.pull_up_en = 0;
            err = gpio_config(&io_conf);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LED pin config failed", ret);
            err = gpio_set_level(BOARD_IO_LED, 1);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LED Green On failed", ret);
            break;
        case BOARD_GPIO_CONFIG_BTNS:
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = ((1ULL << BOARD_IO_BTN1) |
                                    (1ULL << BOARD_IO_BTN2));
            io_conf.pull_down_en = 0;
            io_conf.pull_up_en = 1;
            err = gpio_config(&io_conf);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "Btn pins config failed", ret);
            break;
        case BOARD_GPIO_CONFIG_LTE:
#if (CONFIG_CELLULAR_MB == 1) || (CONFIG_CELLULAR_MB == 2)
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << BOARD_IO_LTE_PWR_ON);
            io_conf.pull_down_en = 0;
            io_conf.pull_up_en = 0;
            err = gpio_config(&io_conf);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "LTE power pins config failed", ret);
#else
            ret == ESP_FAIL;
            BOARD_CHECK(ret == ESP_OK, "LTE Not Set in mikroBus.", ret);
#endif
            break;
        case BOARD_GPIO_CONFIG_SD_CD:
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << BOARD_IO_SPI_SD_nCS);
            io_conf.pull_up_en = 1;
            io_conf.pull_down_en = 0;
            err = gpio_config(&io_conf);
            if (err != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "SD Card detect pin config failed", ret);
            break;
        default:
            ret = XPLR_BOARD_ERROR;
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "Config resource not found", ret);
            break;
    }

    return ret;
}

static xplr_board_error_t board_deconfig_default_gpios(board_gpio_config_t gpio_id)
{
    esp_err_t err[2];
    xplr_board_error_t ret;
    gpio_config_t io_conf = {};

    switch (gpio_id) {
        case BOARD_GPIO_CONFIG_LEDS:
            err[0] = gpio_reset_pin(BOARD_IO_LED);
            if (err[0] != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            XPLR_BOARD_CONSOLE(D, "LED pin deconfigured.");
            break;
        case BOARD_GPIO_CONFIG_LTE:
            err[0] = gpio_reset_pin(BOARD_IO_LTE_PWR_ON);
            err[1] = gpio_reset_pin(BOARD_IO_LTE_nRST);
            for (int i = 0; i < 2; i++) {
                if (err[i] != ESP_OK) {
                    ret = XPLR_BOARD_ERROR;
                    XPLR_BOARD_CONSOLE(E, "Lte power pins reset error.");
                    break;
                } else {
                    ret = XPLR_BOARD_ERROR_OK;
                    XPLR_BOARD_CONSOLE(D, "Lte power pins deconfigured.");
                }
            }
            break;
        case BOARD_GPIO_CONFIG_SD_CD:
            gpio_reset_pin(BOARD_IO_SPI_SD_nCS);
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << BOARD_IO_SPI_SD_nCS);
            io_conf.pull_up_en = 0;
            io_conf.pull_down_en = 0;
            err[0] = gpio_config(&io_conf);
            if (err[0] != ESP_OK) {
                ret = XPLR_BOARD_ERROR;
                break;
            } else {
                ret = XPLR_BOARD_ERROR_OK;
            }
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "SD Card detect pin deconfig failed", ret);
            break;
        default:
            ret = XPLR_BOARD_ERROR;
            BOARD_CHECK(ret == XPLR_BOARD_ERROR_OK, "Config resource not found", ret);
            break;
    }

    return ret;
}

static void board_recover_I2c_bus(void)
{
    /* disable and enable peripheral to make sure bus is not blocked */
    for (uint8_t i = 0; i < BOARD_I2C_BUS_RECOVERY_RETRIES; i++) {
        periph_module_disable(PERIPH_I2C0_MODULE);
        vTaskDelay(pdMS_TO_TICKS(5));
        periph_module_enable(PERIPH_I2C0_MODULE);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}