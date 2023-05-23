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

#ifndef _XPLR_COMMON_HELPERS_TYPES_H_
#define _XPLR_COMMON_HELPERS_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"

/** @file
 * @brief This header file defines the types used in location service service API, mostly
 * location device settings.
 */

/**
 * Simple struct basic device settings.
 * It contains the absolute minimum to setup a device handler
 * Used both for GNSS and LBAND version M9 modules
 */
typedef struct xplrGnssDevBase_type {
    uDeviceHandle_t   dHandler; /**< ubxlib device handler */
    uDeviceCfg_t      dConfig;  /**< ubxlib device settings */
    uNetworkCfgGnss_t dNetwork; /**< ubxlib device network type */
} xplrGnssDevBase_t;

/**
 * Simple struct containing some device info
 */
typedef struct xplrGnssDevInfo_type {
    uint8_t i2cAddress; /**< I2C address in hex */
    uint8_t i2cPort;    /**< I2C port as described by ESP-IDF */
    uint8_t pinSda;     /**< SDA pin value */
    uint8_t pinScl;     /**< SCL pin value */
    uint8_t id[5];      /**< Device id array */
    uGnssVersionType_t pVer;    /**< GNSS/LBAND device version struct */
} xplrGnssDevInfo_t;

#endif