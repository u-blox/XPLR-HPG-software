/*
 * Copyright 2022 u-blox
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
#ifndef _XPLR_HPG2_C214_BOARD_H_
#define _XPLR_HPG2_C214_BOARD_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "stdbool.h"

/** @file
 * @brief This header file defines basic board level functions.
 * To be used by all examples.
 */

/*Definitions of Board*/
#define BOARD_NAME "XPLR-HPG2-C214"
#define BOARD_VERSION "revC"
#define BOARD_VENDOR "uBlox"
#define BOARD_URL "https://www.u-blox.com/en/product/xplr-hpg-2"

/*Definitions of MCU*/
#define BOARD_MCU_NAME "NINA-W106 (ESP32)"
#define BOARD_MCU_FLASH_SIZE "8MB"
#define BOARD_MCU_RAM_SIZE "520KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definitions of IO*/
#define BOARD_IO_BTN1 0U
#define BOARD_IO_LED 2U

#define BOARD_IO_LTE_PWR_ON 26U

#define BOARD_IO_LTE_ON_nSENSE 37U
#define BOARD_IO_LTE_nRST 33U
#define BOARD_IO_UART_LTE_TX 25U
#define BOARD_IO_UART_LTE_RX 34U
#define BOARD_IO_UART_LTE_CTS 36U
#define BOARD_IO_UART_LTE_RTS 27U
#define BOARD_IO_UART_LTE_DTR 15U

#define BOARD_IO_I2C_PERIPHERALS_SCL 22U
#define BOARD_IO_I2C_PERIPHERALS_SDA 21U

#define BOARD_IO_SPI_SD_SCK 18
#define BOARD_IO_SPI_SD_MOSI 23
#define BOARD_IO_SPI_SD_MISO 19
#define BOARD_IO_SPI_SD_nCS 32
#define BOARD_IO_SD_DETECT 38

#define BOARD_IO_UART_DBG_TX 1U
#define BOARD_IO_UART_DBG_RX 3U

/*Definitions of Peripheral*/
#define BOARD_I2C_PERIPHERALS_MODE I2C_MODE_MASTER
#define BOARD_I2C_PERIPHERALS_SPEED CONFIG_BOARD_I2C_PERIPHERALS_SPEED
#define BOARD_I2C_PERIPHERALS_SCL_PULLUP_EN CONFIG_BOARD_I2C_PERIPHERALS_SCL_PULLUP_EN
#define BOARD_I2C_PERIPHERALS_SDA_PULLUP_EN CONFIG_BOARD_I2C_PERIPHERALS_SDA_PULLUP_EN

#define BOARD_UART_DBG_SPEED CONFIG_BOARD_UART_DBG_SPEED

#define BOARD_UART_LTE_SPEED CONFIG_BOARD_UART_LTE_SPEED
#define BOARD_UART_FLOW_CONTROL CONFIG_BOARD_UART_FLOW_CONTROL

/** Error codes specific to xplrBoard module. */
typedef enum {
    XPLR_BOARD_ERROR = -1,          /**< process returned with errors. */
    XPLR_BOARD_ERROR_OK,            /**< indicates success of returning process. */
} xplr_board_error_t;

/** Info codes to retrieve details for. */
typedef enum xplr_board_info_type {
    XPLR_BOARD_INFO_INVALID = -1,
    XPLR_BOARD_INFO_NAME,            /**< board name as shown in ublox site. */
    XPLR_BOARD_INFO_VERSION,         /**< board hw version. */
    XPLR_BOARD_INFO_VENDOR,          /**< vendor name. */
    XPLR_BOARD_INFO_URL,             /**< ublox url to current board. */
    XPLR_BOARD_INFO_MCU,             /**< MCU model of current board. */
    XPLR_BOARD_INFO_FLASH_SIZE,      /**< Flash size of current board. */
    XPLR_BOARD_INFO_RAM_SIZE,        /**< Total RAM size of current board. */
    XPLR_BOARD_INFO_RAM_USER_SIZE    /**< RAM size available to the user. */
} xplr_board_info_t;

/** Board peripheral to control / take action on. */
typedef enum xplr_board_peripheral_id_type {
    XPLR_PERIPHERAL_NA = -1,
    XPLR_PERIPHERAL_LTE_ID       /**< cellular module available on the board. */
} xplr_board_peripheral_id_t;

/** LED specific commands. */
typedef enum xplr_board_led_mode_type {
    XPLR_BOARD_LED_RESET = -1,
    XPLR_BOARD_LED_OFF,          /**< command for turning LED off. */
    XPLR_BOARD_LED_ON,           /**< command for turning LED on. */
    XPLR_BOARD_LED_TOGGLE        /**< command for toggling a LED. */
} xplr_board_led_mode_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize board components.
 *        Initializes board components to their default state.
 *
 * @return  XPLR_BOARD_ERROR_OK on success, XPLR_BOARD_ERROR otherwise.
 */
xplr_board_error_t xplrBoardInit(void);

/**
 * @brief Deinitialize board components.
 *        Resets MCU pins to Hi-Z
 *
 * @return esp_err_t
 */
xplr_board_error_t xplrBoardDeinit(void);

/**
 * @brief Check if board is initialized
 *
 * @return  XPLR_BOARD_ERROR_OK on success, XPLR_BOARD_ERROR otherwise.
 */
bool xplrBoardIsInit(void);

/**
 * @brief Get board information
 *
 * @param id Board info to retrieve.
 * @param info Buffer to store requested info. Make sure to give a well sized buffer.
 */
void xplrBoardGetInfo(xplr_board_info_t id, char *info);

/**
 * @brief Control power to onboard peripherals
 *
 * @param id Peripheral to control power status.
 * @return  XPLR_BOARD_ERROR_OK on success, XPLR_BOARD_ERROR otherwise.
 */
xplr_board_error_t xplrBoardSetPower(xplr_board_peripheral_id_t id, bool on);

/**
 * @brief Set onboard led state
 *
 * @param mode led mode to set (on, off, toggle).
 * @return  XPLR_BOARD_ERROR_OK on success, XPLR_BOARD_ERROR otherwise.
 */
xplr_board_error_t xplrBoardSetLed(xplr_board_led_mode_t mode);

/**
 * @brief Check if the SD card is in the slot
 *
 * @return XPLR_BOARD_ERROR_OK on success, XPLR_BOARD_ERROR otherwise.
*/
xplr_board_error_t xplrBoardDetectSd(void);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_HPG2_C214_BOARD_H_ */