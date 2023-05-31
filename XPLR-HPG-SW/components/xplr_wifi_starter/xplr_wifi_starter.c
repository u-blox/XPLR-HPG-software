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

#include "xplr_wifi_starter.h"
#include "xplr_wifi_webserver.h"
#if (XPLR_CFG_ENABLE_WEBSERVERDNS == 1)
#include "xplr_wifi_dns.h"
#endif
#include <string.h>
#include <freertos/event_groups.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/timer.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRWIFISTARTER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRWIFISTARTER_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrWifiStarter", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRWIFISTARTER_CONSOLE(message, ...) do{} while(0)
#endif

/**
 * Max length of nvs tag name
 */
#define XPLR_WIFI_NVS_TAG_LENGTH_MAX          16

/**
 * Max length of nvs namespace
 */
#define XPLR_WIFI_NVS_NAMESPACE_LENGTH_MAX    16

/**
 * Max retries after a waiting period of RETRY_PERIOD_SECS
 */
#define MAX_RETRIES   10

/**
 * Retry period to wait before retrying to connect MAX_RETRIES times
 */
#define RETRY_PERIOD_SECS 10

/**
 * User notification period for displaying STA info when connected.
 * Available only if webserver is enabled.
 */
#define XPLR_WIFI_SERVERINFO_PERIOD_SECS 10

/**
 * Timeout period after nothing has changed
 */
#define TIMEOUT_WAIT_SECS 300

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* Wi-Fi related variables */

const char *nvsNamespace = "xplrWifiCfg";

static xplrWifiStarterFsmStates_t wifiFsm[2] = {
    XPLR_WIFISTARTER_STATE_DISCONNECT_OK,
    XPLR_WIFISTARTER_STATE_DISCONNECT_OK
};
static esp_event_handler_instance_t instanceAnyId;
static esp_event_handler_instance_t instanceGotIp;
static int sRetryNum = 0;
static uint64_t lastActionTime;
static uint64_t connectedStateTime;

static wifi_config_t wifiConfig;
static wifi_init_config_t cfg;

esp_netif_ip_info_t staIpInfo;
char staIpString[16] = {0};
char *staHostname;
bool diagnosticsInfoUpdated;

static xplrWifiStarterOpts_t userOptions;

static xplrWifiStarterError_t ret;

static bool cleanup = false;

/* access point related variables */

static bool configured = false;
unsigned char macAdr[6] = {0};
char apSsid[XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX];
esp_netif_ip_info_t apIpInfo;
char apIpString[16] = {0};

/* webserver related variables */

xplrWifiWebServerData_t webserverData;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
static esp_err_t wifiNvsInit(void);
static esp_err_t wifiNvsLoad(void);
static esp_err_t wifiNvsWriteDefaults(void);
static esp_err_t wifiNvsReadConfig(void);
static esp_err_t wifiNvsUpdate(uint8_t opt);
static esp_err_t wifiNvsErase(uint8_t opt);
static bool      wifiCredentialsConfigured(void);
static esp_err_t xplrWifiStarterPrivateRegisterHandlers(void);
static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data);
static esp_err_t xplrWifiStarterPrivateUnregisterHandlers(void);
static void xplrWifiStarterPrivateUpdateNextState(xplrWifiStarterFsmStates_t nextState);
static void xplrWifiStarterPrivateUpdateNextStateToError(void);
static void xplrWifiStarterPrivateInitCfg(void);
static esp_err_t xplrWifiStarterPrivateDisconnect(void);
static void wifiGetMac(void);
static void wifiGetIp(void);
static void wifiPrintMac(const unsigned char *mac);
static void wifiStaPrintInfo(void);
static void wifiApGetIp(void);
static void wifiApPrintInfo(void);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

xplrWifiStarterError_t xplrWifiStarterFsm()
{
    esp_err_t esp_ret;

    switch (wifiFsm[0]) {
        case XPLR_WIFISTARTER_STATE_CONFIG_WIFI:
            xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_FLASH_INIT);
            XPLRWIFISTARTER_CONSOLE(D, "Config WiFi OK.");
            break;

        case XPLR_WIFISTARTER_STATE_FLASH_INIT:
            esp_ret = wifiNvsInit();
            if (esp_ret == ESP_OK) {
                esp_ret = wifiNvsLoad();
                if (esp_ret == ESP_OK) {
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_NETIF_INIT);
                    XPLRWIFISTARTER_CONSOLE(D, "Init flash successful!");
                } else {
                    xplrWifiStarterPrivateUpdateNextStateToError();
                    XPLRWIFISTARTER_CONSOLE(E, "Init flash failed!");
                }
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Init flash failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_FLASH_ERASE:
            esp_ret = wifiNvsErase(0);
            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(D,
                                        "Flash erased successful, going to error state. Please restart the device!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Flash erase failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_NETIF_INIT:
            esp_ret = esp_netif_init();
            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_EVENT_LOOP_INIT);
                XPLRWIFISTARTER_CONSOLE(D, "Init netif successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Init netif failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_EVENT_LOOP_INIT:
            esp_ret = esp_event_loop_create_default();
            if (esp_ret == ESP_OK) {
                if (userOptions.mode == XPLR_WIFISTARTER_MODE_STA) {
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CREATE_STATION);
                    XPLRWIFISTARTER_CONSOLE(D, "Init event loop successful!");
                } else if (userOptions.mode == XPLR_WIFISTARTER_MODE_AP) {
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CREATE_AP);
                    XPLRWIFISTARTER_CONSOLE(D, "Init event loop successful!");
                } else if (userOptions.mode == XPLR_WIFISTARTER_MODE_STA_AP) {
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CREATE_STATION_AND_AP);
                    XPLRWIFISTARTER_CONSOLE(D, "Init event loop successful!");
                } else {
                    xplrWifiStarterPrivateUpdateNextStateToError();
                    XPLRWIFISTARTER_CONSOLE(E, "Init event loop failed");
                }
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Init event loop failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_CREATE_STATION:
            if (esp_netif_create_default_wifi_sta() != NULL) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_INIT);
                XPLRWIFISTARTER_CONSOLE(D, "Create station successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Create station failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_CREATE_AP:
            if (esp_netif_create_default_wifi_ap() != NULL) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_INIT);
                XPLRWIFISTARTER_CONSOLE(D, "Create access-point successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Create access-point failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_CREATE_STATION_AND_AP:
            if ((esp_netif_create_default_wifi_sta() != NULL) &&
                (esp_netif_create_default_wifi_ap() != NULL)) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_INIT);
                XPLRWIFISTARTER_CONSOLE(D, "Create station and ap successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Create station and ap failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_WIFI_INIT:
            esp_ret = esp_wifi_init(&cfg);
            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_REGISTER_HANDLERS);
                XPLRWIFISTARTER_CONSOLE(D, "WiFi init successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "WiFi init failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_REGISTER_HANDLERS:
            esp_ret = xplrWifiStarterPrivateRegisterHandlers();
            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_FLASH_CHECK_CFG);
                XPLRWIFISTARTER_CONSOLE(D, "Register handlers successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "Register handlers failed");
            }
            break;

        case XPLR_WIFISTARTER_STATE_FLASH_CHECK_CFG:
            configured = wifiCredentialsConfigured();
            if (!configured) {
                webserverData.diagnostics.configured = -1;
                webserverData.diagnostics.connected = -1;
                webserverData.diagnostics.ready = -1;
            } else {
                webserverData.diagnostics.configured = 0;
                webserverData.diagnostics.connected = 0;
                webserverData.diagnostics.ready = -1;
            }
            xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_SET_MODE);
            XPLRWIFISTARTER_CONSOLE(D, "Wifi credentials configured:%d ", (uint8_t)configured);
            break;

        case XPLR_WIFISTARTER_STATE_WIFI_SET_MODE:
            if (configured) {
                esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
                XPLRWIFISTARTER_CONSOLE(D, "WiFi mode selected: STATION");
            } else {
                esp_ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
                XPLRWIFISTARTER_CONSOLE(D, "WiFi mode selected: STATION and AP");
            }

            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_SET_CONFIG);
                XPLRWIFISTARTER_CONSOLE(D, "WiFi set mode successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "WiFi set mode failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_WIFI_SET_CONFIG:
            if (configured) {
                esp_ret = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
            } else {
                wifiGetMac();
                if((strstr(BOARD_NAME, "HPG2-C214") != NULL) || (strstr(BOARD_NAME, "MAZGCH-HPG-SOLUTION") != NULL)) {
                    snprintf(apSsid, 16, "%s-%x%x", "xplr-hpg-2", macAdr[4], macAdr[5]);
                } else if(strstr(BOARD_NAME, "HPG1-C213") != NULL) {
                    snprintf(apSsid, 16, "%s-%x%x", "xplr-hpg-1", macAdr[4], macAdr[5]);
                } else {
                    snprintf(apSsid, 16, "%s-%x%x", "xplr-hpg", macAdr[4], macAdr[5]);
                }
                /* configure ap settings */
                memset(wifiConfig.ap.ssid, 0x00, XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
                memcpy(wifiConfig.ap.ssid, apSsid, strlen(apSsid));
                memcpy(wifiConfig.ap.password, XPLR_WIFISTARTER_AP_PWD, strlen(XPLR_WIFISTARTER_AP_PWD));
                wifiConfig.ap.ssid_len = strlen(apSsid);
                wifiConfig.ap.max_connection = XPLR_WIFISTARTER_AP_CLIENTS_MAX;
                webserverData.diagnostics.ssid = apSsid;
                /* enable password authentication if a password is present */
                if ((strlen((const char *)wifiConfig.ap.password) >= 8) &&
                    memcmp((const char *)wifiConfig.ap.password, "password", sizeof("password") != 0)) {
                    XPLRWIFISTARTER_CONSOLE(D, "AP auth mode: WPA2-PSK ");
                    wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
                } else {
                    XPLRWIFISTARTER_CONSOLE(D, "AP auth mode: Open");
                    wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
                }
                esp_ret = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConfig);
            }

            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_START);
                XPLRWIFISTARTER_CONSOLE(D, "WiFi set config successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "WiFi set config failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_WIFI_START:
            esp_ret = esp_wifi_start();
            if (esp_ret == ESP_OK) {
                if (!configured) {
                    wifiApGetIp();
                    wifiApPrintInfo();
                    xplrWifiWebserverStart(&webserverData);
#if (XPLR_CFG_ENABLE_WEBSERVERDNS == 1)
                    xplrWifiDnsStart();
#endif
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_WIFI_WAIT_CONFIG);
                } else {
                    if (userOptions.webserver) {

                        xplrWifiWebserverStart(&webserverData);
#if (XPLR_CFG_ENABLE_WEBSERVERDNS == 1)
                        staHostname = xplrWifiStaDnsStart();
#endif
                    }
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CONNECT_WIFI);
                }
                XPLRWIFISTARTER_CONSOLE(D, "WiFi started successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "WiFi start failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_WIFI_WAIT_CONFIG:
            if ((webserverData.wifi.set) && (webserverData.pointPerfect.set)) {
                memset(userOptions.storage.ssid,
                       0x00,
                       XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
                memset(userOptions.storage.password,
                       0x00,
                       XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX);

                memcpy(userOptions.storage.ssid,
                       webserverData.wifi.ssid,
                       strlen(webserverData.wifi.ssid));
                memcpy(userOptions.storage.password,
                       webserverData.wifi.password,
                       strlen(webserverData.wifi.password));

                userOptions.storage.rootCa = webserverData.pointPerfect.rootCa;
                userOptions.storage.ppClientId = webserverData.pointPerfect.clientId;
                userOptions.storage.ppClientCert = webserverData.pointPerfect.certificate;
                userOptions.storage.ppClientKey = webserverData.pointPerfect.privateKey;
                userOptions.storage.ppClientRegion = webserverData.pointPerfect.region;
                userOptions.storage.set = true;

                esp_ret = wifiNvsUpdate(0); //update all

                if (esp_ret != ESP_OK) {
                    ret = XPLR_WIFISTARTER_STATE_ERROR;
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_ERROR);
                    XPLRWIFISTARTER_CONSOLE(E, "WiFi NVS Update failed, going to error state.");
                } else {
                    XPLRWIFISTARTER_CONSOLE(W, "NVS updated, restarting device...");
                    esp_restart();
                }
            } else {
                ret = XPLR_WIFISTARTER_OK;
            }
            break;

        case XPLR_WIFISTARTER_STATE_CONNECT_WIFI:
            esp_ret = esp_wifi_connect();
            if (esp_ret == ESP_OK) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CONNECT_WAIT);
                XPLRWIFISTARTER_CONSOLE(D, "Call esp_wifi_connect success!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                lastActionTime = MICROTOSEC(esp_timer_get_time());
                XPLRWIFISTARTER_CONSOLE(E, "Call esp_wifi_connect failed!");
            }
            break;

        case XPLR_WIFISTARTER_STATE_CONNECT_WAIT:
            if (MICROTOSEC(esp_timer_get_time()) - lastActionTime >= TIMEOUT_WAIT_SECS) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_TIMEOUT);
                XPLRWIFISTARTER_CONSOLE(E,
                                        "Connection timed out. Waited for %d secs!",
                                        TIMEOUT_WAIT_SECS);
            }
            break;

        case XPLR_WIFISTARTER_STATE_CONNECT_OK:
            if (userOptions.webserver) {
                if ((MICROTOSEC(esp_timer_get_time()) - connectedStateTime) >= XPLR_WIFI_SERVERINFO_PERIOD_SECS) {
                    wifiStaPrintInfo();
                    connectedStateTime = MICROTOSEC(esp_timer_get_time());
                }
                if (!diagnosticsInfoUpdated) {
                    webserverData.diagnostics.ssid = userOptions.storage.ssid;
                    webserverData.diagnostics.hostname = staHostname;
                    webserverData.diagnostics.ip = staIpString;
                    diagnosticsInfoUpdated = true;
                }
            }
            ret = XPLR_WIFISTARTER_OK;
            break;

        case XPLR_WIFISTARTER_STATE_SCHEDULE_RECONNECT:
            if ((MICROTOSEC(esp_timer_get_time()) - lastActionTime) >= RETRY_PERIOD_SECS) {
                XPLRWIFISTARTER_CONSOLE(D, "Trying to reconnect!");
                sRetryNum = 0;
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CONNECT_WIFI);
            }
            break;

        case XPLR_WIFISTARTER_STATE_STOP_WIFI:
            esp_ret = xplrWifiStarterPrivateDisconnect();
            if (esp_ret == ESP_OK) {
                if (wifiFsm[1] != XPLR_WIFISTARTER_STATE_TIMEOUT && wifiFsm[1] != XPLR_WIFISTARTER_STATE_ERROR) {
                    /**
                     * We are here because user requested to drop the connection
                     */
                    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_DISCONNECT_OK);
                } else {
                    /**
                     * ERROR state requested a cleanup
                     */
                    xplrWifiStarterPrivateUpdateNextStateToError();
                }
                XPLRWIFISTARTER_CONSOLE(D, "WiFi disconnected successful!");
            } else {
                xplrWifiStarterPrivateUpdateNextStateToError();
                XPLRWIFISTARTER_CONSOLE(E, "WiFi disconnect failed!");
            }

            break;

        case XPLR_WIFISTARTER_STATE_DISCONNECT_OK:

            break;

        case XPLR_WIFISTARTER_STATE_TIMEOUT:
        case XPLR_WIFISTARTER_STATE_ERROR:
            if (!cleanup) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_STOP_WIFI);
                cleanup = true;
            }
            break;

        default:
            /**
             * should never come here
             */
            ret = XPLR_WIFISTARTER_ERROR;
            if (wifiFsm[0] != XPLR_WIFISTARTER_STATE_UNKNOWN) {
                xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_UNKNOWN);
            }
    }

    return ret;
}

esp_err_t xplrWifiStarterInitConnection(xplrWifiStarterOpts_t *wifiOptions)
{
    wifiFsm[0] = XPLR_WIFISTARTER_STATE_UNKNOWN;

    xplrWifiStarterPrivateInitCfg();

    wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    /* get wifi user options from app */
    memcpy(&userOptions, wifiOptions, sizeof(xplrWifiStarterOpts_t));

    if (strlen(userOptions.ssid) > (ELEMENTCNT(wifiConfig.sta.ssid) - 1)) {
        return ESP_FAIL;
    } else {
        memcpy(wifiConfig.sta.ssid,
               (uint8_t *)userOptions.ssid,
               (strlen(userOptions.ssid) + 1));
    }

    if (strlen(userOptions.password) > (ELEMENTCNT(wifiConfig.sta.password) - 1)) {
        return ESP_FAIL;
    } else {
        memcpy(wifiConfig.sta.password,
               (uint8_t *)userOptions.password,
               (strlen(userOptions.password) + 1));
    }

    /**
     * We prime the FSM to start connecting
     */
    wifiFsm[0] = XPLR_WIFISTARTER_STATE_CONFIG_WIFI;

    return ESP_OK;
}

esp_err_t xplrWifiStarterDisconnect(void)
{
    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_STOP_WIFI);
    return ESP_OK;
}

xplrWifiStarterFsmStates_t xplrWifiStarterGetCurrentFsmState(void)
{
    return wifiFsm[0];
}

xplrWifiStarterFsmStates_t xplrWifiStarterGetPreviousFsmState(void)
{
    return wifiFsm[1];
}

esp_err_t xplrWifiStarterScanNetwork(xplrWifiStarterScanList_t *scanInfo)
{
    esp_err_t ret;
    wifi_ap_record_t  apInfo[XPLR_WIFISTARTER_SSID_SCAN_MAX];
    size_t index = 0;
    //wifi_scan_config_t scanConfig;

    memset(apInfo, 0, sizeof(apInfo));
    memset(scanInfo, 0, sizeof(xplrWifiStarterScanList_t));

    ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK) {
        XPLRWIFISTARTER_CONSOLE(W, "SSID scan start failed with error:[%s]", esp_err_to_name(ret));
    } else {
        scanInfo->found = XPLR_WIFISTARTER_SSID_SCAN_MAX;
        ret = esp_wifi_scan_get_ap_records(&scanInfo->found, apInfo);
        if (ret != ESP_OK) {
            XPLRWIFISTARTER_CONSOLE(W, "SSID scan failed with error:[%s]", esp_err_to_name(ret));
        } else {
            ret = esp_wifi_scan_get_ap_num(&scanInfo->found);
        }

        if (ret != ESP_OK) {
            XPLRWIFISTARTER_CONSOLE(W, "SSID scan failed with error:[%s]", esp_err_to_name(ret));
        } else {
            /* clear list */
            for (int i = 0; i < XPLR_WIFISTARTER_SSID_SCAN_MAX; i++) {
                memset(&scanInfo->name[i][0], 0x00, XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
            }
            /* copy scan info */
            for (int i = 0; ((i < scanInfo->found) && (i < XPLR_WIFISTARTER_SSID_SCAN_MAX)); i++) {
                if (strlen((char *)apInfo[i].ssid) < XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX) {
                    memcpy(&scanInfo->name[index][0], apInfo[i].ssid, strlen((char *)apInfo[i].ssid));
                    scanInfo->rssi[index] = apInfo[i].rssi;
                    index++;
                    XPLRWIFISTARTER_CONSOLE(D,
                                            "Found SSID %s with RSSI:[%d]",
                                            &scanInfo->name[index - 1][0],
                                            scanInfo->rssi[index - 1]);
                    vTaskDelay(pdMS_TO_TICKS(5));
                } else {
                    XPLRWIFISTARTER_CONSOLE(W,
                                            "SSID name is more than %d chars, skipping scanlist",
                                            XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
                }
            }
            scanInfo->found = index;
        }
    }

    return ret;
}

void xplrWifiStarterDeviceReboot(void)
{
    XPLRWIFISTARTER_CONSOLE(W, "Device is rebooting");
    esp_restart();
}

xplrWifiStarterError_t xplrWifiStarterDeviceErase(void)
{
    esp_err_t res;
    xplrWifiStarterError_t ret;

    res = wifiNvsErase(0);

    if (res != ESP_OK) {
        ret = XPLR_WIFISTARTER_ERROR;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to erase NVS.");
    } else {
        if (userOptions.webserver) {
            XPLRWIFISTARTER_CONSOLE(W, "NVS erased, rebooting device...");
            esp_restart();
        }
        ret = XPLR_WIFISTARTER_OK;
        XPLRWIFISTARTER_CONSOLE(W, "NVS erased.");
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterDeviceEraseWifi(void)
{
    esp_err_t res;
    xplrWifiStarterError_t ret;

    res = wifiNvsErase(1);

    if (res != ESP_OK) {
        ret = XPLR_WIFISTARTER_ERROR;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to erase NVS.");
    } else {
        if (userOptions.webserver) {
            XPLRWIFISTARTER_CONSOLE(W, "NVS Wi-Fi creds erased, rebooting device...");
            esp_restart();
        }
        ret = XPLR_WIFISTARTER_OK;
        XPLRWIFISTARTER_CONSOLE(W, "NVS Wi-Fi creds erased.");
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterDeviceEraseThingstream(void)
{
    esp_err_t res;
    xplrWifiStarterError_t ret;

    res = wifiNvsErase(2);

    if (res != ESP_OK) {
        ret = XPLR_WIFISTARTER_ERROR;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to erase NVS.");
    } else {
        if (userOptions.webserver) {
            XPLRWIFISTARTER_CONSOLE(W, "NVS Thingstream creds erased, rebooting device...");
            esp_restart();
        }
        ret = XPLR_WIFISTARTER_OK;
        XPLRWIFISTARTER_CONSOLE(W, "NVS Thingstream creds erased.");
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterDeviceForceSaveWifi(void)
{
    esp_err_t res;
    xplrWifiStarterError_t ret;

    res = wifiNvsUpdate(1);

    if (res != ESP_OK) {
        ret = XPLR_WIFISTARTER_ERROR;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to save Wi-Fi creds in NVS.");
    } else {
        ret = XPLR_WIFISTARTER_OK;
        XPLRWIFISTARTER_CONSOLE(W, "NVS Wi-Fi creds saved.");
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterDeviceForceSaveThingstream(uint8_t opts)
{
    esp_err_t res;
    xplrWifiStarterError_t ret;

    res = wifiNvsUpdate(2 + opts);

    if (res != ESP_OK) {
        ret = XPLR_WIFISTARTER_ERROR;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to save Thingstream creds in NVS.");
    } else {
        ret = XPLR_WIFISTARTER_OK;
        XPLRWIFISTARTER_CONSOLE(W, "NVS Thingstream creds saved.");
    }

    return ret;
}

char *xplrWifiStarterWebserverDataGet(xplrWifiStarterServerData_t opt)
{
    char *ret;

    if ((userOptions.webserver) && (userOptions.storage.set)) {
        switch (opt) {
            case XPLR_WIFISTARTER_SERVERDATA_SSID:
                ret = userOptions.storage.ssid;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_PASSWORD:
                ret = userOptions.storage.password;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_ROOTCA:
                ret = userOptions.storage.rootCa;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_CLIENTID:
                ret = userOptions.storage.ppClientId;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_CLIENTCERT:
                ret = userOptions.storage.ppClientCert;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_CLIENTKEY:
                ret = userOptions.storage.ppClientKey;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_CLIENTREGION:
                ret = userOptions.storage.ppClientRegion;
                break;
            case XPLR_WIFISTARTER_SERVERDATA_CLIENTPLAN:
                ret = userOptions.storage.ppClientPlan;
                break;

            default:
                ret = NULL;
                break;
        }
    } else {
        ret = NULL;
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterWebserverDiagnosticsSet(xplrWifiStarterServerData_t opt,
                                                              void *value)
{
    int8_t *i8Val;
    uint32_t *u32Val;
    char *cStr;
    xplrWifiStarterError_t ret;

    if (userOptions.webserver) {
        switch (opt) {
            case XPLR_WIFISTARTER_SERVERDIAG_CONNECTED:
                i8Val = (int8_t *)value;
                webserverData.diagnostics.connected = *i8Val;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED:
                i8Val = (int8_t *)value;
                webserverData.diagnostics.configured = *i8Val;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_READY:
                i8Val = (int8_t *)value;
                webserverData.diagnostics.ready = *i8Val;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_GNSS_ACCURACY:
                u32Val = (uint32_t *)value;
                webserverData.diagnostics.gnssAccuracy = *u32Val;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_UPTIME:
                cStr = (char *)value;
                webserverData.diagnostics.upTime = cStr;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_FIXTIME:
                cStr = (char *)value;
                webserverData.diagnostics.timeToFix = cStr;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_MQTTSTATS:
                cStr = (char *) value;
                webserverData.diagnostics.mqttTraffic = cStr;
                ret = XPLR_WIFISTARTER_OK;
                break;
            case XPLR_WIFISTARTER_SERVERDIAG_FWVERSION:
                cStr = (char *) value;
                webserverData.diagnostics.version = cStr;
                ret = XPLR_WIFISTARTER_OK;
                break;

            default:
                ret = XPLR_WIFISTARTER_ERROR;
                break;
        }
    } else {
        ret = XPLR_WIFISTARTER_ERROR;
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterWebserverLocationSet(char *location)
{
    esp_err_t err;
    xplrWifiStarterError_t ret;

    err = xplrWifiWebserverSendLocation(location);

    if ((err == ESP_OK) || (err == ESP_ERR_NOT_FINISHED)) {
        ret = XPLR_WIFISTARTER_OK;
    } else {
        ret = XPLR_WIFISTARTER_ERROR;
    }

    return ret;
}

xplrWifiStarterError_t xplrWifiStarterWebserverMessageSet(char *message)
{
    esp_err_t err;
    xplrWifiStarterError_t ret;

    err = xplrWifiWebserverSendMessage(message);

    if ((err == ESP_OK) || (err == ESP_ERR_NOT_FINISHED)) {
        ret = XPLR_WIFISTARTER_OK;
    } else {
        ret = XPLR_WIFISTARTER_ERROR;
    }

    return ret;
}

bool xplrWifiStarterWebserverisConfigured(void)
{
    return configured;
}

char *xplrWifiStarterGetStaIp(void)
{
    char *ret;
    if (strlen(staIpString) > 0) {
        ret = staIpString;
    } else {
        ret = NULL;
    }

    return ret;
}

char *xplrWifiStarterGetApIp(void)
{
    char *ret;
    if (strlen(apIpString) > 0) {
        ret = apIpString;
    } else {
        ret = NULL;
    }

    return ret;
}

char *xplrWifiStarterGetApSsid(void)
{
    char *ret;
    if (strlen(apSsid) > 0) {
        ret = apSsid;
    } else {
        ret = NULL;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (sRetryNum < MAX_RETRIES) {
            ret = esp_wifi_connect();

            lastActionTime = MICROTOSEC(esp_timer_get_time());
            sRetryNum++;
            XPLRWIFISTARTER_CONSOLE(D, "Retry no [%d] to connect", sRetryNum);
        } else {
            sRetryNum = 0;
            xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_SCHEDULE_RECONNECT);
            XPLRWIFISTARTER_CONSOLE(D, "Scheduling reconnect in %d seconds.", RETRY_PERIOD_SECS);
        }

        if (userOptions.webserver) {
            webserverData.diagnostics.connected = 0;
            webserverData.diagnostics.configured = 0;
            memset(staIpString, 0x00, 16);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        sRetryNum = 0;
        wifiGetIp();
        wifiStaPrintInfo();
        if (userOptions.webserver) {
            webserverData.diagnostics.connected = 1;
            memset(apIpString, 0x00, 16);
        }
        xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_CONNECT_OK);
    }
}

static esp_err_t wifiNvsInit(void)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err;
    esp_err_t ret;

    /* create namespace tag */
    memset(storage->nvs.tag, 0x00, XPLR_WIFI_NVS_TAG_LENGTH_MAX);
    memset(storage->id, 0x00, XPLR_WIFI_NVS_NAMESPACE_LENGTH_MAX);
    strcat(storage->nvs.tag, nvsNamespace);
    strcat(storage->id, storage->nvs.tag);
    /* init nvs */
    XPLRWIFISTARTER_CONSOLE(D, "Trying to init nvs namespace <%s>.", storage->id);
    err = xplrNvsInit(&storage->nvs, storage->id);

    if (err != XPLR_NVS_OK) {
        ret = ESP_FAIL;
        XPLRWIFISTARTER_CONSOLE(E, "Failed to init nvs namespace <%s>.", storage->id);
    } else {
        if (userOptions.webserver) {
            storage->rootCa = webserverData.pointPerfect.rootCa;
            storage->ppClientId = webserverData.pointPerfect.clientId;
            storage->ppClientCert = webserverData.pointPerfect.certificate;
            storage->ppClientKey = webserverData.pointPerfect.privateKey;
            storage->ppClientRegion = webserverData.pointPerfect.region;
            storage->ppClientPlan = webserverData.pointPerfect.plan;
        }
        XPLRWIFISTARTER_CONSOLE(D, "nvs namespace <%s> for wifi client, init ok", storage->id);
        ret = ESP_OK;
    }

    return ret;
}

static esp_err_t wifiNvsLoad(void)
{
    char storedId[NVS_KEY_NAME_MAX_SIZE] = {0x00};
    bool writeDefaults;
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err;
    size_t size = NVS_KEY_NAME_MAX_SIZE;
    esp_err_t ret;

    /* try read id key */
    err = xplrNvsReadString(&storage->nvs, "id", storedId, &size);
    if ((err != XPLR_NVS_OK) || (strlen(storedId) < 1)) {
        writeDefaults = true;
        XPLRWIFISTARTER_CONSOLE(W, "id key not found in <%s>, write defaults", storage->id);
    } else {
        writeDefaults = false;
        XPLRWIFISTARTER_CONSOLE(D, "id key <%s> found in <%s>", storedId, storage->id);
    }

    if (writeDefaults) {
        ret = wifiNvsWriteDefaults();
        if (ret == ESP_OK) {
            ret = wifiNvsReadConfig();
        }
    } else {
        ret = wifiNvsReadConfig();
    }

    return ret;
}

static esp_err_t wifiNvsWriteDefaults(void)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err[10];
    size_t numOfNvsEntries;
    esp_err_t ret;

    XPLRWIFISTARTER_CONSOLE(D, "Writing default settings in NVS");
    err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
    err[1] = xplrNvsWriteString(&storage->nvs, "ssid", userOptions.ssid);
    err[2] = xplrNvsWriteString(&storage->nvs, "pwd", userOptions.password);

    if (userOptions.webserver) {
        err[3] = xplrNvsWriteString(&storage->nvs, "rootCa", "n/a");
        err[4] = xplrNvsWriteString(&storage->nvs, "ppId", "n/a");
        err[5] = xplrNvsWriteString(&storage->nvs, "ppCert", "n/a");
        err[6] = xplrNvsWriteString(&storage->nvs, "ppKey", "n/a");
        err[7] = xplrNvsWriteString(&storage->nvs, "ppRegion", "n/a");
        err[8] = xplrNvsWriteString(&storage->nvs, "ppPlan", "n/a");
        err[9] = xplrNvsWriteU8(&storage->nvs, "configured", 0);

        storage->rootCa = webserverData.pointPerfect.rootCa;
        storage->ppClientId = webserverData.pointPerfect.clientId;
        storage->ppClientCert = webserverData.pointPerfect.certificate;
        storage->ppClientKey = webserverData.pointPerfect.privateKey;
        storage->ppClientRegion = webserverData.pointPerfect.region;
        storage->ppClientPlan = webserverData.pointPerfect.plan;

        numOfNvsEntries = 10;
    } else {
        numOfNvsEntries = 3;
    }

    for (int i = 0; i < numOfNvsEntries; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            XPLRWIFISTARTER_CONSOLE(E, "Error writing element %u of default settings in NVS", i);
            break;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

static esp_err_t wifiNvsReadConfig(void)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err[10];
    size_t size[] = {XPLR_WIFI_NVS_NAMESPACE_LENGTH_MAX,
                     XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX,
                     XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX,
                     XPLR_WIFIWEBSERVER_PPID_SIZE,
                     XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE,
                     XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE,
                     XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE,
                     XPLR_WIFIWEBSERVER_PPREGION_SIZE,
                     XPLR_WIFIWEBSERVER_PPPLAN_SIZE
                    };
    size_t numOfNvsEntries;
    esp_err_t ret;

    err[0] = xplrNvsReadString(&storage->nvs, "id", storage->id, &size[0]);
    err[1] = xplrNvsReadString(&storage->nvs, "ssid", storage->ssid, &size[1]);
    err[2] = xplrNvsReadString(&storage->nvs, "pwd", storage->password, &size[2]);
    if (userOptions.webserver) {
        err[3] = xplrNvsReadString(&storage->nvs, "ppId", storage->ppClientId, &size[3]);
        err[4] = xplrNvsReadString(&storage->nvs, "rootCa", storage->rootCa, &size[4]);
        err[5] = xplrNvsReadString(&storage->nvs, "ppCert", storage->ppClientCert, &size[5]);
        err[6] = xplrNvsReadString(&storage->nvs, "ppKey", storage->ppClientKey, &size[6]);
        err[7] = xplrNvsReadString(&storage->nvs, "ppRegion", storage->ppClientRegion, &size[7]);
        err[8] = xplrNvsReadString(&storage->nvs, "ppPlan", storage->ppClientPlan, &size[8]);
        err[9] = xplrNvsReadU8(&storage->nvs, "configured", (uint8_t *)&storage->set);

        numOfNvsEntries = 10;
    } else {
        numOfNvsEntries = 3;
    }

    for (int i = 0; i < numOfNvsEntries; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            break;
        } else {
            ret = ESP_OK;
        }
    }

    if (ret == ESP_OK) {
        XPLRWIFISTARTER_CONSOLE(D, "id: <%s>", storage->id);
        XPLRWIFISTARTER_CONSOLE(D, "ssid: <%s>", storage->ssid);
        XPLRWIFISTARTER_CONSOLE(D, "pwd: <%s>", storage->password);
        if (userOptions.webserver) {
            if (storage->set) {
                memcpy(wifiConfig.sta.ssid,
                       (uint8_t *)storage->ssid,
                       (strlen(storage->ssid) + 1));

                memcpy(wifiConfig.sta.password,
                       (uint8_t *)storage->password,
                       (strlen(storage->password) + 1));
            }
            XPLRWIFISTARTER_CONSOLE(D, "rootCa: <%s>", storage->rootCa);
            XPLRWIFISTARTER_CONSOLE(D, "ppId: <%s>", storage->ppClientId);
            XPLRWIFISTARTER_CONSOLE(D, "ppCert: <%s>", storage->ppClientCert);
            XPLRWIFISTARTER_CONSOLE(D, "ppKey: <%s>", storage->ppClientKey);
            XPLRWIFISTARTER_CONSOLE(D, "ppRegion: <%s>", storage->ppClientRegion);
            XPLRWIFISTARTER_CONSOLE(D, "ppPlan: <%s>", storage->ppClientPlan);
            XPLRWIFISTARTER_CONSOLE(D, "configured: <%u>", (uint8_t)storage->set);
        }
    }

    return ret;
}

static esp_err_t wifiNvsUpdate(uint8_t opt)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err[10];
    size_t numOfNvsEntries;
    esp_err_t ret;

    switch (opt) {
        case 0: //save all
            if ((storage->id != NULL) &&
                (storage->ssid != NULL) &&
                (storage->password != NULL)) {
                xplrNvsEraseKey(&storage->nvs, "ssid");
                xplrNvsEraseKey(&storage->nvs, "pwd");
                xplrNvsEraseKey(&storage->nvs, "rootCa");
                xplrNvsEraseKey(&storage->nvs, "ppId");
                xplrNvsEraseKey(&storage->nvs, "ppCert");
                xplrNvsEraseKey(&storage->nvs, "ppKey");
                xplrNvsEraseKey(&storage->nvs, "ppRegion");
                xplrNvsEraseKey(&storage->nvs, "ppPlan");
                xplrNvsEraseKey(&storage->nvs, "configured");

                err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
                err[1] = xplrNvsWriteString(&storage->nvs, "ssid", storage->ssid);
                err[2] = xplrNvsWriteString(&storage->nvs, "pwd", storage->password);
                if (userOptions.webserver) {
                    err[3] = xplrNvsWriteString(&storage->nvs, "rootCa", storage->rootCa);
                    err[4] = xplrNvsWriteString(&storage->nvs, "ppId", storage->ppClientId);
                    err[5] = xplrNvsWriteString(&storage->nvs, "ppCert", storage->ppClientCert);
                    err[6] = xplrNvsWriteString(&storage->nvs, "ppKey", storage->ppClientKey);
                    err[7] = xplrNvsWriteString(&storage->nvs, "ppRegion", storage->ppClientRegion);
                    err[8] = xplrNvsWriteString(&storage->nvs, "ppPlan", storage->ppClientPlan);
                    err[9] = xplrNvsWriteU8(&storage->nvs, "configured", (uint8_t)storage->set);

                    numOfNvsEntries = 10;
                } else {
                    numOfNvsEntries = 3;
                }

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 1: //save wifi from webserver data
            if ((storage->id != NULL) &&
                (storage->ssid != NULL) &&
                (storage->password != NULL)) {

                memset(userOptions.storage.ssid,
                       0x00,
                       XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
                memset(userOptions.storage.password,
                       0x00,
                       XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX);

                memcpy(userOptions.storage.ssid,
                       webserverData.wifi.ssid,
                       strlen(webserverData.wifi.ssid));
                memcpy(userOptions.storage.password,
                       webserverData.wifi.password,
                       strlen(webserverData.wifi.password));

                xplrNvsEraseKey(&storage->nvs, "ssid");
                xplrNvsEraseKey(&storage->nvs, "pwd");

                err[0] = xplrNvsWriteString(&storage->nvs, "ssid", storage->ssid);
                err[1] = xplrNvsWriteString(&storage->nvs, "pwd", storage->password);
                numOfNvsEntries = 2;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 2: //save thingstream from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.rootCa = webserverData.pointPerfect.rootCa;
                userOptions.storage.ppClientId = webserverData.pointPerfect.clientId;
                userOptions.storage.ppClientCert = webserverData.pointPerfect.certificate;
                userOptions.storage.ppClientKey = webserverData.pointPerfect.privateKey;
                userOptions.storage.ppClientRegion = webserverData.pointPerfect.region;
                userOptions.storage.ppClientPlan = webserverData.pointPerfect.plan;

                xplrNvsEraseKey(&storage->nvs, "rootCa");
                xplrNvsEraseKey(&storage->nvs, "ppId");
                xplrNvsEraseKey(&storage->nvs, "ppCert");
                xplrNvsEraseKey(&storage->nvs, "ppKey");
                xplrNvsEraseKey(&storage->nvs, "ppRegion");
                xplrNvsEraseKey(&storage->nvs, "ppPlan");
                xplrNvsEraseKey(&storage->nvs, "configured");

                err[0] = xplrNvsWriteString(&storage->nvs, "rootCa", storage->rootCa);
                err[1] = xplrNvsWriteString(&storage->nvs, "ppId", storage->ppClientId);
                err[2] = xplrNvsWriteString(&storage->nvs, "ppCert", storage->ppClientCert);
                err[3] = xplrNvsWriteString(&storage->nvs, "ppKey", storage->ppClientKey);
                err[4] = xplrNvsWriteString(&storage->nvs, "ppRegion", storage->ppClientRegion);
                err[5] = xplrNvsWriteString(&storage->nvs, "ppPlan", storage->ppClientPlan);
                err[6] = xplrNvsWriteU8(&storage->nvs, "configured", (uint8_t)storage->set);

                numOfNvsEntries = 7;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 3: //save thingstream rootCa from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.rootCa = webserverData.pointPerfect.rootCa;

                xplrNvsEraseKey(&storage->nvs, "rootCa");

                err[0] = xplrNvsWriteString(&storage->nvs, "rootCa", storage->rootCa);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 4: //save thingstream id from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.ppClientId = webserverData.pointPerfect.clientId;

                xplrNvsEraseKey(&storage->nvs, "ppId");

                err[0] = xplrNvsWriteString(&storage->nvs, "ppId", storage->ppClientId);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 5: //save thingstream certificate from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.ppClientCert = webserverData.pointPerfect.certificate;

                xplrNvsEraseKey(&storage->nvs, "ppCert");

                err[0] = xplrNvsWriteString(&storage->nvs, "ppCert", storage->ppClientCert);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 6: //save thingstream key from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.ppClientKey = webserverData.pointPerfect.privateKey;

                xplrNvsEraseKey(&storage->nvs, "ppKey");

                err[0] = xplrNvsWriteString(&storage->nvs, "ppKey", storage->ppClientKey);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 7: //save thingstream region from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.ppClientRegion = webserverData.pointPerfect.region;

                xplrNvsEraseKey(&storage->nvs, "ppRegion");

                err[0] = xplrNvsWriteString(&storage->nvs, "ppRegion", storage->ppClientRegion);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;
        case 8: //save thingstream plan from webserver data
            if ((storage->id != NULL) && (userOptions.webserver)) {
                userOptions.storage.ppClientPlan = webserverData.pointPerfect.plan;

                xplrNvsEraseKey(&storage->nvs, "ppPlan");

                err[0] = xplrNvsWriteString(&storage->nvs, "ppPlan", storage->ppClientPlan);

                numOfNvsEntries = 1;

                for (int i = 0; i < numOfNvsEntries; i++) {
                    if (err[i] != XPLR_NVS_OK) {
                        ret = ESP_FAIL;
                        break;
                    } else {
                        ret = ESP_OK;
                    }
                }
            } else {
                ret = ESP_FAIL;
                XPLRWIFISTARTER_CONSOLE(E, "Trying to write invalid config, error");
            }
            break;

        default:
            ret = ESP_FAIL;
            XPLRWIFISTARTER_CONSOLE(E, "Invalid save option");
            break;
    }

    return ret;
}

static esp_err_t wifiNvsErase(uint8_t opt)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;
    xplrNvs_error_t err[10];
    size_t numOfNvsEntries;
    esp_err_t ret;

    if (opt == 0) { //erase all
        err[0] = xplrNvsEraseKey(&storage->nvs, "id");
        err[1] = xplrNvsEraseKey(&storage->nvs, "ssid");
        err[2] = xplrNvsEraseKey(&storage->nvs, "pwd");
        if (userOptions.webserver) {
            err[3] = xplrNvsEraseKey(&storage->nvs, "rootCa");
            err[4] = xplrNvsEraseKey(&storage->nvs, "ppId");
            err[5] = xplrNvsEraseKey(&storage->nvs, "ppCert");
            err[6] = xplrNvsEraseKey(&storage->nvs, "ppKey");
            err[7] = xplrNvsEraseKey(&storage->nvs, "ppRegion");
            err[8] = xplrNvsEraseKey(&storage->nvs, "ppPlan");
            err[9] = xplrNvsEraseKey(&storage->nvs, "configured");

            numOfNvsEntries = 10;
        } else {
            numOfNvsEntries = 3;
        }
    } else if (opt == 1) { //erase wifi creds
        err[0] = xplrNvsEraseKey(&storage->nvs, "id");
        err[1] = xplrNvsEraseKey(&storage->nvs, "ssid");
        err[2] = xplrNvsEraseKey(&storage->nvs, "pwd");
        if (userOptions.webserver) {
            err[3] = xplrNvsEraseKey(&storage->nvs, "configured");
            numOfNvsEntries = 4;
        } else {
            numOfNvsEntries = 3;
        }

    } else if (opt == 2) { //erase thingstream creds
        err[0] = xplrNvsEraseKey(&storage->nvs, "rootCa");
        err[1] = xplrNvsEraseKey(&storage->nvs, "ppId");
        err[2] = xplrNvsEraseKey(&storage->nvs, "ppCert");
        err[3] = xplrNvsEraseKey(&storage->nvs, "ppKey");
        err[4] = xplrNvsEraseKey(&storage->nvs, "ppRegion");
        err[5] = xplrNvsEraseKey(&storage->nvs, "ppPlan");
        err[6] = xplrNvsEraseKey(&storage->nvs, "configured");
        numOfNvsEntries = 7;
    } else {
        err[0] = XPLR_NVS_ERROR;
        numOfNvsEntries = 1;
    }

    for (int i = 0; i < numOfNvsEntries; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = ESP_FAIL;
            break;
        } else {
            ret = ESP_OK;
        }
    }

    return ret;
}

static bool wifiCredentialsConfigured(void)
{
    xplrWifiStarterNvs_t *storage = &userOptions.storage;

    if (userOptions.webserver) {
        if (storage->set) {
            ret = true;
        } else {
            ret = false;
        }
    } else {
        ret = true;
        if((strlen(userOptions.ssid) > 0) && (strlen(userOptions.password) > 0)) {
            ret = true;
        } else {
            ret = false;
        }
    }

    return ret;
}

static esp_err_t xplrWifiStarterPrivateRegisterHandlers(void)
{
    esp_err_t ret;

    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &event_handler,
                                              NULL,
                                              &instanceAnyId);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &event_handler,
                                              NULL,
                                              &instanceGotIp);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t xplrWifiStarterPrivateUnregisterHandlers(void)
{
    esp_err_t ret;

    ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instanceGotIp);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        return ret;
    }

    ret = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instanceAnyId);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        return ret;
    }

    return ESP_OK;
}

static void xplrWifiStarterPrivateUpdateNextState(xplrWifiStarterFsmStates_t nextState)
{
    wifiFsm[1] = wifiFsm[0];
    wifiFsm[0] = nextState;

    ret = XPLR_WIFISTARTER_OK;
}

static void xplrWifiStarterPrivateUpdateNextStateToError(void)
{
    xplrWifiStarterPrivateUpdateNextState(XPLR_WIFISTARTER_STATE_ERROR);

    ret = XPLR_WIFISTARTER_ERROR;
}

static void xplrWifiStarterPrivateInitCfg(void)
{
    cfg.event_handler = &esp_event_send_internal;
    cfg.osi_funcs = &g_wifi_osi_funcs;
    cfg.wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs;
    cfg.static_rx_buf_num = CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM;
    cfg.dynamic_rx_buf_num = CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM;
    cfg.tx_buf_type = CONFIG_ESP32_WIFI_TX_BUFFER_TYPE;
    cfg.static_tx_buf_num = WIFI_STATIC_TX_BUFFER_NUM;
    cfg.dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUFFER_NUM;
    cfg.cache_tx_buf_num = WIFI_CACHE_TX_BUFFER_NUM;
    cfg.csi_enable = WIFI_CSI_ENABLED;
    cfg.ampdu_rx_enable = WIFI_AMPDU_RX_ENABLED;
    cfg.ampdu_tx_enable = WIFI_AMPDU_TX_ENABLED;
    cfg.amsdu_tx_enable = WIFI_AMSDU_TX_ENABLED;
    cfg.nvs_enable = WIFI_NVS_ENABLED;
    cfg.nano_enable = WIFI_NANO_FORMAT_ENABLED;
    cfg.rx_ba_win = WIFI_DEFAULT_RX_BA_WIN;
    cfg.wifi_task_core_id = WIFI_TASK_CORE_ID;
    cfg.beacon_max_len = WIFI_SOFTAP_BEACON_MAX_LEN;
    cfg.mgmt_sbuf_num = WIFI_MGMT_SBUF_NUM;
    cfg.feature_caps = g_wifi_feature_caps;
    cfg.sta_disconnected_pm = WIFI_STA_DISCONNECTED_PM_ENABLED;
    cfg.magic = WIFI_INIT_CONFIG_MAGIC;
}

static esp_err_t xplrWifiStarterPrivateDisconnect(void)
{
    esp_err_t ret;

    ret = xplrWifiStarterPrivateUnregisterHandlers();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        return ESP_FAIL;
    }

    ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void wifiGetMac(void)
{
    esp_efuse_mac_get_default(macAdr);
    esp_read_mac(macAdr, ESP_MAC_WIFI_STA);
#if (1 == XPLRWIFISTARTER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    unsigned char mac_local_base[6] = {0};
    unsigned char mac_uni_base[6] = {0};
    esp_derive_local_mac(mac_local_base, mac_uni_base);
    printf("MAC details:\n");
    printf("Local Address: ");
    wifiPrintMac(mac_local_base);
    printf("\nUni Address: ");
    wifiPrintMac(mac_uni_base);
    printf("\nMAC Address: ");
    wifiPrintMac(macAdr);
    printf("\n");
#endif
}

static void wifiGetIp(void)
{
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &staIpInfo);
    inet_ntoa_r(staIpInfo.ip.addr, staIpString, 16);
}

static void wifiPrintMac(const unsigned char *mac)
{
#if (1 == XPLRWIFISTARTER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
}

static void wifiStaPrintInfo(void)
{
#if (1 == XPLRWIFISTARTER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    XPLRWIFISTARTER_CONSOLE(I, "Station connected with following settings:");
    if (userOptions.webserver) {
        printf("SSID: %s\n", userOptions.storage.ssid);
        printf("Password: %s\n", userOptions.storage.password);
    } else {
        printf("SSID: %s\n", userOptions.ssid);
        printf("Password: %s\n", userOptions.password);
    }

    printf("IP: %s\n", staIpString);
#endif
}

static void wifiApGetIp(void)
{
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &apIpInfo);
    inet_ntoa_r(apIpInfo.ip.addr, apIpString, 16);
    webserverData.diagnostics.ip = apIpString;
}

static void wifiApPrintInfo(void)
{
#if (1 == XPLRWIFISTARTER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
    XPLRWIFISTARTER_CONSOLE(I, "Access-Point is up with following settings:");
    printf("SSID: %s\n", apSsid);
    printf("Password: %s\n", XPLR_WIFISTARTER_AP_PWD);
    printf("IP: %s\n", apIpString);
#endif
}
