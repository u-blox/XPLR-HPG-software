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

#ifndef _XPLR_LBAND_TYPES_H_
#define _XPLR_LBAND_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"

/** @file
 * @brief This header file defines the types used in lband service API,
 * mostly lband device settings.
 */

/*INDENT-OFF*/
typedef enum {
    XPLR_LBAND_FREQUENCY_INVALID = -1,
    XPLR_LBAND_FREQUENCY_EU = 0,
    XPLR_LBAND_FREQUENCY_US
} xplrLbandRegion_t;

/**
 * Struct containing settings for LBAND Correction
 * data settings such as frequency and region
 */
typedef struct xplrLbandCorrDataCfg_type {
    xplrLbandRegion_t region; /**< Region configured for frequency */
    uint32_t freq;            /**< Hardware specific settings. */
} xplrLbandCorrDataCfg_t;

/**
 * Struct that contains location metrics
 */
typedef struct xplrLbandDeviceCfg_type {
    xplrLocationDevConf_t hwConf;           /**< Hardware specific settings. */
    xplrLbandCorrDataCfg_t corrDataConf;    /**< Correction data configuration. */
    uDeviceHandle_t *destHandler;           /**< GNSS module destination handler to push data to. */
} xplrLbandDeviceCfg_t;
/*INDENT-ON*/
#endif