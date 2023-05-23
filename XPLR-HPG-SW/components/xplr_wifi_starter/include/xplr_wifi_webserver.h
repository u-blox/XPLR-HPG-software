/*
 * Copyright 2022 u-blox Ltd
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

#ifndef _XPLR_WIFI_WEBSERVER_H_
#define _XPLR_WIFI_WEBSERVER_H_

#include "xplr_wifi_starter.h"
#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* Max length of SSID name */
#define XPLR_WIFIWEBSERVER_SOCKETS_OPEN_MAX                  (4U)

/* Max size of a certificate file */
#define XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE         (2U*2U*1024+128)

/* Point Perfect size of a client id */
#define XPLR_WIFIWEBSERVER_PPID_SIZE                         (36U+1U)

/* Point Perfect size of a region */
#define XPLR_WIFIWEBSERVER_PPREGION_SIZE                     (3U+1U)

/* Max URIs that server can handle */
#define WEBSERVER_URIS_MAX                                   (25U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Wi-Fi credentials for the device to connect to a router.
 *  Configured under the "settings" tab.
*/
typedef struct xplrWifiWebServerDataWifiCredentials_type {
    char ssid[XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX];            /**< SSID name of router to connect to. */
    char password[XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX];    /**< Password for router. */
    bool set;
} xplrWifiWebServerDataWifiCredentials_t;

/** Point perfect credentials for the device to connect to Thingstream's location service.
 *  Configured under the "settings" tab.
*/
typedef struct xplrWifiWebServerDataPpCredentials_type {
    char clientId[XPLR_WIFIWEBSERVER_PPID_SIZE];                          /**< point perfect client id. */
    char rootCa[XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE];            /**< point perfect root ca certificate. */
    char certificate[XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE];       /**< point perfect client certificate. */
    char privateKey[XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE];        /**< point perfect client private key. */
    char region[XPLR_WIFIWEBSERVER_PPREGION_SIZE];                        /**< point perfect region to parse correction data for.
                                                                             Supported values: EU and US */
    bool set;
} xplrWifiWebServerDataPpCredentials_t;

/** Location data for the device to illustrate its position in the "Live tracker" tab. */
typedef struct xplrWifiWebServerDataLocation_type {
    int32_t latitude;            /**< latitude in ten millionths of a degree. */
    int32_t longitude;           /**< longitude in ten millionths of a degree. */
    int64_t timeUtc;             /**< the UTC time at which the location fix was made. */
} xplrWifiWebServerDataLocation_t;

/** Device diagnostics data, used in "home" tab. */
typedef struct xplrWifiWebServerDataDiagnostics_type {
    int8_t  configured;             /**< tristate thingstream status. -1 not configured, 0 set, 1 connected. */
    int8_t  connected;              /**< tristate wifi status. -1 not configured, 0 offline, 1 online. */
    int8_t  ready;                  /**< gnss status. -1,0 no signal, 1 3D fix, 2 DGNSS, 4 RTK-Float, 5 RTK-Fixed, 6 Dead-Reckon */
    char    *ip;                    /**< IP acquired. */
    char    *ssid;                  /**< SSID of AP to connect to. null if in AP mode. */
    char    *hostname;              /**< hostname of module when connected in STA mode. */
    uint32_t gnssAccuracy;          /**< GNSS module current horizontal accuracy. */
    char    *mqttTraffic;           /**< total MQTT traffic. Num of msgs received and total bytes */
    char     *upTime;               /**< time since the module is online aka connected to wifi and thingstream. */
    char     *timeToFix;            /**< time it took for the module to get FIX. */
    char     *version;              /**< firmware version. */
} xplrWifiWebServerDataDiagnostics_t;

/** Webserver data. */
typedef struct xplrWifiWebServerData_type {
    xplrWifiWebServerDataWifiCredentials_t   wifi;
    xplrWifiWebServerDataPpCredentials_t     pointPerfect;
    xplrWifiWebServerDataLocation_t          location;
    xplrWifiWebServerDataDiagnostics_t       diagnostics;
    xplrWifiStarterScanList_t                wifiScan;
} xplrWifiWebServerData_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Starts a simple web and file server that supports websockets.
 *        webserver is used for hosting xplrHpg captive portal and live map tracking.
 *
 * @param data    xplrWifiWebServerData_t type used for updating webserver's pages.
 * @return  httpd server instance.
 */
httpd_handle_t  xplrWifiWebserverStart(xplrWifiWebServerData_t *data);

/**
 * @brief Stops webserver.

 * @param server       httpd server instance to stop.
 * @return             ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t xplrWifiWebserverStop(void);

/**
 * @brief Send a json formatted string containing location info to server.

 * @param jMsg         json formatted string.
 * @return             ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t xplrWifiWebserverSendLocation(char *jMsg);

/**
 * @brief Send a json formatted string to server.

 * @param jMsg         json formatted string.
 * @return             ESP_OK on success, ESP_FAIL on error.
 */
esp_err_t xplrWifiWebserverSendMessage(char *message);

#ifdef __cplusplus
}
#endif

#endif /* #define _XPLR_WIFI_WEBSERVER_H_ */
