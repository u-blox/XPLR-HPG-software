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

#ifndef _XPLR_LOG_H_
#define _XPLR_LOG_H_

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include "./../sd_service/xplr_sd.h"
#include "./../common/xplr_common.h"
#include "./../../xplr_hpglib_cfg.h"

/**
 * @file
 * @brief This header file defines the hih level functions
 * required to be able to configure, initialize, terminate 
 * and execute the logging functions to the onboard SD card 
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

#if (1 == XPLR_HPGLIB_LOG_ENABLED)
#define XPLRLOG(xplrLog, message, ...) xplrLogFile(xplrLog, message)
#else
#define XPLRLOG(message, ...) do{} while(0)
#endif

#define KB  (1024LLU)
#define MB  1024*KB
#define GB  1024*MB

#define LOG_MAXIMUM_NAME_SIZE   20      //Maximum size of log file name
#define LOG_BUFFER_MAX_SIZE     256     //Maximum size of log buffer

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

// *INDENT-OFF*

typedef xplrSd_size_t   xplrLog_size_t;

typedef enum {
    XPLR_LOG_ERROR = -1,
    XPLR_LOG_OK
} xplrLog_error_t;

typedef enum {
    XPLR_LOG_DEVICE_ERROR = 0,      /**< Errors are to be logged in*/
    XPLR_LOG_DEVICE_INFO,           /**< General information for the board is to be logged*/
    XPLR_LOG_DEVICE_ZED,            /**< Device whose data is to be logged is in the ZED family of chips*/
    XPLR_LOG_DEVICE_NEO             /**< Device whose data is to be logged is in the NEO family of chips */
} xplrLog_dvcTag_t;

typedef struct xplrLog_type {
    xplrSd_t            *sd;                                            /**< Pointer to the struct containing the configuration of the SD service*/
    xplrLog_dvcTag_t    tag;                                            /**< Device tag of the logging to determine the type of data to be logged*/
    char                buffer[LOG_BUFFER_MAX_SIZE + 1];                /**< Internal buffer that keeps messages and stores them in the SD card when full
                                                                           < in order to achieve better performance*/
    char                logFilename[LOG_MAXIMUM_NAME_SIZE + 256 + 1];   /**< Name of the logging file*/
    uint16_t            bufferIndex;                                    /**< Index to the internal buffer to keep track of how full it is*/
    uint16_t            maxSize;                                        /**< Maximum logging file size before erase (e.g. if 10MBytes this should be 10)*/
    xplrLog_size_t      maxSizeType;                                    /**< Maximum logging file size before erase (e.g. if 10MBytes this should be XPLR_SIZE_MB)*/
    uint64_t            freeSpace;                                      /**< Free space of the SD card in maxSizeType Bytes*/
    bool                logEnable;                                      /**< Flag that enables/disables logging*/
} xplrLog_t;

// *INDENT-ON*

/**
 * PUBLIC FUNCTIONS
*/

/**
 * @brief Function that initializes the log service
 *
 * @param xplrLog       Pointer to the log struct
 * @param tag           Type of logging desired (In case of Info or Error logging will be in ASCII format, otherwise in binary)
 * @param logFilename   String pointer to the desired filename of the log file
 * @param maxSize       Unsigned integer to show file max allowed size (e.g. if 10MB is to be the max size this value should be 10)
 * @param sizeType      Size type value to indicate the size (e.g. if 10MB is to be the max size this value should be XPLR_SIZE_MB)
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogInit(xplrLog_t *xplrLog,
                            xplrLog_dvcTag_t tag,
                            char *logFilename,
                            uint16_t maxSize,
                            xplrLog_size_t maxSizeType);

/**
 * @brief Function that de-initializes the logging service
 *
 * @param xplrLog       Pointer to the log struct
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogDeInit(xplrLog_t *xplrLog);


/**
 * @brief Function that logs data to the SD card, in the corresponding file.
 *
 * @param xplrLog       Pointer to the log struct
 * @param message       The message to be logged
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogFile(xplrLog_t *xplrLog, char *message);

#ifdef __cplusplus
}
#endif
#endif /* _XPLR_LOG_H_*/