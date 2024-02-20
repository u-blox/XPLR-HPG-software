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
#include "xplr_log_types.h"
#include "./../sd_service/xplr_sd.h"

/**
 * @file
 * @brief This header file defines the hih level functions
 * required to be able to configure, initialize, terminate
 * and execute the logging functions to the onboard SD card
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Function that initializes the log service for a specific instance
 *
 * @param tag           Type of logging desired (In case of Info or Error logging will be in ASCII format, otherwise in binary)
 * @param logFilename   String pointer to the desired filename of the log file
 * @param sizeInterval  Size in bytes. If the file reaches that size the filename gets incremented. If 0 this will happen in default value (4GB)
 * @param replace       When true, if a file exists with the same name it will be cleared. When false the data will get appended if a file exists with the same name.
 * @return              Index to the internal log instance array in success, -1 in failure
*/
int8_t xplrLogInit(xplrLog_dvcTag_t tag, char *logFilename, uint64_t sizeInterval, bool replace);

/**
 * @brief Function that de-initializes the logging service
 *
 * @param dvcProfile    Index to the internal log instance array
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogDeInit(int8_t index);

/**
 * @brief Function that de-initializes all logging instances
 *
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogDeInitAll(void);

/**
 * @brief Function that checks if logging is enabled for the logging instance of the index
 *
 * @param index         Index to the internal log instance array
 * @return              true if logging is enabled, false if it is not
*/
bool xplrLogIsEnabled(int8_t index);

/**
 * @brief Function that enables logging for the specific index
 *
 * @param index         Index to the internal log instance array
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogEnable(int8_t index);

/**
 * @brief Function that disables logging for the specific index
 *
 * @param index         Index to the internal log instance array
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogDisable(int8_t index);

/**
 * @brief Function that enables all logging instances (if initialized with xplrLogInit)
 *
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogEnableAll(void);

/**
 * @brief Function that disables all logging instances
 *
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogDisableAll(void);

/**
 * @brief Function that gets the logging file size
 *
 * @param dvcProfile    Index to the internal log instance array
 * @return              The size of the file in bytes
*/
int64_t xplrLogGetFileSize(int8_t index);

/**
 * @brief Function that changes the log filename. Must have called xplrLogInit first
 *        to obtain a valid index.
 *
 * @param index         Index to the internal log instance array
 * @param filename      The desired new filename (must not exceed 63 characters)
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogSetFilename(int8_t index, char *filename);

/**
 * @brief Function that changes the log device tag. Must have called xplrLogInit first
 *        to obtain a valid index.
 *
 * @param index         Index to the internal log instance array
 * @param tag           Type of logging desired (In case of Info or Error logging will be in ASCII format, otherwise in binary)
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogSetDeviceTag(int8_t index, xplrLog_dvcTag_t tag);

/**
 * @brief Function that logs data to the SD card, in the corresponding file.
 *
 * @param index         Index to the internal log instance array
 * @param fmt           The format of the message to be logged
 * @return              XPLR_LOG_OK in success or XPLR_LOG_ERROR in failure
*/
xplrLog_error_t xplrLogFile(int8_t index, xplrLog_Opt_t opt, char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif /* _XPLR_LOG_H_*/