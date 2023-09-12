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

/*
 * An example for Zero Touch Provisioning (ZTP).
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * makes an HTTPS POST request to Thingstream's ZTP credentials URL using xplr_ztp component,
 * gets different data such as key-certificate, MQTT EU-topics, decryption keys by parsing the HTTPS reply parsed by using xplr_ztp_json_parser component
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "xplr_wifi_starter.h"
#include "xplr_thingstream.h"
#include "xplr_ztp.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define  APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define  APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)\
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/*
 * Simple macros used to set buffer sizes in bytes
 */
#define KIB                         (1024U)
#define APP_ZTP_PAYLOAD_BUF_SIZE    ((10U) * (KIB))
#define APP_KEYCERT_PARSE_BUF_SIZE  ((2U) * (KIB))

/*
 * Button for shutting down device
 */
#define APP_DEVICE_OFF_MODE_BTN     (BOARD_IO_BTN1)

/*
 * Device off press duration in sec
 */
#define APP_DEVICE_OFF_MODE_TRIGGER (3U)

#define APP_TOPICS_ARRAY_MAX_SIZE   (25U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * Using EU as a region
 */
static xplr_thingstream_pp_region_t ppRegion = XPLR_THINGSTREAM_PP_REGION_EU;

/**
 * Required data to make a POST request to Thingstream
 * The data are taken from KConfig or you can overwrite the
 * values as needed here.
 */
xplr_thingstream_t thingstreamSettings;

static const char *urlAwsRootCa = CONFIG_XPLR_AWS_ROOTCA_URL;
static const char *ztpToken = CONFIG_XPLR_TS_PP_ZTP_TOKEN;

/**
 * ZTP data we got from post
 */
static char payload[APP_ZTP_PAYLOAD_BUF_SIZE];
static xplrZtpData_t ztpData = {
    .payload = payload,
    .payloadLength = APP_ZTP_PAYLOAD_BUF_SIZE
};

/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the
 * values as needed here.
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA,
    .webserver = false
};

static uint32_t bufferStackPointer = 0;

/**
 * Error return types
 */
xplrWifiStarterError_t wifistarterErr;
/*INDENT-OFF*/
/**
 * Static log configuration struct and variables
 */
#if (1 == APP_SD_LOGGING_ENABLED)
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_LARGE];
static char appLogFilename[] = "/APPLOG.TXT";               /**< Follow the same format if changing the filename*/
static char errorLogFilename[] = "/ERRORLOG.TXT";           /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize = 100;                        /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType = XPLR_SIZE_MB;    /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif
/*INDENT-ON*/
/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t appInitBoard(void);
static void appInitWiFi(void);
static esp_err_t appGetRootCa(void);
static void appApplyThingstreamCreds(void);
static void appInitLog(void);
static void appDeInitLog(void);
static void appHaltExecution(void);
static void appDeviceOffTask(void *arg);
/* ---------------------------------------------------------------
 * CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
static esp_err_t httpClientEventCB(esp_http_client_event_handle_t evt);

void app_main(void)
{
    bool gotZtp = false;
    xplrWifiStarterError_t wifistarterErr;
    esp_err_t ret;
    xplr_thingstream_error_t tsErr;

    appInitBoard();
    appInitWiFi();

    while (1) {
        wifistarterErr = xplrWifiStarterFsm();
        if (wifistarterErr == XPLR_WIFISTARTER_ERROR) {
            APP_CONSOLE(E, "xplrWifiStarterFsm returned ERROR!");
        }

        switch (xplrWifiStarterGetCurrentFsmState()) {
            case XPLR_WIFISTARTER_STATE_CONNECT_OK:
                if (!gotZtp) {
                    APP_CONSOLE(I, "Performing HTTPS POST request.");
                    thingstreamSettings.connType = XPLR_THINGSTREAM_PP_CONN_WIFI;
                    tsErr = xplrThingstreamInit(ztpToken, &thingstreamSettings);
                    if (tsErr == XPLR_THINGSTREAM_OK) {
                        ret = appGetRootCa();
                        if (ret == ESP_OK) {
                            ret = xplrZtpGetPayloadWifi(&thingstreamSettings,
                                                        &ztpData);
                            if (ret != ESP_OK) {
                                APP_CONSOLE(E, "Performing HTTPS POST failed!");
                            } else {
                                if (ztpData.httpReturnCode == HttpStatus_Ok) {
                                    appApplyThingstreamCreds();
                                    xplrWifiStarterDisconnect();
                                } else {
                                    APP_CONSOLE(W, "HTTPS request returned code: %d", ztpData.httpReturnCode);
                                }
                            }
                        } else {
                            APP_CONSOLE(E, "Error in fetching Root CA certificate");
                        }
                    } else {
                        APP_CONSOLE(E, "error in xplr_thingstream_init");
                    }
                    gotZtp = true;
                }
                break;

            case XPLR_WIFISTARTER_STATE_UNKNOWN:
            case XPLR_WIFISTARTER_STATE_TIMEOUT:
            case XPLR_WIFISTARTER_STATE_ERROR:
                APP_CONSOLE(W, "Major error encountered. Will exit!");
                appHaltExecution();
                break;

            default:
                break;

        }

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_DISCONNECT_OK) {
            break;
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    APP_CONSOLE(I, "ALL DONE!!!");
    appDeInitLog();
}


/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t ret;

    appInitLog();
    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_DEVICE_OFF_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        ret = gpio_config(&io_conf);
    }

    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        if (xTaskCreate(appDeviceOffTask, "deviceOffTask", 2 * 2048, NULL, 10, NULL) == pdPASS) {
            APP_CONSOLE(D, "Boot0 pin configured as button OK");
            APP_CONSOLE(D, "Board Initialized");
        } else {
            APP_CONSOLE(D, "Failed to start deviceOffTask task");
            APP_CONSOLE(E, "Board initialization failed!");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    esp_err_t ret;
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    ret = xplrWifiStarterInitConnection(&wifiOptions);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
    }
}

/**
 * HTTP GET request to fetch the Root CA certificate
*/
static esp_err_t appGetRootCa(void)
{
    int UNUSED_PARAM(len);
    esp_err_t ret;
    esp_http_client_handle_t client = NULL;
    char rootCa[APP_KEYCERT_PARSE_BUF_SIZE];
    xplrZtpData_t userData = {
        .payload = rootCa,
        .payloadLength = APP_KEYCERT_PARSE_BUF_SIZE
    };

    //Configure http client
    esp_http_client_config_t clientConfig = {
        .url = urlAwsRootCa,
        .method = HTTP_METHOD_GET,
        .event_handler = httpClientEventCB,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = &userData
    };

    client = esp_http_client_init(&clientConfig);
    if (client != NULL) {
        ret = esp_http_client_set_header(client, "Accept", "text/html");
        if (ret == ESP_OK) {
            ret = esp_http_client_perform(client); //Blocking no need for retries or while loop
            if (ret == ESP_OK) {
                userData.httpReturnCode = esp_http_client_get_status_code(client);
                if (userData.httpReturnCode == 200) {
                    len = esp_http_client_get_content_length(client);
                    APP_CONSOLE(I,
                                "HTTPS GET request OK: code [%d] - payload size [%d].",
                                userData.httpReturnCode,
                                len);
                } else {
                    APP_CONSOLE(E, "HTTPS GET request failed with code [%d]", userData.httpReturnCode);
                }
            } else {
                APP_CONSOLE(E, "Error in GET request");
            }
        } else {
            APP_CONSOLE(E, "Failed to set HTTP headers");
        }
        esp_http_client_cleanup(client);
    } else {
        APP_CONSOLE(E, "Could not initiate HTTP client");
        ret = ESP_FAIL;
    }

    if (rootCa != NULL) {
        memcpy(thingstreamSettings.server.rootCa, rootCa, APP_KEYCERT_PARSE_BUF_SIZE);
    }
    return ret;
}

/**
 * Function responsible to parse the ZTP payload data and populate the xplr_thingstream_t struct
*/
static void appApplyThingstreamCreds(void)
{
    xplr_thingstream_error_t tsErr;

    tsErr = xplrThingstreamPpConfig(ztpData.payload, ppRegion, &thingstreamSettings);
    if (tsErr != XPLR_THINGSTREAM_OK) {
        APP_CONSOLE(E, "Error in ZTP payload parsing");
        appHaltExecution();
    } else {
        APP_CONSOLE(I, "ZTP Payload parsed successfully");
    }
    /*INDENT-ON*/
}

/**
 * Function responsible to initialize the logging service
*/
static void appInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLog_error_t err = xplrLogInit(&errorLog,
                                      XPLR_LOG_DEVICE_ERROR,
                                      errorLogFilename,
                                      logFileMaxSize,
                                      logFileMaxSizeType);
    if (err == XPLR_LOG_OK) {
        errorLog.logEnable = true;
        err = xplrLogInit(&appLog,
                          XPLR_LOG_DEVICE_INFO,
                          appLogFilename,
                          logFileMaxSize,
                          logFileMaxSizeType);
    }
    if (err == XPLR_LOG_OK) {
        appLog.logEnable = true;
    } else {
        APP_CONSOLE(E, "Error initializing logging...");
    }
#endif
}

/**
 * CALLBACK FUNCTION
*/
static esp_err_t httpClientEventCB(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            APP_CONSOLE(D, "HTTP_EVENT_ON_CONNECTED!");
            break;

        case HTTP_EVENT_HEADERS_SENT:
            break;

        case HTTP_EVENT_ON_HEADER:
            break;

        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                xplrZtpData_t *tempData = (xplrZtpData_t *)evt->user_data;
                if (bufferStackPointer < tempData->payloadLength) {
                    memcpy(tempData->payload + bufferStackPointer, evt->data, evt->data_len);
                    bufferStackPointer += evt->data_len;
                    tempData->payload[bufferStackPointer] = 0;
                } else {
                    APP_CONSOLE(E, "Payload buffer not big enough. Could not copy all data from HTTP!");
                }
            }
            break;

        case HTTP_EVENT_ERROR:
            /*
             * How does the following ESP_LOG work?
             * We are currently receiving data from the internet.
             * We are not sure if the payload is null terminated.
             * We need to define the length of the string to print.
             * The following "%.*s" --> expands to: print string s as many chars as dataLength.
             * It's the equivalent of writing
             *      ESP_LOGE("HTTP_EVENT_ERROR: %.6f", float);
             * which is more intuitive to understand; we define a constant number of decimal places.
             * In our case the number of characters is defined on runtime by ".*"
             * You can see how it works on:
             * https://embeddedartistry.com/blog/2017/07/05/printf-a-limited-number-of-characters-from-a-string/
             */
            APP_CONSOLE(E, "HTTP_EVENT_ERROR: %.*s", evt->data_len, (char *)evt->data);
            break;

        case HTTP_EVENT_ON_FINISH:
            APP_CONSOLE(D, "HTTP_EVENT_ON_FINISH");
            break;

        default:
            break;
    }

    return ESP_OK;
}

/**
 * Function responsible to terminate/deinitialize the logging service
*/
static void appDeInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
#endif
}

/*
 * Function to halt app execution
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appDeviceOffTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;

            if (btnPressDuration >= APP_DEVICE_OFF_MODE_TRIGGER) {
                APP_CONSOLE(W, "Device OFF triggered");
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
                btnPressDuration = 0;
                appHaltExecution();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}