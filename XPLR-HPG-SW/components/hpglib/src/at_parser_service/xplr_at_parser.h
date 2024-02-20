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

#ifndef _XPLR_AT_PARSER_H_
#define _XPLR_AT_PARSER_H_

/** @file
 * @brief AT command parser, hpglib library utlizing at_server_service.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "xplr_at_server.h"
#include "xplr_thingstream_types.h"
#include "xplr_wifi_ntrip_client_types.h"
#include "xplr_gnss.h"
#include "xplr_log.h"
#include "./../../hpglib/src/nvs_service/xplr_nvs.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* Max length of SSID name */
#define XPLR_AT_PARSER_SSID_LENGTH  (32U+1U)

/* Max length of SSID password */
#define XPLR_AT_PARSER_PASSWORD_LENGTH  (64U+1U)

/* Max length of cellular APN */
#define XPLR_AT_PARSER_APN_LENGTH (64U+1U)

/* Max length of thingstream region user input string ("eu"/"us"/"all") */
#define XPLR_AT_PARSER_TSREGION_LENGTH (3U)

/* Max length of thingstream plan user input string ("ip"/"ip+lband"/"lband") */
#define XPLR_AT_PARSER_TSPLAN_LENGTH (9U)

/* NTRIP user configuration length */
#define XPLR_AT_PARSER_NTRIP_HOST_LENGTH (64U)
#define XPLR_AT_PARSER_NTRIP_USERAGENT_LENGTH (64U)
#define XPLR_AT_PARSER_NTRIP_MOUNTPOINT_LENGTH (128U)
#define XPLR_AT_PARSER_NTRIP_CREDENTIALS_LENGTH (32U)
#define XPLR_AT_PARSER_PORT_LENGTH (5U+1U)

#define XPLR_AT_PARSER_BOOL_OPTION_LENGTH (2U)
#define XPLR_AT_PARSER_USER_OPTION_LENGTH (8U)

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

typedef enum {
    XPLR_ATPARSER_ALL,          /**< All */
    XPLR_ATPARSER_NET,          /**< WiFi initialization and query commands */
    XPLR_ATPARSER_THINGSTREAM,  /**< Thingstream key/certificate/plan/region commands */
    XPLR_ATPARSER_NTRIP,        /**< NTRIP related commands/queries */
    XPLR_ATPARSER_MISC,         /**< Miscellaneous AT commands*/
} xplr_at_parser_type_t;

typedef enum {
    XPLR_ATPARSER_NET_INTERFACE_INVALID = -1,         /**< Invalid interface*/
    XPLR_ATPARSER_NET_INTERFACE_NOT_SET = 0,          /**< Not set*/
    XPLR_ATPARSER_NET_INTERFACE_WIFI,                 /**< Wifi */
    XPLR_ATPARSER_NET_INTERFACE_CELL,                 /**< Cellular */
} xplr_at_parser_net_interface_type_t;

typedef enum {
    XPLR_ATPARSER_CORRECTION_MOD_INVALID = -1,        /**< Invalid*/
    XPLR_ATPARSER_CORRECTION_MOD_IP = 0,              /**< Correction source from ip */
    XPLR_ATPARSER_CORRECTION_MOD_LBAND                /**< Correction source from LBAND*/
} xplr_at_parser_correction_mod_type_t;

typedef enum {
    XPLR_ATPARSER_CORRECTION_SOURCE_INVALID = -1,     /**< Invalid */
    XPLR_ATPARSER_CORRECTION_SOURCE_NOT_SET = 0,      /**< Not set*/
    XPLR_ATPARSER_CORRECTION_SOURCE_THINGSTREAM,      /**< Thingstream */
    XPLR_ATPARSER_CORRECTION_SOURCE_NTRIP,            /**< Ntrip */
} xplr_at_parser_correction_source_type_t;

typedef enum {
    XPLR_ATPARSER_MODE_INVALID = -2,     /**< Invalid */
    XPLR_ATPARSER_MODE_ERROR = -1,       /**< Error */
    XPLR_ATPARSER_MODE_NOT_SET = 0,      /**< Not Set*/
    XPLR_ATPARSER_MODE_CONFIG,           /**< Configuration */
    XPLR_ATPARSER_MODE_START,            /**< Start */
    XPLR_ATPARSER_MODE_STOP,             /**< Stop */
} xplr_at_parser_device_mode_t;

typedef enum {
    XPLR_ATPARSER_STATUS_ERROR = -1,
    XPLR_ATPARSER_STATUS_NOT_SET = 0,
    XPLR_ATPARSER_STATUS_READY,
    XPLR_ATPARSER_STATUS_INIT,
    XPLR_ATPARSER_STATUS_CONNECTING,
    XPLR_ATPARSER_STATUS_CONNECTED,
    XPLR_ATPARSER_STATUS_RECONNECTING,
} xplr_at_parser_status_type_t;

typedef enum {
    XPLR_ATPARSER_SUBSYSTEM_ALL,
    XPLR_ATPARSER_SUBSYSTEM_WIFI,
    XPLR_ATPARSER_SUBSYSTEM_CELL,
    XPLR_ATPARSER_SUBSYSTEM_TS,
    XPLR_ATPARSER_SUBSYSTEM_NTRIP,
    XPLR_ATPARSER_SUBSYSTEM_GNSS,
} xplr_at_parser_subsystem_type_t;

typedef enum {
    XPLR_ATPARSER_INTERNAL_FAULT_ALL,
    XPLR_ATPARSER_INTERNAL_FAULT_UART,
    XPLR_ATPARSER_INTERNAL_FAULT_CALLBACK,
    XPLR_ATPARSER_INTERNAL_FAULT_SEMAPHORE,
} xplr_at_parser_internal_driver_fault_type_t;

typedef enum {
    XPLR_ATPARSER_STATHPG_ERROR,
    XPLR_ATPARSER_STATHPG_INIT,
    XPLR_ATPARSER_STATHPG_CONFIG,
    XPLR_ATPARSER_STATHPG_WIFI_INIT,
    XPLR_ATPARSER_STATHPG_CELL_INIT,
    XPLR_ATPARSER_STATHPG_WIFI_CONNECTED,
    XPLR_ATPARSER_STATHPG_CELL_CONNECTED,
    XPLR_ATPARSER_STATHPG_TS_CONNECTED,
    XPLR_ATPARSER_STATHPG_NTRIP_CONNECTED,
    XPLR_ATPARSER_STATHPG_WIFI_ERROR,
    XPLR_ATPARSER_STATHPG_CELL_ERROR,
    XPLR_ATPARSER_STATHPG_TS_ERROR,
    XPLR_ATPARSER_STATHPG_NTRIP_ERROR,
    XPLR_ATPARSER_STATHPG_RECONNECTING,
    XPLR_ATPARSER_STATHPG_STOP,
} xplr_at_parser_hpg_status_type_t;

typedef enum {
    XPLR_AT_PARSER_ERROR = -1,    /**< process returned with errors. */
    XPLR_AT_PARSER_OK,            /**< indicates success of returning process. */
} xplr_at_parser_error_t;

/**
 * At Parser Faults
 */
typedef union __attribute__((__packed__))xplr_at_parser_subsystem_faults_type {
    struct {
        uint8_t wifi            : 1;
        uint8_t cell            : 1;
        uint8_t thingstream     : 1;
        uint8_t ntrip           : 1;
        uint8_t gnss            : 1;
        uint8_t reserved        : 3;
    } fault;                            /**< Faults as single bit-fields. */
    uint8_t value;                      /**< Faults as a single uint8_t. */
} xplr_at_parser_subsystem_faults_t;

typedef union __attribute__((__packed__))xplr_at_parser_internal_driver_faults_type {
    struct {
        uint8_t uart            : 1;    /**< Read/Write fault in UART*/
        uint8_t callback        : 1;    /**< Fault setting callback from a handler*/
        uint8_t semaphore       : 1;    /**< Fault when giving a semaphore*/
        uint8_t reserved        : 5;
    } fault;                            /**< Faults as single bit-fields. */
    uint8_t value;                      /**< Faults as a single uint8_t. */
} xplr_at_parser_internal_driver_faults_t;

typedef struct xplr_at_parser_thingstream_config_type {
    xplr_thingstream_t            thingstream;
    xplr_thingstream_pp_region_t  tsRegion;                                /**< Thingstream Region */
    xplr_thingstream_pp_plan_t    tsPlan;                                  /**< Thingstream Plan */
} xplr_at_parser_thingstream_config_t;

typedef struct xplr_at_parser_misc_options_type {
    xplrGnssDeadReckoningCfg_t          dr;            /**< Dead Reckoning configuration*/
    bool                                sdLogEnable;   /**< SD Logging enabled/disabled*/
} xplr_at_parser_misc_options_t;

typedef struct xplr_at_parser_correction_data_config_type {
    xplr_at_parser_thingstream_config_t     thingstreamCfg;
    xplr_at_parser_correction_source_type_t correctionSource; /**< Correction source Thingstream/NTRIP*/
    xplr_at_parser_correction_mod_type_t    correctionMod;    /**< Correction mode LBAND/IP*/
    xplr_ntrip_config_t                     ntripConfig;      /**< NTRIP configuration data*/
} xplr_at_parser_correction_data_config_t;

typedef struct xplr_at_parser_cell_info_type {
    char cellModel[64];
    char cellFw[64];
    char cellImei[64];
} xplr_at_parser_cell_info_t;

typedef struct xplr_at_parser_status_type {
    xplr_at_parser_status_type_t         wifi;
    xplr_at_parser_status_type_t         cell;
    xplr_at_parser_status_type_t         ts;
    xplr_at_parser_status_type_t         ntrip;
    xplr_at_parser_status_type_t         gnss;
} xplr_at_parser_status_t;

typedef struct  xplr_at_parser_net_interface_config_type {
    char    ssid[XPLR_AT_PARSER_SSID_LENGTH];                  /**< ssid of router to connect to */
    char    password[XPLR_AT_PARSER_PASSWORD_LENGTH];          /**< password of router to connect to */
    char    apn[XPLR_AT_PARSER_APN_LENGTH];                    /**< cellular Access Point Name */
    xplr_at_parser_net_interface_type_t      interface;        /**< Selected interface wifi/cell */
} xplr_at_parser_net_interface_config_t;

// *INDENT-OFF*

typedef struct xplr_at_parser_data_type {
    xplrNvs_t                                nvs;           /**< nvs module to handle operations */
    const char                               *id;           /**< nvs namespace */
    xplr_at_parser_net_interface_config_t    net;
    xplr_at_parser_correction_data_config_t  correctionData;
    xplr_at_parser_misc_options_t            misc;
    xplr_at_parser_device_mode_t             mode;          /**< Device mode*/
    xplr_at_parser_status_t                  status;        /**< Struct containing status of each subsystem wifi/cell/ts/ntrip*/
    xplrGnssLocation_t                       location;      /**< Struct containing current location, must be updated from application code*/
    xplrLocDvcInfo_t                         dvcInfoGnss;
    xplrLocDvcInfo_t                         dvcInfoLband;
    xplr_at_parser_cell_info_t               cellInfo;
    bool                                     restartSignal;
    bool                                     startOnBoot;   /**< If true attempt to connect with NVS saved configuration without waiting for user AT+HPGMODE*/
} xplr_at_parser_data_t;

// *INDENT-ON*

typedef struct xplr_at_parser_type {
    xplr_at_server_t                         server;
    xplr_at_parser_data_t                    data;
    xplr_at_parser_subsystem_faults_t        faults;
    xplr_at_parser_internal_driver_faults_t  internalFaults;
} xplr_at_parser_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize the AT Parser module
 *
 * @param uartCfg      struct storing configuration for the UART interface
 *                     used
 * @return             pointer to the initialized parser instance or NULL in case of initialization error
 */
xplr_at_parser_t *xplrAtParserInit(xplr_at_server_uartCfg_t *uartCfg);

/**
 * @brief Deinitialize the AT Parser module
 *
 * @return              XPLR_AT_PARSER_OK on success,
 *                      XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserDeInit(void);

/**
 * @brief Add a parser for handling a specific group of AT commands
 *
 * @param parserType    Group of AT commands to be added
 * @return              XPLR_AT_PARSER_OK on success,
 *                      XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserAdd(xplr_at_parser_type_t parserType);

/**
 * @brief Remove a parser for handling a specific group of AT commands
 *
 * @param parserType    Group of AT commands to be removed
 */
void xplrAtParserRemove(xplr_at_parser_type_t parserType);

/**
 * @brief Load Thingstream certificates from NVS and store them to their alocated
 * structs in parser->data
 *
 * @return              XPLR_AT_PARSER_OK on success,
 *                      XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserLoadNvsTsCerts(void);

/**
 * @brief Load configuration from NVS and store it to the alocated
 * struct in parser->data
 *
 * @return              XPLR_AT_PARSER_OK on success,
 *                      XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserLoadNvsConfig(void);

/**
 * @brief Check readiness of WiFi subsystem/configuration
 *
 * @return              True on ready, False when not ready
 */
bool xplrAtParserWifiIsReady(void);

/**
 * @brief Check readiness of Cellular subsystem/configuration
 *
 * @return              True on ready, False when not ready
 */
bool xplrAtParserCellIsReady(void);

/**
 * @brief Check readiness of Thingstream subsystem/configuration
 *
 * @return              True on ready, False when not ready
 */
bool xplrAtParserTsIsReady(void);

/**
 * @brief Check readiness of Ntrip subsystem/configuration
 *
 * @return              True on ready, False when not ready
 */
bool xplrAtParserNtripIsReady(void);

/**
 * @brief Copy configuration from application to the library instance and save
 * it to NVS memory
 *
 * @param ntripConfig           pointer to the user configuration
 * @return                      XPLR_AT_PARSER_OK on success,
 *                              XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserSetNtripConfig(xplr_ntrip_config_t *ntripConfig);

/**
 * @brief Copy configuration from application to the library instance and save
 * it to NVS memory
 *
 * @param netInterfaceConfig    pointer to the user configuration
 * @return                      XPLR_AT_PARSER_OK on success,
 *                              XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserSetNetInterfaceConfig(xplr_at_parser_net_interface_type_t
                                                         *netInterfaceConfig);

/**
 * @brief Copy configuration from application to the library instance and save it
 * to NVS memory
 *
 * @param thingstreamConfig     pointer to the user configuration
 * @return                      XPLR_AT_PARSER_OK on success,
 *                              XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserSetThingstreamConfig(xplr_at_parser_thingstream_config_t
                                                        *thingstreamConfig);

/**
 * @brief Return a xplr_at_parser_hpg_status_type_t status updatemessage from the
 * AT interface
 *
 * @param statusMessage     Updated status message
 * @param uint8_t           periodSecs
 * @return                  XPLR_AT_PARSER_OK on success,
 *                          XPLR_AT_PARSER_ERROR on failure.
 */
xplr_at_parser_error_t xplrAtParserStatusUpdate(xplr_at_parser_hpg_status_type_t statusMessage,
                                                uint8_t periodSecs);
/**
 * @brief Set the status of a subsystem wifi/cell/thingstream/ntrip/gnss
 * @param subsystem subsystem to update status
 * @param newStatus updated status
 *
*/
void xplrAtParserSetSubsystemStatus(xplr_at_parser_subsystem_type_t subsystem,
                                    xplr_at_parser_status_type_t newStatus);

/**
 * @brief Reset subsystem fault set by At Parser
 *
 * @param type              Fault to reset
 */
xplr_at_parser_error_t xplrAtParserFaultReset(xplr_at_parser_subsystem_type_t type);

/**
 * @brief Reset internal driver fault set by At Parser
 *
 * @param type              Fault to reset
 */
xplr_at_parser_error_t xplrAtParserInternalDriverFaultReset(
    xplr_at_parser_internal_driver_fault_type_t type);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrAtParserInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops logging of the module.
 *
 * @return  ESP_OK on success, ESP_FAIL otherwise.
*/
esp_err_t xplrAtParserStopLogModule(void);

/**
 * @brief   Set device mode  busy status in order to avoid altering configuration data
 *          while running
*/
xplr_at_parser_error_t setDeviceModeBusyStatus(bool isBusy);

#ifdef __cplusplus
}
#endif
#endif //_AT_PARSER_H

// End of file