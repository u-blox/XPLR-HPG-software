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
#ifndef _XPLR_LOG_TYPES_H_
#define _XPLR_LOG_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines the types used in logging service API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLRLOG(index, opt, fmt, ...) xplrLogFile(index, opt, fmt, ##__VA_ARGS__);

#define XPLR_LOG_MAX_TIMEOUT        (TickType_t)pdMS_TO_TICKS(100)      /**< Maximum timeout for semaphores and mutexes */

/* ----------------------------------------------------------------
 * TYPE DEFINITIONS AND ENUMERATIONS
 * -------------------------------------------------------------- */

typedef enum {
    XPLR_LOG_ERROR = -1,
    XPLR_LOG_OK
} xplrLog_error_t;

typedef enum {
    XPLR_LOG_DEVICE_ERROR = 0,      /**< Error string message type logging */
    XPLR_LOG_DEVICE_INFO,           /**< General string message type logging */
    XPLR_LOG_DEVICE_ZED             /**< Binary message type logging (e.g. ubx type messages from ZED) */
} xplrLog_dvcTag_t;

typedef enum {
    XPLR_LOG_PRINT_ONLY = 0,        /**< Option to print to the console but not to log in the SD card */
    XPLR_LOG_SD_ONLY,               /**< Option to log in the SD card but not to print anything in console */
    XPLR_LOG_SD_AND_PRINT           /**< Option to both print in the console and log to the SD card */
} xplrLog_Opt_t;

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif
#endif //_XPLR_LOG_TYPES_H_
// End of file