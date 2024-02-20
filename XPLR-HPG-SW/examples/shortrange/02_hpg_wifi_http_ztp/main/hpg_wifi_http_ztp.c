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
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_PRINT_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...) XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
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

/*
 * Option to enable/disable the hot plug functionality for the SD card
 */
#define APP_SD_HOT_PLUG_FUNCTIONALITY   (1U) & APP_SD_LOGGING_ENABLED


/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog          : 1;
        uint8_t nvsLog          : 1;
        uint8_t ztpLog          : 1;
        uint8_t thingstreamLog  : 1;
        uint8_t wifistarterLog  : 1;
    } singleLogOpts;
    uint8_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          ztpLogIndex;
    int8_t          thingstreamLogIndex;
    int8_t          nvsLogIndex;
    int8_t          wifiStarterLogIndex;
} appLog_t;

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

static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0,
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .ztpLogIndex = -1,
    .thingstreamLogIndex = -1,
    .wifiStarterLogIndex = -1
};

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */
#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
static void appDeInitLogging(void);
#endif
static esp_err_t appInitBoard(void);
static void appInitWiFi(void);
static esp_err_t appGetRootCa(void);
static void appApplyThingstreamCreds(void);
static void appHaltExecution(void);
static void appDeviceOffTask(void *arg);
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appCardDetectTask(void *arg);
#endif
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
#if (APP_SD_LOGGING_ENABLED == 1)
    ret = appInitLogging();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif
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
                        XPLR_CI_CONSOLE(202, "OK");
                        ret = appGetRootCa();
                        if (ret == ESP_OK) {
                            XPLR_CI_CONSOLE(203, "OK");
                            ret = xplrZtpGetPayloadWifi(&thingstreamSettings,
                                                        &ztpData);
                            if (ret != ESP_OK) {
                                APP_CONSOLE(E, "Performing HTTPS POST failed!");
                                XPLR_CI_CONSOLE(204, "ERROR");
                            } else {
                                if (ztpData.httpReturnCode == HttpStatus_Ok) {
                                    XPLR_CI_CONSOLE(204, "OK");
                                    appApplyThingstreamCreds();
                                    ret = xplrWifiStarterDisconnect();
                                    if (ret == ESP_OK) {
                                        gotZtp = true;
                                    }
                                } else {
                                    APP_CONSOLE(W, "HTTPS request returned code: %d", ztpData.httpReturnCode);
                                    XPLR_CI_CONSOLE(204, "ERROR");
                                }
                            }
                        } else {
                            APP_CONSOLE(E, "Error in fetching Root CA certificate");
                            XPLR_CI_CONSOLE(203, "ERROR");
                        }
                    } else {
                        APP_CONSOLE(E, "error in xplr_thingstream_init");
                        XPLR_CI_CONSOLE(202, "ERROR");
                    }
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

#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif
    APP_CONSOLE(I, "ALL DONE!!!");
}

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;

    /* Configure the SD card */
    sdErr = xplrSdConfigDefaults();
    if (sdErr != XPLR_SD_OK) {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = ESP_FAIL;
    } else {
        /* Create the card detect task */
        sdErr = xplrSdStartCardDetectTask();
        /* A time window so that the card gets detected*/
        vTaskDelay(pdMS_TO_TICKS(50));
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Failed to start the card detect task");
            ret = ESP_FAIL;
        } else {
            /* Initialize the SD card */
            sdErr = xplrSdInit();
            if (sdErr != XPLR_SD_OK) {
                APP_CONSOLE(E, "Failed to initialize the SD card");
                ret = ESP_FAIL;
            } else {
                APP_CONSOLE(D, "SD card initialized");
                ret = ESP_OK;
            }
        }
    }

    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                "main_app.log",
                                                XPLRLOG_FILE_SIZE_INTERVAL,
                                                XPLRLOG_NEW_FILE_ON_BOOT);
            if (appLogCfg.appLogIndex > 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.nvsLog == 1) {
            appLogCfg.nvsLogIndex = xplrNvsInitLogModule(NULL);
            if (appLogCfg.nvsLogIndex > 0) {
                APP_CONSOLE(D, "NVS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.ztpLog == 1) {
            appLogCfg.ztpLogIndex = xplrZtpInitLogModule(NULL);
            if (appLogCfg.ztpLogIndex > 0) {
                APP_CONSOLE(D, "ZTP logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.thingstreamLog == 1) {
            appLogCfg.thingstreamLogIndex = xplrThingstreamInitLogModule(NULL);
            if (appLogCfg.thingstreamLogIndex > 0) {
                APP_CONSOLE(D, "Thingstream logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.wifistarterLog == 1) {
            appLogCfg.wifiStarterLogIndex = xplrWifiStarterInitLogModule(NULL);
            if (appLogCfg.wifiStarterLogIndex > 0) {
                APP_CONSOLE(D, "WiFi Starter logging instance initialized");
            }
        }
    }

    return ret;
}

static void appDeInitLogging(void)
{
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr = XPLR_SD_ERROR;

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    vTaskDelete(cardDetectTaskHandler);
#endif
    logErr = xplrLogDisableAll();
    if (logErr != XPLR_LOG_OK) {
        APP_CONSOLE(E, "Error disabling logging");
    } else {
        logErr = xplrLogDeInitAll();
        if (logErr != XPLR_LOG_OK) {
            APP_CONSOLE(E, "Error de-initializing logging");
        } else {
            //Do nothing
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdStopCardDetectTask();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error stopping the the SD card detect task");
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdDeInit();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error de-initializing the SD card");
        }
    } else {
        //Do nothing
    }

    if (logErr == XPLR_LOG_OK && sdErr == XPLR_SD_OK) {
        APP_CONSOLE(I, "Logging service de-initialized successfully");
    }
}
#endif
/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t ret;

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

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    if (xTaskCreate(appCardDetectTask,
                    "hotPlugTask",
                    4 * 1024,
                    NULL,
                    20,
                    &cardDetectTaskHandler) == pdPASS) {
        APP_CONSOLE(D, "Hot plug for SD card OK");
    } else {
        APP_CONSOLE(W, "Hot plug for SD card failed");
    }
#endif
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
        XPLR_CI_CONSOLE(201, "ERROR");
    } else {
        XPLR_CI_CONSOLE(201, "OK");
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
        XPLR_CI_CONSOLE(205, "ERROR");
        appHaltExecution();
    } else {
        APP_CONSOLE(I, "ZTP Payload parsed successfully");
        XPLR_CI_CONSOLE(205, "OK");
    }
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

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appCardDetectTask(void *arg)
{
    bool prvState = xplrSdIsCardOn();
    bool currState;
    esp_err_t espErr;

    for (;;) {
        currState = xplrSdIsCardOn();

        /* Check if state has changed */
        if (currState ^ prvState) {
            if (currState) {
                if (!xplrSdIsCardInit()) {
                    espErr = appInitLogging();
                    if (espErr == ESP_OK) {
                        APP_CONSOLE(I, "Logging is enabled!");
                    } else {
                        APP_CONSOLE(E, "Failed to enable logging");
                    }
                }
                /* Enable all log instances (the ones enabled during configuration) */
                if (xplrLogEnableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is re-enabled!");
                } else {
                    APP_CONSOLE(E, "Failed to re-enable logging");
                }
            } else {
                if (xplrSdIsCardInit()) {
                    xplrSdDeInit();
                }
                if (xplrLogDisableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is disabled!");
                } else {
                    APP_CONSOLE(E, "Failed to disable logging");
                }
            }
        } else {
            // Do nothing
        }
        prvState = currState;
        /* Window for other tasks to run */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif