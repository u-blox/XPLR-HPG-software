/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _XPLR_WIFI_STARTER_H_
#define _XPLR_WIFI_STARTER_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../hpglib/xplr_hpglib_cfg.h"
#include "./../../hpglib/src/nvs_service/xplr_nvs.h"
#include <stdint.h>
#include "esp_wifi.h"
#include "./../../hpglib/src/log_service/xplr_log.h"

/** @file
 * @brief This header file defines the wifi client API,
 * includes functions to setup a WiFi client and establish a
 * connection to the desired AP.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* Max length of SSID name */
#define XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX        (32U+1U)

/* Max length of SSID password */
#define XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX    (64U+1U)

/* Max number of SSIDs to store during scan */
#define XPLR_WIFISTARTER_SSID_SCAN_MAX              (10U)

#define XPLR_WIFISTARTER_AP_PWD         ""
#define XPLR_WIFISTARTER_AP_CLIENTS_MAX 1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to wifiStarter module. */
typedef enum {
    XPLR_WIFISTARTER_ERROR = -1,  /**< process returned with errors. */
    XPLR_WIFISTARTER_OK,          /**< indicates success of returning process. */
} xplrWifiStarterError_t;

/** Operation mode of wifiStarter module. */
typedef enum {
    XPLR_WIFISTARTER_MODE_INVALID = -1,  /**< selected mode not supported. */
    XPLR_WIFISTARTER_MODE_STA,           /**< wi-fi in station mode. */
    XPLR_WIFISTARTER_MODE_AP,            /**< wi-fi in access-point mode. */
    XPLR_WIFISTARTER_MODE_STA_AP,        /**< wi-fi in access-point and AP mode.
                                              AP mode is enabled by default when
                                              connection to configured router is not
                                              established. */
} xplrWifiStarterMode_t;

/**
 * FSM Sates for wifi starter
 * For internal use only. More detailed states
 */
typedef enum {
    XPLR_WIFISTARTER_STATE_UNKNOWN = -3,          /**< unknown state. */
    XPLR_WIFISTARTER_STATE_TIMEOUT,               /**< timeout state, command/connection failed to change state. */
    XPLR_WIFISTARTER_STATE_ERROR,                 /**< error state. */
    XPLR_WIFISTARTER_STATE_CONNECT_OK = 0,        /**< Wi-Fi is connected. */
    XPLR_WIFISTARTER_STATE_CONFIG_WIFI,           /**< configuring Wi-Fi connection. */
    XPLR_WIFISTARTER_STATE_FLASH_INIT,            /**< initializing flash. */
    XPLR_WIFISTARTER_STATE_FLASH_ERASE,           /**< erase flash. */
    XPLR_WIFISTARTER_STATE_FLASH_CHECK_CFG,       /**< check flash for stored credentials. */
    XPLR_WIFISTARTER_STATE_NETIF_INIT,            /**< netif initialize. */
    XPLR_WIFISTARTER_STATE_EVENT_LOOP_INIT,       /**< create/start Wi-Fi event loop. */
    XPLR_WIFISTARTER_STATE_CREATE_STATION,        /**< create/init station mode. */
    XPLR_WIFISTARTER_STATE_CREATE_AP,             /**< create/init ap mode. */
    XPLR_WIFISTARTER_STATE_CREATE_STATION_AND_AP, /**< create/init station and ap mode. */
    XPLR_WIFISTARTER_STATE_WIFI_INIT,             /**< initialize Wi-Fi. */
    XPLR_WIFISTARTER_STATE_REGISTER_HANDLERS,     /**< register handlers. */
    XPLR_WIFISTARTER_STATE_WIFI_SET_MODE,         /**< sets Wi-Fi mode. */
    XPLR_WIFISTARTER_STATE_WIFI_SET_CONFIG,       /**< sets configuration. */
    XPLR_WIFISTARTER_STATE_WIFI_START,            /**< starts Wi-Fi in selected mode. */
    XPLR_WIFISTARTER_STATE_WIFI_WAIT_CONFIG,      /**< waits for credentials config. */
    XPLR_WIFISTARTER_STATE_CONNECT_WIFI,          /**< connect to Access Point. */
    XPLR_WIFISTARTER_STATE_CONNECT_WAIT,          /**< wait for conenction to AP. */
    XPLR_WIFISTARTER_STATE_SCHEDULE_RECONNECT,    /**< schedule reconnect at period. */
    XPLR_WIFISTARTER_STATE_STOP_WIFI,             /**< stops/disconnects Wi-Fi. */
    XPLR_WIFISTARTER_STATE_DISCONNECT_OK          /**< successfully disconnected. */
} xplrWifiStarterFsmStates_t;

/** Webserver data types. */
typedef enum {
    XPLR_WIFISTARTER_SERVERDATA_UNKNOWN = -1,   /**< unknown data type in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_SSID,           /**< ssid data available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_PASSWORD,       /**< password data available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_CLIENTID,       /**< PointPerfect client id available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_CLIENTCERT,     /**< PointPerfect client certificate available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_CLIENTKEY,      /**< PointPerfect client key available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_CLIENTREGION,   /**< PointPerfect client region available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_CLIENTPLAN,     /**< PointPerfect client plan available in webserver. */
    XPLR_WIFISTARTER_SERVERDATA_ROOTCA,         /**< PointPerfect root ca available in webserver. */
    XPLR_WIFISTARTER_SERVERDIAG_CONNECTED,      /**< webserver diagnostics connected status. */
    XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED,     /**< webserver diagnostics configuration status. */
    XPLR_WIFISTARTER_SERVERDIAG_READY,          /**< webserver diagnostics ready status. */
    XPLR_WIFISTARTER_SERVERDIAG_GNSS_ACCURACY,  /**< webserver diagnostics gnss accuracy. */
    XPLR_WIFISTARTER_SERVERDIAG_UPTIME,         /**< webserver diagnostics total uptime from boot. */
    XPLR_WIFISTARTER_SERVERDIAG_FIXTIME,        /**< webserver diagnostics total time for the device to get a FIX. */
    XPLR_WIFISTARTER_SERVERDIAG_MQTTSTATS,      /**< webserver diagnostics MQTT traffic statistics. */
    XPLR_WIFISTARTER_SERVERDIAG_SDSTATS,        /**< webserver diagnostics SD info. */
    XPLR_WIFISTARTER_SERVERDIAG_DRINFO,         /**< webserver diagnostics GNSS DR info. */
    XPLR_WIFISTARTER_SERVERDIAG_DR_CALIB_INFO,  /**< webserver diagnostics GNSS DR calibration info. */
    XPLR_WIFISTARTER_SERVERDIAG_FWVERSION,      /**< webserver diagnostics firmware version. */
    XPLR_WIFISTARTER_SERVEOPTS_SD,              /**< webserver option SD enable/disable. */
    XPLR_WIFISTARTER_SERVEOPTS_DR,              /**< webserver option GNSS DeadReckon enable/disable. */
    XPLR_WIFISTARTER_SERVEOPTS_DR_CALIBRATION,  /**< webserver option GNSS DeadReckon calibration enable/disable. */
} xplrWifiStarterServerData_t;

// *INDENT-OFF*
/**
 * Wi-Fi NVS struct.
 * contains data to be stored in NVS under namespace <id>
*/
typedef struct xplrWifiStarterNvs_type {
    xplrNvs_t   nvs;                                                  /**< nvs module to handle operations */
    char        id[15];                                               /**< nvs namespace */
    char        ssid[XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX];           /**< ssid of router to connect to */
    char        password[XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX];   /**< password of router to connect to */
    char        *rootCa;                                              /**< Root CA certificate for communicating with Thingstream */
    char        *ppClientId;                                          /**< Thingstream's PointPerfect client ID */
    char        *ppClientCert;                                        /**< Thingstream's PointPerfect client Certificate */
    char        *ppClientKey;                                         /**< Thingstream's PointPerfect client key */
    char        *ppClientRegion;                                      /**< Thingstream's PointPerfect region */
    char        *ppClientPlan;                                        /**< Thingstream's PointPerfect plan */
    bool        set;                                                  /**< Device configuration status */
    bool        ppSet;                                                /**< PointPerfect configuration status */
    bool        sdLog;                                                /**< SD log option set */
    bool        gnssDR;                                                /**< GNSS Dead Reckoning option set */
} xplrWifiStarterNvs_t;
// *INDENT-OΝ*

/**
 * Wi-Fi operation parameters.
 * Contains required parameters to configure the Wi-Fi module.
 */
typedef struct xplrWifiStarterOpts_type {
    const char *ssid;                /**< SSID name of AP to connect to. */
    const char *password;            /**< Password for AP. */
    xplrWifiStarterMode_t mode;      /**< mode of operation. */
    xplrWifiStarterNvs_t storage;    /**< memory module to store/retrieve wi-fi info. */
    bool webserver;                  /**< activate webserver. */
} xplrWifiStarterOpts_t;

// *INDENT-OFF*
/**
 * Wi-Fi SSID scan list.
 * Contains information list of scanned SSIDs.
 */
typedef struct xplrWifiStarterScanList_type {
    char        name[XPLR_WIFISTARTER_SSID_SCAN_MAX][XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX]; /**< List of SSID names found during scan. */
    uint16_t    found;                                                                      /**< Total number of discovered SSIDs. */
    int8_t      rssi[XPLR_WIFISTARTER_SSID_SCAN_MAX];                                       /**< RSSI list of discovered SSIDs. */
} xplrWifiStarterScanList_t;
// *INDENT-OΝ*

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Executes the state machine.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterFsm();

/**
 * @brief Initializes a WiFi Connection with options WiFi service
 * and connection will keep running in the background.
 *
 * @param wifiOptions  options to initialize Wi-Fi with.
 * @return             zero on success, negative error code on failure or
 *                     positive values on states.
 */
esp_err_t xplrWifiStarterInitConnection(xplrWifiStarterOpts_t *wifiOptions);

/**
 * @brief Disconnects and stops WiFi service.
 *
 * @return  zero on success or negative error
 *          code on failure.
 */
esp_err_t xplrWifiStarterDisconnect(void);

/**
 * @brief Returns current WiFI FSM state.
 *
 * @return  zero on success, negative on error
 *          or positive codes on states
 */
xplrWifiStarterFsmStates_t xplrWifiStarterGetCurrentFsmState(void);

/**
 * @brief Returns previous WiFI FSM state.
 *
 * @return  zero on success, negative on error
 *          or positive codes on states
 */
xplrWifiStarterFsmStates_t xplrWifiStarterGetPreviousFsmState(void);

/**
 * @brief Scan for available SSIDs.
 *
 * @param scanInfo scan list of type xplrWifiStarterScanList_t to store scan results.
 *
 * @return  zero on success or negative error
 *          code on failure.
 */
esp_err_t xplrWifiStarterScanNetwork(xplrWifiStarterScanList_t *scanInfo);

/**
 * @brief Triggers software reset.
 *
 */
void xplrWifiStarterDeviceReboot(void);

/**
 * @brief Delete device config from NVS, Wi-Fi and Thingstream credentials.
 * Thingstream credentials will be erased only if webserver is enabled in xplrWifiStarterOpts_t.
 * After completion device will reboot in AP mode (if webserver is enabled).
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceErase(void);

/**
 * @brief Delete Wi-Fi config from NVS.
 * After completion device will reboot in AP mode (if webserver is enabled).
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceEraseWifi(void);

/**
 * @brief Delete Thingstream config from NVS.
 * After completion device will reboot in AP mode.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceEraseThingstream(void);

/**
 * @brief Force save Wi-Fi config to NVS.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceForceSaveWifi(void);

/**
 * @brief Force Save Thingstream config to NVS.
 *
 * @param opt credential to save: 0 = all,
 *                                1 = rootCa,
 *                                2 = id,
 *                                3 = certificate,
 *                                4 = key,
 *                                5 = region.
 *                                6 = plan.
 *                                7 = config flag.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceForceSaveThingstream(uint8_t opts);

/**
 * @brief Force Save Device Misc options to NVS.
 *
 * @param opt credential to save: 0 = all,
 *                                1 = sd log,
 *                                2 = gnss DR,
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterDeviceForceSaveMiscOptions(uint8_t opts);

/**
 * @brief Retrieve webserver data.
 *
 * @param opt type of data to retrieve.
 *
 * @return  pointer to requested data or null if not available or not supported.
 *          Currently diagnostics data can only be set!
 */
char *xplrWifiStarterWebserverDataGet(xplrWifiStarterServerData_t opt);

/**
 * @brief Set webserver diagnostics.
 *
 * @param opt type of data to modify.
 * @param value value to assign.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverDiagnosticsSet(xplrWifiStarterServerData_t opt, void *value);

/**
 * @brief Get webserver diagnostics.
 *
 * @param opt type of data to retrieve.
 * @param value value to store retrieved.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverDiagnosticsGet(xplrWifiStarterServerData_t opt, void *value);

/**
 * @brief Set webserver options.
 *
 * @param opt type of data to modify.
 * @param value value to assign.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverOptionsSet(xplrWifiStarterServerData_t opt, void *value);

/**
 * @brief Get webserver options.
 *
 * @param opt type of data to retrieve.
 * @param value value to store retrieved data.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverOptionsGet(xplrWifiStarterServerData_t opt, void *value);

/**
 * @brief Set webserver location info.
 *
 * @param location json formatted location data to update webserver with.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverLocationSet(char *location);

/**
 * @brief Send message to webserver to display it as alert.
 *
 * @param message json formatted message information to be displayed.
 *
 * @return  XPLR_WIFISTARTER_OK on success or XPLR_WIFISTARTER_ERROR on failure.
 */
xplrWifiStarterError_t xplrWifiStarterWebserverMessageSet(char *message);

/**
 * @brief Check if webserver credentials are set.
 *
 * @return  true if set, false otherwise.
 */
bool xplrWifiStarterWebserverisConfigured(void);

/**
 * @brief Retrieve STA IP.
 *
 * @return  pointer to STA IP address or null (if in AP mode).
 */
char* xplrWifiStarterGetStaIp(void);

/**
 * @brief Retrieve AP IP.
 *
 * @return  pointer to AP IP address or null (if in STA mode).
 */
char* xplrWifiStarterGetApIp(void);

/**
 * @brief Retrieve AP SSID.
 *
 * @return  pointer to AP SSID address or null (if in STA mode).
 */
char* xplrWifiStarterGetApSsid(void);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrWifiStarterInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops the logging of the http cell module
 * 
 * @return  ESP_OK on success, ESP_FAIL otherwise.
*/
esp_err_t xplrWifiStarterStopLogModule(void);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_WIFI_STARTER_H_ */

// End of file