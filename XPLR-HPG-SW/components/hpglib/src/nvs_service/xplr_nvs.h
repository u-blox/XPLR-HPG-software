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

#ifndef _XPLR_NVS_H_
#define _XPLR_NVS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "nvs_flash.h"
#include "./../../xplr_hpglib_cfg.h"


/** @file
 * @brief This header file defines a generic storage drivers utilizing esp-idf
 * nvs library. To be used by components and examples that need to save persistent
 * options and settings.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to xplr_nvs module. */
typedef enum {
    XPLR_NVS_ERROR = -1,    /**< process returned with errors. */
    XPLR_NVS_OK,            /**< indicates success of returning process. */
    XPLR_NVS_BUSY           /**< returning process currently busy. */
} xplrNvs_error_t;

/** XPLR NVS struct.
 * Holds required data and parameters for the API.
*/
typedef struct xplrNvs_type {
    char                tag[16];    /**< namespace for data stored in memory. */
    nvs_handle_t        handler;    /**< NVS handler from esp-idf storage API. */
} xplrNvs_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize nvs driver.
 * 
 * @param  nvs  driver struct to initialize.
 * @return      XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsInit(xplrNvs_t* nvs, const char* namespace);

/**
 * @brief De-initialize nvs driver.
 * 
 * @param  nvs  driver struct to de-initialize.
 * @return      XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsDeInit(xplrNvs_t* nvs);

/**
 * @brief Erase nvs namespace (all data).
 * 
 * @param  nvs  driver struct to de-initialize.
 * @return      XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsErase(xplrNvs_t* nvs);

/**
 * @brief Erase key nvs namespace.
 * 
 * @param  nvs  driver struct to de-initialize.
 * @param  key  key to erase from namespace
 * @return      XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsEraseKey(xplrNvs_t* nvs, const char* key);

/**
 * @brief Read unsigned byte from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to uint8_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadU8(xplrNvs_t* nvs, const char* key, uint8_t* value);

/**
 * @brief Read unsigned half-word (uint16_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to uint16_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadU16(xplrNvs_t* nvs, const char* key, uint16_t* value);

/**
 * @brief Read unsigned word (uint32_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to uint32_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadU32(xplrNvs_t* nvs, const char* key, uint32_t* value);

/**
 * @brief Read unsigned long long (uint64_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to uint64_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadU64(xplrNvs_t* nvs, const char* key, uint64_t* value);

/**
 * @brief Read signed byte from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to int8_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadI8(xplrNvs_t* nvs, const char* key, int8_t* value);

/**
 * @brief Read signed half-word (int16_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to int16_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadI16(xplrNvs_t* nvs, const char* key, int16_t* value);

/**
 * @brief Read signed word (int32_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to int32_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadI32(xplrNvs_t* nvs, const char* key, int32_t* value);

/**
 * @brief Read signed long long (int64_t) from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read
 * @param  value  pointer to int64_t to store key value
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadI64(xplrNvs_t* nvs, const char* key, int64_t* value);

/**
 * @brief Read string from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read.
 * @param  value  pointer to char array to store key value.
 * @param  size   pointer to size_t holding value length.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadString(xplrNvs_t* nvs, const char* key, char* value, size_t *size);

/**
 * @brief Read hex string from namespace key.
 * 
 * @param  nvs    driver struct to perform read.
 * @param  key    keyname value to read.
 * @param  value  pointer to char array to store key value.
 * @param  size   pointer to size_t holding value length.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsReadStringHex(xplrNvs_t* nvs, const char* key, char* value, size_t *size);

/**
 * @brief Write unsigned byte to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  uint8_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteU8(xplrNvs_t* nvs, const char* key, uint8_t value);

/**
 * @brief Write unsigned half-word (uint16_t) to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  uint16_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteU16(xplrNvs_t* nvs, const char* key, uint16_t value);

/**
 * @brief Write unsigned word (uint32_t) to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  uint32_t to write.
 * @return XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteU32(xplrNvs_t* nvs, const char* key, uint32_t value);

/**
 * @brief Write unsigned long long (uint64_t) to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  uint64_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteU64(xplrNvs_t* nvs, const char* key, uint64_t value);

/**
 * @brief Write signed byte to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  int8_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteI8(xplrNvs_t* nvs, const char* key, int8_t value);

/**
 * @brief Write signed half-word (int16_t) to namespace key.
 * 
 * @param  nvs    driver struct to write.
 * @param  key    keyname value to write.
 * @param  value  int16_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteI16(xplrNvs_t* nvs, const char* key, int16_t value);

/**
 * @brief Write signed word (int32_t) to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  int32_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteI32(xplrNvs_t* nvs, const char* key, int32_t value);

/**
 * @brief Write signed long long (int64_t) to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  int64_t to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteI64(xplrNvs_t* nvs, const char* key, int64_t value);

/**
 * @brief Write string to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  pointer to char array to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteString(xplrNvs_t* nvs, const char* key, const char* value);

/**
 * @brief Write hex string to namespace key.
 * 
 * @param  nvs    driver struct to perform write.
 * @param  key    keyname value to write.
 * @param  value  pointer to char array to write.
 * @return        XPLR_NVS_OK on success, XPLR_NVS_ERROR otherwise.
 */
xplrNvs_error_t xplrNvsWriteStringHex(xplrNvs_t* nvs, const char* key, const char* value);

#ifdef __cplusplus
}
#endif
#endif //_XPLR_NVS_H_

// End of file