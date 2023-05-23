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

#ifndef _XPLR_COMMON_H_
#define _XPLR_COMMON_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "esp_err.h"

/** @file
 * @brief This header file defines common functions used by other hpglib modules.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Useful for getting element count from arrays
 */
#define ELEMENTCNT(X)   (sizeof(X)) / sizeof((X)[0])

/*
 * Converts microseconds to seconds
 */
#define MICROTOSEC(X)   ((X) / 1000000ULL)

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Convert an md5 hash to binary DER format.
 *        Used in xplr_com component when storing / retrieving certificates to memory.
 *
 * @param  pHex  md5 hash as returned from cellular module.
 * @param  pBin  array to store the converted binary code.
 * @return       true on success, false otherwise.
 */
bool xplrCommonConvertHash(const char *pHex, char *pBin);

/**
 * @brief Calculate md5 hash of given ascii stream.
 *
 * @param  pInput  pointer to input buffer.
 * @param  size    size of input buffer.
 * @param  pOut    pointer to out buffer containing digest message.
 * @return         0 on success, negative error code otherwise.
 */
int32_t xplrCommonMd5Get(const unsigned char *pInput, size_t size, unsigned char *pOut);

/**
 * @brief Remove port info suffix from a given url.
 *
 * @param  serverUrl       url containing port info at the end.
 * @param  serverName      array to store modified url.
 * @param  serverNameSize  size of the array to store the url.
 * @return                 num of chars removed, negative error code otherwise.
 */
int32_t xplrRemovePortInfo(const char *serverUrl, char *serverName, size_t serverNameSize);

/**
 * @brief Add port info at the end of a given string (aka server hostname).
 *
 * @param  str   string to process.
 * @param  port  port number to append in given string.
 * @return       zero on success, negative error code otherwise.
 */
int32_t xplrAddPortInfo(char *str, uint16_t port);

/**
 * @brief Remove all instances of a specified character from a given string.
 *
 * @param  str  string to process.
 * @param  ch   character to remove from string.
 * @return      num of chars removed, negative error code otherwise.
 */
int32_t xplrRemoveChar(char *str, const char ch);

/**
 * @brief Retrieve wifi mac address of host mcu.
 *
 * @param  mac  array to store retrieved mac. Must be at least 6 bytes long.
 * @return      zero on success, negative error code otherwise.
 */
int32_t xplrGetDeviceMac(uint8_t *mac);

/**
 * @brief Convert epoch timestamp (secs) to human readable date
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t timestampToDate(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Convert epoch timestamp (secs) to human readable time.
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t timestampToTime(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Convert epoch timestamp (secs) to human readable date-time.
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t timestampToDateTime(int64_t timeStamp, char *res, uint8_t maxLen);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_COMMON_H_