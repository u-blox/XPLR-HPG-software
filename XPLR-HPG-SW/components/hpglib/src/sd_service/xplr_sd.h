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

#ifndef _XPLR_SD_H_
#define _XPLR_SD_H_


/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "./../../xplr_hpglib_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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

/** XPLR SD struct.
 * Holds required data and parameters for the API.
*/
typedef sdmmc_card_t xplrSd_card_t ;
typedef esp_vfs_fat_mount_config_t xplrSd_mount_config_t;
typedef spi_bus_config_t xplrSd_spi_config_t;
typedef sdspi_device_config_t xplrSd_device_config_t;

typedef struct xplrSd_file_type {
    char filename[256];                 /**< file path (name)*/
    bool isEmpty;                       /**< boolean flag to determine if a file is empty*/
} xplrSd_file_t;

typedef struct xplrSd_fs_type {
    uint8_t maximumFiles;               /**< maximum number of files that can be present in the filesystem*/
    uint8_t existingFiles;              /**< number of files existing in the filesystem*/
    char protectedFilename[256];        /**< filename protected by deletion*/
    xplrSd_file_t *files;               /**< pointer to the files existing in the filesystem*/
} xplrSd_fs_t;

typedef struct xplrSd_space_type {
    uint64_t freeSpace;                 /**< Free space of the card. Needs to be populated by xplrSdGetFreeSpace, value updated manually */
    uint64_t totalSpace;                /**< Total space of the card. Needs to be populated by xplrSdGetTotalSpace, value updated manually */
    uint64_t usedSpace;                 /**< Used space of the card. Needs to be populated by xplrSdGetUsedSpace, value updated manually */
    xplrSd_size_t sizeUnit;             /**< Size unit in which the space of the card is calculated (see xplrSd_size_t enumeration options) */
} xplrSd_space_t;

typedef struct xplrSd_type {
    xplrSd_card_t card;                 /**< SD/MMC card configuration struct */
    xplrSd_mount_config_t mountConfig;  /**< VFS configuration */
    xplrSd_spi_config_t spiConfig;      /**< SPI bus configuration */
    xplrSd_device_config_t devConfig;   /**< Configuration for the SD SPI device */
    xplrSd_fs_t fileSystem;             /**< List of the files existing in the SD card. Updated by the xplrSdFileList function */
    xplrSd_space_t spaceConfig;         /**< Struct containing the card's space stats and configuration */
    char mountPoint[256];               /**< Filesystem's mounting point. Default value is "/sdcard", can be modified, must start with "/" */
    bool isInit;                        /**< Flag that indicates that the card and spi bus are initialized. */
    bool isDetected;                    /**< Flag that indicated an SD card is present on the board. Updated manually by the xplrBoardDetectSd.
                                             Also populated during xplrSdConfig function during the initialization. */
    double maxTimeout;                  /**< Max timeout before an SD operation is terminated, due to timeout */
} xplrSd_t;
/*INDENT-ON*/

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Function that configures the SD card before initialization
 *
 * @param sd pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdConfig(xplrSd_t *sd);

/**
 * @brief Function that initializes spi bus, and mounts SD card and VFS
 *
 * @param sd Double pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdInit(xplrSd_t **sd);

/**
 * @brief Function that unmounts SD card and frees SPI bus
 *
 * @param sd pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdDeInit(xplrSd_t *sd);

/**
 * @brief Function that seeks a file in the filesystem and returns the index it is stored in the filesystem
 *
 * @param sd pointer to the struct that contains the card info
 * @param filename pointer to the string that contains the name of the file to be seeked
 * @return index number in which the file is stored in sd->fileSystem.files[index], -1 if not found
*/
int xplrSdSeekFile(xplrSd_t *sd, const char *filename);

/**
 * @brief Function that prints information about the mounted SD card.
 *
 * @param sd pointer to the struct that contains the card info
*/
void xplrSdPrintInfo(xplrSd_t *sd);

/**
 * @brief Function that opens a file
 *
 * @param filepath pointer to the path of the file to be opened
 * @param mode  mode in which the file will be opened
 * @return pointer to the file if successful, NULL if failed
*/
FILE *xplrSdOpenFile(const char *file, xplrSd_file_mode_t mode);

/**
 * @brief Function that closes a file
 *
 * @param file pointer to the file to be closed
 * @param filepath pointer to the path of the file to be closed
 * @param erase  true for deleting file after closing, false otherwise
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdCloseFile(FILE *file, const char *filepath, bool erase);

/**
 * @brief Function that renames a file and deletes file with the same name if found
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the file to be renamed
 * @param renamedFile  pointer to the path to store the renamed file
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdRenameFile(xplrSd_t *sd, const char *filepath, const char *renamedFile);

/**
 * @brief Function that creates a list of the paths of all the present files in the filesystem. The list is stored in sd->fileSystem.files struct
 *
 * @param sd pointer to the struct that contains the card info
 * @return number of files found, or -1 in error
*/
int xplrSdFileList(xplrSd_t *sd);

/**
 * @brief Function that deletes a file
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the path of the file to be deleted
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdEraseFile(xplrSd_t *sd, const char *filepath);

/**
 * @brief Function that erases the whole memory of the SD card
 *
 * @param sd pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdEraseAll(xplrSd_t *sd);

/**
 * @brief Function that reads the contents of a file containing strings
 *
 * @param filepath pointer to the path of the file to be read.
 * @param value pointer to the buffer to store the data read from the file
 * @param length number of bytes to be read from file
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdReadFileString(const char *filepath, char *value, size_t length);

/**
 * @brief Function that reads the contents of a file containing uint8
 *
 * @param filepath pointer to the path of the file to be read.
 * @param value pointer to the buffer to store the data read from the file
 * @param length number of bytes to be read from file
 * @return number of bytes read from file, -1 in failure
*/
int xplrSdReadFileU8(const char *filepath, uint8_t *value, size_t length);

/**
 * @brief Function that writes ASCII characters to a file
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the path of the file to be read.
 * @param value pointer to the buffer containing the data to write to the file
 * @param mode  mode in which the file will be opened
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdWriteFileString(xplrSd_t *sd,
                                     const char *filepath,
                                     char *value,
                                     xplrSd_file_mode_t mode);

/**
 * @brief Function that writes uint8 values separated by space to a file
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the path of the file to be read.
 * @param value pointer to the buffer containing the data to write to the file
 * @param length number of bytes to be written to the file
 * @param mode  mode in which the file will be opened
 * @return number of bytes written, -1 in failure
*/
int xplrSdWriteFileU8(xplrSd_t *sd,
                      const char *filepath,
                      uint8_t *value,
                      size_t length,
                      xplrSd_file_mode_t mode);

/**
 * @brief Function that converts a given filename to FAT32 format
 *
 * @param filename  string that contains the filename
*/
void xplrSdFormatFilename(char *filename);

/**
 * @brief Function that returns the total space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @param sd pointer to the struct that contains the card info
 * @return uint64_t value containing the total card space in success, 0 in error.
*/
uint64_t xplrSdGetTotalSpace(xplrSd_t *sd);

/**
 * @brief Function that returns the free space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @param sd pointer to the struct that contains the card info
 * @return uint64_t value containing the free card space in success, 0 in error.
*/
uint64_t xplrSdGetFreeSpace(xplrSd_t *sd);

/**
 * @brief Function that returns the used space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @param sd pointer to the struct that contains the card info
 * @return uint64_t value containing the used card space in success, 0 in error.
*/
uint64_t xplrSdGetUsedSpace(xplrSd_t *sd);

#ifdef __cplusplus
}
#endif
#endif //_XPLR_SD_H_

// End of file


