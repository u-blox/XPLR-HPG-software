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
#ifndef _XPLR_SD_TYPES_H_
#define _XPLR_SD_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "./../../xplr_hpglib_cfg.h"

/** @file
 * @brief This header file defines the types used in sd card service API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * MACROS FOR DEFAULT SD CONFIGURATION
 * -------------------------------------------------------------- */

/**
 * Maximum timeout for semaphores and mutexes
*/
#define XPLR_SD_MAX_TIMEOUT (TickType_t)pdMS_TO_TICKS(2000)

/**
 * Default mount configuration
*/
#define XPLR_SD_MOUNT_CFG_DEFAULT() {\
        .format_if_mount_failed = true, \
                                  .max_files = 2, \
                                               .allocation_unit_size = (8 * 2 * 1024), \
    }

/**
 * Default SPI bus configuration
*/
#define XPLR_SD_SPI_BUS_CFG_DEFAULT() {\
        .mosi_io_num = SPI_SD_MOSI, \
                       .miso_io_num = SPI_SD_MISO, \
                                      .sclk_io_num = SPI_SD_SCK, \
                                                     .quadwp_io_num = -1, \
                                                                      .quadhd_io_num = -1, \
                                                                                       .max_transfer_sz = 4000, \
    }

/**
 * Default Mount Point of the Filesystem
*/
#define DEFAULT_MOUNT_POINT "/sdcard"

/**
 * System Volume Information filename (to protect from deletion)
*/
#define XPLR_SD_SVI_FILENAME "System Volume Information"

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */
/*INDENT-OFF*/
/** Error codes specific to xplr_sd module. */
typedef enum {
    XPLR_SD_ERROR = -1,                 /**< process returned with errors. */
    XPLR_SD_OK,                         /**< indicates success of returning process. */
    XPLR_SD_BUSY,                       /**< returning process currently busy. */
    XPLR_SD_NOT_INIT,                   /**< returning SD card is not initialized*/
    XPLR_SD_NOT_FOUND,                  /**< returning if file is not found in the filesystem*/
    XPLR_SD_TIMEOUT                     /**< returning if operation passed the maxTimeout*/
} xplrSd_error_t;

/** Enumeration for size*/
typedef enum {
    XPLR_SIZE_UNKNOWN = -1,
    XPLR_SIZE_KB,
    XPLR_SIZE_MB,
    XPLR_SIZE_GB
} xplrSd_size_t;

typedef enum {
    XPLR_FILE_MODE_UNKNOWN = -1,        /**<Not valid mode.*/
    XPLR_FILE_MODE_READ,                /**<Opens a file for reading. The file must exist.*/
    XPLR_FILE_MODE_WRITE,               /**<Creates an empty file for writing. If a file with the same name already exists, its content is erased and the file is considered as a new empty file*/
    XPLR_FILE_MODE_APPEND,              /**<Appends to a file. Writing operations, append data at the end of the file. The file is created if it does not exist.*/
    XPLR_FILE_MODE_READ_PLUS,           /**<Opens a file to update both reading and writing. The file must exist.*/
    XPLR_FILE_MODE_WRITE_PLUS,          /**<Creates an empty file for both reading and writing.*/
    XPLR_FILE_MODE_APPEND_PLUS          /**<Opens a file for reading and appending.*/
} xplrSd_file_mode_t;

/**
 * Typedefs for the necessary SD driver initialization structs, coming from other libraries.
*/
typedef sdmmc_card_t                xplrSd_card_t;
typedef esp_vfs_fat_mount_config_t  xplrSd_mount_config_t;
typedef spi_bus_config_t            xplrSd_spi_config_t;
typedef sdspi_device_config_t       xplrSd_device_config_t;

typedef struct xplrSd_space_type {
    TaskHandle_t    sizeTaskHandler;           /**< Task handler for the check size task */
    uint64_t        freeSpace;                 /**< Free space of the card. Value in KBytes */
    uint64_t        totalSpace;                /**< Total space of the card. Value in KBytes */
    uint64_t        usedSpace;                 /**< Used space of the card. Value in KBytes */
} xplrSd_space_t;

typedef struct xplrSd_type {
    xplrSd_card_t           card;               /**< SD/MMC card configuration struct. */
    xplrSd_mount_config_t   mountConfig;        /**< VFS configuration. */
    xplrSd_spi_config_t     spiConfig;          /**< SPI bus configuration. */
    xplrSd_device_config_t  devConfig;          /**< Configuration for the SD SPI device. */
    xplrSd_space_t          spaceConfig;        /**< Struct containing the card's space stats and configuration. */
    char                    mountPoint[64];     /**< Filesystem's mounting point. Must start with "/". */
    bool                    isInit;             /**< Flag that indicates that the card and spi bus are initialized. */
    bool                    isDetected;         /**< Flag that indicated an SD card is present on the board. */
    bool                    semaphoreCreated;   /**< Flag that indicates a semaphore/mutex is created */
    double                  maxTimeout;         /**< Max timeout before an SD operation is terminated, due to timeout. */
    char                    protectFilename[64];/**< Filename to be protected by an erase command. */
} xplrSd_t;

#ifdef __cplusplus
}
#endif
#endif //_XPLR_SD_TYPES_H_
// End of file