/*
 * Copyright 2023 u-blox
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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "./../common/xplr_common.h"
#include "./../../xplr_hpglib_cfg.h"
#include "xplr_log.h"
#include <sys/stat.h>
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

#if (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED && 1 == XPLRLOG_DEBUG_ACTIVE)
#define XPLRLOG_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrLog", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRLOG_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* Enumeration of states in xplrLogUpdateBuffer function*/
typedef enum {
    XPLR_UPDATE_CASE_ERROR = -1,
    XPLR_UPDATE_CASE_INITIAL,
    XPLR_UPDATE_CASE1,
    XPLR_UPDATE_CASE2
} xplrLog_updBuf_t;
/*INDENT-OFF*/
typedef struct xplrLog_type {
    bool                isLogEnabled;                                   /**< Flag that enables/disables logging */
    bool                isLogInit;                                      /**< Flag that indicates logging has been initialized */
    SemaphoreHandle_t   xMutex;                                         /**< Mutex to provide thread safety and atomic access to the struct */
    xplrLog_dvcTag_t    tag;                                            /**< Device tag of the logging to determine the type of data to be logged */
    char                filename[64];                                   /**< Name of the logging file */
    int64_t             fileSize;                                       /**< Size of the log file */
    uint8_t             fileIncrement;                                  /**< Integer showing the number of filename increments performed in the file (0 means none, 1 means one etc.) */
    bool                isFileModified;                                 /**< Flag that indicates if the logging module has modified the file in the SD card */
    char                buffer[XPLR_LOG_BUFFER_MAX_SIZE + 1];           /**< Internal buffer that keeps messages and stores them in the SD card when full
                                                                           < in order to achieve better performance */
    uint16_t            bufferIndex;                                    /**< Index to the internal buffer to keep track of how full it is */
    uint64_t            sizeInterval;                                   /**< Size in bytes to increment filename */
} xplrLog_t;
/*INDENT-ON*/
static xplrLog_t    logInstance[XPLR_LOG_MAX_INSTANCES];
static uint8_t      logDvcs = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static int8_t          dvcGetFirstFreeSlot(void);
static xplrLog_error_t dvcRemoveSlot(int8_t index);
static bool            dvcIsIndexValid(int8_t index);
static xplrLog_error_t logUpdateBuffer(int8_t index, char *pBuffer);
static xplrLog_error_t logFlushBuffer(int8_t index);
static void            logIncrementFilename(char *filename, uint8_t inc);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

int8_t xplrLogInit(xplrLog_dvcTag_t tag, char *logFilename, uint64_t sizeInterval, bool replace)
{
    int8_t index;
    FILE *fp = NULL;

    if (logFilename == NULL || strlen(logFilename) > 63) {
        XPLRLOG_CONSOLE(E, "Invalid Filename");
        index = -1;
    } else {
        index = dvcGetFirstFreeSlot();
        if (index < 0) {
            XPLRLOG_CONSOLE(E, "Could not initialize logging in file %s", logFilename);
        } else {
            logInstance[index].xMutex = xSemaphoreCreateMutex();
            if (xSemaphoreTake(logInstance[index].xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                logInstance[index].tag = tag;
                strncpy(logInstance[index].filename, logFilename, 64);
                if (replace) {
                    fp = xplrSdOpenFile(logInstance[index].filename, XPLR_FILE_MODE_WRITE);
                    xplrSdCloseFile(fp);
                }
                logInstance[index].fileSize = xplrSdGetFileSize(logFilename);
                logInstance[index].sizeInterval = sizeInterval;
                logInstance[index].isLogEnabled = true;
                logInstance[index].isLogInit = true;
                xSemaphoreGive(logInstance[index].xMutex);
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                index = -1;
            }
        }
    }

    if (index == -1) {
        XPLR_CI_CONSOLE(12, "ERROR");
    } else {
        // do nothing
    }

    return index;
}

xplrLog_error_t xplrLogDeInit(int8_t index)
{
    xplrLog_error_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid) {
        instance = &logInstance[index];
        /* Try to take the semaphore */
        if (instance->xMutex != NULL && instance->isLogInit) {
            if (xSemaphoreTake(instance->xMutex, portMAX_DELAY) == pdTRUE) {
                /* Fill the rest of the log buffer with zeros*/
                memset(&instance->buffer[instance->bufferIndex],
                       0,
                       XPLR_LOG_BUFFER_MAX_SIZE - instance->bufferIndex);
                /* Flush the contents of log buffer in the log file in the SD card*/
                ret = logFlushBuffer(index);
                vTaskDelay(pdMS_TO_TICKS(10)); // Need a small window to finish the SPI communication
                if (ret == XPLR_LOG_OK) {
                    ret = dvcRemoveSlot(index);
                } else {
                    XPLRLOG_CONSOLE(E, "Could not flush the remaining content to the SD card");
                }
                if (instance->xMutex != NULL) {
                    xSemaphoreGive(instance->xMutex);
                }
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                ret = XPLR_LOG_ERROR;
            }
        } else {
            XPLRLOG_CONSOLE(W, "NULL or Uninitialized index: <%d>. Nothing to de-initialize", index);
            ret = XPLR_LOG_OK;
        }

    } else {
        XPLRLOG_CONSOLE(E, "Invalid Arguments. De-Init failed");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogDeInitAll(void)
{
    xplrLog_error_t ret = XPLR_LOG_ERROR;

    for (uint8_t index = 0; index < logDvcs; index++) {
        ret = xplrLogDeInit(index);
        if (ret != XPLR_LOG_OK) {
            XPLRLOG_CONSOLE(E, "Error in De Init All function. Index number :<%d>. Aborting...", index);
            break;
        }
    }
    logDvcs = 0;
    return ret;
}

bool xplrLogIsEnabled(int8_t index)
{
    bool ret = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (ret) {
        instance = &logInstance[index];
        if (instance->xMutex != NULL) {
            if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                ret = instance->isLogEnabled;
                xSemaphoreGive(instance->xMutex);
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                ret = false;
            }
        } else {
            ret = false;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid index : <%d>", index);
    }

    return ret;
}

xplrLog_error_t xplrLogEnable(int8_t index)
{
    xplrLog_error_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid) {
        instance = &logInstance[index];
        if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
            instance->isLogEnabled = true;
            ret = XPLR_LOG_OK;
            xSemaphoreGive(instance->xMutex);
        } else {
            XPLRLOG_CONSOLE(W, "Could not take semaphore");
            ret = XPLR_LOG_ERROR;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid index");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogDisable(int8_t index)
{
    xplrLog_error_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid) {
        instance = &logInstance[index];
        if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
            instance->isLogEnabled = false;
            ret = XPLR_LOG_OK;
            xSemaphoreGive(instance->xMutex);
        } else {
            XPLRLOG_CONSOLE(W, "Could not take semaphore");
            ret = XPLR_LOG_ERROR;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid index");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogEnableAll(void)
{
    xplrLog_error_t ret;
    xplrLog_t *instance = NULL;

    for (uint8_t index = 0; index < logDvcs; index++) {
        instance = &logInstance[index];
        if (instance->xMutex != NULL) {
            if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                if (instance->isLogInit) {
                    instance->isLogEnabled = true;
                }
                ret = XPLR_LOG_OK;
                xSemaphoreGive(instance->xMutex);
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                ret = XPLR_LOG_ERROR;
                break;
            }
        } else {
            // Do nothing. Instance Semaphore is NULL (not initialized)
            ret = XPLR_LOG_OK;
        }
    }

    return ret;
}

xplrLog_error_t xplrLogDisableAll(void)
{
    xplrLog_error_t ret;
    xplrLog_t *instance = NULL;

    for (uint8_t index = 0; index < XPLR_LOG_MAX_INSTANCES; index++) {
        instance = &logInstance[index];
        if (instance->isLogInit && (instance->xMutex != NULL)) {
            if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                instance->isLogEnabled = false;
                ret = XPLR_LOG_OK;
                xSemaphoreGive(instance->xMutex);
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                ret = XPLR_LOG_ERROR;
                break;
            }
        } else {
            /* Instance not initialized so no disabling needed */
            ret = XPLR_LOG_OK;
        }
    }

    return ret;
}

int64_t xplrLogGetFileSize(int8_t index)
{
    int64_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid) {
        instance = &logInstance[index];
        if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
            ret = instance->fileSize;
            xSemaphoreGive(instance->xMutex);
        } else {
            XPLRLOG_CONSOLE(W, "Could not take semaphore");
            ret = -1;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid index");
        ret = -1;
    }

    return ret;
}

xplrLog_error_t xplrLogSetFilename(int8_t index, char *filename)
{
    xplrLog_error_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid && (filename != NULL) && (strlen(filename) < 63)) {
        instance = &logInstance[index];
        if (instance->xMutex != NULL) {
            if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                /* Erase previous filename */
                memset(instance->filename, 0, 64);
                /* Set new filename */
                strncpy(instance->filename, filename, strlen(filename));
                /* New file, so new size is 0 */
                instance->fileSize = 0;
                /* Also file increment is 0 */
                instance->fileIncrement = 0;
                ret = XPLR_LOG_OK;
                xSemaphoreGive(instance->xMutex);
            } else {
                XPLRLOG_CONSOLE(W, "Could not take semaphore");
                ret = XPLR_LOG_ERROR;
            }
        } else {
            XPLRLOG_CONSOLE(W, "Log instance semaphore is NULL");
            ret = XPLR_LOG_ERROR;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid arguments");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogSetDeviceTag(int8_t index, xplrLog_dvcTag_t tag)
{
    xplrLog_error_t ret;
    bool isValid = dvcIsIndexValid(index);
    xplrLog_t *instance = NULL;

    if (isValid) {
        instance = &logInstance[index];
        if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
            instance->tag = tag;
            ret = XPLR_LOG_OK;
            xSemaphoreGive(instance->xMutex);
        } else {
            XPLRLOG_CONSOLE(W, "Could not take semaphore");
            ret = XPLR_LOG_ERROR;
        }
    } else {
        XPLRLOG_CONSOLE(E, "Invalid index");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogFile(int8_t index, xplrLog_Opt_t opt, char *fmt, ...)
{
    xplrLog_error_t ret = XPLR_LOG_ERROR;
    va_list args;
    xplrLog_t *instance = NULL;
    bool isValid;
    bool fileInc = false;
    char buf[XPLR_LOG_MAX_PRINT_SIZE] = {0};
    char filename[64];

    if (fmt != NULL) {
        va_start(args, fmt);
        /*  vsnprintf can alter va_list in some cases,
            so it would be better to avoid that, since
            many times a valid pointer comes as an argument.*/
        vsnprintf(buf, XPLR_LOG_MAX_PRINT_SIZE - 1, fmt, args);
        va_end(args);

        /* Console Print Part */
        if ((opt == XPLR_LOG_PRINT_ONLY) || (opt == XPLR_LOG_SD_AND_PRINT)) {
            esp_rom_printf(buf);
            ret = XPLR_LOG_OK;
        }

        /* Logging Part */
        if ((opt == XPLR_LOG_SD_ONLY) || (opt == XPLR_LOG_SD_AND_PRINT)) {
            isValid = xplrLogIsEnabled(index);
            if (isValid && (fmt != NULL)) {
                instance = &logInstance[index];
                /* Take the semaphore */
                if (xSemaphoreTake(instance->xMutex, XPLR_LOG_MAX_TIMEOUT) == pdTRUE) {
                    /* Update internal buffer */
                    ret = logUpdateBuffer(index, buf);
                    /* Check if maximum size is reached (4GBytes in FAT) */
                    if (instance->fileSize >= instance->sizeInterval) {
                        strncpy(filename, instance->filename, 63);
                        logIncrementFilename(filename, instance->fileIncrement);
                        xplrSdEraseFile(filename);
                        instance->fileSize = 0;
                        instance->fileIncrement++;
                        fileInc = true;
                    }
                    xSemaphoreGive(instance->xMutex);
                    if (fileInc) {
                        xplrLogSetFilename(index, filename);
                    }
                } else {
                    XPLRLOG_CONSOLE(W, "Could not take semaphore");
                    ret = XPLR_LOG_ERROR;
                }
            } else {
                ret = XPLR_LOG_ERROR;
            }
        }
    }
    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * Function that returns the index of the first available local instance
*/
static int8_t dvcGetFirstFreeSlot(void)
{
    int8_t ret;

    if (logDvcs < XPLR_LOG_MAX_INSTANCES && &logInstance[logDvcs] != NULL) {
        ret = logDvcs;
        logDvcs++;
    } else {
        ret = -1;
    }

    XPLRLOG_CONSOLE(D, "ret index: %d", ret);
    return ret;
}

/**
 * Function that removes the log instance
*/
static xplrLog_error_t dvcRemoveSlot(int8_t index)
{
    xplrLog_error_t ret;

    if ((index < XPLR_LOG_MAX_INSTANCES) && (&logInstance[index] != NULL)) {
        memset(&logInstance[index], (int)NULL, sizeof(xplrLog_t));
        ret = XPLR_LOG_OK;
        XPLRLOG_CONSOLE(D, "slot %d removed", index);
    } else {
        ret = XPLR_LOG_ERROR;
        XPLRLOG_CONSOLE(E, "failed to remove slot %d", index);
    }

    return ret;
}

/**
 * Function that checks if the given index is valid (in bounds for the internal array)
*/
static bool dvcIsIndexValid(int8_t index)
{
    bool ret;

    if ((index >= 0) && (index < XPLR_LOG_MAX_INSTANCES)) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

/**
 * Function that handles the update of the message to the corresponding buffer
 * and handles the writing to the SD
*/
static xplrLog_error_t logUpdateBuffer(int8_t index, char *pBuffer)
{
    xplrLog_error_t ret;
    xplrSd_error_t err;
    xplrLog_updBuf_t updateCase;
    bool logDone = false;
    size_t pBufLen;
    char *filename;
    int bytesWritten;
    xplrLog_t *instance = &logInstance[index];

    /* Null pointer check*/
    if (pBuffer != NULL) {
        pBufLen = strlen(pBuffer);
        filename = logInstance[index].filename;
        updateCase = XPLR_UPDATE_CASE_INITIAL;
        /**
         * Cases:
         *      1-> instance.buffer[] has space to take the data
         *              -> buffer gets full
         *                  -> store in memory, clear buffer, update bufferIndex and make logDone true
         *              -> buffer doesn't get full
         *                  -> update bufferIndex and make logDone true
         *      2-> instance.buffer[] doesn't have space to take the data
         *              -> fill the rest of the buffer and store to memory
         *              -> do this repeatedly until reaching case 1
        */
        while (!logDone) {
            switch (updateCase) {
                /* pBuffer is smaller than freeSpace*/
                case XPLR_UPDATE_CASE_INITIAL:
                    if ((XPLR_LOG_BUFFER_MAX_SIZE - instance->bufferIndex) >= pBufLen) {
                        updateCase = XPLR_UPDATE_CASE1;
                    } else {
                        updateCase = XPLR_UPDATE_CASE2;
                    }
                    break;
                /* xplrLog.buffer[] has space to take the data*/
                case XPLR_UPDATE_CASE1:
                    memcpy(&instance->buffer[instance->bufferIndex], pBuffer, pBufLen);
                    instance->bufferIndex += pBufLen;
                    if (instance->bufferIndex < XPLR_LOG_BUFFER_MAX_SIZE) {
                        logDone = true;
                    } else {
                        if (instance->tag == XPLR_LOG_DEVICE_ERROR || instance->tag == XPLR_LOG_DEVICE_INFO) {
                            err = xplrSdWriteFileString(filename, instance->buffer, XPLR_FILE_MODE_APPEND);
                            if (err == XPLR_SD_OK) {
                                bytesWritten = XPLR_LOG_BUFFER_MAX_SIZE;
                            } else {
                                bytesWritten = 0;
                            }
                        } else {
                            bytesWritten = xplrSdWriteFileU8(filename,
                                                             (uint8_t *)instance->buffer,
                                                             XPLR_LOG_BUFFER_MAX_SIZE,
                                                             XPLR_FILE_MODE_APPEND);
                            if (bytesWritten > 0) {
                                err = XPLR_SD_OK;
                            } else {
                                err = XPLR_SD_ERROR;
                            }
                        }
                        instance->fileSize += bytesWritten;
                        if (err == XPLR_SD_OK) {
                            XPLRLOG_CONSOLE(D, "Log to file %s successful", filename);
                            memset(instance->buffer, 0x00, XPLR_LOG_BUFFER_MAX_SIZE);
                            instance->bufferIndex = 0;
                            logDone = true;
                        } else {
                            XPLRLOG_CONSOLE(E, "Error in logging to file %s", filename);
                            ret = XPLR_LOG_ERROR;
                            updateCase = XPLR_UPDATE_CASE_ERROR;
                        }
                    }
                    break;
                /* xplrLog.buffer[] doesn't have space to take the data*/
                case XPLR_UPDATE_CASE2:
                    memcpy(&instance->buffer[instance->bufferIndex],
                           pBuffer,
                           (XPLR_LOG_BUFFER_MAX_SIZE - instance->bufferIndex));
                    if (instance->tag == XPLR_LOG_DEVICE_ERROR || instance->tag == XPLR_LOG_DEVICE_INFO) {
                        err = xplrSdWriteFileString(filename, instance->buffer, XPLR_FILE_MODE_APPEND);
                        if (err == XPLR_SD_OK) {
                            bytesWritten = XPLR_LOG_BUFFER_MAX_SIZE;
                        } else {
                            bytesWritten = 0;
                        }
                    } else {
                        bytesWritten = xplrSdWriteFileU8(filename,
                                                         (uint8_t *)instance->buffer,
                                                         XPLR_LOG_BUFFER_MAX_SIZE,
                                                         XPLR_FILE_MODE_APPEND);
                        if (bytesWritten > 0) {
                            err = XPLR_SD_OK;
                        } else {
                            bytesWritten = 0;
                            err = XPLR_SD_ERROR;
                        }
                    }
                    instance->fileSize += bytesWritten;
                    if (err == XPLR_SD_OK) {
                        XPLRLOG_CONSOLE(D, "Log to file %s successful", filename);
                        pBuffer = pBuffer + XPLR_LOG_BUFFER_MAX_SIZE - instance->bufferIndex;
                        pBufLen = pBufLen - XPLR_LOG_BUFFER_MAX_SIZE + instance->bufferIndex;
                        memset(instance->buffer, 0x00, XPLR_LOG_BUFFER_MAX_SIZE);
                        instance->bufferIndex = 0;
                        updateCase = XPLR_UPDATE_CASE_INITIAL;
                    } else {
                        XPLRLOG_CONSOLE(E, "Error in logging to file %s", filename);
                        ret = XPLR_LOG_ERROR;
                        updateCase = XPLR_UPDATE_CASE_ERROR;
                    }
                    break;
                /* Something led to an error*/
                case XPLR_UPDATE_CASE_ERROR:
                    ret = XPLR_LOG_ERROR;
                    logDone = true;
                    XPLRLOG_CONSOLE(E, "Logging procedure failed");
                    break;
                default:
                    ret = XPLR_LOG_ERROR;
                    logDone = true;
                    XPLRLOG_CONSOLE(E, "Logging procedure failed with unexpected error");
                    break;
            }
        }
    } else {
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

/**
 * Function that is called to force the log buffer to write it's contents in the SD card
*/
static xplrLog_error_t logFlushBuffer(int8_t index)
{

    xplrLog_error_t ret;
    xplrSd_error_t err;
    int bytesWritten;
    xplrLog_t *instance = &logInstance[index];

    if (instance->bufferIndex > 0) {
        if (instance->tag == XPLR_LOG_DEVICE_ERROR || instance->tag == XPLR_LOG_DEVICE_INFO) {
            err = xplrSdWriteFileString(instance->filename,
                                        instance->buffer,
                                        XPLR_FILE_MODE_APPEND);
            if (err == XPLR_SD_OK) {
                bytesWritten = instance->bufferIndex;
            } else {
                bytesWritten = 0;
            }
        } else {
            bytesWritten = xplrSdWriteFileU8(instance->filename,
                                             (uint8_t *)instance->buffer,
                                             instance->bufferIndex,
                                             XPLR_FILE_MODE_APPEND);
            if (bytesWritten > 0) {
                err = XPLR_SD_OK;
            } else {
                bytesWritten = 0;
                err = XPLR_SD_ERROR;
            }
        }
        instance->fileSize += bytesWritten;
        if (err == XPLR_SD_OK) {
            memset(instance->buffer, 0x00, XPLR_LOG_BUFFER_MAX_SIZE);
            instance->bufferIndex = 0;
            ret = XPLR_LOG_OK;
        } else {
            ret = XPLR_LOG_ERROR;
        }
    } else {
        /* No data in buffer so nothing to flush to the SD card */
        ret = XPLR_LOG_OK;
    }

    return ret;
}

static void logIncrementFilename(char *filename, uint8_t inc)
{
    char buf[64] = "";
    char extStr[64] = "";
    char *ext, *incStr;
    const char dot = '.';
    const char par = '(';
    size_t offset = 0, extLen = 0, incLen = 0, len;

    len = strlen(filename);

    ext = strchr(filename, dot);
    if (ext != NULL) {
        offset = strlen(ext);
        extLen = offset;
        memcpy(extStr, ext, extLen);
    } else {
        extLen = strlen(extStr);
    }

    incStr = strchr(filename, par);
    if (incStr != NULL) {
        offset = strlen(incStr);
        incLen = offset - extLen;
    } else {
        //Do nothing
    }

    if (inc == 0) {
        incLen += 3;
    } else if (inc == 9 || inc == 99) {
        incLen += 1;
    } else {
        //Do nothing
    }

    if (((len - offset) + incLen + extLen) >= 63) {
        memcpy(buf, filename, len - incLen - extLen);
    } else {
        memcpy(buf, filename, len - offset);
    }

    inc++;
    memset(filename, 0, len);
    snprintf(&filename[0], len - offset + incLen + extLen + 1, "%s(%d)%s", buf, inc, extStr);
}