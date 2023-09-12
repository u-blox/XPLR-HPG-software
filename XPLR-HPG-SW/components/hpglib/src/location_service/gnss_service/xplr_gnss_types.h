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
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

/** @file
 * @brief This header file defines the types used in gnss service API, such as
 * location data, dead reckoning data, and gnss device settings.
 */

/**
 * Maximum available slots for sensors
 */
#define XPLR_GNSS_SENSORS_MAX_CNT   15

/**
 * Length of decryption keys.
 * This is standard at 60 bytes.
 * If for some reason this length changes than
 * you will have to adjust this value accordingly
 */
#define XPLR_GNSS_DECRYPTION_KEYS_LEN 128

/*INDENT-OFF*/

/**
 * GNSS FSM return values
 */
typedef enum {
    XPLR_GNSS_ERROR = -1,   /**< process returned with errors. */
    XPLR_GNSS_OK = 0,       /**< indicates success of returning process. */
    XPLR_GNSS_BUSY          /**< indicates process is busy. */
} xplrGnssError_t;

/** States describing the MQTT Client. */
typedef enum {
    XPLR_GNSS_STATE_UNKNOWN = -4,           /**< Unknown state due to invalid device profile */
    XPLR_GNSS_STATE_UNCONFIGURED,           /**< GNSS is not initialized */
    XPLR_GNSS_STATE_TIMEOUT,                /**< timeout state. */
    XPLR_GNSS_STATE_ERROR,                  /**< error state. */
    XPLR_GNSS_STATE_DEVICE_READY = 0,       /**< ok state. */
    XPLR_GNSS_STATE_ENABLE_LOG,             /**< enables logging if configured. */
    XPLR_GNSS_STATE_DEVICE_OPEN,            /**< opening device state. */
    XPLR_GNSS_STATE_CREATE_SEMAPHORE,       /**< creating semaphore state. */
    XPLR_GNSS_STATE_SET_GEN_LOC_SETTINGS,   /**< setting up generic GNSS settings. */
    XPLR_GNSS_STATE_SET_CFG_DECR_KEYS,      /**< sets potential configured/saved keys. */
    XPLR_GNSS_STATE_SET_CFG_CORR_SOURCE,    /**< sets potential configured/saved correction data source. */
    XPLR_GNSS_STATE_START_ASYNCS,           /**< starts all asyncs. */
    XPLR_GNSS_STATE_NVS_INIT,               /**< initializing nvs. */
    XPLR_GNSS_STATE_DR_INIT,                /**< initializing Dead Reckoning. */
    XPLR_GNSS_STATE_DR_MANUAL_CALIB,        /**< execute manual calibration for Dead Reckoning. */
    XPLR_GNSS_STATE_DR_AUTO_CALIB,          /**< execute auto calibration for Dead Reckoning. */
    XPLR_GNSS_STATE_DR_START,               /**< start Dead Reckoning. */
    XPLR_GNSS_STATE_DEVICE_RESTART,         /**< restarting device. */
    XPLR_GNSS_STATE_WAIT,                   /**< wait non blocking state. */
    XPLR_GNSS_STATE_DEVICE_STOP,            /**< stops device - unconfigured state. */
    XPLR_GNSS_STATE_NVS_UPDATE,             /**< update/save data to NVS. */
} xplrGnssStates_t;

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
 * IMPORTANT: Never change the order of the following enum.
 * The following values describe the source of correction data
*/
typedef enum {
    XPLR_GNSS_CORRECTION_FROM_IP = 0,   /**< source is IP - MQTT. */
    XPLR_GNSS_CORRECTION_FROM_LBAND     /**< source is LBAND. */
} xplrGnssCorrDataSrc_t;

/**
 * IMU/DR Calibration mode
 */
typedef enum {
    XPLR_GNSS_IMU_CALIBRATION_MANUAL = 0,   /**< Auto calibrate Yaw - Pitch - Roll. */
    XPLR_GNSS_IMU_CALIBRATION_AUTO          /**< Used manually provided Yaw - Pitch - Roll. */
} xplrGnssImuCalibMode_t;

/**
 * Vehicle dynamics mode
 */
typedef enum {
    XPLR_GNSS_DYNMODE_PORTABLE = 0,     /**< Portable. */
    XPLR_GNSS_DYNMODE_STATIONARY = 2,   /**< Stationary. */
    XPLR_GNSS_DYNMODE_PEDESTRIAN,       /**< Pedestrian. */
    XPLR_GNSS_DYNMODE_AUTOMOTIVE,       /**< Default - Automotive. */
    XPLR_GNSS_DYNMODE_SEA,              /**< Sea - Maritime */
    XPLR_GNSS_DYNMODE_AIR1,             /**< Airborne mode 1. */
    XPLR_GNSS_DYNMODE_AIR2,             /**< Airborne mode 2. */
    XPLR_GNSS_DYNMODE_AIR4,             /**< Airborne mode 4. */
    XPLR_GNSS_DYNMODE_WRIST = 9,        /**< Wristwatch. */
    XPLR_GNSS_DYNMODE_LAWN_MOWER = 11,  /**< Lawnmower robot. */
    XPLR_GNSS_DYNMODE_ESCOOTER          /**< E-Scooter. */
} xplrGnssDynMode_t;

/**
 * IMU/DR calibration status
 */
typedef enum {
    XPLR_GNSS_ALG_STATUS_UNKNOWN = -1,              /**< Unknown state. */
    XPLR_GNSS_ALG_STATUS_USER_DEFINED = 0,          /**< User defined calibration data (Manual calib). */
    XPLR_GNSS_ALG_STATUS_ROLLPITCH_CALIBRATING,     /**< Roll - Pitch calibrating. */
    XPLR_GNSS_ALG_STATUS_ROLLPITCHYAW_CALIBRATING,  /**< Roll - Pitch - Yaw calibrating. */
    XPLR_GNSS_ALG_STATUS_USING_COARSE_ALIGNMENT,    /**< Using coarse alignment (no wheel tick available). */
    XPLR_GNSS_ALG_STATUS_USING_FINE_ALIGNMENT       /**< Using fine alignment (wheel tick in use). */
} xplrGnssEsfAlgStatus_t;

/**
 * IMU/DR Fusion mode/status
 */
typedef enum {
    XPLR_GNSS_FUSION_MODE_UNKNOWN = -1,         /**< Unknown state. */
    XPLR_GNSS_FUSION_MODE_INITIALIZATION = 0,   /**< Fusion mode initializing/calibrating. */
    XPLR_GNSS_FUSION_MODE_ENABLED,              /**< Fusion mode is enabled and used. */
    XPLR_GNSS_FUSION_MODE_SUSPENDED,            /**< Fusion mode is suspended. */
    XPLR_GNSS_FUSION_MODE_DISABLED              /**< Fusion mode is disabled. */
} xplrGnssFusionMode_t;

/**
 * Sensor type
 */
typedef enum {
    XPLR_GNSS_SENSOR_GYRO_Z_ANG_RATE = 5,       /**< Gyro Z axis angular rate. */
    XPLR_GNSS_SENSOR_WT_RL_WHEEL = 8,           /**< Wheel Tick - Real Left. */
    XPLR_GNSS_SENSOR_WT_RR_WHEEL,               /**< Wheel Tick - Rear Right. */
    XPLR_GNSS_SENSOR_WT_ST_WHEEL,               /**< Wheel Tick - SIngle Tick. */
    XPLR_GNSS_SENSOR_SPEED,                     /**< Sensor speed */
    XPLR_GNSS_SENSOR_GYRO_TEMP,                 /**< Gyro temperature sensor. */
    XPLR_GNSS_SENSOR_GYRO_Y_ANG_RATE,           /**< Gyro Y axis angular rate. */
    XPLR_GNSS_SENSOR_GYRO_X_ANG_RATE,           /**< Gyro X axis angular rate. */
    XPLR_GNSS_SENSOR_ACCEL_X_SPCF_FORCE = 16,   /**< Accelerometer X axis specific force. */
    XPLR_GNSS_SENSOR_ACCEL_Y_SPCF_FORCE,        /**< Accelerometer Y axis specific force. */
    XPLR_GNSS_SENSOR_ACCEL_Z_SPCF_FORCE         /**< Accelerometer Z axis specific force. */
} xplrGnssSensorType_t;

/**
 * Sensor calibration status
 */
typedef enum {
    XPLR_GNNS_SENSOR_CALIB_UNKNOWN = -1,        /**< Sensors calibration status unknown. */
    XPLR_GNNS_SENSOR_CALIB_NOT_CALIBRATED = 0,  /**< Sensor is not calibrated.. */
    XPLR_GNNS_SENSOR_CALIB_CALIBRATING,         /**< Sensors is calibrating.. */
    XPLR_GNNS_SENSOR_CALIB_CALIBRATED           /**< Sensors is calibrated.. */
} xplrGnssSensorCalibStatus_t;

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
 * Alignment angle values
 */
typedef struct xplrGnssImuAlignmentVals_type {
    uint32_t yaw;       /**< Yaw alignment value. */
    int16_t  pitch;     /**< Pitch alignment value. */  
    int16_t  roll;      /**< Roll alignment value. */
} xplrGnssImuAlignmentVals_t;

/**
 * IMU/DR Alignment information as read from the module
 */
typedef struct xplrGnssImuAlignmentInfo_type {
    xplrGnssImuCalibMode_t mode;        /**< Calibration mode. */
    xplrGnssEsfAlgStatus_t status;      /**< Calibration status. */
    xplrGnssImuAlignmentVals_t data;    /**< Alignment angle values. */
} xplrGnssImuAlignmentInfo_t;

/**
 * IMU Sensor faults
 */
typedef union __attribute__ ((__packed__)) xplrGnssImuEsfStatSensorFaults_type{
    struct {
        uint8_t badMeasurements    : 1; /**< Bad measurements detected. */
        uint8_t badTTag            : 1; /**< Bad measurement time-tags detected. */
        uint8_t missingMeasurments : 1; /**< Missing or time-misaligned measurements detected. */
        uint8_t noisyMeas          : 1; /**< High measurement noise-level detected. */
    } singleFaults;                     /**< Faults as single bit-fields. */    
    uint8_t allFaults;                  /**< Faults as a single uint8_t. */
} xplrGnssImuEsfStatSensorFaults_t;

/**
 * Sensor information struct
 */
typedef struct xplrGnssImuSensorStatus_type {
    xplrGnssSensorType_t type;                  /**< Sensor type. */
    bool used;                                  /**< Is sensors used in fusion. */
    bool ready;                                 /**< Is sensor ready to be used. */
    xplrGnssSensorCalibStatus_t calibStatus;    /**< Is sensor calibrated. */
    uint8_t freq;                               /**< Sensors refresh frequency */
    xplrGnssImuEsfStatSensorFaults_t faults;    /**< Sensor faults. */
} xplrGnssImuSensorStatus_t;

/**
 * Contains information regarding fusion status
 */
typedef struct xplrGnssImuFusionStatus_type {
    xplrGnssFusionMode_t fusionMode;                                /**< Current fusion mode achieved */
    uint8_t numSens;                                                /**< Total number of sensors used by GNSS module */
    xplrGnssImuSensorStatus_t sensor[XPLR_GNSS_SENSORS_MAX_CNT];    /**< Sensors statuses  */
} xplrGnssImuFusionStatus_t;

/**
 * Validity flags for sensors measurements
 */
typedef union __attribute__ ((__packed__)) xplrGnssImuVehicleDynamicsFlags_type {
    struct {
        uint8_t xAngRateValid : 1;  /**< Compensated x-axis angular rate data validity flag */
        uint8_t yAngRateValid : 1;  /**< Compensated y-axis angular rate data validity flag */
        uint8_t zAngRateValid : 1;  /**< Compensated z-axis angular rate data validity flag */
        uint8_t xAccelValid   : 1;  /**< Compensated x-axis acceleration data validity flag */
        uint8_t yAccelValid   : 1;  /**< Compensated y-axis acceleration data validity flag */
        uint8_t zAccelValid   : 1;  /**< Compensated z-axis acceleration data validity flag */
    } singleFlags;                  /**< flags as single bit-fields. */
    uint8_t allFlags;               /**< flags as a single uint8_t. */
} xplrGnssImuVehicleDynamicsFlags_t;

/**
 * Vehicle dynamics measurement data
 */
typedef struct xplrGnssImuVehDynMeas_type {
    xplrGnssImuVehicleDynamicsFlags_t valFlags; /**< Validity Flags */
    int32_t xAngRate;   /**< Compensated x-axis angular rate */
    int32_t yAngRate;   /**< Compensated y-axis angular rate */
    int32_t zAngRate;   /**< Compensated x-axis angular rate */
    int32_t xAccel;     /**< Compensated x-axis acceleration (gravity-free) */
    int32_t yAccel;     /**< Compensated y-axis acceleration (gravity-free) */
    int32_t zAccel;     /**< Compensated z-axis acceleration (gravity-free) */
} xplrGnssImuVehDynMeas_t;

/**
 * GNSS Dead Reckoning settings
 */
typedef struct xplrGnssDeadReckoningCfg_type {
    bool enable;
    xplrGnssDynMode_t vehicleDynMode;       /**< Vehicle dynamics mode. */
    xplrGnssImuCalibMode_t mode;            /**< Calibration mode. */   
    xplrGnssImuAlignmentVals_t alignVals;   /**< ALignment values if Manual calibration is used. */
} xplrGnssDeadReckoningCfg_t;

/**
 * Correction data decryption keys settings
 */
typedef struct xplrGnssDecryptionKeys_type {
    char keys[XPLR_GNSS_DECRYPTION_KEYS_LEN];   /**< Key in ubx ready format. */
    uint16_t size;                              /**< key length. */
} xplrGnssDecryptionKeys_t;

/**
 * Correction data settings
 */
typedef struct xplrGnssCorrectionCfg_type {
    xplrGnssDecryptionKeys_t keys;  /**< Correction data decryption keys settings. */
    xplrGnssCorrDataSrc_t source;   /**< Correction data source. */
} xplrGnssCorrectionCfg_t;

/**
 * Struct that contains location metrics
 */
typedef struct xplrGnssDeviceCfg_type {
    xplrLocationDevConf_t hw;           /**< Hardware specific settings. */
    xplrGnssDeadReckoningCfg_t dr;      /**< Dead Reckoning settings. */
    xplrGnssCorrectionCfg_t corrData;   /**< Correction data settings. */
} xplrGnssDeviceCfg_t;

/**
 * Enumeration that contains the different logging submodules for the gnss module
*/
typedef enum{
    XPLR_GNSS_LOG_MODULE_INVALID = -1,  /**< Unknown or invalid submodule */
    XPLR_GNSS_LOG_MODULE_CONSOLE,       /**< Log messages of serial debug */
    XPLR_GNSS_LOG_MODULE_UBX,           /**< Async messages of ZED module */
    XPLR_GNSS_LOG_MODULE_ALL            /**< All gnss log submodules */
} xplrGnssLogModule_t;

/**
 * Struct that contains the required variables for the async logging
 * of gnss messages.
*/
typedef struct xplrGnssAsyncLog_type{
    RingbufHandle_t     xRingBuffer;        /**< Ring Buffer to store messages coming from the GNSS async callback */
    TaskHandle_t        gnssLogTaskHandle;  /**< Handler of the async logging task */
    xplrLog_t           logCfg;             /**< log struct for the async logging*/
    uint8_t             firstDvcProfile;    /**< The device profile of the first device initializing the logging task */
    bool                isInit;             /**< Flag raised when the first device has initialized the logging task */
    bool                isEnabled;          /**< Flag that enables/halts the logging task */
} xplrGnssAsyncLog_t;
/*INDENT-ON*/

#endif

// End of file