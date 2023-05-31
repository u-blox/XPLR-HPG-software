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

#include <stdio.h>
#include "string.h"
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_vfs.h"
#include "xplr_wifi_starter.h"
#include "xplr_wifi_webserver.h"
#include "cJSON.h"
#include "./../hpglib/xplr_hpglib_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* Data buffer for websocket transactions */
#define WEBSOCKET_BUFSIZE           (CONFIG_WS_BUFFER_SIZE)

#if (1 == XPLRWIFIWEBSERVER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRWIFIWEBSERVER_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrWifiWebserver", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRWIFIWEBSERVER_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

typedef struct xplrWifiWebserverUris_type {
    httpd_uri_t index;
    httpd_uri_t home;
    httpd_uri_t settings;
    httpd_uri_t liveTracker;
    httpd_uri_t error;
    httpd_uri_t bootstrap;
    httpd_uri_t bootstrapMap;
    httpd_uri_t bootstrapCss;
    httpd_uri_t bootstrapCssMap;
    httpd_uri_t fontAwesome;
    httpd_uri_t fontAwesomeCss;
    httpd_uri_t jQuery;
    httpd_uri_t favIcon;
    httpd_uri_t ubxLogo;
    httpd_uri_t xplrHpg;
    httpd_uri_t xplrHpgCss;
    httpd_uri_t ws;
} xplrWifiWebserverUris_t;

typedef struct xplrWifiWebserver_type {
    httpd_handle_t              instance;
    httpd_config_t              config;
    xplrWifiWebserverUris_t     uris;
    bool                        running;
    uint8_t                     wsBuf[WEBSOCKET_BUFSIZE];
    xplrWifiWebServerData_t     *wsData;
} xplrWifiWebserver_t;

typedef enum {
    XPLR_WEBSERVER_WSREQ_INVALID = -1,
    XPLR_WEBSERVER_WSREQ_STATUS,
    XPLR_WEBSERVER_WSREQ_INFO,
    XPLR_WEBSERVER_WSREQ_REBOOT,
    XPLR_WEBSERVER_WSREQ_ERASE_ALL,
    XPLR_WEBSERVER_WSREQ_ERASE_WIFI,
    XPLR_WEBSERVER_WSREQ_ERASE_THINGSTREAM,
    XPLR_WEBSERVER_WSREQ_SCAN,
    XPLR_WEBSERVER_WSREQ_WIFI_SET,
    XPLR_WEBSERVER_WSREQ_PPID_SET,
    XPLR_WEBSERVER_WSREQ_ROOTCA_SET,
    XPLR_WEBSERVER_WSREQ_PPCERT_SET,
    XPLR_WEBSERVER_WSREQ_PPKEY_SET,
    XPLR_WEBSERVER_WSREQ_PPREGION_SET,
    XPLR_WEBSERVER_WSREQ_PPPLAN_SET,
    XPLR_WEBSERVER_WSREQ_LOCATION,
    XPLR_WEBSERVER_WSREQ_MESSAGE,
    XPLR_WEBSERVER_WSREQ_SUPPORTED  //number of requests supported
} xplrWifiWebserverWsReqType_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

xplrWifiWebserver_t     webserver;
httpd_ws_frame_t        locationFrame;
char                    locationFrameBuff[512];
httpd_ws_frame_t        messageFrame;
char                    messageFrameBuff[512];

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* webserver */

static esp_err_t indexGetHandler(httpd_req_t *req);
static esp_err_t settingsGetHandler(httpd_req_t *req);
static esp_err_t liveTrackerGetHandler(httpd_req_t *req);
static esp_err_t errorGetHandler(httpd_req_t *req);
static esp_err_t bootstrapGetHandler(httpd_req_t *req);
static esp_err_t bootstrapMapGetHandler(httpd_req_t *req);
static esp_err_t bootstrapCssGetHandler(httpd_req_t *req);
static esp_err_t bootstrapCssMapGetHandler(httpd_req_t *req);
static esp_err_t fontAwesomeGetHandler(httpd_req_t *req);
static esp_err_t fontAwesomeCssGetHandler(httpd_req_t *req);
static esp_err_t jQueryGetHandler(httpd_req_t *req);
static esp_err_t favIconGetHandler(httpd_req_t *req);
static esp_err_t ubloxLogoSvgGetHandler(httpd_req_t *req);
static esp_err_t xplrHpgGetHandler(httpd_req_t *req);
static esp_err_t xplrHpgCssGetHandler(httpd_req_t *req);
static esp_err_t error404Handler(httpd_req_t *req, httpd_err_code_t err);
static bool xplrHpgThingstreamCredsConfigured(void);

/* websocket */

static esp_err_t wsGetHandler(httpd_req_t *req);
static esp_err_t wsParseData(httpd_req_t *req, uint8_t *data);
static esp_err_t wsServeReq(httpd_req_t *req, xplrWifiWebserverWsReqType_t type);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS DESCRIPTORS
 * -------------------------------------------------------------- */

httpd_handle_t  xplrWifiWebserverStart(xplrWifiWebServerData_t *data)
{
    esp_err_t err;

    if (!webserver.running) {
        /* configure webserver */
        webserver.instance = NULL;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        memcpy(&webserver.config, &config, sizeof(httpd_config_t));
        //server.config = HTTPD_DEFAULT_CONFIG();
        webserver.config.max_open_sockets = XPLR_WIFIWEBSERVER_SOCKETS_OPEN_MAX;
        webserver.config.max_uri_handlers = WEBSERVER_URIS_MAX;
        webserver.config.lru_purge_enable = true;
        webserver.config.uri_match_fn = httpd_uri_match_wildcard;

        /* set uris to handle */
        webserver.uris.index.uri = "/";
        webserver.uris.index.method = HTTP_GET;
        webserver.uris.index.handler = indexGetHandler;

        /* set uris to handle */
        webserver.uris.home.uri = "/index.html";
        webserver.uris.home.method = HTTP_GET;
        webserver.uris.home.handler = indexGetHandler;

        webserver.uris.settings.uri = "/settings.html";
        webserver.uris.settings.method = HTTP_GET;
        webserver.uris.settings.handler = settingsGetHandler;

        webserver.uris.liveTracker.uri = "/tracker.html";
        webserver.uris.liveTracker.method = HTTP_GET;
        webserver.uris.liveTracker.handler = liveTrackerGetHandler;

        webserver.uris.error.uri = "/error.html";
        webserver.uris.error.method = HTTP_GET;
        webserver.uris.error.handler = errorGetHandler;

        webserver.uris.bootstrap.uri = "/static/js/bootstrap.bundle.min.js";
        webserver.uris.bootstrap.method = HTTP_GET;
        webserver.uris.bootstrap.handler = bootstrapGetHandler;

        webserver.uris.bootstrapMap.uri = "/static/js/bootstrap.bundle.min.js.map";
        webserver.uris.bootstrapMap.method = HTTP_GET;
        webserver.uris.bootstrapMap.handler = bootstrapMapGetHandler;

        webserver.uris.bootstrapCss.uri = "/static/css/bootstrap.min.css";
        webserver.uris.bootstrapCss.method = HTTP_GET;
        webserver.uris.bootstrapCss.handler = bootstrapCssGetHandler;

        webserver.uris.bootstrapCssMap.uri = "/static/css/bootstrap.min.css.map";
        webserver.uris.bootstrapCssMap.method = HTTP_GET;
        webserver.uris.bootstrapCssMap.handler = bootstrapCssMapGetHandler;

        webserver.uris.fontAwesome.uri = "/static/js/fontawesome.min.js";
        webserver.uris.fontAwesome.method = HTTP_GET;
        webserver.uris.fontAwesome.handler = fontAwesomeGetHandler;

        webserver.uris.fontAwesomeCss.uri = "/static/css/fontawesome.min.css";
        webserver.uris.fontAwesomeCss.method = HTTP_GET;
        webserver.uris.fontAwesomeCss.handler = fontAwesomeCssGetHandler;

        webserver.uris.jQuery.uri = "/static/js/jquery-1.7.1.min.js";
        webserver.uris.jQuery.method = HTTP_GET;
        webserver.uris.jQuery.handler = jQueryGetHandler;

        webserver.uris.favIcon.uri = "/static/img/favicon.ico";
        webserver.uris.favIcon.method = HTTP_GET;
        webserver.uris.favIcon.handler = favIconGetHandler;

        webserver.uris.ubxLogo.uri = "/static/img/ublox_logo.svg";
        webserver.uris.ubxLogo.method = HTTP_GET;
        webserver.uris.ubxLogo.handler = ubloxLogoSvgGetHandler;

        webserver.uris.xplrHpg.uri = "/static/js/xplrHpg.js";
        webserver.uris.xplrHpg.method = HTTP_GET;
        webserver.uris.xplrHpg.handler = xplrHpgGetHandler;

        webserver.uris.xplrHpgCss.uri = "/static/css/xplrHpg.css";
        webserver.uris.xplrHpgCss.method = HTTP_GET;
        webserver.uris.xplrHpgCss.handler = xplrHpgCssGetHandler;

        webserver.uris.ws.uri = "/xplrHpg";
        webserver.uris.ws.method = HTTP_GET;
        webserver.uris.ws.handler = wsGetHandler;
        webserver.uris.ws.user_ctx = NULL;
        webserver.uris.ws.is_websocket  = true;

        // Start the httpd server
        XPLRWIFIWEBSERVER_CONSOLE(D, "Starting server on port: '%d'", webserver.config.server_port);
        err = httpd_start(&webserver.instance, &webserver.config);
        if (err == ESP_OK) {
            // Set URI handlers
            XPLRWIFIWEBSERVER_CONSOLE(D, "Registering URI handlers");
            httpd_register_uri_handler(webserver.instance, &webserver.uris.index);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.home);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.settings);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.liveTracker);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.error);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.bootstrap);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.bootstrapMap);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.bootstrapCss);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.bootstrapCssMap);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.fontAwesome);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.fontAwesomeCss);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.jQuery);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.favIcon);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.ubxLogo);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.xplrHpg);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.xplrHpgCss);
            httpd_register_uri_handler(webserver.instance, &webserver.uris.ws);
            httpd_register_err_handler(webserver.instance, HTTPD_404_NOT_FOUND, error404Handler);

            webserver.wsData = data;
            webserver.running = true;
        } else {
            XPLRWIFIWEBSERVER_CONSOLE(E, "Error starting webserver: %s", esp_err_to_name(err));
        }
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(W, "Webserver already running");
    }

    return webserver.instance;
}

esp_err_t xplrWifiWebserverStop(void)
{
    esp_err_t ret;

    ret = httpd_stop(webserver.instance);
    webserver.instance = NULL;
    webserver.wsData = NULL;

    return ret;
}

esp_err_t xplrWifiWebserverSendLocation(char *jMsg)
{
    esp_err_t ret;

    memset(&locationFrame, 0, sizeof(httpd_ws_frame_t));
    memset(locationFrameBuff, 0, 512);
    memcpy(locationFrameBuff, jMsg, strlen(jMsg));
    locationFrame.type = HTTPD_WS_TYPE_TEXT;
    locationFrame.payload = (uint8_t *)locationFrameBuff;
    locationFrame.len = strlen(locationFrameBuff);
    ret = ESP_OK;

    return ret;
}

esp_err_t xplrWifiWebserverSendMessage(char *message)
{
    esp_err_t ret;

    memset(&messageFrame, 0, sizeof(httpd_ws_frame_t));
    memset(messageFrameBuff, 0, 512);
    memcpy(messageFrameBuff, message, strlen(message));
    messageFrame.type = HTTPD_WS_TYPE_TEXT;
    messageFrame.payload = (uint8_t *)messageFrameBuff;
    messageFrame.len = strlen(messageFrameBuff);
    ret = ESP_OK;

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

static esp_err_t indexGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char indexStart[] asm("_binary_index_html_start");
    extern const unsigned char indexEnd[]   asm("_binary_index_html_end");
    const size_t pageSize = (indexEnd - indexStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for index.html");
    ret = httpd_resp_set_type(req, "text/html");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for index");
    } else {
        ret = httpd_resp_send(req, (const char *)indexStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to index get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to index get request, ok");
    }

    return ret;
}

static esp_err_t settingsGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char settingsStart[] asm("_binary_settings_html_start");
    extern const unsigned char settingsEnd[]   asm("_binary_settings_html_end");
    const size_t pageSize = (settingsEnd - settingsStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for settings.html");
    ret = httpd_resp_set_type(req, "text/html");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for settings");
    } else {
        ret = httpd_resp_send(req, (const char *)settingsStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to settings get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to settings get request, ok");
    }

    return ret;
}

static esp_err_t liveTrackerGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char trackerStart[] asm("_binary_tracker_html_start");
    extern const unsigned char trackerEnd[]   asm("_binary_tracker_html_end");
    const size_t pageSize = (trackerEnd - trackerStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for tracker.html");
    ret = httpd_resp_set_type(req, "text/html");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for index");
    } else {
        ret = httpd_resp_send(req, (const char *)trackerStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to tracker get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to tracker get request, ok");
    }

    return ret;
}

static esp_err_t errorGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char errorStart[] asm("_binary_error_html_start");
    extern const unsigned char errorEnd[]   asm("_binary_error_html_end");
    const size_t pageSize = (errorEnd - errorStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for error.html");
    ret = httpd_resp_set_type(req, "text/html");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for error");
    } else {
        ret = httpd_resp_send(req, (const char *)errorStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to error get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to error get request, ok");
    }

    return ret;
}

static esp_err_t bootstrapGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char bootstrapJsStart[] asm("_binary_bootstrap_bundle_min_js_start");
    extern const unsigned char bootstrapJsEnd[]   asm("_binary_bootstrap_bundle_min_js_end");
    const size_t pageSize = (bootstrapJsEnd - bootstrapJsStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for bootstrap.bundle.min.js");
    ret = httpd_resp_set_type(req, "text/javascript");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for js");
    } else {
        ret = httpd_resp_send(req, (const char *)bootstrapJsStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to bootstrap get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to bootstrap get request, ok");
    }

    return ret;
}

static esp_err_t bootstrapMapGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char bootstrapJsMapStart[] asm("_binary_bootstrap_bundle_min_js_map_start");
    extern const unsigned char bootstrapJsMapEnd[]   asm("_binary_bootstrap_bundle_min_js_map_end");
    const size_t pageSize = (bootstrapJsMapEnd - bootstrapJsMapStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for bootstrap.bundle.min.js.map");
    ret = httpd_resp_set_type(req, "application/json");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for js");
    } else {
        ret = httpd_resp_send(req, (const char *)bootstrapJsMapStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to bootstrap get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to bootstrap get request, ok");
    }

    return ret;
}

static esp_err_t bootstrapCssGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char bootstrapCssStart[] asm("_binary_bootstrap_min_css_start");
    extern const unsigned char bootstrapCssEnd[]   asm("_binary_bootstrap_min_css_end");
    const size_t pageSize = (bootstrapCssEnd - bootstrapCssStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for bootstrap.min.css");
    ret = httpd_resp_set_type(req, "text/css");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for css");
    } else {
        ret = httpd_resp_send(req, (const char *)bootstrapCssStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to bootstrap css get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to bootstrap css get request, ok");
    }

    return ret;
}

static esp_err_t bootstrapCssMapGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char bootstrapCssMapStart[] asm("_binary_bootstrap_min_css_map_start");
    extern const unsigned char bootstrapCssMapEnd[]   asm("_binary_bootstrap_min_css_map_end");
    const size_t pageSize = (bootstrapCssMapEnd - bootstrapCssMapStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for bootstrap.min.css.map");
    ret = httpd_resp_set_type(req, "application/json");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for css.map");
    } else {
        ret = httpd_resp_send(req, (const char *)bootstrapCssMapStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to bootstrap css get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to bootstrap css get request, ok");
    }

    return ret;
}

static esp_err_t fontAwesomeGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char fontAwesomeJsStart[] asm("_binary_fontawesome_min_js_start");
    extern const unsigned char fontAwesomeJsEnd[]   asm("_binary_fontawesome_min_js_end");
    const size_t pageSize = (fontAwesomeJsEnd - fontAwesomeJsStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for fontawesome.min.js");
    ret = httpd_resp_set_type(req, "text/javascript");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for js");
    } else {
        ret = httpd_resp_send(req, (const char *)fontAwesomeJsStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to fontawesome get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to fontawesome get request, ok");
    }

    return ret;
}

static esp_err_t fontAwesomeCssGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char fontawesomeCssStart[] asm("_binary_fontawesome_min_css_start");
    extern const unsigned char fontawesomeCssEnd[]   asm("_binary_fontawesome_min_css_end");
    const size_t pageSize = (fontawesomeCssEnd - fontawesomeCssStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for fontawesome.min.css");
    ret = httpd_resp_set_type(req, "text/css");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for css");
    } else {
        ret = httpd_resp_send(req, (const char *)fontawesomeCssStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to fontawesome css get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to fontawesome css get request, ok");
    }

    return ret;
}

static esp_err_t jQueryGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char jQueryJsStart[] asm("_binary_jquery_min_js_start");
    extern const unsigned char jQueryJsEnd[]   asm("_binary_jquery_min_js_end");
    const size_t pageSize = (jQueryJsEnd - jQueryJsStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for jquery.min.js");
    ret = httpd_resp_set_type(req, "text/javascript");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for js");
    } else {
        ret = httpd_resp_send(req, (const char *)jQueryJsStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to jQuery get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to jQuery get request, ok");
    }

    return ret;
}

static esp_err_t favIconGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char favIconStart[] asm("_binary_favicon_ico_start");
    extern const unsigned char favIconEnd[]   asm("_binary_favicon_ico_end");
    const size_t pageSize = (favIconEnd - favIconStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for favicon.ico");
    ret = httpd_resp_set_type(req, "image/x-icon");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for ico");
    } else {
        ret = httpd_resp_send(req, (const char *)favIconStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to favicon get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to favicon get request, ok");
    }

    return ret;
}

static esp_err_t ubloxLogoSvgGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char ubxLogoSvgStart[] asm("_binary_ublox_logo_svg_start");
    extern const unsigned char ubxLogoSvgEnd[]   asm("_binary_ublox_logo_svg_end");
    const size_t pageSize = (ubxLogoSvgEnd - ubxLogoSvgStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for ublox_logo.svg");
    ret = httpd_resp_set_type(req, "image/svg+xml");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for svg");
    } else {
        ret = httpd_resp_send(req, (const char *)ubxLogoSvgStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to ublox logo (svg) get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to ublox logo (svg) get request, ok");
    }

    return ret;
}

static esp_err_t xplrHpgGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char xplrHpgJsStart[] asm("_binary_xplrHpg_js_start");
    extern const unsigned char xplrHpgJsEnd[]   asm("_binary_xplrHpg_js_end");
    const size_t pageSize = (xplrHpgJsEnd - xplrHpgJsStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for xplrHpg.js");
    ret = httpd_resp_set_type(req, "text/javascript");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for js");
    } else {
        ret = httpd_resp_send(req, (const char *)xplrHpgJsStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to xplrHpg get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to xplrHpg get request, ok");
    }

    return ret;
}

static esp_err_t xplrHpgCssGetHandler(httpd_req_t *req)
{
    esp_err_t ret;
    extern const unsigned char xplrHpgCssStart[] asm("_binary_xplrHpg_css_start");
    extern const unsigned char xplrHpgCssEnd[]   asm("_binary_xplrHpg_css_end");
    const size_t pageSize = (xplrHpgCssEnd - xplrHpgCssStart);

    XPLRWIFIWEBSERVER_CONSOLE(D, "Got request for xplrHpg.css");
    ret = httpd_resp_set_type(req, "text/css");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set rsp type for css");
    } else {
        ret = httpd_resp_send(req, (const char *)xplrHpgCssStart, pageSize);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Error responding to xplrHpg css get request");
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(D, "Responded to xplrHpg css get request, ok");
    }

    return ret;
}

esp_err_t error404Handler(httpd_req_t *req, httpd_err_code_t err)
{
    esp_err_t ret;
    /* Set status */
    ret = httpd_resp_set_status(req, "302 Temporary Redirect");
    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set server status to 302");
    } else {
        if (strstr(req->uri, "generate_204") != NULL) {
            /* Redirect to home page */
            ret = httpd_resp_set_hdr(req, "Location", "/");
        } else {
            /* Redirect to error page */
            ret = httpd_resp_set_hdr(req, "Location", "/error.html");
        }
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Could not set server redirection link");
    } else {
        /* iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient. */
        ret = httpd_resp_send(req, "Redirect to error page", HTTPD_RESP_USE_STRLEN);
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(E, "Redirect to error page failed");
    } else {
        if (strstr(req->uri, "generate_204") != NULL) {
            XPLRWIFIWEBSERVER_CONSOLE(W, "Redirecting to home page");
        } else {
            XPLRWIFIWEBSERVER_CONSOLE(W, "Redirecting to error page");
        }
    }

    return ret;
}

static bool xplrHpgThingstreamCredsConfigured(void)
{
    int res[6];
    bool ret;

    res[0] = memcmp(webserver.wsData->pointPerfect.clientId, "n/a", strlen("n/a"));
    res[1] = memcmp(webserver.wsData->pointPerfect.certificate, "n/a", strlen("n/a"));
    res[2] = memcmp(webserver.wsData->pointPerfect.privateKey, "n/a", strlen("n/a"));
    res[3] = memcmp(webserver.wsData->pointPerfect.region, "n/a", strlen("n/a"));
    res[4] = memcmp(webserver.wsData->pointPerfect.rootCa, "n/a", strlen("n/a"));
    res[5] = memcmp(webserver.wsData->pointPerfect.plan, "n/a", strlen("n/a"));

    for (int i = 0; i < 6; i++) {
        if (res[i] == 0) {
            ret = false;
            break;
        } else {
            ret = true;
        }
    }

    return ret;
}

static esp_err_t wsGetHandler(httpd_req_t *req)
{
    httpd_ws_frame_t ws_pkt;
    esp_err_t ret;

    if (req->method == HTTP_GET) {
        ret = ESP_OK;
        XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket handshake done, connection is open");
    } else {
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        /* Set max_len = 0 to get the frame len */
        ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

        if (ret != ESP_OK) {
            XPLRWIFIWEBSERVER_CONSOLE(E, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        } else {
            XPLRWIFIWEBSERVER_CONSOLE(D, "ws frame len is %d", ws_pkt.len);
            if (ws_pkt.len) {
                /* reset websocket data buffer */
                memset(webserver.wsBuf, 0x00, WEBSOCKET_BUFSIZE);
                ws_pkt.payload = webserver.wsBuf;
                /* Set max_len = ws_pkt.len to get the frame payload */
                ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
                if (ret != ESP_OK) {
                    XPLRWIFIWEBSERVER_CONSOLE(E, "httpd_ws_recv_frame failed with %d", ret);
                } else {
                    XPLRWIFIWEBSERVER_CONSOLE(D, "Got packet with message: %s", ws_pkt.payload);
                    ret = wsParseData(req, ws_pkt.payload);
                }
            }
        }
    }

    if (ret != ESP_OK) {
        XPLRWIFIWEBSERVER_CONSOLE(W, "Websocket failed to parse received data");
    }

    return ret;
}

static esp_err_t wsParseData(httpd_req_t *req, uint8_t *data)
{
    char *reqValue;
    cJSON *wsIn = cJSON_Parse((const char *)data);
    xplrWifiWebserverWsReqType_t reqType;
    esp_err_t ret;

    if (cJSON_GetObjectItem(wsIn, "req")) {
        reqValue = cJSON_GetObjectItem(wsIn, "req")->valuestring;
        for (int i = 0; i < XPLR_WEBSERVER_WSREQ_SUPPORTED; i++) {
            if (memcmp(reqValue,
                       "dvcStatus",
                       strlen("dvcStatus")) == 0) {
                reqType = XPLR_WEBSERVER_WSREQ_STATUS;
                break;
            } else if (memcmp(reqValue,
                              "dvcInfo",
                              strlen("dvcInfo")) == 0) {
                reqType = XPLR_WEBSERVER_WSREQ_INFO;
                break;
            } else if (memcmp(reqValue,
                              "dvcReboot",
                              strlen("dvcReboot")) == 0) {
                esp_restart();
                reqType = XPLR_WEBSERVER_WSREQ_REBOOT;
                break;
            } else if (memcmp(reqValue,
                              "dvcErase",
                              strlen("dvcErase")) == 0) {
                xplrWifiStarterDeviceErase();
                reqType = XPLR_WEBSERVER_WSREQ_ERASE_ALL;
                break;
            } else if (memcmp(reqValue,
                              "dvcSsidScan",
                              strlen("dvcSsidScan")) == 0) {
                reqType = XPLR_WEBSERVER_WSREQ_SCAN;
                break;
            } else if (memcmp(reqValue,
                              "dvcWifiSet",
                              strlen("dvcWifiSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "ssid")->valuestring;
                memset(webserver.wsData->wifi.ssid,
                       0x00,
                       XPLR_WIFISTARTER_NVS_SSID_LENGTH_MAX);
                memcpy(webserver.wsData->wifi.ssid, reqValue, strlen(reqValue));
                reqValue = cJSON_GetObjectItem(wsIn, "pwd")->valuestring;
                if (strlen(reqValue) > 0) {
                    memset(webserver.wsData->wifi.password,
                           0x00,
                           XPLR_WIFISTARTER_NVS_PASSWORD_LENGTH_MAX);
                    memcpy(webserver.wsData->wifi.password, reqValue, strlen(reqValue));
                }
                webserver.wsData->wifi.set = true;
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nWi-Fi credentials parsed:\nSSID: %s\nPassword: %s",
                                          webserver.wsData->wifi.ssid,
                                          webserver.wsData->wifi.password);
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveWifi();
                }
                reqType = XPLR_WEBSERVER_WSREQ_WIFI_SET;
                break;
            } else if (memcmp(reqValue,
                              "dvcEraseWifi",
                              strlen("dvcEraseWifi")) == 0) {
                xplrWifiStarterDeviceEraseWifi();
                reqType = XPLR_WEBSERVER_WSREQ_ERASE_WIFI;
                break;
            } else if (memcmp(reqValue,
                              "dvcThingstreamPpIdSet",
                              strlen("dvcThingstreamPpIdSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "id")->valuestring;
                memset(webserver.wsData->pointPerfect.clientId,
                       0x00,
                       XPLR_WIFIWEBSERVER_PPID_SIZE);
                memcpy(webserver.wsData->pointPerfect.clientId, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect client ID parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.clientId,
                                          webserver.wsData->pointPerfect.set);
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(2);
                }
                reqType = XPLR_WEBSERVER_WSREQ_PPID_SET;
                break;
            } else if (memcmp(reqValue,
                              "dvcThingstreamPpRootCaSet",
                              strlen("dvcThingstreamPpRootCaSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "root")->valuestring;
                memset(webserver.wsData->pointPerfect.rootCa,
                       0x00,
                       XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE);
                memcpy(webserver.wsData->pointPerfect.rootCa, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect rootCa parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.rootCa,
                                          webserver.wsData->pointPerfect.set);
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(1);
                }
                reqType = XPLR_WEBSERVER_WSREQ_ROOTCA_SET;
                break;
            }  else if (memcmp(reqValue,
                               "dvcThingstreamPpCertSet",
                               strlen("dvcThingstreamPpCertSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "cert")->valuestring;
                memset(webserver.wsData->pointPerfect.certificate,
                       0x00,
                       XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE);
                memcpy(webserver.wsData->pointPerfect.certificate, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect client cert parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.certificate,
                                          webserver.wsData->pointPerfect.set);
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(3);
                }
                reqType = XPLR_WEBSERVER_WSREQ_PPCERT_SET;
                break;
            }  else if (memcmp(reqValue,
                               "dvcThingstreamPpKeySet",
                               strlen("dvcThingstreamPpKeySet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "key")->valuestring;
                memset(webserver.wsData->pointPerfect.privateKey,
                       0x00,
                       XPLR_WIFIWEBSERVER_CERTIFICATE_MAX_FILE_SIZE);
                memcpy(webserver.wsData->pointPerfect.privateKey, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect client key parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.privateKey,
                                          webserver.wsData->pointPerfect.set);
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(4);
                }
                reqType = XPLR_WEBSERVER_WSREQ_PPKEY_SET;
                break;
            }  else if (memcmp(reqValue,
                               "dvcThingstreamPpRegionSet",
                               strlen("dvcThingstreamPpRegionSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "region")->valuestring;
                memset(webserver.wsData->pointPerfect.region,
                       0x00,
                       XPLR_WIFIWEBSERVER_PPREGION_SIZE);
                memcpy(webserver.wsData->pointPerfect.region, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect region parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.region,
                                          webserver.wsData->pointPerfect.set);
                reqType = XPLR_WEBSERVER_WSREQ_PPREGION_SET;
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(5);
                }
                break;
            }  else if (memcmp(reqValue,
                               "dvcThingstreamPpPlanSet",
                               strlen("dvcThingstreamPpPlanSet")) == 0) {
                reqValue = cJSON_GetObjectItem(wsIn, "plan")->valuestring;
                memset(webserver.wsData->pointPerfect.plan,
                       0x00,
                       XPLR_WIFIWEBSERVER_PPPLAN_SIZE);
                memcpy(webserver.wsData->pointPerfect.plan, reqValue, strlen(reqValue));
                webserver.wsData->pointPerfect.set = xplrHpgThingstreamCredsConfigured();
                XPLRWIFIWEBSERVER_CONSOLE(I, "\nPointPerfect plan parsed:\nID: %s\nCredentials set:%d",
                                          webserver.wsData->pointPerfect.plan,
                                          webserver.wsData->pointPerfect.set);
                reqType = XPLR_WEBSERVER_WSREQ_PPPLAN_SET;
                if (xplrWifiStarterWebserverisConfigured()) {
                    xplrWifiStarterDeviceForceSaveThingstream(6);
                }
                break;
            }  else if (memcmp(reqValue,
                               "dvcEraseThingstream",
                               strlen("dvcEraseThingstream")) == 0) {
                xplrWifiStarterDeviceEraseThingstream();
                reqType = XPLR_WEBSERVER_WSREQ_ERASE_THINGSTREAM;
                break;
            } else if (memcmp(reqValue,
                              "dvcLocation",
                              strlen("dvcLocation")) == 0) {
                reqType = XPLR_WEBSERVER_WSREQ_LOCATION;
                break;
            } else if (memcmp(reqValue,
                              "dvcMessage",
                              strlen("dvcMessage")) == 0) {
                reqType = XPLR_WEBSERVER_WSREQ_MESSAGE;
                break;
            } else {
                reqType = XPLR_WEBSERVER_WSREQ_INVALID;
                break;
            }
        }

        if (reqValue != NULL) {
            //cJSON_free(reqValue); //!heap error if executed
            reqValue = NULL;
        }
        if (wsIn != NULL) {
            cJSON_Delete(wsIn);
        }

        memset(data, 0x00, strlen((char *)data));
        ret = wsServeReq(req, reqType);
    } else {
        XPLRWIFIWEBSERVER_CONSOLE(W, "Websocket msg not of type \"req\"");
        ret = ESP_FAIL;
    }

    return ret;
}

static esp_err_t wsServeReq(httpd_req_t *req, xplrWifiWebserverWsReqType_t type)
{
    cJSON *wsOut;
    char *wsOutBuf;
    cJSON *array;
    cJSON *element;
    httpd_ws_frame_t ws_pkt;
    esp_err_t ret;

    switch (type) {
        case XPLR_WEBSERVER_WSREQ_STATUS:
            XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket device status request received, creating response");
            wsOut = cJSON_CreateObject();
            if (wsOut != NULL) {
                cJSON_AddStringToObject(wsOut, "rsp", "dvcStatus");
                cJSON_AddNumberToObject(wsOut, "wifi", (double)webserver.wsData->diagnostics.connected);
                cJSON_AddNumberToObject(wsOut, "thingstream", (double)webserver.wsData->diagnostics.configured);
                cJSON_AddNumberToObject(wsOut, "gnss", (double)webserver.wsData->diagnostics.ready);
                wsOutBuf = cJSON_Print(wsOut);
                if (wsOutBuf != NULL) {
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                    ws_pkt.payload = (uint8_t *)wsOutBuf;
                    ws_pkt.len = strlen(wsOutBuf);
                    XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket sending data (%db):%s", ws_pkt.len, ws_pkt.payload);
                    ret = httpd_ws_send_frame(req, &ws_pkt);
                } else {
                    XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to create output buffer");
                    ret = ESP_FAIL;
                }

                if (wsOutBuf != NULL) {
                    cJSON_free(wsOutBuf);
                    wsOutBuf = NULL;
                }
            } else {
                XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to create cJson obj");
                ret = ESP_FAIL;
            }

            if (wsOut != NULL) {
                cJSON_Delete(wsOut);
            }
            break;
        case XPLR_WEBSERVER_WSREQ_INFO:
            XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket device info request received, creating response");
            wsOut = cJSON_CreateObject();
            if (wsOut != NULL) {
                cJSON_AddStringToObject(wsOut, "rsp", "dvcInfo");
                if (webserver.wsData->diagnostics.ssid != NULL) {
                    cJSON_AddStringToObject(wsOut, "ssid", webserver.wsData->diagnostics.ssid);
                } else {
                    cJSON_AddStringToObject(wsOut, "ssid", "n/a");
                }

                if (webserver.wsData->diagnostics.ip != NULL) {
                    cJSON_AddStringToObject(wsOut, "ip", webserver.wsData->diagnostics.ip);
                } else {
                    cJSON_AddStringToObject(wsOut, "ip", "n/a");
                }

                if (webserver.wsData->diagnostics.hostname != NULL) {
                    cJSON_AddStringToObject(wsOut, "host", webserver.wsData->diagnostics.hostname);
                } else {
                    cJSON_AddStringToObject(wsOut, "host", "n/a");
                }

                if (webserver.wsData->diagnostics.upTime != NULL) {
                    cJSON_AddStringToObject(wsOut, "uptime", webserver.wsData->diagnostics.upTime);
                } else {
                    cJSON_AddStringToObject(wsOut, "uptime", "n/a");
                }

                if (webserver.wsData->diagnostics.upTime != NULL) {
                    cJSON_AddStringToObject(wsOut, "timeToFix", webserver.wsData->diagnostics.timeToFix);
                } else {
                    cJSON_AddStringToObject(wsOut, "timeToFix", "n/a");
                }

                cJSON_AddStringToObject(wsOut, "mqttTraffic", webserver.wsData->diagnostics.mqttTraffic);
                cJSON_AddNumberToObject(wsOut, "accuracy", (double)webserver.wsData->diagnostics.gnssAccuracy);
                cJSON_AddStringToObject(wsOut, "fwVersion", webserver.wsData->diagnostics.version);
                wsOutBuf = cJSON_Print(wsOut);
                if (wsOutBuf != NULL) {
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                    ws_pkt.payload = (uint8_t *)wsOutBuf;
                    ws_pkt.len = strlen(wsOutBuf);
                    XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket sending data (%db):%s", ws_pkt.len, ws_pkt.payload);
                    ret = httpd_ws_send_frame(req, &ws_pkt);
                } else {
                    XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to create output buffer");
                    ret = ESP_FAIL;
                }

                if (wsOutBuf != NULL) {
                    cJSON_free(wsOutBuf);
                    wsOutBuf = NULL;
                }
            } else {
                XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to create cJson obj");
                ret = ESP_FAIL;
            }

            if (wsOut != NULL) {
                cJSON_Delete(wsOut);
            }
            break;
        case XPLR_WEBSERVER_WSREQ_SCAN:
            XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket device SSID scan request received, creating response");
            ret = xplrWifiStarterScanNetwork(&webserver.wsData->wifiScan);
            if (ret != ESP_OK) {
                XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to scan network");
            } else {
                wsOut = cJSON_CreateObject();
                if (wsOut != NULL) {
                    cJSON_AddStringToObject(wsOut, "rsp", "dvcSsidScan");
                    array = cJSON_AddArrayToObject(wsOut, "scan");
                    for (int i = 0; i < webserver.wsData->wifiScan.found; i++) {
                        element = cJSON_CreateString(&webserver.wsData->wifiScan.name[i][0]);
                        cJSON_AddItemToArray(array, element);
                        //element = cJSON_CreateNumber(webserver.wsData->wifiScan.rssi[i]);
                        //cJSON_AddItemToArray(array, element);
                    }
                    wsOutBuf = cJSON_Print(wsOut);
                    if (wsOutBuf != NULL) {
                        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                        ws_pkt.payload = (uint8_t *)wsOutBuf;
                        ws_pkt.len = strlen(wsOutBuf);
                        XPLRWIFIWEBSERVER_CONSOLE(D, "Websocket sending data (%db):%s", ws_pkt.len, ws_pkt.payload);
                        ret = httpd_ws_send_frame(req, &ws_pkt);
                    } else {
                        XPLRWIFIWEBSERVER_CONSOLE(W, "Failed to create output buffer");
                        ret = ESP_FAIL;
                    }

                    if (wsOutBuf != NULL) {
                        cJSON_free(wsOutBuf);
                        wsOutBuf = NULL;
                    }
                }
            }
            break;
        case XPLR_WEBSERVER_WSREQ_LOCATION:
            if ((locationFrame.len > 0) && (locationFrame.payload != NULL)) {
                XPLRWIFIWEBSERVER_CONSOLE(D,
                                          "Websocket updating location data (%db):%s",
                                          locationFrame.len,
                                          locationFrame.payload);
                ret = httpd_ws_send_frame(req, &locationFrame);
            } else {
                ret = ESP_OK;
            }
            break;
        case XPLR_WEBSERVER_WSREQ_MESSAGE:
            if ((messageFrame.len > 0) && (messageFrame.payload != NULL)) {
                XPLRWIFIWEBSERVER_CONSOLE(D,
                                          "Websocket updating message data (%db):%s",
                                          messageFrame.len,
                                          messageFrame.payload);
                ret = httpd_ws_send_frame(req, &messageFrame);
            } else {
                ret = ESP_OK;
            }
            break;
        case XPLR_WEBSERVER_WSREQ_WIFI_SET:
        case XPLR_WEBSERVER_WSREQ_ROOTCA_SET:
        case XPLR_WEBSERVER_WSREQ_PPID_SET:
        case XPLR_WEBSERVER_WSREQ_PPCERT_SET:
        case XPLR_WEBSERVER_WSREQ_PPKEY_SET:
        case XPLR_WEBSERVER_WSREQ_PPREGION_SET:
        case XPLR_WEBSERVER_WSREQ_PPPLAN_SET:
        case XPLR_WEBSERVER_WSREQ_REBOOT:
        case XPLR_WEBSERVER_WSREQ_ERASE_ALL:
        case XPLR_WEBSERVER_WSREQ_ERASE_WIFI:
        case XPLR_WEBSERVER_WSREQ_ERASE_THINGSTREAM:
            ret = ESP_OK;
            break;

        default: //XPLR_WEBSERVER_WSREQ_INVALID
            ret = ESP_FAIL;
            XPLRWIFIWEBSERVER_CONSOLE(W, "Websocket msg \"req\" invalid");
            break;
    }

    return ret;
}
