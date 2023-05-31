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

/**
 * Device and network config
 */
typedef struct xplrLbandDeviceCfg_type {
    uDeviceCfg_t dvcSettings;        /**< Device config */
    uNetworkCfgGnss_t dvcNetwork;    /**< Network config */
} xplrLbandDeviceCfg_t;

typedef enum {
    XPLR_LBAND_FREQUENCY_EU = 0,
    XPLR_LBAND_FREQUENCY_US
} xplrLbandRegion;

#endif