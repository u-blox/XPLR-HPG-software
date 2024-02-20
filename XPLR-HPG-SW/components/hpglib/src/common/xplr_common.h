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
#include <stdbool.h>
#include <stdint.h>

/** Certificate type */
typedef enum {
    XPLR_COMMON_CERT_INVALID = -1,    /**< invalid or not supported supported. */
    XPLR_COMMON_CERT,            /**< PointPerfect client certificate. */
    XPLR_COMMON_CERT_KEY,             /**< PointPerfect client private key. */
    XPLR_COMMON_CERT_ROOTCA           /**< aws root ca certificate*/
} xplr_common_cert_type_t;

/** @file
 * @brief This header file defines common functions used by other hpglib modules.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLR_COMMON_CERT_SIZE_MAX          (2 * 1024)

/**
 * Useful for getting element count from arrays
 */
#define ELEMENTCNT(X)   (sizeof(X)) / sizeof((X)[0])

/*
 * Converts microseconds to seconds
 */
#define MICROTOSEC(X)   ((X) / 1000000ULL)

/*
 * Converts microseconds to milliseconds
 */
#define MICROTOMILL(X)  ((X) / 1000ULL)

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

typedef struct xplr_cfg_app_type {
    uint32_t runTime;
    uint32_t locInterval;
    uint32_t statInterval;
    bool     mqttWdgEnable;
} xplr_cfg_app_t;

typedef struct xplr_cfg_cell_type {
    char apn[32];
} xplr_cfg_cell_t;

typedef struct xplr_cfg_wifi_type {
    char ssid[64];
    char pwd[64];
} xplr_cfg_wifi_t;

typedef struct xplr_cfg_thingstream_type {
    char region[32];
    char uCenterConfigFilename[64];
} xplr_cfg_thingstream_t;

typedef struct xplr_cfg_logInstance_type {
    char description[64];
    char filename[64];
    bool enable;
    bool erasePrev;
    uint64_t sizeInterval;
} xplr_cfg_logInstance_t;

typedef struct xplr_cfg_log_type {
    uint8_t numOfInstances;
    uint64_t filenameInterval;
    bool hotPlugEnable;
    xplr_cfg_logInstance_t instance[XPLR_LOG_MAX_INSTANCES];
} xplr_cfg_log_t;

typedef struct xplr_cfg_dr_type {
    bool enable;
    bool printImuData;
    uint32_t printInterval;
} xplr_cfg_dr_t;

typedef struct xplr_cfg_gnss_type {
    int8_t module;
    uint8_t corrDataSrc;
} xplr_cfg_gnss_t;

typedef struct xplr_cfg_type {
    xplr_cfg_app_t appCfg;
    xplr_cfg_cell_t cellCfg;
    xplr_cfg_wifi_t wifiCfg;
    xplr_cfg_thingstream_t tsCfg;
    xplr_cfg_log_t logCfg;
    xplr_cfg_dr_t drCfg;
    xplr_cfg_gnss_t gnssCfg;
} xplr_cfg_t;

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
 * @brief Set base MAC address of the MCU to u-blox MAC. Base MAC is the WIFI_STA address,
 * the other 3 MAC addresses (WIFI_AP, BT, ETH) are calculated from the base MAC. So we only need to set the base MAC.
 *
 * @return      ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t xplrSetDeviceMacToUblox(void);

/**
 * @brief Convert epoch timestamp (secs) to human readable date
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t xplrTimestampToDate(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Convert epoch timestamp (secs) to human readable time.
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t xplrTimestampToTime(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Convert epoch timestamp (secs) to human readable date-time.
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           zero on success, negative error code otherwise.
 */
esp_err_t xplrTimestampToDateTime(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Convert epoch timestamp (secs) to human readable date-time for use in filenames.
 *
 * @param timeStamp  epoch timestamp in seconds.
 * @param res        result buffer.
 * @param maxLen     buffer max length.
 * @return           length of timestamp on success, -1 otherwise.
 */
int8_t xplrTimestampToDateTimeForFilename(int64_t timeStamp, char *res, uint8_t maxLen);

/**
 * @brief Function that periodically prints the complete list of tasks and also prints
 *        the current, minimum and maximum value of heap memory.
 *
 * @param periodSecs time in seconds for the periodic print of memory usage.
*/
void xplrMemUsagePrint(uint8_t periodSecs);

/**
 * @brief Function that parses module settings from the xplr_config.json
 *        configuration file
 *
 * @param payload   the data fetched from the json file
 * @param settings  struct containing the parsed configuration options and settings
 * @return          ESP_OK if parsing was successful, ESP_FAIL if unsuccessful
*/
esp_err_t xplrParseConfigSettings(char *payload, xplr_cfg_t *settings);

/**
 * @brief Function that converts certificates by adding appropriate line termination
 *        characters.
 *
 * @param cert          Certificate to convert
 * @param type          Certificate type
 * @param addNewLines   Set to true boolean value if new line characters are required
 *                      to the certificate format.
 *
 * @return          ESP_OK on success, ESP_FAIL on error
*/
esp_err_t xplrPpConfigFileFormatCert(char *cert, xplr_common_cert_type_t type, bool addNewLines);

#ifdef __cplusplus
}
#endif
#endif // _XPLR_COMMON_H_