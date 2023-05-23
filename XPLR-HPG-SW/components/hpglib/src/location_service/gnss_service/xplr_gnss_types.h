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

#ifndef _XPLR_GNSS_TYPES_H_
#define _XPLR_GNSS_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include <stdint.h>
#include "./../../../../components/ubxlib/ubxlib.h"

/** @file
 * @brief This header file defines the types used in gnss service API, such as
 * location data and gnss device settings.
 */

/**
 * Struct that contains location metrics
 */
typedef struct xplrGnssDeviceCfg_type {
    uDeviceCfg_t dvcSettings; 
    uNetworkCfgGnss_t dvcNetwork;
} xplrGnssDeviceCfg_t;

/**
 * Location Fix type
 * RTK: Real Time Kinematics
*/
typedef enum {
    XPLR_GNSS_LOCFIX_INVALID = 0,      /**< Invalid fix */
    XPLR_GNSS_LOCFIX_2D3D,             /**< 2D/3D fix */
    XPLR_GNSS_LOCFIX_DGNSS,            /**< Differential GNSS */
    XPLR_GNSS_LOCFIX_NOTUSED = 3,      /**< Value 3 is not defined */
    XPLR_GNSS_LOCFIX_FIXED_RTK,        /**< Fixed RTK*/
    XPLR_GNSS_LOCFIX_FLOAT_RTK,        /**< Floating RTK */
    XPLR_GNSS_LOCFIX_DEAD_RECKONING    /**< Dead Reckoning */
} xplrGnssLocFixType_t;

/**
 * Struct that contains accuracy metrics
 */
typedef struct xplrGnssAccuracy_type {
    uint32_t horizontal; /**< horizontal accuracy value in mm */
    uint32_t vertical;   /**< vertical   accuracy value in mm */
} xplrGnssAccuracy_t;

/**
 * Struct that contains location metrics
 */
typedef struct xplrGnssLocation_type {
    xplrGnssAccuracy_t   accuracy;      /**< accuracy metrics */
    uLocation_t          location;      /**< ubxlib location struct */
    xplrGnssLocFixType_t locFixType;    /**< location fix type */
} xplrGnssLocation_t;

/**
 * IMPORTANT: Never change the order of the following enum.
 * The following values describe the source of correction data
*/
typedef enum {
    XPLR_GNSS_CORRECTION_FROM_IP = 0,    /**< source is IP - MQTT */
    XPLR_GNSS_CORRECTION_FROM_LBAND      /**< source is LBAND */
} xplrGnssCorrDataSrc_t;

#endif

// End of file