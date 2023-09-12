/*
 * Copyright 2019-2022 u-blox
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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include <stdint.h>
#include <limits.h>
#include "string.h"
#include "esp_task_wdt.h"
#include "u_cfg_app_platform_specific.h"
#include "xplr_gnss.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRLOCATION_LOG_ACTIVE))
#define XPLRGNSS_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrGnss", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRGNSS_CONSOLE(tag, message, ...)  esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrGnss", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrGnss", __FUNCTION__, __LINE__, ## __VA_ARGS__);\
    XPLRLOG(&locationLog,buff2Log);
#elif ((0 == XPLRGNSS_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
#define XPLRGNSS_CONSOLE(tag, message, ...)\
    snprintf(&buff2Log[0], ELEMENTCNT(buff2Log), "[(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrGnss", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    XPLRLOG(&locationLog,buff2Log)
#else
#define XPLRGNSS_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Maximum time for watchdog semaphore to wait
 */
#define XPLR_GNSS_MAX_WATCHDOG_SEM_WAITMS   (500U)

/**
 * Maximum threshold for watchdog timeout
 */
#define XPLR_GNSS_WATCHDOG_TIMEOUT_SECS     (10U)

/**
 * Timeout for GNSS device openning
 */
#define XPLR_GNSS_DEVICE_OPEN_TIMEOUT       (60U)

/**
 * Waiting/Grace period in seconds before first device open command
 * after restart
 */
#define XPLR_GNSS_WAIT_OPEN_AFTER_RESTART   (2U)

/**
 * Waiting/Grace period in seconds before consecutive
 * device open retries
 */
#define XPLR_GNSS_WAIT_OPEN                 (1U)

/**
 * Maximum times to retry reading a value from
 * GNSS
 */
#define XPLR_GNSS_MAX_READ_RETRIES          (20U)

/**
 * Buffer sizes
 */
#define XPLR_GNSS_NMEA_BUFF_SIZE            (256U)
#define XPLR_GNSS_UBX_BUFF_SIZE             (896U)
#define XPLR_GNSS_SENS_ERR_BUFF_SIZE        (64U)

/**
 * Set this option to 1 if you want the logging initialization to be blocking
 * (it needs to be initialized without errors, otherwise all gnss init returns error)
*/
#define XPLR_GNSS_LOG_BLOCKING  0

/**
 * Set this option to 1 if you want extra debug messages
 */
#define XPLR_GNSS_XTRA_DEBUG    0

/**
 * Buffer length used in location fix type
 */
#define XPLR_GNSS_LOCFIX_STR_MAX_LEN    (16U)

#define XPLR_GNSS_LOG_RING_BUF_SIZE     3*1024 //bytes
#define XPLR_GNSS_LOG_RING_BUF_TIMEOUT  portMAX_DELAY

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

typedef enum {
    XPLR_GNSS_DR_STOP,  /**< stop opt for Dead Reckoning */
    XPLR_GNSS_DR_START  /**< start opt for Dead Reckoning */
} xplrGnssDrStartOpt_t;

/**
 * Struct that contains location data
 */
typedef struct xplrGnssLocData_type {
    xplrGnssLocation_t locData;     /**< location info */
} xplrGnssLocData_t;

/**
 * Dead Reckoning data
 */
typedef struct xplrGnssDrData_type {
    xplrGnssImuAlignmentInfo_t info;    /**< IMU Alignment Information */
    xplrGnssImuFusionStatus_t status;   /**< IMU Fusion status */
    xplrGnssImuVehDynMeas_t dynamics;   /**< IMU Vehicle dynamics */
} xplrGnssDrData_t;

/**
 * Async Ids
 **/
typedef struct xplrGnssAsyncIds_type {
    int32_t ahUbxId;  /**< UBX message async ID */
    int32_t ahNmeaId; /**< NMEA message async ID */
} xplrGnssAsyncIds_t;

/**
 * Flags for different functions
 */
typedef union __attribute__((__packed__)) xplrGnssStatus_type {
    struct {
        uint8_t gnssIsConfigured    : 1;    /**< is gnss configured and ready to start */
        uint8_t gnssIsDrEnabled     : 1;    /**< flags FSM that DR enable was run */
        uint8_t gnssComingFromConf  : 1;    /**< shows if the previous state (before wait) was CONFIG */
        uint8_t gnssRequestStop     : 1;    /**< flags FSM to stop device*/
        uint8_t gnssRequestRestart  : 1;    /**< flags FSM to restart device*/
        uint8_t drExecManualCalib   : 1;    /**< flags FSM to switch to/execute manual calibration */
        uint8_t drUpdateNvs         : 2;    /**< flags FSM to save calibration data to NVS
                                                 0-> do nothing, 1->update nvs, 2->nvs updated*/
        uint8_t drIsCalibrated      : 1;    /**< is Calibration done */
        uint8_t locMsgDataRefreshed : 1;    /**< check if any type of location metrics was changed */
        uint8_t locMsgDataAvailable : 1;    /**< check if GNSS message is available for reading */
        uint8_t errorFlag           : 1;    /**< send GNSS FSM to error state. Used from functions */
        uint8_t reserved            : 4;    /**< reserved/padding */
    } status;
    uint16_t value;
} xplrGnssStatus_t;

/**
 * Running Context data
 */
typedef struct xplrGnssOptions_type {
    uDeviceHandle_t dvcHandler;     /**< ubxlib device handler */
    SemaphoreHandle_t xSemWatchdog; /**< locking semaphore for watchdog */
    xplrLocNvs_t storage;           /**< NVS storage */
    xplrGnssAsyncIds_t asyncIds;    /**< message async IDs */
    xplrGnssStates_t state[2];      /**< FSM states */
    xplrGnssStatus_t flags;         /**< flags */
    int64_t lastActTime;            /**< Last time an action was executed */
    int64_t lastWatchdogTime;       /**< Last time DR flag was refreshed */
    int64_t genericTimer;           /**< Used to time misc actions */
    uint8_t ubxRetries;             /**< ubx lib read command retry */
} xplrGnssOptions_t;
/*INDENT-ON*/

/**
 * Settings and data struct for GNSS devices
 */
typedef struct xplrGnss_type {
    xplrGnssDeviceCfg_t *conf;  /**< GNSS module configuration */
    xplrGnssOptions_t options;  /**< options */
    xplrGnssLocData_t locData;  /**< location data */
    xplrGnssDrData_t drData;    /**< dead reckoning data */
    xplrLog_t *log;             /**< logging struct */
} xplrGnss_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

// *INDENT-OFF*

/**
 * Default calibration angles
 * All vals in integer degrees
 */
static const int32_t gnssSensDefaultCalibValYaw   = 40000;  // 400.00
static const int32_t gnssSensDefaultCalibValPitch = 10000;  // 100.00
static const int32_t gnssSensDefaultCalibValRoll  = 20000;  // 200.00

/**
 * Valid calibration limits
 * All vals in integer degrees
 */
static const uint32_t gnssSensMaxValYaw   =  36000;   // 360.00
static const int32_t  gnssSensMaxValPitch =   9000;   //  90.00
static const int32_t  gnssSensMinValPitch =  -9000;   // -90.00
static const int32_t  gnssSensMaxValRoll  =  18000;   // 360.00
static const int32_t  gnssSensMinValRoll  = -18000;   // 360.00

/**
 * Fusion type strings
 * Taken as is from ZED F9R specs file
 */
static const char gnssStrFusionModeUnknown[]   = "Unknown";
static const char gnssStrFusionModeInit[]      = "Initializing";
static const char gnssStrFusionModeEnable[]    = "Enabled";
static const char gnssStrFusionModeSuspended[] = "Suspended";
static const char gnssStrFusionModeDisabled[]  = "Disabled";

/**
 * Calibration status strings
 * Taken as is from ZED F9R specs file
 */
static const char gnssStrCalibStatusUnknown[]  = "Unknown";
static const char gnssStrCalibStatusUserDef[]  = "user-defined";
static const char gnssStrCalibStatusRpCalib[]  = "IMU-mount roll/pitch angles alignment is ongoing";
static const char gnssStrCalibStatusRpyCalib[] = "IMU-mount roll/pitch/yaw angles alignment is ongoing";
static const char gnssStrCalibStatusCoarse[]   = "coarse IMU-mount alignment are used";
static const char gnssStrCalibStatusFine[]     = "fine IMU-mount alignment are used";

/**
 * Sensors type strings
 * Taken as is from ZED F9R specs file
 */
static const char gnssStrSensTypeGyroZAng[]   = "Gyroscope Z Angular Rate";
static const char gnssStrSensTypeWtRL[]       = "Wheel Tick Rear Left";
static const char gnssStrSensTypeWtRR[]       = "Wheel Tick Rear Right";
static const char gnssStrSensTypeWtST[]       = "Wheel Tick Single Tick";
static const char gnssStrSensTypeSpeed[]      = "Speed";
static const char gnssStrSensTypeGyroTemp[]   = "Gyroscope TEMP";
static const char gnssStrSensTypeGyroYAng[]   = "Gyroscope Y Angular Rate";
static const char gnssStrSensTypeGyroXAng[]   = "Gyroscope X Angular Rate";
static const char gnssStrSensTypeAccelXSpcF[] = "Accelerometer X Specific Force";
static const char gnssStrSensTypeAccelYSpcF[] = "Accelerometer Y Specific Force";
static const char gnssStrSensTypeAccelZSpcF[] = "Accelerometer Z Specific Force";
static const char gnssStrSensTypeUnknown[]    = "Unknown Type";

/**
 * Sensors error strings
 * Taken as is from ZED F9R specs file
 */
static const char gnssStrSensStateErrNone[]      = "No Errors";
static const char gnssStrSensStateErrBadMeas[]   = "Bad Meas";
static const char gnssStrSensStateErrBadTtag[]   = "Bad Time Tag";
static const char gnssStrSensStateErrMissMeas[]  = "Missing Meas";
static const char gnssStrSensStateErrNoisyMeas[] = "High Noise";

/**
 * Used to translate fix types to strings for printing
 */
static const char gnssStrLocfixInvalid[]  = "NO FIX";
static const char gnssStrLocfix3D[]       = "3D";
static const char gnssStrLocfixDgnss[]    = "DGNSS";
static const char gnssStrLocfixRtkFixed[] = "RTK-FIXED";
static const char gnssStrLocfixRtkFloat[] = "RTK-FLOAT";
static const char gnssStrLocfixDeadReck[] = "DEAD RECKONING";

// *INDENT-ON*

/**
 * In order to find the size of each key we have to look at the end of the name.
 * According to that value we can find the size:
 *
 * -----+------------------------------------------------------------
 * CODE | DESCRIPTION
 * -----+------------------------------------------------------------
 * U1   | unsigned 8-bit integer 1 0…28-1 1
 * -----+------------------------------------------------------------
 * I1   | signed 8-bit integer, two's complement 1 -27…27-1 1
 * -----+------------------------------------------------------------
 * X1   | 8-bit bitfield 1 n/a n/a
 * -----+------------------------------------------------------------
 * U2   | unsigned little-endian 16-bit integer 2 0…216-1 1
 * -----+------------------------------------------------------------
 * I2   | signed little-endian 16-bit integer, two's complement 2 -215…215-1 1
 * -----+------------------------------------------------------------
 * X2   | 16-bit little-endian bitfield 2 n/a n/a
 * -----+------------------------------------------------------------
 * U4   | unsigned little-endian 32-bit integer 4 0…232-1 1
 * -----+------------------------------------------------------------
 * I4   | signed little-endian 32-bit integer, two's complement 4 -231…231-1 1
 * -----+------------------------------------------------------------
 * X4   | 32-bit little-endian bitfield 4 n
 * -----+------------------------------------------------------------
 * R4   | IEEE 754 single (32-bit) precision 4 -2127…2127 ~ value·2-24
 * -----+------------------------------------------------------------
 * R8   | IEEE 754 double (64-bit) precision 8 -21023…21023 ~ value·2-53
 * -----+------------------------------------------------------------
 * CH   | ASCII / ISO 8859-1 char (8-bit) 1 n/a n/a
 * -----+------------------------------------------------------------
 * U:n  | unsigned bitfield value of n bits width var. variable variable
 * -----+------------------------------------------------------------
 * I:n  | signed (two's complement) bitfield value of n bits width var. variable variable
 * -----+------------------------------------------------------------
 * S:n  | signed bitfield value of n bits width, in sign (most significant bit) and magnitude (remaining bits) notation var. variable variable
 * -----+------------------------------------------------------------
 *
 * Example of value
 *      U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1 <--- ending
 * has an ending of "U1" that means
 * that it is an 8bit integer according to the table
 *
 * Taken from the following docs:
 * https://content.u-blox.com/sites/default/files/documents/u-blox-F9-HPG-1.32_InterfaceDescription_UBX-22008968.pdf
 */

/**
 * Common Location Generic GNSS settings
 */
static const uGnssCfgVal_t gnssGenericSettings[] = {
    {U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L, 1},                 /**< High precision mode */
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1, 1},  /**< HPPOSLLH messages enable on I2C*/
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1, 1},       /**< PVT messages enable on I2C */
    {U_GNSS_CFG_VAL_KEY_ID_SFCORE_USE_SF_L, 0},                 /**< Disable internal IMU */

    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_I2C_U1, 1},       /**< ~700bytes */
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_I2C_U1, 1},

    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_I2C_U1, 1},
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_I2C_U1, 1}
};

/*INDENT-OFF*/
/**
 * Common Dead Reckoning Generic settings
 */
static const uGnssCfgVal_t gnssGenericDrSettings[] = {
    {U_GNSS_CFG_VAL_KEY_ID_SFODO_USE_WT_PIN_L, 0},          /**< disable ODO AKA Wheel Tick */
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_I2C_U1, 1},   /**< ESF-INS sensor information message enable on I2C */
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_I2C_U1, 1},   /**< ESF-ALG calibration information message enable on I2C */
    {U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_I2C_U1, 1} /**< ESF-STATUS calibration status message enable on I2C */
};

/**
 * Message ID for UBX_NAV_PVT
 * Geolocation reading
 */
static const uGnssMessageId_t msgIdNavPvt = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x0107 /**< main class message id: a | message id: b ==> 0xaabb*/
};

/**
 * Message ID for HPPOSLLH
 * Accuracy reading
 */
static const uGnssMessageId_t msgIdHpposllh = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x0114
};

/**
 * Message ID for ESF-INS
 * Vehicle dynamics information (vehicle dynamics)
 */
static const uGnssMessageId_t msgIdEsfIns = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x1015
};

/**
 * Message ID for ESF-STATUS
 * Imu Fusion Sensors Statuses
 */
static const uGnssMessageId_t msgIdEsfStatus = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x1010
};

/**
 * Message ID for ESF-ALG
 * Imu Fusion Alignment data
 */
static const uGnssMessageId_t msgIdEsfAlg = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = 0x1014
};

/**
 * All UBX protocol message IDs
 */
static const uGnssMessageId_t msgIdUbxMessages = {
    .type = U_GNSS_PROTOCOL_UBX,
    .id.ubx = U_GNSS_UBX_MESSAGE_ALL
};

/**
 * Message ID for GNGGA
 * Fix type reading
 */
static const uGnssMessageId_t msgIdFixType = {
    .type = U_GNSS_PROTOCOL_NMEA,
    .id.pNmea = "GNGGA"
};

/**
 * All NMEA protocol message IDs
 */
static const uGnssMessageId_t msgIdUNmeaMessages = {
    .type = U_GNSS_PROTOCOL_NMEA,
    .id.pNmea = ""
};

/**
 * NVS namespace prefix
 */
static const char nvsNamespace[] = "gnssDvc_";

/**
 * An array of gnss devices
 */
static xplrGnss_t dvc[XPLRGNSS_NUMOF_DEVICES] = {{.options.state = {XPLR_GNSS_STATE_UNCONFIGURED, 
                                                                    XPLR_GNSS_STATE_UNCONFIGURED},
                                                  .options.xSemWatchdog = NULL,
                                                  .options.flags.value = 0,
                                                  .options.asyncIds.ahNmeaId = -1,
                                                  .options.asyncIds.ahUbxId  = -1,
                                                  .options.lastActTime = 0,
                                                  .options.lastWatchdogTime = 0,
                                                  .options.genericTimer = 0}};

static const char *gLocationUrlPart = "https://maps.google.com/?q=";

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
// Semaphore to guarantee atomic access to the async log task struct
SemaphoreHandle_t xSemaphore = NULL;
/* Async logging task struct */
xplrGnssAsyncLog_t asyncLog = {0};
/* Flag to indicate that the semaphore for the log struct has been created*/
static bool semaphoreCreated;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t gnssDeviceOpen(uint8_t dvcProfile);
static esp_err_t gnssDeviceRestart(uint8_t dvcProfile);
static esp_err_t gnssDeviceStop(uint8_t dvcProfile);
static esp_err_t gnssDeviceClose(uint8_t dvcProfile);
static void gnssResetoptionsFlags(uint8_t dvcProfile);
static void gnssResetoptionsTimers(uint8_t dvcProfile);
static int32_t gnssAsyncStopper(uint8_t dvcProfile, int32_t handler);
static bool gnssUbxIsMessageId(const uGnssMessageId_t *msgIdIncoming, 
                               const uGnssMessageId_t *msgIdToFilter);
static bool gnssNmeaIsMessageId(const uGnssMessageId_t *msgIdIncoming, 
                                const uGnssMessageId_t *msgIdToFilter);
static void gnssUpdateNextState(uint8_t dvcProfile,
                                xplrGnssStates_t nextState);
static esp_err_t gnssLocSetGenericSettings(uint8_t dvcProfile);
static bool gnssIsDvcProfileValid(uint8_t dvcProfile);

/* ------- NVS ------ */

static esp_err_t gnssNvsWriteDefaults(uint8_t dvcProfile);
static esp_err_t gnssNvsInit(uint8_t dvcProfile);
static esp_err_t gnssNvsLoad(uint8_t dvcProfile);
static esp_err_t gnssNvsReadConfig(uint8_t dvcProfile);
static esp_err_t gnssNvsUpdate(uint8_t dvcProfile);
static esp_err_t gnssNvsErase(uint8_t dvcProfile);

/* ---- Location ---- */

static esp_err_t gnssSetCorrDataSource(uint8_t dvcProfile);
static esp_err_t gnssSetDecrKeys(uint8_t dvcProfile);
static esp_err_t gnssGetLocFixType(xplrGnss_t *locDvc, char *buffer);
static esp_err_t gnssGeolocationParser(xplrGnss_t *locDvc, char *buffer);
static esp_err_t gnssAccuracyParser(xplrGnss_t *locDvc, char *buffer);

/* - Dead Reckoning - */

static esp_err_t gnssDrManualCalib(uint8_t dvcProfile);
static esp_err_t gnssDrAutoCalib(uint8_t dvcProfile);
static esp_err_t gnssDrSetGenericSettings(uint8_t dvcProfile);
static esp_err_t gnssDrStartStop(uint8_t dvcProfile, xplrGnssDrStartOpt_t opt);
static bool gnssIsDrEnabled(uint8_t dvcProfile);
static esp_err_t gnssFeedWatchdog(xplrGnss_t *gnssDvc);
static bool gnssCheckWatchdog(uint8_t dvcProfile);
static esp_err_t gnssDrSetVehicleType(uint8_t dvcProfile);
static esp_err_t gnssEsfInsParser(xplrGnss_t *locDvc, char *buffer);
static esp_err_t gnssImuSetCalibData(uint8_t dvcProfile);
static esp_err_t gnssEsfAlgParser(xplrGnss_t *locDvc, char *buffer);
static esp_err_t gnssEsfStatusParser(xplrGnss_t *locDvc, char *buffer);
static bool gnssCheckYawValLimits(uint32_t yaw);
static bool gnssCheckPitchValLimits(int16_t pitch);
static bool gnssCheckRollValLimits(int16_t roll);
static bool gnssCheckAlignValsLimits(uint8_t dvcProfile);
static esp_err_t gnssDrSetAlignMode(uint8_t dvcProfile);

/* ---- PRINTERS ---- */

static esp_err_t gnssSensorMeasErrToString(xplrGnssImuEsfStatSensorFaults_t *faults,
                                           char *errStr,
                                           uint8_t maxLen);
static esp_err_t gnssFixTypeToString(xplrGnssLocation_t *locData,
                                     char *buffer,
                                     uint8_t maxLen);
static esp_err_t gnssCalibModeToString(xplrGnssImuCalibMode_t *type,
                                       char *typeStr,
                                       uint8_t maxLen);
static esp_err_t gnssCalibStatToString(xplrGnssEsfAlgStatus_t *status,
                                       char *statusStr,
                                       uint8_t maxLen);
static esp_err_t gnssFusionModeToString(xplrGnssFusionMode_t *mode,
                                        char *modeStr,
                                        uint8_t maxLen);
static esp_err_t gnssSensorTypeToString(xplrGnssSensorType_t *type,
                                        char *typeStr,
                                        uint8_t maxLen);
static esp_err_t gnssImuAlignStatPrinter(xplrGnssImuFusionStatus_t *status);
static void gnssLocationPrinter(char *pLocFixTypeStr, xplrGnssLocation_t *locData);
static void gnssLocPrintLocType(xplrGnssLocation_t *locData);
static void gnssLocPrintFixType(char *pLocFixTypeStr);
static void gnssLocPrintLongLat(xplrGnssLocation_t *locData);
static void gnssLocPrintAlt(xplrGnssLocation_t *locData);
static void gnssLocPrintRad(xplrGnssLocation_t *locData);
static void gnssLocPrintSpeed(xplrGnssLocation_t *locData);
static void gnssLocPrintAcc(xplrGnssLocation_t *locData);
static void gnssLocPrintSatNo(xplrGnssLocation_t *locData);
static void gnssLocPrintTime(xplrGnssLocation_t *locData);

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
static void gnssLogLocationPrinter(char *pLocFixTypeStr,
                                   xplrGnssLocation_t *locData);
#endif
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
// Task that reads from the ring buffer and logs to the SD card
static void gnssLogTask(void *pvParams);
static void gnssLogCallback(char *buffer, uint16_t length);
#endif

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void gnssUbxProtocolCB(uDeviceHandle_t gnssHandle,
                              const uGnssMessageId_t *msgIdToFilter,
                              int32_t errorCodeOrLength,
                              void *callbackParam);
static void gnssNmeaProtocolCB(uDeviceHandle_t gnssHandle,
                               const uGnssMessageId_t *msgIdToFilter,
                               int32_t errorCodeOrLength,
                               void *callbackParam);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

esp_err_t xplrGnssUbxlibInit(void)
{
    esp_err_t ret;
    ret = xplrHelpersUbxlibInit();
    return ret;
}

esp_err_t xplrGnssStartDevice(uint8_t dvcProfile, xplrGnssDeviceCfg_t *conf)
{
    xplrGnss_t *locDvc;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        if ((locDvc->options.flags.status.gnssIsConfigured == 1) && 
            (xplrGnssGetCurrentState(dvcProfile) != XPLR_GNSS_STATE_UNCONFIGURED)) {
            XPLRGNSS_CONSOLE(W, "Gnss with ID [%d] is already configured and running.", dvcProfile);
        } else {
            locDvc->conf = conf;
            locDvc->options.flags.status.gnssIsConfigured = 1;
            XPLRGNSS_CONSOLE(D, "GNSS module configured successfully.");
        }
        ret = ESP_OK;
    }

    return ret;
}

xplrGnssError_t xplrGnssFsm(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t espRet;
    xplrGnssError_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        switch (locDvc->options.state[0]) {
            case XPLR_GNSS_STATE_UNCONFIGURED:
                if (locDvc->options.flags.status.gnssIsConfigured) {
                    locDvc->options.flags.status.gnssComingFromConf = 1;
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ENABLE_LOG);
                } else {
                    // do nothing
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_ENABLE_LOG:
#if (1 == XPLRGNSS_LOG_ACTIVE && 1 == XPLR_HPGLIB_LOG_ENABLED)
                XPLRGNSS_CONSOLE(D, "Logging is enabled. Trying to initialize.");
                espRet = xplrGnssAsyncLogInit(dvcProfile);
                if (espRet == ESP_OK || !XPLR_GNSS_LOG_BLOCKING) {
                    XPLRGNSS_CONSOLE(D, "Sucessfully initialized GNSS logging.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_OPEN);
                } else {
                    XPLRGNSS_CONSOLE(E, "GNSS init failed from the async log initialization");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
#else
                XPLRGNSS_CONSOLE(D, "Logging is not enabled. Skipping.");
                gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_OPEN);
#endif
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DEVICE_OPEN:
                locDvc->options.ubxRetries = 0;

                if (locDvc->options.lastActTime == 0) {
                    locDvc->options.lastActTime = esp_timer_get_time();
                } else {
                    // do nothing
                }

                if (!(MICROTOSEC(esp_timer_get_time() - locDvc->options.lastActTime) > XPLR_GNSS_DEVICE_OPEN_TIMEOUT)) {
                    espRet = gnssDeviceOpen(dvcProfile);
                    XPLRGNSS_CONSOLE(D, "Trying to open device.");
                    if (espRet == ESP_OK) {
                        espRet = xplrGnssPrintDeviceInfo(dvcProfile);
                        if (espRet != ESP_OK) {
                            XPLRGNSS_CONSOLE(W, "Failed to print GNSS device info");
                        } else {
                            // do nothing
                        }

                        if (locDvc->options.flags.status.gnssComingFromConf == 1) {
                            XPLRGNSS_CONSOLE(D, "Configuration Completed.");
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_RESTART);
                        } else {
                            XPLRGNSS_CONSOLE(D, "Restart Completed.");
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_CREATE_SEMAPHORE);
                        }
                        ret = XPLR_GNSS_OK;
                    } else {
                        locDvc->options.genericTimer = esp_timer_get_time();
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_WAIT);
                        ret = XPLR_GNSS_BUSY;
                    }
                } else {
                    XPLRGNSS_CONSOLE(E, "Openning GNSS device timed out!");
                    XPLRGNSS_CONSOLE(E, "Waited for [%d] seconds!", XPLR_GNSS_DEVICE_OPEN_TIMEOUT);
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_TIMEOUT);
                    ret = XPLR_GNSS_OK;
                }
                break;

            case XPLR_GNSS_STATE_CREATE_SEMAPHORE:
                if (locDvc->options.xSemWatchdog == NULL) {
                    locDvc->options.xSemWatchdog = xSemaphoreCreateBinary();
                    if (locDvc->options.xSemWatchdog == NULL) {
                        XPLRGNSS_CONSOLE(E, "Failed to create xSemWatchdog!");
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                    } else {
                        XPLRGNSS_CONSOLE(D, "xSemWatchdog created successfully");
                        if (xSemaphoreGive(locDvc->options.xSemWatchdog) == pdTRUE) {
                            XPLRGNSS_CONSOLE(D, "Successfully released xSemWatchdog!");
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_SET_GEN_LOC_SETTINGS);
                        } else {
                            XPLRGNSS_CONSOLE(E, "Failed to release xSemWatchdog!");
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                        }
                    }
                } else {
                    XPLRGNSS_CONSOLE(D, "xSemWatchdog is already initialiazed. No need to initialize.");
                    if (xSemaphoreTake(locDvc->options.xSemWatchdog, pdMS_TO_TICKS(500)) == pdTRUE) {
                        XPLRGNSS_CONSOLE(D, "Successfully released xSemWatchdog!");
                        xSemaphoreGive(locDvc->options.xSemWatchdog);
                    } else {
                        XPLRGNSS_CONSOLE(D, "xSemWatchdog is already free no need to release.");
                    }
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_SET_GEN_LOC_SETTINGS);
                }

                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_SET_GEN_LOC_SETTINGS:
                XPLRGNSS_CONSOLE(D, "Trying to set GNSS generic location settings.");
                espRet = gnssLocSetGenericSettings(dvcProfile);
                if (espRet == ESP_OK) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_SET_CFG_DECR_KEYS);
                    XPLRGNSS_CONSOLE(D, "Set generic location settings on GNSS module.");
                } else {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_SET_CFG_DECR_KEYS:
                XPLRGNSS_CONSOLE(D, "Trying to set correction data decryption keys.");
                if (locDvc->conf->corrData.keys.size > 0) {
                    espRet = gnssSetDecrKeys(dvcProfile);
                    if (espRet == ESP_OK) {
                        XPLRGNSS_CONSOLE(D, "Set configured decryption keys.");
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_SET_CFG_CORR_SOURCE);
                    } else {
                        XPLRGNSS_CONSOLE(E, "Failed to set configured decryption keys.");
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                    }
                } else {
                    XPLRGNSS_CONSOLE(D, "No keys stored. Skipping.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_SET_CFG_CORR_SOURCE);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_SET_CFG_CORR_SOURCE:
                XPLRGNSS_CONSOLE(D, "Trying to set correction data source.");
                espRet = gnssSetCorrDataSource(dvcProfile);
                if (espRet == ESP_OK) {
                    XPLRGNSS_CONSOLE(D, "Set configured correction data source.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_START_ASYNCS);
                } else {
                    XPLRGNSS_CONSOLE(E, "Failed to set configured correction data source.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_START_ASYNCS:
                XPLRGNSS_CONSOLE(D, "Trying to start async getters.");
                espRet = gnssFeedWatchdog(locDvc);
                if (espRet == ESP_OK) {
                    espRet = xplrGnssStartAllAsyncs(dvcProfile);
                    if (espRet == ESP_OK) {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_NVS_INIT);
                    } else {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                    }
                } else {
                    XPLRGNSS_CONSOLE(E, "Could not feed the watchdog for the first time!");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_NVS_INIT:
                XPLRGNSS_CONSOLE(D, "Trying to init NVS.");
                espRet = gnssNvsInit(dvcProfile);
                if (espRet == ESP_OK) {
                    if (locDvc->conf->dr.enable) {
                        XPLRGNSS_CONSOLE(D, "Detected Dead Reckoning enable option in config. Initializing Dead Reckoning.");
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_INIT);
                    } else {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_READY);
                    }
                    XPLRGNSS_CONSOLE(D, "Initialized NVS.");
                } else {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                    XPLRGNSS_CONSOLE(E, "Failed to initialize NVS!");
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DR_INIT:
                XPLRGNSS_CONSOLE(D, "Trying to init Dead Reckoning.");
                if (locDvc->conf->dr.mode == XPLR_GNSS_IMU_CALIBRATION_MANUAL) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_MANUAL_CALIB);
                } else if (locDvc->conf->dr.mode == XPLR_GNSS_IMU_CALIBRATION_AUTO) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_AUTO_CALIB);
                } else {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DR_MANUAL_CALIB:
                XPLRGNSS_CONSOLE(D, "Trying to execute IMU manual calibration.");
                locDvc->options.flags.status.drExecManualCalib = 0;
                espRet = gnssDrManualCalib(dvcProfile);
                if (espRet == ESP_OK) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_START);
                    XPLRGNSS_CONSOLE(D, "Executed Manual Calibration.");
                } else {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DR_AUTO_CALIB:
                XPLRGNSS_CONSOLE(D, "Trying to execute IMU automatic calibration.");
                espRet = gnssDrAutoCalib(dvcProfile);
                if (espRet == ESP_OK) {
                    if (locDvc->options.flags.status.drExecManualCalib) {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_MANUAL_CALIB);
                    } else {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_START);
                    }
                    XPLRGNSS_CONSOLE(D, "Executed Auto Calibration.");
                } else {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DR_START:
                XPLRGNSS_CONSOLE(D, "Trying to enable Dead Reckoning.");
                espRet = gnssDrStartStop(dvcProfile, XPLR_GNSS_DR_START);
                if (espRet == ESP_OK) {
                    locDvc->options.flags.status.gnssIsDrEnabled = 1;
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_READY);
                    XPLRGNSS_CONSOLE(D, "Started dead Reckoning.");
                } else {
                    XPLRGNSS_CONSOLE(E, "Failed to start dead Reckoning.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_NVS_UPDATE:
                XPLRGNSS_CONSOLE(D, "Trying to update/save to NVS.");
                locDvc->options.flags.status.drUpdateNvs = 2;
                locDvc->conf->dr.alignVals.yaw   = locDvc->drData.info.data.yaw;
                locDvc->conf->dr.alignVals.pitch = locDvc->drData.info.data.pitch;
                locDvc->conf->dr.alignVals.roll  = locDvc->drData.info.data.roll;
                espRet = gnssNvsUpdate(dvcProfile);
                if (espRet == ESP_OK) {
                    XPLRGNSS_CONSOLE(D, "Saved alignemnt data to NVS.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_READY);
                } else {
                    XPLRGNSS_CONSOLE(E, "Saving alignemnt data to NVS failed.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DEVICE_READY:
                if (locDvc->options.flags.status.errorFlag) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                } else if (locDvc->options.flags.status.gnssRequestStop) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_STOP);
                } else if (locDvc->options.flags.status.gnssRequestRestart ||
                           gnssCheckWatchdog(dvcProfile)) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_RESTART);
                } else {
                    if ((gnssIsDrEnabled(dvcProfile) == false) && 
                        (locDvc->conf->dr.enable == true)) {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DR_INIT);
                    } else if ((gnssIsDrEnabled(dvcProfile) == true) && 
                        (locDvc->conf->dr.enable == false)) {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_RESTART);
                    } else {
                        // do nothing
                    }

                    if (locDvc->options.flags.status.drUpdateNvs == 1) {
                        gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_NVS_UPDATE);
                    } else {
                        // do nothing
                    }
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DEVICE_RESTART:
                XPLRGNSS_CONSOLE(D, "Trying to restart GNSS device.");
                espRet = gnssDeviceRestart(dvcProfile);
                if (espRet == ESP_OK) {
                    XPLRGNSS_CONSOLE(D, "Restart succeeded.");
                    locDvc->options.flags.status.gnssIsConfigured = 1;
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_WAIT);
                } else {
                    XPLRGNSS_CONSOLE(E, "Restart failed.");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_DEVICE_STOP:
                XPLRGNSS_CONSOLE(D, "Trying to stop GNSS device.");
                espRet = gnssDeviceStop(dvcProfile);
                if (espRet == ESP_OK) {
                    XPLRGNSS_CONSOLE(D, "Device stopped.");
                    locDvc->options.state[0] = XPLR_GNSS_STATE_UNCONFIGURED;
                    locDvc->options.state[1] = locDvc->options.state[0];
                } else {
                    XPLRGNSS_CONSOLE(E, "Failed to stop device!");
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                }
                ret = XPLR_GNSS_OK;
                break;

            case XPLR_GNSS_STATE_WAIT:
                switch (locDvc->options.state[1]) {
                    case XPLR_GNSS_STATE_DEVICE_RESTART:
                        if (MICROTOSEC(esp_timer_get_time() - locDvc->options.genericTimer) > XPLR_GNSS_WAIT_OPEN_AFTER_RESTART) {
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_OPEN);
                        } else {
                            // do nothing
                        }
                        break;

                    case XPLR_GNSS_STATE_DEVICE_OPEN:
                        if (MICROTOSEC(esp_timer_get_time() - locDvc->options.genericTimer) > XPLR_GNSS_WAIT_OPEN) {
                            gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_OPEN);
                        } else {
                            // do nothing
                        }
                        break;

                    default:
                        // do nothing
                        break;
                }
                ret = XPLR_GNSS_BUSY;
                break;

            case XPLR_GNSS_STATE_TIMEOUT:
            case XPLR_GNSS_STATE_ERROR:
                if (locDvc->options.flags.status.gnssRequestStop == 1) {
                    gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_DEVICE_STOP);
                } else {
                    // do nothing
                }
                locDvc->options.ubxRetries = 0;
                ret = XPLR_GNSS_ERROR;
                break;

            default:
                XPLRGNSS_CONSOLE(E, "Unknown state detected!");
                gnssUpdateNextState(dvcProfile, XPLR_GNSS_STATE_ERROR);
                ret = XPLR_GNSS_OK;
                break;
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = XPLR_GNSS_ERROR;
    }

    return ret;
}

esp_err_t xplrGnssStopDevice(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    xplrGnssStates_t currentState;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        ret = ESP_ERR_INVALID_ARG;
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
    } else {
        locDvc = &dvc[dvcProfile];
        currentState = xplrGnssGetCurrentState(dvcProfile);
        if (currentState == XPLR_GNSS_STATE_DEVICE_READY ||
            currentState == XPLR_GNSS_STATE_ERROR ||
            currentState == XPLR_GNSS_STATE_TIMEOUT) {
            locDvc->options.flags.status.gnssRequestStop = 1;
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(W, "Gnss device is not in a valid state: Ready, Error or Timeout. Nothing to execute.");
            ret = ESP_OK;
        }
    }

    return ret;
}

esp_err_t xplrGnssUbxlibDeinit(void)
{
    esp_err_t ret;
    ret = xplrHlprLocSrvcUbxlibDeinit();
    return ret;
}

uDeviceHandle_t *xplrGnssGetHandler(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    uDeviceHandle_t *devHandler;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        devHandler = xplrHlprLocSrvcGetHandler(&locDvc->options.dvcHandler);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        devHandler = NULL;
    }

    return devHandler;
}

esp_err_t xplrGnssSetCorrectionDataSource(uint8_t dvcProfile, xplrGnssCorrDataSrc_t src)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    xplrGnssStates_t currentState;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        currentState = xplrGnssGetCurrentState(dvcProfile);
        if (currentState == XPLR_GNSS_STATE_DEVICE_READY) {
            if ((src == XPLR_GNSS_CORRECTION_FROM_IP) || (src == XPLR_GNSS_CORRECTION_FROM_LBAND)) {
                locDvc->conf->corrData.source = src;
                ret = gnssSetCorrDataSource(dvcProfile);
                if (ret != ESP_OK) {
                    XPLRGNSS_CONSOLE(E, "Error setting Correction Data source!");
                } else {
                    XPLRGNSS_CONSOLE(D, "Successfully set Correction Data source.");
                }
            } else {
                XPLRGNSS_CONSOLE(E, "Invalid correction data source [%d]!", src);
                ret = ESP_FAIL;
            }
        } else {
            XPLRGNSS_CONSOLE(E, "Gnss device is not in Ready state. Nothing to execute.");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t xplrGnssEnableDeadReckoning(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    xplrGnssStates_t currentState;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        currentState = xplrGnssGetCurrentState(dvcProfile);
        if (currentState == XPLR_GNSS_STATE_DEVICE_READY) {
            locDvc->conf->dr.enable = true;
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(E, "Gnss device is not in Ready state. Nothing to execute.");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t xplrGnssDisableDeadReckoning(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    xplrGnssStates_t currentState;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        currentState = xplrGnssGetCurrentState(dvcProfile);
        if (currentState == XPLR_GNSS_STATE_DEVICE_READY) {
            locDvc->conf->dr.enable = false;
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(E, "Gnss device is not in Ready state. Nothing to execute.");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t xplrGnssNmeaMessagesAsyncStart(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (locDvc->options.asyncIds.ahNmeaId >= 0) {
            XPLRGNSS_CONSOLE(D, "Looks like Gnss NMEA async is already running!");
            ret = ESP_OK;
        } else {
            locDvc->options.asyncIds.ahNmeaId = uGnssMsgReceiveStart(locDvc->options.dvcHandler,
                                                                     &msgIdUNmeaMessages,
                                                                     gnssNmeaProtocolCB,
                                                                     locDvc);
            if (locDvc->options.asyncIds.ahNmeaId < 0) {
                locDvc->options.asyncIds.ahNmeaId = -1;
                XPLRGNSS_CONSOLE(E,
                                 "Gnss NMEA async failed to start with error code [%d]",
                                 locDvc->options.asyncIds.ahNmeaId);
                ret = ESP_FAIL;
            } else {
                XPLRGNSS_CONSOLE(D, "Started Gnss NMEA async.");
                ret = ESP_OK;
            }
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssUbxMessagesAsyncStart(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (locDvc->options.asyncIds.ahUbxId >= 0) {
            XPLRGNSS_CONSOLE(D, "Looks like Gnss UBX Messages async is already running!");
            ret = ESP_OK;
        } else {
            locDvc->options.asyncIds.ahUbxId = uGnssMsgReceiveStart(locDvc->options.dvcHandler,
                                                                    &msgIdUbxMessages,
                                                                    gnssUbxProtocolCB,
                                                                    locDvc);
            if (locDvc->options.asyncIds.ahUbxId < 0) {
                XPLRGNSS_CONSOLE(E,
                                 "Gnss UBX Messages async failed to start with error code [%d]",
                                 locDvc->options.asyncIds.ahUbxId);
                locDvc->options.asyncIds.ahUbxId = -1;
                ret = ESP_FAIL;
            } else {
                XPLRGNSS_CONSOLE(D, "Started Gnss UBX Messages async.");
                ret = ESP_OK;
            }
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssStartAllAsyncs(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrGnssNmeaMessagesAsyncStart(dvcProfile);
        if (ret == ESP_OK) {
            ret = xplrGnssUbxMessagesAsyncStart(dvcProfile);
            if (ret == ESP_OK) {
                XPLRGNSS_CONSOLE(D, "Started all async getters.");
            } else {
                // do nothing
            }
        } else {
            // do nothing
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssNmeaMessagesAsyncStop(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    int32_t intRet;

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (locDvc->options.asyncIds.ahNmeaId < 0) {
            XPLRGNSS_CONSOLE(D, "Looks like Gnss Get Fix Type async is not running. Nothing to do.");
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(D, "Trying to stop Gnss Get Fix Type async.");
            xSemaphoreGive(locDvc->options.xSemWatchdog);
            intRet = gnssAsyncStopper(dvcProfile, locDvc->options.asyncIds.ahNmeaId);
            if (intRet == 0) {
                locDvc->options.asyncIds.ahNmeaId = -1;
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
            }
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssUbxMessagesAsyncStop(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    int32_t intRet;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (locDvc->options.asyncIds.ahUbxId < 0) {
            XPLRGNSS_CONSOLE(D, "Looks like Gnss UBX Messages async is not running. Nothing to do.");
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(D, "Trying to stop Gnss UBX Messages async.");
            xSemaphoreGive(locDvc->options.xSemWatchdog);
            intRet = gnssAsyncStopper(dvcProfile, locDvc->options.asyncIds.ahUbxId);
            if (intRet == 0) {
                locDvc->options.asyncIds.ahUbxId = -1;
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
            }
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssStopAllAsyncs(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        ret = xplrGnssUbxMessagesAsyncStop(dvcProfile);
        if (ret == ESP_OK) {
            ret = xplrGnssNmeaMessagesAsyncStop(dvcProfile);
            if (ret == ESP_OK) {
                XPLRGNSS_CONSOLE(D, "Stopped all async getters.");
            } else {
                // do nothing
            }
        } else {
            // do nothing
        }
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssOptionSingleValSet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     uint64_t value,
                                     uGnssCfgValLayer_t layer)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcOptionSingleValSet(&locDvc->options.dvcHandler,
                                                keyId,
                                                value,
                                                U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                layer);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssOptionMultiValSet(uint8_t dvcProfile,
                                    const uGnssCfgVal_t *list,
                                    size_t numValues,
                                    uGnssCfgValLayer_t layer)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcOptionMultiValSet(&locDvc->options.dvcHandler,
                                               list,
                                               numValues,
                                               U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                               layer);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssOptionSingleValGet(uint8_t dvcProfile,
                                     uint32_t keyId,
                                     void *value,
                                     size_t size,
                                     uGnssCfgValLayer_t layer)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcOptionSingleValGet(&locDvc->options.dvcHandler,
                                                keyId,
                                                value,
                                                size,
                                                layer);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssOptionMultiValGet(uint8_t dvcProfile,
                                    const uint32_t *keyIdList,
                                    size_t numKeyIds,
                                    uGnssCfgVal_t **list,
                                    uGnssCfgValLayer_t layer)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcOptionMultiValGet(&locDvc->options.dvcHandler,
                                               keyIdList,
                                               numKeyIds,
                                               list,
                                               layer);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

bool xplrGnssIsDrEnabled(uint8_t dvcProfile)
{
    bool ret, boolRet = gnssIsDvcProfileValid(dvcProfile);
    
    if (boolRet) {
        ret = gnssIsDrEnabled(dvcProfile);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = false;
    }

    return ret;
}

bool xplrGnssIsDrCalibrated(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    bool ret, boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (!locDvc->options.flags.status.drIsCalibrated) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = false;
    }

    return ret;
}

esp_err_t xplrGnssSendDecryptionKeys(uint8_t dvcProfile, const char *buffer, size_t size)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (buffer == NULL)) {
        ret = ESP_ERR_INVALID_ARG;
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
    } else {
        locDvc = &dvc[dvcProfile];
        if ((size > 0) && (size < XPLR_GNSS_DECRYPTION_KEYS_LEN)) {
            memcpy(locDvc->conf->corrData.keys.keys, buffer, size);
            locDvc->conf->corrData.keys.size = size;
            XPLRGNSS_CONSOLE(D, "Saved keys into config struct.");
            ret = gnssSetDecrKeys(dvcProfile);
            ret = ESP_OK;
        } else {
            XPLRGNSS_CONSOLE(E, "Size [%d] seems to be invalid for storing key!", size);
            XPLRGNSS_CONSOLE(E, "Will not send keys!");
            locDvc->conf->corrData.keys.size = 0;
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t xplrGnssSendCorrectionData(uint8_t dvcProfile, const char *buffer, size_t size)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || ((buffer == NULL))) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = xplrGnssSendFormattedCommand(dvcProfile, buffer, size);
    }

    return ret;
}

esp_err_t xplrGnssSendFormattedCommand(uint8_t dvcProfile, const char *buffer, size_t size)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    int32_t intRet;

    if (!boolRet || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        intRet = xplrHlprLocSrvcSendUbxFormattedCommand(&locDvc->options.dvcHandler,
                                                        buffer,
                                                        size);
        if (intRet <= 0) {
            XPLRGNSS_CONSOLE(E,
                             "GNSS send formatted command failed with error code [%d]!",
                             intRet);
            ret = ESP_FAIL;
        } else if (intRet != size) {
            XPLRGNSS_CONSOLE(E,
                             "Parameter payload size [%d] mismatch with sent payload size [%d]",
                             size,
                             intRet);
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

esp_err_t xplrGnssSendRtcmCorrectionData(uint8_t dvcProfile, const char *buffer, size_t size)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_FAIL;
    } else {
        ret = xplrGnssSendRtcmFormattedCommand(dvcProfile, buffer, size);
        if (ret != ESP_OK) {
            XPLRGNSS_CONSOLE(E,
                             "GNSS send RTC command failed with error code %s!",
                             esp_err_to_name(ret));
        } else {
            // do nothing
        }
    }

    return ret;
}

esp_err_t xplrGnssSendRtcmFormattedCommand(uint8_t dvcProfile, const char *buffer, size_t size)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcSendRtcmFormattedCommand(&locDvc->options.dvcHandler,
                                                      buffer,
                                                      size);
    }

    return ret;
}

bool xplrGnssHasMessage(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    bool ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        if (locDvc->options.flags.status.locMsgDataAvailable && 
            locDvc->options.flags.status.locMsgDataRefreshed) {
            ret = true;
        } else {
            ret = false;
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = false;
    }

    return ret;
}

esp_err_t xplrGnssConsumeMessage(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        locDvc = &dvc[dvcProfile];
        locDvc->options.flags.status.locMsgDataRefreshed = 0;
        ret = ESP_OK;
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssNvsDeleteCalibrations(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if(boolRet) {
        ret = gnssNvsErase(dvcProfile);
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssGetGmapsLocation(uint8_t dvcProfile, char *gmapsLocationRes, uint16_t maxLen)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    int writeLen;

    if (!boolRet || (gmapsLocationRes == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        writeLen = snprintf(gmapsLocationRes,
                            maxLen,
                            "%s%f,%f",
                            gLocationUrlPart,
                            locDvc->locData.locData.location.latitudeX1e7  * (1e-7),
                            locDvc->locData.locData.location.longitudeX1e7 * (1e-7));

        if (writeLen < 0) {
            XPLRGNSS_CONSOLE(E, "Getting GMaps location failed with error code[%d]!", writeLen);
            ret = ESP_FAIL;
        } else if (writeLen == 0) {
            XPLRGNSS_CONSOLE(E, "Getting GMpas location failed!");
            XPLRGNSS_CONSOLE(E, "Nothing was written in the buffer");
            ret = ESP_FAIL;
        } else if (writeLen >= maxLen) {
            XPLRGNSS_CONSOLE(E, "Getting GMaps location failed!");
            XPLRGNSS_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
            ret = ESP_FAIL;
        } else {
#if 1 == XPLR_GNSS_XTRA_DEBUG
            XPLRGNSS_CONSOLE(D, "Got GMaps location successfully.");
#endif
            ret = ESP_OK;
        }
    }

    return ret;
}

esp_err_t xplrGnssPrintGmapsLocation(uint8_t dvcProfile)
{
    esp_err_t ret;

#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char gmapsLocationRes[64];
    bool hasMessage;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (boolRet) {
        hasMessage = xplrGnssHasMessage(dvcProfile);
        if (hasMessage) {
            ret = xplrGnssGetGmapsLocation(dvcProfile, gmapsLocationRes, ELEMENTCNT(gmapsLocationRes));
            if (ret != ESP_OK) {
                XPLRGNSS_CONSOLE(E, "Error printing GMaps location!");
            } else {
                XPLRGNSS_CONSOLE(I, "Printing GMapsLocation!");
                XPLRGNSS_CONSOLE(I, "Gmaps Location: %s", gmapsLocationRes);
            }
        }
        ret = ESP_OK;
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }
#else
    ret = ESP_OK;
#endif

    return ret;
}

esp_err_t xplrGnssGetDeviceInfo(uint8_t dvcProfile, xplrLocDvcInfo_t *dvcInfo)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (dvcInfo == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        ret = xplrHlprLocSrvcGetDeviceInfo(&locDvc->conf->hw,
                                           locDvc->options.dvcHandler,
                                           dvcInfo);
    }

    return ret;
}

esp_err_t xplrGnssPrintDeviceInfo(uint8_t dvcProfile)
{
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    xplrLocDvcInfo_t dvcInfo;

    if (boolRet) {
        ret = xplrGnssGetDeviceInfo(dvcProfile, &dvcInfo);
        if (ret == ESP_OK) {
            ret = xplrHlprLocSrvcPrintDeviceInfo(&dvcInfo);
        } else {
            XPLRGNSS_CONSOLE(E, "Failed getting device info!");
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    }

    return ret;
}

esp_err_t xplrGnssGetLocationData(uint8_t dvcProfile, xplrGnssLocation_t *locData)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (locData == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        memcpy(locData, &locDvc->locData.locData, sizeof(xplrGnssLocation_t));
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t xplrGnssPrintLocationData(xplrGnssLocation_t *locData)
{
    esp_err_t ret;

#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char locFixTypeStr[XPLR_GNSS_LOCFIX_STR_MAX_LEN];

    if (locData == NULL) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        ret = gnssFixTypeToString(locData, locFixTypeStr, ELEMENTCNT(locFixTypeStr));
        if (ret == ESP_OK) {
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
            if (locationLog.logEnable) {
                gnssLogLocationPrinter(locFixTypeStr, locData);
            } else {
                // do nothing
            }
#endif
            gnssLocationPrinter(locFixTypeStr, locData);
        } else {
            XPLRGNSS_CONSOLE(E, "Error converting Fix type to string!");
        }
    }
#else
    ret = ESP_OK;
#endif

    return ret;
}

esp_err_t xplrGnssGetImuAlignmentInfo(uint8_t dvcProfile,
                                      xplrGnssImuAlignmentInfo_t *info)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (info == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        memcpy(info,
               &locDvc->drData.info,
               sizeof(xplrGnssImuAlignmentInfo_t));
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t xplrGnssPrintImuAlignmentInfo(xplrGnssImuAlignmentInfo_t *info)
{
    esp_err_t ret;

#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    char tmpStr[64];

    if (info == NULL) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        memset(tmpStr, 0, ELEMENTCNT(tmpStr));

        XPLRGNSS_CONSOLE(I, "Printing Imu Alignment Info.");
        printf("===== Imu Alignment Info ======\n");
        ret = gnssCalibModeToString(&info->mode, tmpStr, ELEMENTCNT(tmpStr));
        if (ret != ESP_OK) {
            XPLRGNSS_CONSOLE(E, "Error converting calib mode to string!");
            tmpStr[0] = 0;
        } else {
            printf("Calibration Mode: %s\n", tmpStr);
            ret = gnssCalibStatToString(&info->status, tmpStr, ELEMENTCNT(tmpStr));
            if (ret != ESP_OK) {
                XPLRGNSS_CONSOLE(E, "Error converting calib status to string!");
                tmpStr[0] = 0;
            } else {
                printf("Calibration Status: %s\n", tmpStr);
                printf("Aligned yaw: %.2f\n", info->data.yaw * (1e-2));
                printf("Aligned pitch: %.2f\n", info->data.pitch * (1e-2));
                printf("Aligned roll: %.2f\n", info->data.roll * (1e-2));
                printf("-------------------------------\n");
            }
        }
    }
#else
    ret = ESP_OK;
#endif

    return ret;
}

esp_err_t xplrGnssGetImuAlignmentStatus(uint8_t dvcProfile,
                                        xplrGnssImuFusionStatus_t *status)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (status == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        memcpy(status,
               &locDvc->drData.status,
               sizeof(xplrGnssImuFusionStatus_t));
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t xplrGnssPrintImuAlignmentStatus(xplrGnssImuFusionStatus_t *status)
{
    esp_err_t ret;

#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    if (status == NULL || (status == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        if (status->numSens > XPLR_GNSS_SENSORS_MAX_CNT) {
            XPLRGNSS_CONSOLE(E,
                             "Imu Fusion sensors count [%d] is larger than permited [%d]!",
                             status->numSens,
                             XPLR_GNSS_SENSORS_MAX_CNT);
            ret = ESP_FAIL;
        } else {
            ret = gnssImuAlignStatPrinter(status);
            if (ret != ESP_OK) {
                XPLRGNSS_CONSOLE(E, "Failed to print Imu Alignment status!");
            } else {
                // do nothing
            }
        }
    }
#else
    ret = ESP_OK;
#endif

    return ret;
}

esp_err_t xplrGnssGetImuVehicleDynamics(uint8_t dvcProfile,
                                        xplrGnssImuVehDynMeas_t *dynamics)
{
    xplrGnss_t *locDvc = NULL;
    esp_err_t ret;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (dynamics == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        memcpy(dynamics,
               &locDvc->drData.dynamics,
               sizeof(xplrGnssImuVehDynMeas_t));
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t xplrGnssPrintImuVehicleDynamics(xplrGnssImuVehDynMeas_t *dynamics)
{
    esp_err_t ret;

    if (dynamics == NULL) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
#if (1 == XPLRGNSS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
        XPLRGNSS_CONSOLE(I, "Printing vehicle dynamics");

        printf("======= Vehicle Dynamics ======\n");
        printf("----- Meas Validity Flags -----\n");
        printf("Gyro  X: %d | Gyro  Y: %d | Gyro  Z: %d\n",
               dynamics->valFlags.singleFlags.xAngRateValid,
               dynamics->valFlags.singleFlags.yAngRateValid,
               dynamics->valFlags.singleFlags.zAngRateValid);
        printf("Accel X: %d | Accel Y: %d | Accel Z: %d\n",
               dynamics->valFlags.singleFlags.xAccelValid,
               dynamics->valFlags.singleFlags.yAccelValid,
               dynamics->valFlags.singleFlags.zAccelValid);
        printf("- Dynamics Compensated Values -\n");
        printf("X-axis angular rate: %.3f deg/s\n",
               dynamics->xAngRate * (1e-3));
        printf("Y-axis angular rate: %.3f deg/s\n",
               dynamics->yAngRate * (1e-3));
        printf("Z-axis angular rate: %.3f deg/s\n",
               dynamics->zAngRate * (1e-3));
        printf("X-axis acceleration (gravity-free): %.2f m/s^2\n",
               dynamics->xAccel * (1e-2));
        printf("Y-axis acceleration (gravity-free): %.2f m/s^2\n",
               dynamics->yAccel * (1e-2));
        printf("Z-axis acceleration (gravity-free): %.2f m/s^2\n",
               dynamics->zAccel * (1e-2));
        printf("===============================\n");
        ret = ESP_OK;
#else
    ret = ESP_OK;
#endif
    }

    return ret;
}

uErrorCode_t xplrGnssGetGgaMessage(uint8_t dvcProfile, char **buffer, size_t size)
{
    uErrorCode_t ret;
    uDeviceHandle_t dvcHandle = NULL;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);

    if (!boolRet || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = U_ERROR_COMMON_INVALID_PARAMETER;
    } else {
        dvcHandle = *(xplrGnssGetHandler(dvcProfile));
        if (dvcHandle != NULL) {
            ret = uGnssMsgReceive(dvcHandle,
                                  &msgIdFixType,
                                  buffer,
                                  size,
                                  XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS,
                                  NULL);
        } else {
            XPLRGNSS_CONSOLE(E, "Returned dvcHandle is NULL!");
            ret = U_ERROR_COMMON_NOT_FOUND;
        }
    }

    return ret;
}

xplrGnssStates_t xplrGnssGetCurrentState(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    xplrGnssStates_t ret;

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = XPLR_GNSS_STATE_UNKNOWN;
    } else {
        locDvc = &dvc[dvcProfile];
        ret = locDvc->options.state[0];
    }

    return ret;
}

xplrGnssStates_t xplrGnssGetPreviousState(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = NULL;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    xplrGnssStates_t ret;

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = XPLR_GNSS_STATE_UNKNOWN;
    } else {
        locDvc = &dvc[dvcProfile];
        ret = locDvc->options.state[1];
    }

    return ret;
}

esp_err_t xplrGnssAsyncLogInit(uint8_t dvcProfile)
{
    esp_err_t ret = ESP_FAIL;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
    xplrGnss_t *locDvc = NULL;
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    xplrLog_error_t err;

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc = &dvc[dvcProfile];
        if (!asyncLog.isInit) {
            XPLRGNSS_CONSOLE(I, "Initializing async logging");
            err = xplrLogInit(&asyncLog.logCfg,
                              XPLR_LOG_DEVICE_ZED,
                              "/ZEDLOG.ubx",
                              4,
                              XPLR_SIZE_GB);
            if (err == XPLR_LOG_OK) {
                // Keep the dvcProfile of the device that initiates logging
                asyncLog.firstDvcProfile = dvcProfile;
                // Create the ring buffer
                asyncLog.xRingBuffer = xRingbufferCreate(XPLR_GNSS_LOG_RING_BUF_SIZE, RINGBUF_TYPE_NOSPLIT);
                if (asyncLog.xRingBuffer == NULL) {
                    // Ring buffer creation was unsuccessful so disable logging
                    asyncLog.isInit = false;
                    ret = ESP_FAIL;
                    XPLRGNSS_CONSOLE(E, "Error initializing logging ring buffer!");
                } else {
                    // Create Task for SD logging
                    xTaskCreate(gnssLogTask, "gnssLogTask", 3 * 1024, NULL, 1, &asyncLog.gnssLogTaskHandle);
                    // Create Semaphore
                    xSemaphore = xSemaphoreCreateMutex();
                    // Take Semaphore and then give it to ensure it is created successfully
                    if (xSemaphoreTake(xSemaphore, XPLR_GNSS_LOG_RING_BUF_TIMEOUT) == pdTRUE) {
                        // Since no errors occurred enable logging and give the mutex
                        asyncLog.isInit = true;
                        asyncLog.isEnabled = true;
                        semaphoreCreated = true;
                        // Point the dvc log to the task's log struct
                        locDvc->log = &asyncLog.logCfg;
                        ret = ESP_OK;
                        xSemaphoreGive(xSemaphore);
                    } else {
                        XPLRGNSS_CONSOLE(E, "Could not create semaphore");
                        asyncLog.isInit = false;
                        ret = ESP_FAIL;
                    }
                }
            } else {
                XPLRGNSS_CONSOLE(E, "Could not initialize async logging");
                asyncLog.isInit = false;
                ret = ESP_FAIL;
            }
        } else {
            XPLRGNSS_CONSOLE(W, "Async logging task already initialized");
            locDvc->log = &asyncLog.logCfg;
            ret = ESP_OK;
        }
    }
#endif
    return ret;
}

esp_err_t xplrGnssAsyncLogDeInit(uint8_t dvcProfile)
{
    esp_err_t ret = ESP_FAIL;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
    bool boolRet = gnssIsDvcProfileValid(dvcProfile);
    xplrLog_error_t err;

    if (!boolRet) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        if (xSemaphoreTake(xSemaphore, XPLR_GNSS_LOG_RING_BUF_TIMEOUT) == pdTRUE) {
            asyncLog.isEnabled = false;
            /* De init logging*/
            err = xplrLogDeInit(&asyncLog.logCfg);
            if (err == XPLR_LOG_OK) {
                /* Delete the task*/
                vTaskDelete(asyncLog.gnssLogTaskHandle);
                /* Free the ring buffer*/
                vRingbufferDelete(asyncLog.xRingBuffer);
                XPLRGNSS_CONSOLE(W, "Async logging task disabled");
                asyncLog.isInit = false;
                ret = ESP_OK;
            } else {
                XPLRGNSS_CONSOLE(E, "Could not terminate logging");
                ret = ESP_FAIL;
            }
            /* Free the semaphore*/
            xSemaphoreGive(xSemaphore);
        } else {
            XPLRGNSS_CONSOLE(E, "Could not take the semaphore to terminate async logging");
            ret = ESP_FAIL;
        }
    }
#endif
    return ret;
}

bool xplrGnssHaltLogModule(xplrGnssLogModule_t module)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
    switch (module) {
        case XPLR_GNSS_LOG_MODULE_CONSOLE:
            locationLog.logEnable = false;
            ret = !locationLog.logEnable;
            break;
        case XPLR_GNSS_LOG_MODULE_UBX:
            asyncLog.isEnabled = false;
            ret = !asyncLog.isEnabled;
            break;
        case XPLR_GNSS_LOG_MODULE_ALL:
            locationLog.logEnable = false;
            asyncLog.isEnabled = false;
            ret = !(locationLog.logEnable || asyncLog.isEnabled);
            break;
        case XPLR_GNSS_LOG_MODULE_INVALID:
        default:
            ret = false;
            XPLRGNSS_CONSOLE(E, "Invalid submodule name. Cannot Halt logging!");
            break;
    }
#else
    ret = false;
#endif
    return ret;
}

bool xplrGnssStartLogModule(xplrGnssLogModule_t module)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
    xplrLog_error_t err;
    switch (module) {
        case XPLR_GNSS_LOG_MODULE_CONSOLE:
            err = xplrLogInit(&locationLog,
                              XPLR_LOG_DEVICE_INFO,
                              "/location.log",
                              100,
                              XPLR_SIZE_MB);
            if (err == XPLR_LOG_OK) {
                locationLog.logEnable = true;
            } else {
                locationLog.logEnable = false;
            }
            ret = locationLog.logEnable;
            break;
        case XPLR_GNSS_LOG_MODULE_UBX:
            xplrGnssAsyncLogInit(0);
            ret = asyncLog.isInit;
            break;
        case XPLR_GNSS_LOG_MODULE_ALL:
            err = xplrLogInit(&locationLog,
                              XPLR_LOG_DEVICE_INFO,
                              "/location.log",
                              100,
                              XPLR_SIZE_MB);
            if (err == XPLR_LOG_OK) {
                locationLog.logEnable = true;
            } else {
                locationLog.logEnable = false;
            }
            xplrGnssAsyncLogInit(0);
            ret = asyncLog.isInit && locationLog.logEnable;
            break;
        case XPLR_GNSS_LOG_MODULE_INVALID:
        default:
            XPLRGNSS_CONSOLE(E, "Invalid submodule name. Cannot Start logging!");
            ret = false;
            break;
    }
#else
    ret = false;
#endif
    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * All static functions are getting checked paramaters
 * Hence the reason check device valitiy is missing from them.
 */

/**
 * Opens communication with a device.
 */
static esp_err_t gnssDeviceOpen(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    ret = xplrHlprLocSrvcDeviceOpenNonBlocking(&locDvc->conf->hw,
                                               &locDvc->options.dvcHandler);
    return ret;
}

static esp_err_t gnssDeviceRestart(uint8_t dvcProfile)
{
    esp_err_t ret;
    uint8_t message[4] = {0};
    uint8_t buffer[ELEMENTCNT(message) + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES] = {0};
    int16_t length = 0;

    length = uUbxProtocolEncode(0x06, 0x04,
                                (const char*) message,
                                ELEMENTCNT(message),
                                (char*) buffer);
    if (length > 0) {
        ret = xplrGnssSendFormattedCommand(dvcProfile, (const char*) buffer, length);
        if (ret == ESP_OK) {
            XPLRGNSS_CONSOLE(D, "Reset command issued succesfully");
            ret = gnssDeviceStop(dvcProfile);
            if (ret == ESP_OK) {
                XPLRGNSS_CONSOLE(D, "Device stop command issued succesfully");
                XPLRGNSS_CONSOLE(D, "Restart routine executed succesfully.");
            } else {
                XPLRGNSS_CONSOLE(E, "Failed to issue reset command!");
                XPLRGNSS_CONSOLE(E, "Restart routine failed!");
            }
        } else {
            XPLRGNSS_CONSOLE(E, "Failed to issue reset command!");
            XPLRGNSS_CONSOLE(E, "Restart routine failed!");
            ret = ESP_FAIL;
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Encoding UBX command failed with error code [%d]!", length);
        XPLRGNSS_CONSOLE(E, "Restart routine failed!");
        ret = ESP_FAIL;
    }

    return ret;
}

static esp_err_t gnssDeviceStop(uint8_t dvcProfile)
{
    esp_err_t ret;

    ret = xplrGnssStopAllAsyncs(dvcProfile);
    gnssResetoptionsTimers(dvcProfile);
    gnssResetoptionsFlags(dvcProfile);
    if (ret == ESP_OK) {
        ret = gnssDeviceClose(dvcProfile);
        if (ret == ESP_OK) {
            XPLRGNSS_CONSOLE(D, "Sucessfully stoped GNSS device.");
        } else {
            XPLRGNSS_CONSOLE(E, "Failed to stop GNSS device!");
        }
    } else {
        XPLRGNSS_CONSOLE(W, "Failed to stop Async getters!");
    }

    return ret;
}

/**
 * Closes communication with a device
 */
static esp_err_t gnssDeviceClose(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    ret = xplrHlprLocSrvcDeviceClose(&locDvc->options.dvcHandler);
    return ret;
}

static void gnssResetoptionsFlags(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    locDvc->options.flags.value = 0;
}

static void gnssResetoptionsTimers(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    locDvc->options.lastActTime       = 0;
    locDvc->options.lastWatchdogTime  = 0;
    locDvc->options.genericTimer      = 0;
}

/**
 * Writes the location fix type to a string buffer
 */
static esp_err_t gnssFixTypeToString(xplrGnssLocation_t *locData, char *buffer, uint8_t maxLen)
{
    esp_err_t ret;
    int writeLen;

    memset(buffer, 0, maxLen);

    if ((locData == NULL) || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        switch (locData->locFixType)
        {
            case XPLR_GNSS_LOCFIX_INVALID:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixInvalid);
                break;
            case XPLR_GNSS_LOCFIX_2D3D:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfix3D);
                break;
            case XPLR_GNSS_LOCFIX_DGNSS:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixDgnss);
                break;
            case XPLR_GNSS_LOCFIX_FLOAT_RTK:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixRtkFloat);
                break;
            case XPLR_GNSS_LOCFIX_FIXED_RTK:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixRtkFixed);
                break;
            case XPLR_GNSS_LOCFIX_DEAD_RECKONING:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixDeadReck);
                break;
            default:
                writeLen = snprintf(buffer, maxLen, gnssStrLocfixInvalid);
                break;
        }

        if (writeLen < 0) {
            XPLRGNSS_CONSOLE(E, "Printing location fix type failed with error code[%d]!", writeLen);
            ret = ESP_FAIL;
        } else if (writeLen == 0) {
            XPLRGNSS_CONSOLE(E, "Printing location fix type failed!");
            XPLRGNSS_CONSOLE(E, "Nothing was written in the buffer");
            ret = ESP_FAIL;
        } else if (writeLen >= maxLen) {
            XPLRGNSS_CONSOLE(E, "Printing location fix type failed!");
            XPLRGNSS_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * Parses the appropriate NMEA message (GNGGA) and tries to extract
 * fix type
 */
static esp_err_t gnssGetLocFixType(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;
    uint8_t strIdx;
    uint8_t commaCnt = 0;
    static uint8_t noFixCnt;

    if ((locDvc == NULL) || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        /**
         * We are parsing the following message:
         *      $GNGGA,185115.00,3758.82530,N,02339.41564,E,1,12,0.54,64.8,M,33.1,M,,*7E
         * 7th part is what we are looking for -------------^
         */
        if (strlen(buffer) > 1) {
            /**
             * Try to find 6th comma and stop
             */
            for (strIdx = 0; strIdx < strlen(buffer); strIdx++) {
                if (buffer[strIdx] == ',') {
                    commaCnt++;
                } else {
                    // do nothing
                }

                if (commaCnt == 6) {
                    break;
                } else {
                    // do nothing
                }
            }

            if (commaCnt < 6) {
                XPLRGNSS_CONSOLE(W, "Could not reach the 7th segment for the GNGGA message!");
                ret = ESP_FAIL;
            } else {
                // do nothing
            }

            /**
             * Check if next character is another comma or terminator
             */
            if (buffer[strIdx + 1] == 0 || buffer[strIdx + 2] == 0) {
                XPLRGNSS_CONSOLE(W, "Seems like GNGGA is terminating early.");
                ret = ESP_FAIL;
            } else if (buffer[strIdx + 1] == ',') {
                if (noFixCnt == 10) {
                    locDvc->locData.locData.locFixType = XPLR_GNSS_LOCFIX_INVALID;
#if 1 == XPLR_GNSS_XTRA_DEBUG
                    XPLRGNSS_CONSOLE(W, "Seems like location fix type has not been parsed for the last 10 messages!");
#endif
                } else if (noFixCnt < 10) {
                    noFixCnt++;
                } else {
                    // do nothing
                }
                ret = ESP_OK;
            } else if (buffer[strIdx + 2] != ',') {
                XPLRGNSS_CONSOLE(W, "Seems like location fix type is not a single char!");
                ret = ESP_FAIL;
            } else if (buffer[strIdx + 1] < '0' || buffer[strIdx + 1] > '6' || buffer[strIdx + 1] == '3') {
                XPLRGNSS_CONSOLE(W, "Seems like location fix type is not a valid char [%c]!", buffer[strIdx + 1]);
                ret = ESP_FAIL;
            } else {
                noFixCnt = 0;
                locDvc->locData.locData.locFixType = buffer[strIdx + 1] - '0';

                switch (locDvc->locData.locData.locFixType) {
                    case XPLR_GNSS_LOCFIX_2D3D:
                    case XPLR_GNSS_LOCFIX_DGNSS:
                    case XPLR_GNSS_LOCFIX_FIXED_RTK:
                    case XPLR_GNSS_LOCFIX_FLOAT_RTK:
                    case XPLR_GNSS_LOCFIX_DEAD_RECKONING:
                    case XPLR_GNSS_LOCFIX_INVALID:
                        ret = ESP_OK;
                        break;

                    default:
                        ret = ESP_FAIL;
                        break;
                }
            }
        } else {
            if (noFixCnt < 10) {
                noFixCnt++;
            } else {
                // do nothing
            }
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * Generic async stopper
 */
static int32_t gnssAsyncStopper(uint8_t dvcProfile, int32_t handler)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    int32_t intRet;

    intRet = uGnssMsgReceiveStop(locDvc->options.dvcHandler, handler);

    if (intRet < 0) {
        XPLRGNSS_CONSOLE(E, "Failed to stop async function with error code [%d]!", intRet);
    } else {
        XPLRGNSS_CONSOLE(D, "Successfully stoped async function.");
    }

    return intRet;
}


/**
 * Accuracy parser
 */
static esp_err_t gnssAccuracyParser(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;

    if ((buffer == NULL) || ((buffer + 34) == NULL) || ((buffer + 38) == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc->locData.locData.accuracy.horizontal = 0;
        locDvc->locData.locData.accuracy.vertical   = 0;

        locDvc->locData.locData.accuracy.horizontal = (uint32_t) uUbxProtocolUint32Decode(buffer + 34);
        locDvc->locData.locData.accuracy.vertical   = (uint32_t) uUbxProtocolUint32Decode(buffer + 38);

        locDvc->options.flags.status.locMsgDataRefreshed = 1;

        ret = ESP_OK;
    }

    return ret;
}

/**
 * DR manual calibration routine
 */
static esp_err_t gnssDrManualCalib(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;

    ret = gnssDrSetGenericSettings(dvcProfile);

    if (ret == ESP_OK) {
        locDvc->conf->dr.mode = XPLR_GNSS_IMU_CALIBRATION_MANUAL;

        ret = gnssDrSetVehicleType(dvcProfile);
        if (ret == ESP_OK) {
            ret = gnssDrSetAlignMode(dvcProfile);
            if (ret == ESP_OK) {
                ret = gnssImuSetCalibData(dvcProfile);
                if (ret == ESP_OK) {
                    XPLRGNSS_CONSOLE(D, "Wrote all settings for Manual Calibration!");
                } else {
                    XPLRGNSS_CONSOLE(E, "Could mot write calibration data!");
                }
            } else {
                XPLRGNSS_CONSOLE(E, "Could not set alignment mode!");
            }
        } else {
            XPLRGNSS_CONSOLE(E, "Could not set Dead Reckoning vehicle type!");
        }

    } else {
        XPLRGNSS_CONSOLE(E, "Could not set Dead Reckoning generic settings!");
    }

    return ret;
}

/**
 * Sets correction data source
 */
static esp_err_t gnssSetCorrDataSource(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;

    ret = xplrGnssOptionSingleValSet(dvcProfile,
                                     U_GNSS_CFG_VAL_KEY_ID_SPARTN_USE_SOURCE_E1,
                                     (uint64_t)locDvc->conf->corrData.source,
                                     U_GNSS_CFG_VAL_LAYER_RAM);

    return ret;
}

/**
 * Send decryption keys to module
 */
static esp_err_t gnssSetDecrKeys(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;

    ret = xplrGnssSendFormattedCommand(dvcProfile,
                                       locDvc->conf->corrData.keys.keys,
                                       locDvc->conf->corrData.keys.size);

    return ret;
}

/**
 * DR auto calibration routine
 */
static esp_err_t gnssDrAutoCalib(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    bool valsValid;

    ret = gnssDrSetGenericSettings(dvcProfile);

    if (ret == ESP_OK) {
        ret = gnssDrSetVehicleType(dvcProfile);
        if (ret == ESP_OK) {
            ret = gnssNvsLoad(dvcProfile);
            if (ret == ESP_OK) {
                valsValid = gnssCheckAlignValsLimits(dvcProfile);
                if (valsValid) {
                    XPLRGNSS_CONSOLE(D, "Loaded valid Alignment Data from NVS.");
                    XPLRGNSS_CONSOLE(D, "Switching to Manual Calibration to write data on GNSS module.");
                    locDvc->options.flags.status.drExecManualCalib = 1;
                } else {
                    ret = gnssDrSetAlignMode(dvcProfile);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(E, "Failed to set Alignment Mode!");
                    }
                }
            } else {
                XPLRGNSS_CONSOLE(E, "Failed loading Alignment Data from NVS!");
            }
        } else {
            XPLRGNSS_CONSOLE(E, "Could not set Dead Reckoning vehicle settings!");
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Could not set Dead Reckoning generic settings!");
    }

    return ret;
}

/**
 * Used internally to setup alignments during
 * Dead Reckoning startup
 */
static esp_err_t gnssImuSetCalibData(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    const uGnssCfgVal_t pGnssOpts[] = {
        {U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_YAW_U4, locDvc->conf->dr.alignVals.yaw},
        {U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_PITCH_I2, locDvc->conf->dr.alignVals.pitch},
        {U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_ROLL_I2, locDvc->conf->dr.alignVals.roll}
    };

    ret = xplrGnssOptionMultiValSet(dvcProfile,
                                    pGnssOpts,
                                    ELEMENTCNT(pGnssOpts),
                                    U_GNSS_CFG_VAL_LAYER_RAM);

    return ret;
}

/**
 * UBX-ESF-ALG (0x10 0x14)
 * Check F9-HPS Interface description for more info
 */
static esp_err_t gnssEsfAlgParser(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;
    uint8_t flags = 0;

    if ((locDvc == NULL) || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        flags = buffer[11];
        locDvc->drData.info.mode = (flags & 1);
        locDvc->drData.info.data.yaw   = (uint32_t) uUbxProtocolUint32Decode(buffer + 14);
        locDvc->drData.info.data.pitch = (int16_t)  uUbxProtocolUint16Decode(buffer + 18);
        locDvc->drData.info.data.roll  = (int16_t)  uUbxProtocolUint16Decode(buffer + 20);
        flags >>= 1;
        locDvc->drData.info.status = flags;

        switch (locDvc->drData.info.status) {
            case XPLR_GNSS_ALG_STATUS_USER_DEFINED:
                if (locDvc->conf != NULL) {
                    if (locDvc->conf->dr.mode == XPLR_GNSS_IMU_CALIBRATION_MANUAL) {
                        locDvc->options.flags.status.drIsCalibrated = 1;
                    } else {
                        locDvc->options.flags.status.drIsCalibrated = 0;
                    }
                } else {
                    locDvc->options.flags.status.drIsCalibrated = 0;
                }
                break;
            case XPLR_GNSS_ALG_STATUS_ROLLPITCH_CALIBRATING:
            case XPLR_GNSS_ALG_STATUS_ROLLPITCHYAW_CALIBRATING:
                locDvc->options.flags.status.drIsCalibrated = 0;
                break;

            case XPLR_GNSS_ALG_STATUS_USING_COARSE_ALIGNMENT:
            case XPLR_GNSS_ALG_STATUS_USING_FINE_ALIGNMENT:
                if ((locDvc->options.flags.status.drUpdateNvs == 0) &&
                    (locDvc->conf->dr.mode == XPLR_GNSS_IMU_CALIBRATION_AUTO)) {
                    locDvc->options.flags.status.drUpdateNvs = 1;
                } else {
                    // do nothing
                }
                locDvc->options.flags.status.drIsCalibrated = 1;
                break;

            default:
                locDvc->drData.info.status = XPLR_GNSS_ALG_STATUS_UNKNOWN;
                locDvc->options.flags.status.drIsCalibrated = 0;
                break;
        }
        ret = ESP_OK;
    }

    return ret;
}

/**
 * Check F9-HPS Interface description for more info
 * UBX-ESF-STATUS (0x10 0x10)
 */
static esp_err_t gnssEsfStatusParser(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;
    uint8_t numSens;

    if ((locDvc == NULL) || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        if (buffer[21] > XPLR_GNSS_SENSORS_MAX_CNT) {
            XPLRGNSS_CONSOLE(E, "Too many sensors!");
            ret = ESP_FAIL;
        } else {
            locDvc->drData.status.fusionMode = buffer[18];
            locDvc->drData.status.numSens = buffer[21];

            for (numSens = 0; numSens < buffer[21]; numSens++) {
                locDvc->drData.status.sensor[numSens].type  = buffer[22 + (4*numSens)] & 0b00111111;
                locDvc->drData.status.sensor[numSens].used  = (buffer[22 + (4*numSens)] >> 6) & 1;
                locDvc->drData.status.sensor[numSens].ready = (buffer[22 + (4*numSens)] >> 7) & 1;

                switch (buffer[23 + (4*numSens)] & 0b00000011) {
                    case 0:
                        locDvc->drData.status.sensor[numSens].calibStatus = XPLR_GNNS_SENSOR_CALIB_NOT_CALIBRATED;
                        break;
                    case 1:
                        locDvc->drData.status.sensor[numSens].calibStatus = XPLR_GNNS_SENSOR_CALIB_CALIBRATING;
                        break;

                    case 2:
                    case 3:
                        locDvc->drData.status.sensor[numSens].calibStatus = XPLR_GNNS_SENSOR_CALIB_CALIBRATED;
                        break;

                    default:
                        locDvc->drData.status.sensor[numSens].calibStatus = XPLR_GNNS_SENSOR_CALIB_UNKNOWN;
                        break;
                }

                locDvc->drData.status.sensor[numSens].freq = buffer[24 + (4*numSens)];
                locDvc->drData.status.sensor[numSens].faults.allFaults = buffer[25 + (4*numSens)];
            }
        }

        ret = ESP_OK;
    }

    return ret;
}

/**
 * ESF-INS
 * Vehicle dynamics parser
 */
static esp_err_t gnssEsfInsParser(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;

    if ((locDvc == NULL) || (buffer == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        locDvc->drData.dynamics.valFlags.allFlags = buffer[7] & 0b00111111;

        locDvc->drData.dynamics.xAngRate = (int32_t) uUbxProtocolUint32Decode(buffer + 18);
        locDvc->drData.dynamics.yAngRate = (int32_t) uUbxProtocolUint32Decode(buffer + 22);
        locDvc->drData.dynamics.zAngRate = (int32_t) uUbxProtocolUint32Decode(buffer + 26);
        locDvc->drData.dynamics.xAccel = (int32_t) uUbxProtocolUint32Decode(buffer + 30);
        locDvc->drData.dynamics.yAccel = (int32_t) uUbxProtocolUint32Decode(buffer + 34);
        locDvc->drData.dynamics.zAccel = (int32_t) uUbxProtocolUint32Decode(buffer + 38);
        ret = ESP_OK;
    }

    return ret;
}

/**
 * Geolocation parser
 */
static esp_err_t gnssGeolocationParser(xplrGnss_t *locDvc, char *buffer)
{
    esp_err_t ret;
    int32_t months;
    int32_t year;
    int64_t t = -1;

    switch(locDvc->conf->hw.dvcConfig.deviceType) {
        case U_DEVICE_TYPE_GNSS:
            locDvc->locData.locData.location.type = U_LOCATION_TYPE_GNSS;
            break;
        default:
            locDvc->locData.locData.location.type = U_LOCATION_TYPE_NONE;
            break;
    }

    /**
     * Time conversion taken from ubxlib
     */
    if ((buffer[17] & 0x03) == 0x03) {
        /**
         * Time and date are valid; we don't indicate
         * success based on this but we report it anyway
         * if it is valid
         */
        t = 0;

        /**
         * Year is 1999-2099, so need to adjust to get year since 1970
         */
        year = ((int32_t) uUbxProtocolUint16Decode(buffer + 10) - 1999) + 29;

        /**
         * Month (1 to 12), so take away 1 to make it zero-based
         */
        months = buffer[12] - 1;
        months += year * 12;

        // Work out the number of seconds due to the year/month count
        t += uTimeMonthsToSecondsUtc(months);
        // Day (1 to 31)
        t += ((int32_t) buffer[13] - 1) * 3600 * 24;
        // Hour (0 to 23)
        t += ((int32_t) buffer[14]) * 3600;
        // Minute (0 to 59)
        t += ((int32_t) buffer[15]) * 60;
        // Second (0 to 60)
        t += buffer[16];
    }

    locDvc->locData.locData.location.timeUtc = t;

    if (buffer[27] & 0x01) {
        locDvc->locData.locData.location.svs = (int32_t) buffer[29];
        locDvc->locData.locData.location.longitudeX1e7 = (int32_t) uUbxProtocolUint32Decode(buffer + 30);
        locDvc->locData.locData.location.latitudeX1e7 = (int32_t) uUbxProtocolUint32Decode(buffer + 34);

        if (buffer[26] == 0x03) {
            locDvc->locData.locData.location.altitudeMillimetres = (int32_t) uUbxProtocolUint32Decode(buffer + 42);
        } else {
            locDvc->locData.locData.location.altitudeMillimetres = INT_MIN;
        }

        locDvc->locData.locData.location.radiusMillimetres = (int32_t) uUbxProtocolUint32Decode(buffer + 46);
        locDvc->locData.locData.location.speedMillimetresPerSecond = (int32_t) uUbxProtocolUint32Decode(buffer + 66);

        locDvc->options.flags.status.locMsgDataAvailable = 1;
        locDvc->options.flags.status.locMsgDataRefreshed = 1;
    } else {
        locDvc->options.flags.status.locMsgDataAvailable = 0;
    }

    ret = ESP_OK;

    return ret;
}

/**
 * Callback UBX id checker
 */
static bool gnssUbxIsMessageId(const uGnssMessageId_t *msgIdIncoming, 
                               const uGnssMessageId_t *msgIdToFilter)
{
    bool ret;

    if ((msgIdIncoming->type == msgIdToFilter->type) &&
        (msgIdIncoming->id.ubx == msgIdToFilter->id.ubx)) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

/**
 * Callback NMEA id checker
 */
static bool gnssNmeaIsMessageId(const uGnssMessageId_t *msgIdIncoming, 
                                const uGnssMessageId_t *msgIdToFilter)
{
    bool ret;
    int16_t strcmpRes = -1;

    strcmpRes = strcmp(msgIdIncoming->id.pNmea, msgIdToFilter->id.pNmea);
    if ((msgIdIncoming->type == msgIdToFilter->type) &&
        (strcmpRes == 0)) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

/**
 * String helper for calibration mode
 */
static esp_err_t gnssCalibModeToString(xplrGnssImuCalibMode_t *type,
                                       char *typeStr,
                                       uint8_t maxLen)
{
    esp_err_t ret;
    int writeLen;

    if ((type == NULL) || (typeStr== NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        switch (*type) {
            case XPLR_GNSS_IMU_CALIBRATION_MANUAL:
                writeLen = snprintf(typeStr, maxLen, "Manual");
                break;

            case XPLR_GNSS_IMU_CALIBRATION_AUTO:
                writeLen = snprintf(typeStr, maxLen, "Auto");
                break;

            default:
                writeLen = snprintf(typeStr, maxLen, "Unknown");
                break;
        }

        if (writeLen < 0) {
            XPLRGNSS_CONSOLE(E, "Printing calibration mode failed with error code[%d]!", writeLen);
            ret = ESP_FAIL;
        } else if (writeLen == 0) {
            XPLRGNSS_CONSOLE(E, "Printing calibration mode failed!");
            XPLRGNSS_CONSOLE(E, "Nothing was written in the buffer");
            ret = ESP_FAIL;
        } else if (writeLen >= maxLen) {
            XPLRGNSS_CONSOLE(E, "Printing calibration mode failed!");
            XPLRGNSS_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * String helper for calibration statuses
 */
static esp_err_t gnssCalibStatToString(xplrGnssEsfAlgStatus_t *status,
                                       char *statusStr,
                                       uint8_t maxLen)
{
    esp_err_t ret;
    int writeLen;

    if ((status == NULL) || (statusStr == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        switch (*status) {
            case XPLR_GNSS_ALG_STATUS_USER_DEFINED:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusUserDef);
                break;

            case XPLR_GNSS_ALG_STATUS_ROLLPITCH_CALIBRATING:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusRpCalib);
                break;

            case XPLR_GNSS_ALG_STATUS_ROLLPITCHYAW_CALIBRATING:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusRpyCalib);
                break;

            case XPLR_GNSS_ALG_STATUS_USING_COARSE_ALIGNMENT:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusCoarse);
                break;

            case XPLR_GNSS_ALG_STATUS_USING_FINE_ALIGNMENT:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusFine);
                break;

            default:
                writeLen = snprintf(statusStr, maxLen, gnssStrCalibStatusUnknown);
                break;
        }

        if (writeLen < 0) {
            XPLRGNSS_CONSOLE(E, "Printing calibration status failed with error code[%d]!", writeLen);
            ret = ESP_FAIL;
        } else if (writeLen == 0) {
            XPLRGNSS_CONSOLE(E, "Printing calibration status failed!");
            XPLRGNSS_CONSOLE(E, "Nothing was written in the buffer");
            ret = ESP_FAIL;
        } else if (writeLen >= maxLen) {
            XPLRGNSS_CONSOLE(E, "Printing calibration status failed!");
            XPLRGNSS_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * String helper for fusion mode
 */
static esp_err_t gnssFusionModeToString(xplrGnssFusionMode_t *mode,
                                        char *modeStr,
                                        uint8_t maxLen)
{
    esp_err_t ret;
    int intRet;

    if ((mode == NULL) || ((modeStr == NULL))) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        switch (*mode) {
            case XPLR_GNSS_FUSION_MODE_INITIALIZATION:
                intRet = snprintf(modeStr, maxLen, gnssStrFusionModeInit);
                break;
            case XPLR_GNSS_FUSION_MODE_ENABLED:
                intRet = snprintf(modeStr, maxLen, gnssStrFusionModeEnable);
                break;
            case XPLR_GNSS_FUSION_MODE_SUSPENDED:
                intRet = snprintf(modeStr, maxLen, gnssStrFusionModeSuspended);
                break;
            case XPLR_GNSS_FUSION_MODE_DISABLED:
                intRet = snprintf(modeStr, maxLen, gnssStrFusionModeDisabled);
                break;
            default:
                intRet = snprintf(modeStr, maxLen, gnssStrFusionModeUnknown);
                break;
        }

        if (intRet < 0) {
            XPLRGNSS_CONSOLE(E, "Getting fusion mode string failed with error code [%d]!", intRet);
            modeStr[0] = 0;
            ret = ESP_FAIL;
        } else if (intRet == 0) {
            XPLRGNSS_CONSOLE(E, "Could not write anything on the fusion mode string buffer!");
            modeStr[0] = 0;
            ret = ESP_FAIL;
        } else if (intRet >= maxLen) {
            XPLRGNSS_CONSOLE(E,
                             "Fusion mode string buffer [%d] not large enough to store message [%d]",
                             maxLen,
                             intRet);
            modeStr[0] = 0;
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * String helper for sensor type
 */
static esp_err_t gnssSensorTypeToString(xplrGnssSensorType_t *type,
                                        char *typeStr,
                                        uint8_t maxLen)
{
    esp_err_t ret;
    int intRet;

    if ((type == NULL) || (typeStr == NULL)) {
        XPLRGNSS_CONSOLE(E, "Type string pointer is null!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        switch (*type) {
            case XPLR_GNSS_SENSOR_GYRO_Z_ANG_RATE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeGyroZAng);
                break;
            case XPLR_GNSS_SENSOR_WT_RL_WHEEL:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeWtRL);
                break;
            case XPLR_GNSS_SENSOR_WT_RR_WHEEL:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeWtRR);
                break;
            case XPLR_GNSS_SENSOR_WT_ST_WHEEL:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeWtST);
                break;
            case XPLR_GNSS_SENSOR_SPEED:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeSpeed);
                break;
            case XPLR_GNSS_SENSOR_GYRO_TEMP:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeGyroTemp);
                break;
            case XPLR_GNSS_SENSOR_GYRO_Y_ANG_RATE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeGyroYAng);
                break;
            case XPLR_GNSS_SENSOR_GYRO_X_ANG_RATE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeGyroXAng);
                break;
            case XPLR_GNSS_SENSOR_ACCEL_X_SPCF_FORCE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeAccelXSpcF);
                break;
            case XPLR_GNSS_SENSOR_ACCEL_Y_SPCF_FORCE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeAccelYSpcF);
                break;
            case XPLR_GNSS_SENSOR_ACCEL_Z_SPCF_FORCE:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeAccelZSpcF);
                break;
            default:
                intRet = snprintf(typeStr, maxLen, gnssStrSensTypeUnknown);
                break;
        }

        if (intRet < 0) {
            XPLRGNSS_CONSOLE(E, "Getting sensor string type failed with error code[%d]!", intRet);
            typeStr[0] = 0;
            ret = ESP_FAIL;
        } else if (intRet == 0) {
            XPLRGNSS_CONSOLE(E, "Could not write anything on the sensor string type buffer!");
            typeStr[0] = 0;
            ret = ESP_FAIL;
        } else if (intRet >= maxLen) {
            XPLRGNSS_CONSOLE(E,
                             "Sensor type string buffer [%d] not large enbough to store message [%d]",
                             maxLen,
                             intRet);
            typeStr[0] = 0;
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

static esp_err_t gnssImuAlignStatPrinter(xplrGnssImuFusionStatus_t *status)
{
    esp_err_t ret;
    uint8_t numSens;
    char tmpStr[XPLR_GNSS_SENS_ERR_BUFF_SIZE];

    XPLRGNSS_CONSOLE(I, "Printing Imu Alignment Statuses.");

    memset(tmpStr, 0, sizeof(tmpStr));
    printf("===== Imu Alignment Status ====\n");
    ret = gnssFusionModeToString(&status->fusionMode,
                                 tmpStr,
                                 ELEMENTCNT(tmpStr));
    if (ret == ESP_OK) {
        printf("Fusion mode: %s\n", tmpStr);

        printf("Number of sensors: %d\n", status->numSens);
        printf("-------------------------------\n");

        for (numSens = 0; numSens < status->numSens; numSens++) {
            memset(tmpStr, 0, sizeof(tmpStr));
            ret = gnssSensorTypeToString(&status->sensor[numSens].type,
                                         tmpStr,
                                         ELEMENTCNT(tmpStr));
            if (ret == ESP_OK) {
                printf("Sensor type: %s\n", tmpStr);
                printf("Used: %d | Ready: %d\n",
                        status->sensor[numSens].used,
                        status->sensor[numSens].ready);
                printf("Sensor observation frequency: %d Hz\n",
                        status->sensor[numSens].freq);
                memset(tmpStr, 0, sizeof(tmpStr));
                ret = gnssSensorMeasErrToString(&status->sensor[numSens].faults,
                                                tmpStr,
                                                ELEMENTCNT(tmpStr));
                if (ret != ESP_OK) {
                    XPLRGNSS_CONSOLE(E, "Error getting sensor faults!");
                    tmpStr[0] = 0;
                    break;
                } else {
                    printf("Sensor faults: %s\n", tmpStr);
                }
            } else {
                XPLRGNSS_CONSOLE(E, "Error getting sensor type!");
                tmpStr[0] = 0;
                break;
            }
            printf("-------------------------------\n");
        }
    } else {
        printf("Fusion mode: Error getting string\n");
    }

    return ret;
}

/**
 * String helper for sensor error type
 */
static esp_err_t gnssSensorMeasErrToString(xplrGnssImuEsfStatSensorFaults_t *faults, 
                                           char *errStr, 
                                           uint8_t maxLen)
{
    esp_err_t ret;
    int intRet = 0;

    if ((faults == NULL) || (errStr == NULL)) {
        XPLRGNSS_CONSOLE(E, "Invalid argument!");
        ret = ESP_ERR_INVALID_ARG;
    } else {
        memset(errStr, 0, maxLen);

        if (faults->allFaults == 0) {
            intRet = snprintf(errStr, maxLen, gnssStrSensStateErrNone);
            if (intRet < 0) {
                XPLRGNSS_CONSOLE(E, "Error writing error type to buffer!");
                ret = ESP_FAIL;
            } else if (intRet >= maxLen) {
                XPLRGNSS_CONSOLE(E, "Error buffer not large enough!");
                ret = ESP_FAIL;
            } else {
                ret = ESP_OK;
            }
        } else {
            if (faults->singleFaults.badMeasurements) {
                intRet = snprintf(errStr, maxLen, gnssStrSensStateErrBadMeas);
                if (intRet < 0) {
                    XPLRGNSS_CONSOLE(E, "Error writing error type to buffer!");
                    ret = ESP_FAIL;
                } else if (intRet >= maxLen) {
                    XPLRGNSS_CONSOLE(E, "Error buffer not large enough!");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                ret = ESP_OK;
            }

            if ((ret == ESP_OK) && faults->singleFaults.badTTag) {
                if (intRet != 0) {
                    intRet += snprintf(errStr + intRet, maxLen, ", ");
                } else {
                    // do nothing
                }
                intRet += snprintf(errStr + intRet, maxLen, "%s", gnssStrSensStateErrBadTtag);
                if (intRet < 0) {
                    XPLRGNSS_CONSOLE(E, "Error writing error type to buffer!");
                    ret = ESP_FAIL;
                } else if (intRet >= maxLen) {
                    XPLRGNSS_CONSOLE(E, "Error buffer not large enough!");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                ret = ESP_OK;
            }

            if ((ret == ESP_OK) && faults->singleFaults.missingMeasurments) {
                if (intRet != 0) {
                    intRet += snprintf(errStr + intRet, maxLen, ", ");
                } else {
                    // do nothing
                }
                intRet += snprintf(errStr + intRet, maxLen, "%s", gnssStrSensStateErrMissMeas);
                if (intRet < 0) {
                    XPLRGNSS_CONSOLE(E, "Error writing error type to buffer!");
                    ret = ESP_FAIL;
                } else if (intRet >= maxLen) {
                    XPLRGNSS_CONSOLE(E, "Error buffer not large enough!");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                ret = ESP_OK;
            }

            if ((ret == ESP_OK) && faults->singleFaults.noisyMeas) {
                if (intRet != 0) {
                    intRet += snprintf(errStr + intRet, maxLen, ", ");
                } else {
                    // do nothing
                }
                intRet += snprintf(errStr + intRet, maxLen, "%s", gnssStrSensStateErrNoisyMeas);
                if (intRet < 0) {
                    XPLRGNSS_CONSOLE(E, "Error writing error type to buffer!");
                    ret = ESP_FAIL;
                } else if (intRet >= maxLen) {
                    XPLRGNSS_CONSOLE(E, "Error buffer not large enough!");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                ret = ESP_OK;
            }
        }
    }

    return ret;
}

/**
 * Initializes NVS for transactions
 */
static esp_err_t gnssNvsInit(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    char id[4] = {0x00};
    xplrLocNvs_t *storage = &locDvc->options.storage;
    xplrNvs_error_t nvsRet;

    /* create namespace tag for given client */
    sprintf(id, "%u", dvcProfile);
    memset(storage->nvs.tag, 0x00, NVS_KEY_NAME_MAX_SIZE);
    memset(storage->id, 0x00, NVS_KEY_NAME_MAX_SIZE);
    strcat(storage->nvs.tag, nvsNamespace);
    strcat(storage->id, storage->nvs.tag);
    strcat(storage->id, id);

    /* init nvs */
    nvsRet = xplrNvsInit(&storage->nvs, storage->id);

    if (nvsRet != XPLR_NVS_OK) {
        XPLRGNSS_CONSOLE(E, "Failed to init nvs namespace <%s>.", storage->id);
        ret = ESP_FAIL;
    } else {
        XPLRGNSS_CONSOLE(D, "NVS namespace <%s> for GNSS, init ok", storage->id);
        ret = ESP_OK;
    }

    return ret;
}

/**
 * Loads calibration data from NVS if present.
 * If not populates default invalid values 
 * (i.e. values which are out of bounds)
 */
static esp_err_t gnssNvsLoad(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    char storedId[NVS_KEY_NAME_MAX_SIZE] = {0x00};
    bool writeDefaults;
    xplrLocNvs_t *storage = &locDvc->options.storage;
    xplrNvs_error_t err;
    size_t size = NVS_KEY_NAME_MAX_SIZE;

    /* try read id key */
    err = xplrNvsReadString(&storage->nvs, "id", storedId, &size);
    if ((err != XPLR_NVS_OK) || (strlen(storedId) < 1)) {
        XPLRGNSS_CONSOLE(W, "id key not found in <%s>, write defaults", storage->id);
        writeDefaults = true;
    } else {
        XPLRGNSS_CONSOLE(D, "id key <%s> found in <%s>", storedId, storage->id);
        writeDefaults = false;
    }

    if (writeDefaults) {
        ret = gnssNvsWriteDefaults(dvcProfile);
        if (ret == ESP_OK) {
            ret = gnssNvsReadConfig(dvcProfile);
        } else {
            // do nothing
        }
    } else {
        ret = gnssNvsReadConfig(dvcProfile);
    }

    return ret;
}

/**
 * writes default values
 */
static esp_err_t gnssNvsWriteDefaults(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    xplrLocNvs_t *storage = &locDvc->options.storage;
    xplrNvs_error_t err[4];

    XPLRGNSS_CONSOLE(D, "Writing default settings in NVS");
    err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
    err[1] = xplrNvsWriteU32(&storage->nvs, "yaw", gnssSensDefaultCalibValYaw);
    err[2] = xplrNvsWriteI16(&storage->nvs, "pitch", gnssSensDefaultCalibValPitch);
    err[3] = xplrNvsWriteI16(&storage->nvs, "roll", gnssSensDefaultCalibValRoll);

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            XPLRGNSS_CONSOLE(E, "Error writing element %u of default settings in NVS", i);
            break;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * Loads stored configuration in NVS
 */
static esp_err_t gnssNvsReadConfig(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    xplrLocNvs_t *storage = &locDvc->options.storage;
    size_t size = NVS_KEY_NAME_MAX_SIZE;
    xplrNvs_error_t err[4];

    err[0] = xplrNvsReadString(&storage->nvs,"id", storage->id, &size);

    err[1] = xplrNvsReadU32(&storage->nvs,
                            "yaw",
                            &locDvc->conf->dr.alignVals.yaw);
    err[2] = xplrNvsReadI16(&storage->nvs,
                            "pitch",
                            &locDvc->conf->dr.alignVals.pitch);
    err[3] = xplrNvsReadI16(&storage->nvs,
                            "roll",
                            &locDvc->conf->dr.alignVals.roll);

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            break;
        } else {
            ret = ESP_OK;
        }
    }

    if (ret == ESP_OK) {
        XPLRGNSS_CONSOLE(D, "Read NVS id: <%s>", storage->id);
        XPLRGNSS_CONSOLE(D, "Read NVS yaw: <%d>", locDvc->conf->dr.alignVals.yaw);
        XPLRGNSS_CONSOLE(D, "Read NVS pitch: <%d>", locDvc->conf->dr.alignVals.pitch);
        XPLRGNSS_CONSOLE(D, "Read NVS roll: <%d>", locDvc->conf->dr.alignVals.roll);
    } else {
        // do nothing
    }

    return ret;
}

/**
 * Updates/Writes calibration values into NVS
 */
esp_err_t gnssNvsUpdate(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    xplrLocNvs_t *storage = &locDvc->options.storage;
    xplrNvs_error_t err[4];
    bool valsValid;

    valsValid = gnssCheckAlignValsLimits(dvcProfile);

    if ((storage->id != NULL) && valsValid) {
        err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);

        err[1] = xplrNvsWriteU32(&storage->nvs,
                                 "yaw",
                                 locDvc->conf->dr.alignVals.yaw);
        err[2] = xplrNvsWriteI16(&storage->nvs,
                                 "pitch",
                                 locDvc->conf->dr.alignVals.pitch);
        err[3] = xplrNvsWriteI16(&storage->nvs,
                                 "roll",
                                 locDvc->conf->dr.alignVals.roll);

        for (int i = 0; i < 4; i++) {
            if (err[i] != XPLR_NVS_OK) {
                ret = ESP_FAIL;
                break;
            } else {
                ret = ESP_OK;
            }
        }
    } else {
        ret = ESP_FAIL;
        XPLRGNSS_CONSOLE(E, "Trying to write invalid config data!");
    }

    return ret;
}

/**
 * Deletes stored calibration values from NVS
 */
esp_err_t gnssNvsErase(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    xplrLocNvs_t *storage = &locDvc->options.storage;
    xplrNvs_error_t err[4];

    err[0] = xplrNvsEraseKey(&storage->nvs, "id");
    err[1] = xplrNvsEraseKey(&storage->nvs, "yaw");
    err[2] = xplrNvsEraseKey(&storage->nvs, "pitch");
    err[3] = xplrNvsEraseKey(&storage->nvs, "roll");

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            break;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

/**
 * Checks if Yaw value is valid
 */
static bool gnssCheckYawValLimits(uint32_t yaw)
{
    bool ret;
    if (yaw > gnssSensMaxValYaw) {
        ret = false;
    } else {
        ret = true;
    }
    return ret;
}

/**
 * Checks if Pitch value is valid
 */
static bool gnssCheckPitchValLimits(int16_t pitch)
{
    bool ret;
    if ((pitch < gnssSensMinValPitch) || 
        (pitch > gnssSensMaxValPitch)) {
        ret = false;
    } else {
        ret = true;
    }
    return ret;
}

/**
 * Checks if Roll value is valid
 */
static bool gnssCheckRollValLimits(int16_t roll)
{
    bool ret;
    if ((roll < gnssSensMinValRoll) || 
        (roll > gnssSensMaxValRoll)) {
        ret = false;
    } else {
        ret = true;
    }
    return ret;
}

/**
 * Checks if all calibrations values are valid
 */
static bool gnssCheckAlignValsLimits(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    bool validYaw, validRoll, validPitch, ret;

    validYaw = gnssCheckYawValLimits(locDvc->conf->dr.alignVals.yaw);
    validPitch = gnssCheckPitchValLimits(locDvc->conf->dr.alignVals.pitch);
    validRoll = gnssCheckRollValLimits(locDvc->conf->dr.alignVals.roll);
    if (validYaw && validRoll && validPitch) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

/**
 * Sets up generic location settings needed by GNSS
 */
static esp_err_t gnssLocSetGenericSettings(uint8_t dvcProfile)
{
    esp_err_t ret;
    ret = xplrGnssOptionMultiValSet(dvcProfile,
                                    gnssGenericSettings,
                                    ELEMENTCNT(gnssGenericSettings),
                                    U_GNSS_CFG_VAL_LAYER_RAM);
    return ret;
}

/**
 * Sets up generic DR settings
 */
static esp_err_t gnssDrSetGenericSettings(uint8_t dvcProfile)
{
    esp_err_t ret;
    ret = xplrGnssOptionMultiValSet(dvcProfile,
                                    gnssGenericDrSettings,
                                    ELEMENTCNT(gnssGenericDrSettings),
                                    U_GNSS_CFG_VAL_LAYER_RAM);
    return ret;
}

/**
 * Sets up vehicle type for DR
 */
static esp_err_t gnssDrSetVehicleType(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    ret = xplrGnssOptionSingleValSet(dvcProfile,
                                     U_GNSS_CFG_VAL_KEY_ID_NAVSPG_DYNMODEL_E1,
                                     (uint64_t)locDvc->conf->dr.vehicleDynMode,
                                     U_GNSS_CFG_VAL_LAYER_RAM);
    return ret;
}

/**
 * Sets DR alignment mode Auto or Manual
 */
static esp_err_t gnssDrSetAlignMode(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    esp_err_t ret;
    ret = xplrGnssOptionSingleValSet(dvcProfile,
                                     U_GNSS_CFG_VAL_KEY_ID_SFIMU_AUTO_MNTALG_ENA_L,
                                     (uint64_t)locDvc->conf->dr.mode,
                                     U_GNSS_CFG_VAL_LAYER_RAM);
    return ret;
}

/**
 * Starts or stops DR
 */
static esp_err_t gnssDrStartStop(uint8_t dvcProfile, xplrGnssDrStartOpt_t opt)
{
    esp_err_t ret;
    ret = xplrGnssOptionSingleValSet(dvcProfile,
                                     U_GNSS_CFG_VAL_KEY_ID_SFCORE_USE_SF_L,
                                     (uint64_t)opt,
                                     U_GNSS_CFG_VAL_LAYER_RAM);
    return ret;
}

/**
 * Checks if SFCORE is enabled denoting that DR will be available
 */
static bool gnssIsDrEnabled(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    bool ret;

    if (locDvc->options.flags.status.gnssIsDrEnabled == 0) {
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

/**
 * Asyncs feed watchdog timer in order to detect a timeout
 */
static esp_err_t gnssFeedWatchdog(xplrGnss_t *locDvc)
{
    esp_err_t ret;

    if (xSemaphoreTake(locDvc->options.xSemWatchdog, portMAX_DELAY) == pdTRUE) {
        locDvc->options.lastWatchdogTime = esp_timer_get_time();
        ret = ESP_OK;
    } else {
        XPLRGNSS_CONSOLE(E,
                         "Feeding watchdog failed after [%d] ms!",
                         XPLR_GNSS_MAX_WATCHDOG_SEM_WAITMS);
        ret = ESP_FAIL;
    }

    if ((ret == ESP_OK) &&
        (xSemaphoreGive(locDvc->options.xSemWatchdog) != pdTRUE)) {
        XPLRGNSS_CONSOLE(E,
                         "Failed releasing xSemWatchdog!");
        ret = ESP_FAIL;
    } else {
        // do nothing
    }

    return ret;
}

/**
 * Check is watchdog has timed out
 */
static bool gnssCheckWatchdog(uint8_t dvcProfile)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    bool ret;

    if (xSemaphoreTake(locDvc->options.xSemWatchdog, portMAX_DELAY) == pdTRUE) {
        if (MICROTOSEC(esp_timer_get_time() - locDvc->options.lastWatchdogTime) > XPLR_GNSS_WATCHDOG_TIMEOUT_SECS) {
            XPLRGNSS_CONSOLE(E,
                             "Watchdog triggered. No messages in the last [%d] seconds!",
                             XPLR_GNSS_WATCHDOG_TIMEOUT_SECS);
            ret = true;
        } else {
            ret = false;
        }
    } else {
        XPLRGNSS_CONSOLE(E,
                         "Taking xSemWatchdog failed after [%d] ms!",
                         XPLR_GNSS_MAX_WATCHDOG_SEM_WAITMS);
        ret = true;
    }

    if ((ret == ESP_OK) &&
        (xSemaphoreGive(locDvc->options.xSemWatchdog) != pdTRUE)) {
        XPLRGNSS_CONSOLE(E,
                         "Failed releasing xSemWatchdog!");
        ret = true;
    } else {
        // do nothing
    }

    return ret;
}

/**
 * Checks if input device profile is valid
 */
static bool gnssIsDvcProfileValid(uint8_t dvcProfile)
{
    bool ret;
    ret = xplrHlprLocSrvcCheckDvcProfileValidity(dvcProfile, XPLRGNSS_NUMOF_DEVICES);
    return ret;
}

/**
 * Location main printer helper
 */
static void gnssLocationPrinter(char *pLocFixTypeStr, xplrGnssLocation_t *locData)
{
    XPLRGNSS_CONSOLE(I, "Printing location info.");
    printf("======== Location Info ========\n");
    gnssLocPrintLocType(locData);
    gnssLocPrintFixType(pLocFixTypeStr);
    gnssLocPrintLongLat(locData);
    gnssLocPrintAlt(locData);
    gnssLocPrintRad(locData);
    gnssLocPrintSpeed(locData);
    gnssLocPrintAcc(locData);
    gnssLocPrintSatNo(locData);
    gnssLocPrintTime(locData);
    printf("===============================\n");
}

/**
 * Location type printer
 */
static void gnssLocPrintLocType(xplrGnssLocation_t *locData)
{
    printf("Location type: %d\n", locData->location.type); 
}

/**
 * Fix type printer
 */
static void gnssLocPrintFixType(char *pLocFixTypeStr)
{
    printf("Location fix type: %s\n", pLocFixTypeStr);
}

/**
 * Longitude-Latitude printer
 */
static void gnssLocPrintLongLat(xplrGnssLocation_t *locData)
{
    printf("Location latitude: %f (raw: %d)\n", 
           locData->location.latitudeX1e7  * (1e-7),
           locData->location.latitudeX1e7);
    printf("Location longitude: %f (raw: %d)\n", 
           locData->location.longitudeX1e7 * (1e-7),
           locData->location.longitudeX1e7);
}

/**
 * Altitude printer
 */
static void gnssLocPrintAlt(xplrGnssLocation_t *locData)
{
    if (locData->location.altitudeMillimetres != INT_MIN) {
        printf("Location altitude: %f (m) | %d (mm)\n", 
               locData->location.altitudeMillimetres * (1e-3),
               locData->location.altitudeMillimetres);
    } else {
        printf("Location altitude: N/A\n");
    }
}

/**
 * Radius printer
 */
static void gnssLocPrintRad(xplrGnssLocation_t *locData)
{
    if (locData->location.radiusMillimetres != -1) {
        printf("Location radius: %f (m) | %d (mm)\n", 
        locData->location.radiusMillimetres * (1e-3),
        locData->location.radiusMillimetres);
    } else {
        printf("Location radius: N/A\n");
    }
}

/**
 * Speed printer
 */
static void gnssLocPrintSpeed(xplrGnssLocation_t *locData)
{
    if (locData->location.speedMillimetresPerSecond != INT_MIN) {
        printf("Speed: %f (km/h) | %f (m/s) | %d (mm/s)\n",
               locData->location.speedMillimetresPerSecond * (1e-6) * 3600,
               locData->location.speedMillimetresPerSecond * (1e-3),
               locData->location.speedMillimetresPerSecond);
    } else {
        printf("Location radius: N/A\n");
    }
}

/**
 * Accuracy prnter
 */
static void gnssLocPrintAcc(xplrGnssLocation_t *locData)
{
    printf("Estimated horizontal accuracy: %.4f (m) | %.2f (mm)\n",
           locData->accuracy.horizontal * (1e-4),
           locData->accuracy.horizontal * (1e-1));

    printf("Estimated vertical accuracy: %.4f (m) | %.2f (mm)\n",
           locData->accuracy.vertical * (1e-4),
           locData->accuracy.vertical * (1e-1));
}

/**
 * Helper update state for FSM
 */
static void gnssUpdateNextState(uint8_t dvcProfile,
                                xplrGnssStates_t nextState)
{
    xplrGnss_t *locDvc = &dvc[dvcProfile];
    locDvc->options.state[1] = locDvc->options.state[0];
    locDvc->options.state[0] = nextState;
}

/**
 * Sattelite number printer
 */
static void gnssLocPrintSatNo(xplrGnssLocation_t *locData)
{
    if (locData->location.svs != -1) {
        printf("Satellite number: %d\n", locData->location.svs);
    } else {
        printf("Satellite number: N/A\n");
    }
}

/**
 * Time printer
 */
static void gnssLocPrintTime(xplrGnssLocation_t *locData)
{
    esp_err_t ret;
    char timeToHuman[32];

    ret = xplrTimestampToTime(locData->location.timeUtc,
                              timeToHuman,
                              ELEMENTCNT(timeToHuman));
    if (ret == ESP_OK) {
        printf("Time UTC: %s\n", timeToHuman);
    } else {
        printf("Time UTC: Error Parsing Time\n");
    }

    ret = xplrTimestampToDate(locData->location.timeUtc,
                              timeToHuman,
                              ELEMENTCNT(timeToHuman));
    if (ret == ESP_OK) {
        printf("Date UTC: %s\n", timeToHuman);
    } else {
        printf("Date UTC: Error Parsing Time\n");
    }

    ret = xplrTimestampToDateTime(locData->location.timeUtc,
                                  timeToHuman,
                                  ELEMENTCNT(timeToHuman));
    if (ret == ESP_OK) {
        printf("Calendar Time UTC: %s\n", timeToHuman);
    } else {
        printf("Calendar Time UTC: Error Parsing Time\n");
    }
}

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRGNSS_LOG_ACTIVE)
static void gnssLogTask(void *pvParams)
{
    char *item;
    size_t retSize;
    uint32_t cntWaiting;

    while (1) {
        // Take semaphore
        if (xSemaphore != NULL) {
            if (xSemaphoreTake(xSemaphore, XPLR_GNSS_LOG_RING_BUF_TIMEOUT) == pdTRUE) {
                if (asyncLog.isEnabled) {
                    // Get items waiting number
                    vRingbufferGetInfo(asyncLog.xRingBuffer, NULL, NULL, NULL, NULL, &cntWaiting);
                    // if items found log them to the SD
                    if (cntWaiting > 0) {
                        for (int i = 0; i < cntWaiting; i++) {
                            // take all messages and log
                            item = (char *)xRingbufferReceiveFromISR(asyncLog.xRingBuffer, &retSize);
                            if (retSize > XPLR_GNSS_LOG_RING_BUF_SIZE) {
                                XPLRGNSS_CONSOLE(E, "token larger than slot!");
                            } else {
                                if (item != NULL) {
                                    xplrSdWriteFileU8(asyncLog.logCfg.sd,
                                                      asyncLog.logCfg.logFilename,
                                                      (uint8_t *)item,
                                                      retSize,
                                                      XPLR_FILE_MODE_APPEND);
                                } else {
                                    XPLRGNSS_CONSOLE(W, "Empty item came from ring buffer!");
                                }
                            }
                            // return the item to ringbuffer to free the slot
                            vRingbufferReturnItem(asyncLog.xRingBuffer, (void *)item);
                        }
                    } else {
                        //Do nothing
                    }
                }
                // give semaphore
                xSemaphoreGive(xSemaphore);
            } else {
                XPLRGNSS_CONSOLE(E, "Semaphore timeout");
            }
            // Give time for other tasks
            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }
}
#endif

#if (1 == XPLRGNSS_LOG_ACTIVE) && (1 == XPLR_HPGLIB_LOG_ENABLED)
static void gnssLogCallback(char *buffer, uint16_t length)
{
    BaseType_t ringRet, dummy;

    /* Check if semaphore was created*/
    if (semaphoreCreated){
        // Take semaphore
        if (xSemaphoreTake(xSemaphore, XPLR_GNSS_LOG_RING_BUF_TIMEOUT) == pdTRUE) {
            if (asyncLog.isEnabled && asyncLog.isInit) {
                // Send item to ring buffer
                ringRet = xRingbufferSendFromISR(asyncLog.xRingBuffer,
                                                 buffer,
                                                 length,
                                                 &dummy);
                if (ringRet != pdTRUE) {
                    XPLRGNSS_CONSOLE(W, "Send to ring buffer failed!");
                } else {
                    // Data successfully sent to ring buffer do nothing...
                }
            } else {
                // Logging is halted do nothing...
            }
            // Give semaphore
            xSemaphoreGive(xSemaphore);
        } else {
            XPLRGNSS_CONSOLE(E, "Could not take semaphore!");
        }
    }
}
#endif

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRLOCATION_LOG_ACTIVE)
// Function that logs the print location messages to the SD card
static void gnssLogLocationPrinter(char *pLocFixTypeStr, xplrGnssLocation_t *locData)
{

    size_t maxBuff = 511;
    char temp[1024] = {0};
    char timeToHuman[32];
    uint8_t timeParseRet;

    // strncat(temp, "Printing location info.", maxBuff);
    snprintf(temp + strlen(temp), maxBuff, "Printing location info.\n");
    snprintf(temp + strlen(temp), maxBuff, "======== Location Info ========\n");
    // strncat(temp, "======== Location Info ========\n", maxBuff);
    snprintf(temp + strlen(temp), maxBuff, "Location type: %d\n", locData->location.type);
    snprintf(temp + strlen(temp), maxBuff, "Location fix type: %s\n", pLocFixTypeStr);
    snprintf(temp + strlen(temp), maxBuff, "Location latitude: %f (raw: %d)\n",
             locData->location.latitudeX1e7  * (1e-7),
             locData->location.latitudeX1e7);
    snprintf(temp + strlen(temp), maxBuff, "Location longitude: %f (raw: %d)\n",
             locData->location.longitudeX1e7 * (1e-7),
             locData->location.longitudeX1e7);

    if (locData->location.altitudeMillimetres != INT_MIN) {
        snprintf(temp + strlen(temp), maxBuff, "Location altitude: %f (m) | %d (mm)\n",
                 locData->location.altitudeMillimetres * (1e-3),
                 locData->location.altitudeMillimetres);
    } else {
        strncat(temp, "Location altitude: N/A\n", maxBuff);
    }

    if (locData->location.radiusMillimetres != -1) {
        snprintf(temp + strlen(temp), maxBuff, "Location radius: %f (m) | %d (mm)\n",
                 locData->location.radiusMillimetres * (1e-3),
                 locData->location.radiusMillimetres);
    } else {
        strncat(temp, "Location radius: N/A\n", maxBuff);
    }

    if (locData->location.speedMillimetresPerSecond != INT_MIN) {
        snprintf(temp + strlen(temp), maxBuff, "Speed: %f (km/h) | %f (m/s) | %d (mm/s)\n",
                 locData->location.speedMillimetresPerSecond * (1e-6) * 3600,
                 locData->location.speedMillimetresPerSecond * (1e-3),
                 locData->location.speedMillimetresPerSecond);
    } else {
        strncat(temp, "Location radius: N/A\n", maxBuff);
    }

    snprintf(temp + strlen(temp), maxBuff, "Estimated horizontal accuracy: %.4f (m) | %.2f (mm)\n",
             locData->accuracy.horizontal * (1e-4),
             locData->accuracy.horizontal * (1e-1));

    snprintf(temp + strlen(temp), maxBuff, "Estimated vertical accuracy: %.4f (m) | %.2f (mm)\n",
             locData->accuracy.vertical * (1e-4),
             locData->accuracy.vertical * (1e-1));

    if (locData->location.svs != -1) {
        snprintf(temp + strlen(temp), maxBuff, "Satellite number: %d\n", locData->location.svs);
    } else {
        strncat(temp, "Satellite number: N/A\n", maxBuff);
    }

    if (locData->location.timeUtc != -1) {
        timeParseRet = xplrTimestampToTime(locData->location.timeUtc,
                                           timeToHuman,
                                           ELEMENTCNT(timeToHuman));
        if (timeParseRet == ESP_OK) {
            snprintf(temp + strlen(temp), maxBuff, "Time UTC: %s\n", timeToHuman);

        } else {
            strncat(temp, "Time UTC: Error Parsing Time\n", maxBuff);
        }

        timeParseRet = xplrTimestampToDate(locData->location.timeUtc,
                                           timeToHuman,
                                           ELEMENTCNT(timeToHuman));
        if (timeParseRet == ESP_OK) {
            snprintf(temp + strlen(temp), maxBuff, "Date UTC: %s\n", timeToHuman);
        } else {
            strncat(temp, "Date UTC: Error Parsing Time\n", maxBuff);
        }

        timeParseRet = xplrTimestampToDateTime(locData->location.timeUtc,
                                               timeToHuman,
                                               ELEMENTCNT(timeToHuman));
        if (timeParseRet == ESP_OK) {
            snprintf(temp + strlen(temp), maxBuff, "Calendar Time UTC: %s\n", timeToHuman);
        } else {
            strncat(temp, "Date UTC: Error Parsing Time\n", maxBuff);
        }
    } else {
        strncat(temp, "Time UTC: N/A\n", maxBuff);
    }

    strncat(temp, "===============================\n", maxBuff);

    XPLRLOG(&locationLog, temp);
}
#endif

/* ----------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

/**
 * All payloads in this callback are in binary form
 * **/
static void gnssUbxProtocolCB(uDeviceHandle_t gnssHandle,
                              const uGnssMessageId_t *msgIdToFilter,
                              int32_t errorCodeOrLength,
                              void *callbackParam)
{
    esp_err_t ret;
    int cbRead = 0;
    xplrGnss_t *locDvc = (xplrGnss_t *)callbackParam;
    char buffer[XPLR_GNSS_UBX_BUFF_SIZE];

    if (errorCodeOrLength > 0) {
        if (errorCodeOrLength < XPLR_GNSS_UBX_BUFF_SIZE) {
            cbRead = uGnssMsgReceiveCallbackRead(gnssHandle, buffer, errorCodeOrLength);
            if (cbRead == errorCodeOrLength) {
                gnssFeedWatchdog(locDvc);
#if (1 == XPLRGNSS_LOG_ACTIVE) && (1 == XPLR_HPGLIB_LOG_ENABLED)
                gnssLogCallback(buffer, cbRead);
#endif
                if (gnssUbxIsMessageId(msgIdToFilter, &msgIdHpposllh)) {
                    ret = gnssAccuracyParser(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss Accuracy parser failed!");
                    }
                } else if (gnssUbxIsMessageId(msgIdToFilter, &msgIdNavPvt)) {
                    ret = gnssGeolocationParser(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss Geolocation parser failed!");
                    }
                } else if (gnssUbxIsMessageId(msgIdToFilter, &msgIdEsfAlg)) {
                    ret = gnssEsfAlgParser(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss ESF-ALG parser failed!");
                    }
                } else if (gnssUbxIsMessageId(msgIdToFilter, &msgIdEsfStatus)) {
                    ret = gnssEsfStatusParser(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss ESF-STAT parser failed!");
                    }
                } else if (gnssUbxIsMessageId(msgIdToFilter, &msgIdEsfIns)) {
                    ret = gnssEsfInsParser(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss ESF-INS parser failed!");
                    } else {
                        // do nothing
                    }
                }
            } else {
                XPLRGNSS_CONSOLE(W,
                                 "Ubx protocol async length read missmatch: read [%d] bytes - message must be size [%d]!",
                                 cbRead,
                                 errorCodeOrLength);
            }
        } else {
            XPLRGNSS_CONSOLE(E,
                             "Ubx protocol buffer [%d] not large enough: read size [%d]!",
                             XPLR_GNSS_UBX_BUFF_SIZE,
                             cbRead);
        }
    } else {
        XPLRGNSS_CONSOLE(E, "Ubx protocol async read error: [%d]!", cbRead);
    }
}

/**
 * All payloads in this callback are in text form
 * **/
static void gnssNmeaProtocolCB(uDeviceHandle_t gnssHandle,
                               const uGnssMessageId_t *msgIdToFilter,
                               int32_t errorCodeOrLength,
                               void *callbackParam)
{
    esp_err_t ret;
    int cbRead = 0;
    xplrGnss_t *locDvc = (xplrGnss_t *)callbackParam;
    char buffer[XPLR_GNSS_NMEA_BUFF_SIZE];

    if (errorCodeOrLength > 0) {
        if (errorCodeOrLength < XPLR_GNSS_NMEA_BUFF_SIZE) {
            cbRead = uGnssMsgReceiveCallbackRead(gnssHandle, buffer, errorCodeOrLength);
            if (cbRead == errorCodeOrLength) {
                gnssFeedWatchdog(locDvc);
#if (1 == XPLRGNSS_LOG_ACTIVE) && (1 == XPLR_HPGLIB_LOG_ENABLED)
                gnssLogCallback(buffer, cbRead);
#endif
                buffer[cbRead] = 0;
                if (gnssNmeaIsMessageId(msgIdToFilter, &msgIdFixType)) {
                    ret = gnssGetLocFixType(locDvc, buffer);
                    if (ret != ESP_OK) {
                        XPLRGNSS_CONSOLE(W, "Gnss LOC-FIX parser failed!");
                    } else {
                        // do nothing
                    }
                } else {
                    // do nothing
                }
            } else {
                XPLRGNSS_CONSOLE(W, 
                                 "NMEA protocol async length read missmatch: read [%d] bytes - message must be size [%d]!",
                                 cbRead, 
                                 errorCodeOrLength);
            }
        } else {
            XPLRGNSS_CONSOLE(E,
                             "NMEA protocol buffer [%d] not large enough: read size [%d]!",
                             XPLR_GNSS_NMEA_BUFF_SIZE,
                             cbRead);
        }
    } else {
        XPLRGNSS_CONSOLE(E, "NMEA protocol async read error: [%d]!", cbRead);
    }
}
