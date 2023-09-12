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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/*
 * Simple char buffer to print info
 */
char buff_to_print[64] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

void app_main(void)
{
    printf("XPLR-HPG kit Demo\n");

    /*
     * Initialize HPG-XPLR-HPG kit using its board file
     */
    xplrBoardInit();

    /*
     * Check that board has been initialized
     */
    if (xplrBoardIsInit()) {
        printf("XPLR-HPG kit has already initialized. \n");
    } else {
        printf("XPLR-HPG kit has not been initialized. \n");
    }

    /*
     * Print board info
     */
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
