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
#include "xplr_sd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

xplrSd_error_t xplrSdStartCardDetectTask(void);
xplrSd_error_t xplrSdStopCardDetectTask(void);
/**
 * @brief Function that configures the SD card struct with default values
 *
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdConfigDefaults(void);

/**
 * @brief Function that configures the SD card before initialization
 *
 * @param mountCfg pointer to the struct that contains the mount configuration
 * @param slotCfg pointer to the struct that contains the slot configuration
 * @param busCfg pointer to the struct that contains the spi bus configuration
 * @param card pointer to the struct that contains the card configuration
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdConfig(xplrSd_mount_config_t *mountCfg,
                            xplrSd_device_config_t *slotCfg,
                            xplrSd_spi_config_t *busCfg,
                            xplrSd_card_t *card,
                            const char *mountPoint);

/**
 * @brief Function that initializes spi bus, and mounts SD card and VFS
 *
 * @param sd Double pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdInit(void);

/**
 * @brief Function that unmounts SD card and frees SPI bus
 *
 * @param sd pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdDeInit(void);

/**
 * @brief Function that prints information about the mounted SD card.
 *
 * @param sd pointer to the struct that contains the card info
*/
void xplrSdPrintInfo(void);

/**
 * @brief Function that opens a file
 *
 * @param filename pointer to the path of the file to be opened
 * @param mode  mode in which the file will be opened
 * @return pointer to the file if successful, NULL if failed
*/
FILE *xplrSdOpenFile(const char *filename, xplrSd_file_mode_t mode);

/**
 * @brief Function that closes a file
 *
 * @param file pointer to the file to be closed
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdCloseFile(FILE *file);

/**
 * @brief Function that renames a file and deletes file with the same name if found
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the file to be renamed
 * @param renamedFile  pointer to the path to store the renamed file
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdRenameFile(const char *original, const char *renamed);

/**
 * @brief Function that deletes a file
 *
 * @param sd pointer to the struct that contains the card info
 * @param filepath pointer to the path of the file to be deleted
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdEraseFile(const char *filename);

/**
 * @brief Function that erases the whole memory of the SD card
 *
 * @param sd pointer to the struct that contains the card info
 * @return XPLR_SD_ERROR on failure, XPLR_SD_OK on success
*/
xplrSd_error_t xplrSdEraseAll(void);

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
xplrSd_error_t xplrSdWriteFileString(const char *filepath,
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
int xplrSdWriteFileU8(const char *filepath,
                      uint8_t *value,
                      size_t length,
                      xplrSd_file_mode_t mode);

/**
 * @brief Function that returns the size of the requested file
 *
 * @param filename  The requested file's filename
 * @return          The size of the file in bytes
*/
int64_t xplrSdGetFileSize(const char *filename);

/**
 * @brief Function that returns the total space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @return uint64_t value containing the total card space in success, 0 in error.
*/
uint64_t xplrSdGetTotalSpace(void);

/**
 * @brief Function that returns the free space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @return uint64_t value containing the free card space in success, 0 in error.
*/
uint64_t xplrSdGetFreeSpace(void);

/**
 * @brief Function that returns the used space of SD card in xplrSd_size_t units,
 *        as is configured in sd.spaceConfig.sizeUnit variable
 *
 * @return uint64_t value containing the used card space in success, 0 in error.
*/
uint64_t xplrSdGetUsedSpace(void);

bool xplrSdIsCardOn(void);
bool xplrSdIsCardInit(void);

#ifdef __cplusplus
}
#endif
#endif //_XPLR_SD_H_

// End of file


