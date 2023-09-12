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
    XPLR_UPDATE_CASE1,
    XPLR_UPDATE_CASE1A,
    XPLR_UPDATE_CASE1B,
    XPLR_UPDATE_CASE2
} xplrLog_updBuf_t;

/* Mutex created by the first comer in the xplrLogInit function to guarantee atomic access
 * to the private logging functions*/
SemaphoreHandle_t xMutex = NULL;
/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplrLog_error_t xplrLogUpdateBuffer(xplrLog_t *xplrLog, char *pBuffer);
static xplrLog_error_t xplrLogConfig(xplrLog_t *xplrLog,
                                     xplrLog_dvcTag_t tag,
                                     char *logFilename,
                                     uint16_t maxSize,
                                     xplrLog_size_t maxSizeType);
static xplrLog_error_t xplrLogMakeSpace(xplrLog_t *xplrLog);
static xplrLog_error_t xplrLogCheckSpace(xplrLog_t *xplrLog);
static xplrLog_error_t xplrLogFlushBuffer(xplrLog_t *xplrLog);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrLog_error_t xplrLogInit(xplrLog_t *xplrLog,
                            xplrLog_dvcTag_t tag,
                            char *logFilename,
                            uint16_t maxSize,
                            xplrLog_size_t maxSizeType)
{
    xplrLog_error_t ret;
    xplrSd_error_t err;
    /* Flag that needs to be raised by the first comer in the xplrLogInit function*/
    static bool localFlag;
    if (!localFlag) {
        // First comer creates mutex and sets flag
        xMutex = xSemaphoreCreateMutex();
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
            xSemaphoreGive(xMutex);
            localFlag = true;
        } else {
            XPLRLOG_CONSOLE(E, "Error in mutex creation!");
        }
    }
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        if (xplrLog->sd != NULL) {
            /* SD pointer not null so let's check if initialized*/
            if (xplrLog->sd->isInit) {
                XPLRLOG_CONSOLE(D, "SD card already initialized");
                ret = xplrLogConfig(xplrLog, tag, logFilename, maxSize, maxSizeType);
            } else {
                err = xplrSdInit(&xplrLog->sd);
                if (err == XPLR_SD_OK) {
                    XPLRLOG_CONSOLE(D, "Logging to file <%s> is enabled", logFilename);
                    ret = XPLR_LOG_OK;
                } else {
                    ret = XPLR_LOG_ERROR;
                }
            }
        } else {
            /* Pointer is NULL so we need to initialize*/
            err = xplrSdInit(&xplrLog->sd);
            if (err == XPLR_SD_OK) {
                XPLRLOG_CONSOLE(D, "Logging to file <%s> is enabled", logFilename);
                ret = xplrLogConfig(xplrLog, tag, logFilename, maxSize, maxSizeType);
            } else {
                ret = XPLR_LOG_ERROR;
            }
        }
        xSemaphoreGive(xMutex);
    } else {
        XPLRLOG_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogDeInit(xplrLog_t *xplrLog)
{
    xplrLog_error_t ret;

    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        /* Fill the rest of the log buffer with zeros*/
        memset(&xplrLog->buffer[xplrLog->bufferIndex], 0, LOG_BUFFER_MAX_SIZE - xplrLog->bufferIndex);
        /* Flush the contents of log buffer in the log file in the SD card*/
        ret = xplrLogFlushBuffer(xplrLog);
        /* Disable logging for this log instance*/
        xplrLog->logEnable = false;
        /* Remove the SD card pointer*/
        xplrLog->sd = NULL;
        xSemaphoreGive(xMutex);
    } else {
        XPLRLOG_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

xplrLog_error_t xplrLogFile(xplrLog_t *xplrLog, char *message)
{
    xplrLog_error_t ret;

    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        /* Check if logging is enabled for this log instance*/
        if (xplrLog->logEnable) {
            /* Check for available space*/
            ret = xplrLogCheckSpace(xplrLog);
            if (ret != XPLR_LOG_OK) {
                if (xplrLog->freeSpace == 0) {
                    XPLRLOG_CONSOLE(W, "No more space we must erase data.");
                    /* Not enough space need to erase files*/
                    ret = xplrLogMakeSpace(xplrLog);
                    if (ret == XPLR_LOG_OK) {
                        /* Check that after the erase there is enough space*/
                        ret = xplrLogCheckSpace(xplrLog);
                    } else {
                        XPLRLOG_CONSOLE(E, "Could not make space for logging in SD card!");
                    }
                } else {
                    ret = XPLR_LOG_ERROR;
                    XPLRLOG_CONSOLE(E, "Error in checking free space logging stopped");
                }
            } else {
                // Do nothing
            }
            if (ret == XPLR_LOG_OK) {
                /* The message can be inserted in the log buffer*/
                ret = xplrLogUpdateBuffer(xplrLog, message);
            } else {
                XPLRLOG_CONSOLE(E, "Error in logging to file %s", xplrLog->logFilename);
                ret = XPLR_LOG_ERROR;
            }
        } else {
            XPLRLOG_CONSOLE(W, "Logging to file <%s> is not enabled...", xplrLog->logFilename);
            ret = XPLR_LOG_ERROR;
        }
        xSemaphoreGive(xMutex);
    } else {
        XPLRLOG_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_LOG_ERROR;
    }
    return ret;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * Function that handles the update of the message to the corresponding buffer
 * and and handles the writing to the SD
*/
static xplrLog_error_t xplrLogUpdateBuffer(xplrLog_t *xplrLog, char *pBuffer)
{
    xplrLog_error_t ret;
    xplrSd_error_t err;
    xplrLog_updBuf_t updateCase;
    bool logDone = false;
    size_t pBufLen = strlen(pBuffer);
    char fullFilepath[LOG_MAXIMUM_NAME_SIZE + 256 + 1];

    /* Null pointer check*/
    if (pBuffer != NULL && xplrLog != NULL) {
        strcpy(fullFilepath, xplrLog->logFilename);
        if (pBufLen <= xplrLog->freeSpace) {
            updateCase = XPLR_UPDATE_CASE1;
        } else {
            updateCase = XPLR_UPDATE_CASE2;
        }
        /**
         * Cases:
         *
         *  1. pBuffer is smaller than freeSpace
         *      a-> xplrLog.buffer[] has space to take the data
         *              -> buffer gets full
         *                  -> store in memory, clear buffer, update bufferIndex and make logDone true
         *              -> buffer doesn't get full
         *                  -> update bufferIndex and make logDone true
         *      b-> xplrLog.buffer[] doesn't have space to take the data
         *              -> fill the rest of the buffer and store to memory
         *              -> do this repeatedly until reaching case 1a
         *  2. pBuffer is bigger than freeSpace
         *      -> xplrLogMakeSpace() to delete the file and create a new empty one
         *          -> if pBuffer is still bigger than freeSpace abort with error message
         *          -> if pBuffer is now smaller than the new freeSpace it falls to case 1
        */
        while (!logDone) {
            switch (updateCase) {
                /* pBuffer is smaller than freeSpace*/
                case XPLR_UPDATE_CASE1:
                    if ((LOG_BUFFER_MAX_SIZE - xplrLog->bufferIndex) >= pBufLen) {
                        updateCase = XPLR_UPDATE_CASE1A;
                    } else {
                        updateCase = XPLR_UPDATE_CASE1B;
                    }
                    break;
                /* xplrLog.buffer[] has space to take the data*/
                case XPLR_UPDATE_CASE1A:
                    memcpy(&xplrLog->buffer[xplrLog->bufferIndex], pBuffer, pBufLen);
                    xplrLog->bufferIndex += pBufLen;
                    if (xplrLog->bufferIndex < LOG_BUFFER_MAX_SIZE) {
                        logDone = true;
                    } else {
                        if (xplrLog->tag == XPLR_LOG_DEVICE_ERROR || xplrLog->tag == XPLR_LOG_DEVICE_INFO) {
                            err = xplrSdWriteFileString(xplrLog->sd, fullFilepath, xplrLog->buffer, XPLR_FILE_MODE_APPEND);
                        } else {
                            err = xplrSdWriteFileU8(xplrLog->sd,
                                                    fullFilepath,
                                                    (uint8_t *)xplrLog->buffer,
                                                    LOG_BUFFER_MAX_SIZE,
                                                    XPLR_FILE_MODE_APPEND);
                            if (err > 0) {
                                err = XPLR_SD_OK;
                            }
                        }
                        if (err == XPLR_SD_OK) {
                            XPLRLOG_CONSOLE(D, "Log to file %s successful", xplrLog->logFilename);
                            memset(xplrLog->buffer, 0x00, LOG_BUFFER_MAX_SIZE + 1);
                            xplrLog->bufferIndex = 0;
                            logDone = true;
                        } else {
                            XPLRLOG_CONSOLE(E, "Error in logging to file %s", xplrLog->logFilename);
                            ret = XPLR_LOG_ERROR;
                            updateCase = XPLR_UPDATE_CASE_ERROR;
                        }
                    }
                    break;
                /* xplrLog.buffer[] doesn't have space to take the data*/
                case XPLR_UPDATE_CASE1B:
                    memcpy(&xplrLog->buffer[xplrLog->bufferIndex], pBuffer,
                           (LOG_BUFFER_MAX_SIZE - xplrLog->bufferIndex));
                    if (xplrLog->tag == XPLR_LOG_DEVICE_ERROR || xplrLog->tag == XPLR_LOG_DEVICE_INFO) {
                        err = xplrSdWriteFileString(xplrLog->sd, fullFilepath, xplrLog->buffer, XPLR_FILE_MODE_APPEND);
                    } else {
                        err = xplrSdWriteFileU8(xplrLog->sd, fullFilepath, (uint8_t *)xplrLog->buffer,
                                                LOG_BUFFER_MAX_SIZE,
                                                XPLR_FILE_MODE_APPEND);
                        if (err > 0) {
                            err = XPLR_SD_OK;
                        } else {
                            // Do nothing, err has the return value of write operation
                        }
                    }
                    if (err == XPLR_SD_OK) {
                        XPLRLOG_CONSOLE(D, "Log to file %s successful", xplrLog->logFilename);
                        pBuffer = pBuffer + LOG_BUFFER_MAX_SIZE - xplrLog->bufferIndex;
                        pBufLen = pBufLen - LOG_BUFFER_MAX_SIZE + xplrLog->bufferIndex;
                        memset(xplrLog->buffer, 0x00, LOG_BUFFER_MAX_SIZE);
                        xplrLog->bufferIndex = 0;
                        updateCase = XPLR_UPDATE_CASE1;
                    } else {
                        XPLRLOG_CONSOLE(E, "Error in logging to file %s", xplrLog->logFilename);
                        ret = XPLR_LOG_ERROR;
                        updateCase = XPLR_UPDATE_CASE_ERROR;
                    }
                    break;
                /* pBuffer is bigger than freeSpace*/
                case XPLR_UPDATE_CASE2:
                    if (xplrLogMakeSpace(xplrLog) == XPLR_LOG_OK) {
                        if (xplrLogCheckSpace(xplrLog) == XPLR_LOG_OK) {
                            if (pBufLen <= xplrLog->freeSpace) {
                                updateCase = XPLR_UPDATE_CASE1;
                            } else {
                                XPLRLOG_CONSOLE(E, "Data to be logged is too big to store to memory");
                                updateCase = XPLR_UPDATE_CASE_ERROR;
                            }
                        } else {
                            XPLRLOG_CONSOLE(E, "Could not make space for logging file");
                            updateCase = XPLR_UPDATE_CASE_ERROR;
                        }
                    } else {
                        updateCase = XPLR_UPDATE_CASE_ERROR;
                        XPLRLOG_CONSOLE(E, "Could not make space for logging file");
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
        /* In case of error message we need to log this immediately to the SD card (without waiting for the buffer to be full)*/
        if (xplrLog->tag == XPLR_LOG_DEVICE_ERROR && ret == XPLR_LOG_OK) {
            ret = xplrLogFlushBuffer(xplrLog);
        }
    } else {
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

/**
 * Function that configures the xplrLog struct before initialization
*/
static xplrLog_error_t xplrLogConfig(xplrLog_t *xplrLog,
                                     xplrLog_dvcTag_t tag,
                                     char *logFilename,
                                     uint16_t maxSize,
                                     xplrLog_size_t maxSizeType)
{
    xplrLog_error_t ret;
    xplrSd_error_t err;
    FILE *fp;

    snprintf(xplrLog->logFilename,
             LOG_MAXIMUM_NAME_SIZE + 256 + 1,
             "%s%s",
             xplrLog->sd->mountPoint,
             logFilename);
    memset(xplrLog->buffer, 0x00, LOG_BUFFER_MAX_SIZE + 1);
    xplrLog->bufferIndex = 0;
    xplrLog->maxSize = maxSize;
    xplrLog->maxSizeType = maxSizeType;
    xplrLog->tag = tag;
    fp = xplrSdOpenFile(xplrLog->logFilename, XPLR_FILE_MODE_APPEND);
    err = xplrSdCloseFile(fp, xplrLog->logFilename, false);
    if (err == XPLR_SD_OK) {
        ret = XPLR_LOG_OK;
    } else {
        ret = XPLR_LOG_ERROR;
    }
    return ret;
}

/**
 * Function that is called to make space in the SD card by emptying the log file
*/
static xplrLog_error_t xplrLogMakeSpace(xplrLog_t *xplrLog)
{
    xplrLog_error_t ret;
    xplrSd_error_t err;
    FILE *fp;

    /* If the space is not enough to log we erase the file (more options to be added in future update)*/
    err = xplrSdEraseFile(xplrLog->sd, xplrLog->logFilename);
    if (err != XPLR_SD_OK) {
        XPLRLOG_CONSOLE(E, "Error in freeing space for log file");
    } else {
        fp = xplrSdOpenFile(xplrLog->logFilename, XPLR_FILE_MODE_WRITE);
        err = xplrSdCloseFile(fp, xplrLog->logFilename, false);
        if (err == XPLR_SD_OK) {
            XPLRLOG_CONSOLE(D, "New file created for logging");
        } else {
            XPLRLOG_CONSOLE(E, "Error in creating file for logging");
        }
    }
    if (err == XPLR_SD_OK) {
        XPLRLOG_CONSOLE(D, "Successfully freed space for logging");
        ret = XPLR_LOG_OK;
    } else {
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}

/**
 * Function that is called to check if there is free space for a message to be logged
*/
static xplrLog_error_t xplrLogCheckSpace(xplrLog_t *xplrLog)
{
    xplrLog_error_t ret;
    uint64_t total, free;
    struct stat st;
    FILE *fp;
    int tmp;

    free = xplrSdGetFreeSpace(xplrLog->sd);
    if (free == 0) {
        XPLRLOG_CONSOLE(W, "Free size in SD is not enough to log");
        ret = XPLR_LOG_ERROR;
    } else {
        fp = xplrSdOpenFile(xplrLog->logFilename, XPLR_FILE_MODE_APPEND);
        if (fp == NULL) {
            XPLRLOG_CONSOLE(E, "Error in opening log file <%s>", xplrLog->logFilename);
            ret = XPLR_LOG_ERROR;
        } else {
            xplrSdCloseFile(fp, xplrLog->logFilename, false);
            tmp = stat(xplrLog->logFilename, &st);
            if (tmp == 0) {
                switch (xplrLog->maxSizeType) {
                    case XPLR_SIZE_GB:
                        total = (xplrLog->maxSize * GB);
                        break;
                    case XPLR_SIZE_MB:
                        total = (xplrLog->maxSize * MB);
                        break;
                    case XPLR_SIZE_KB:
                        total = (xplrLog->maxSize * KB);
                        break;
                    default:
                        XPLRLOG_CONSOLE(E, "Error in max file size configuration");
                        total = 0;
                        break;
                }
                if (total > 0) {
                    if (total >= st.st_size) {
                        xplrLog->freeSpace = total - st.st_size;
                    } else {
                        xplrLog->freeSpace = 0;
                    }
                    ret = XPLR_LOG_OK;
                } else {
                    ret = XPLR_LOG_ERROR;
                }
            } else {
                XPLRLOG_CONSOLE(E, "Error in finding log file's size");
                ret = XPLR_LOG_ERROR;
            }
        }
    }

    return ret;
}

/**
 * Function that is called to force the log buffer to write it's contents in the SD card
*/
static xplrLog_error_t xplrLogFlushBuffer(xplrLog_t *xplrLog)
{

    xplrLog_error_t ret;
    xplrSd_error_t err;
    int u8Err;

    if (xplrLog->tag == XPLR_LOG_DEVICE_ERROR || xplrLog->tag == XPLR_LOG_DEVICE_INFO) {
        err = xplrSdWriteFileString(xplrLog->sd,
                                    xplrLog->logFilename,
                                    xplrLog->buffer,
                                    XPLR_FILE_MODE_APPEND);
    } else {
        u8Err = xplrSdWriteFileU8(xplrLog->sd,
                                  xplrLog->logFilename,
                                  (uint8_t *)xplrLog->buffer,
                                  xplrLog->bufferIndex,
                                  XPLR_FILE_MODE_APPEND);
        if (u8Err > 0) {
            err = XPLR_SD_OK;
        } else {
            err = XPLR_SD_ERROR;
        }
    }
    if (err == XPLR_SD_OK) {
        ret = XPLR_LOG_OK;
    } else {
        ret = XPLR_LOG_ERROR;
    }

    return ret;
}