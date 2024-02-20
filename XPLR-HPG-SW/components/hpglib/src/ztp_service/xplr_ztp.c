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

#include <stdio.h>
#include <string.h>
#include "esp_http_client.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "xplr_ztp.h"
#include "xplr_http_client.h"
#include "xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRZTP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRZTP_LOG_ACTIVE))
#define XPLRZTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgZtp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRZTP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRZTP_LOG_ACTIVE)
#define XPLRZTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgZtp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRZTP_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRZTP_LOG_ACTIVE)
#define XPLRZTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgZtp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRZTP_CONSOLE(message, ...) do{} while(0)
#endif
/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * --------------------------------------------------------------- */
static uint32_t bufferStackPointer = 0;
static size_t max_buffer_size;
static xplrCell_http_client_t httpCellClient;
static xplrCell_http_session_t httpSessionCell;
static esp_http_client_handle_t httpWifiClient;
static int8_t logIndex = -1;
/* ----------------------------------------------------------------
 * CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t httpWifiCallback(esp_http_client_event_handle_t evt);
static void httpCellCallback(uDeviceHandle_t devHandle,
                             int32_t statusCodeOrError,
                             size_t responseSize,
                             void *pResponseCallbackParam);

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * WIFI
*/
static esp_err_t ztpWifiSetHeaders(esp_http_client_handle_t httpWifiClient);
static esp_err_t ztpWifiSetPostData(char *post_data,
                                    size_t maxLen,
                                    xplr_thingstream_t *thingstream,
                                    esp_http_client_handle_t httpWifiClient);
static esp_err_t ztpWifiPostMsg(esp_http_client_handle_t httpWifiClient,
                                xplrZtpData_t *ztpData);
static void ztpWifiHttpCleanup(esp_http_client_handle_t httpWifiClient);
static void ztpWifiPopulateBuffer(char *payload, char *source, uint16_t length);
/**
 * CELL
*/
static void ztpCellClientConfig(xplrCell_http_client_t *httpCellClient,
                                xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData,
                                const char *rootCaName);
static esp_err_t ztpCellHttpConnect(xplrCom_cell_config_t *cellConfig,
                                    xplrCell_http_client_t *httpCellClient);
static esp_err_t ztpCellPostMsg(xplrCell_http_client_t *httpCellClient,
                                xplrZtpData_t *ztpData,
                                xplrCom_cell_config_t *cellConfig);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

esp_err_t xplrZtpGetPayloadWifi(xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData)
{
    esp_err_t ret;
    char post_data[150] = {0};
    // Setup HTTP client
    esp_http_client_config_t config_post = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .event_handler = httpWifiCallback,
        .cert_pem = thingstream->server.rootCa,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = ztpData->payload
    };

    strcat(thingstream->server.serverUrl, thingstream->pointPerfect.urlPath);
    config_post.url = thingstream->server.serverUrl;
    max_buffer_size = ztpData->payloadLength;

    /* Null pointer check*/
    if (ztpData->payload == NULL ||
        ztpData == NULL ||
        thingstream == NULL ||
        thingstream->server.ppToken == NULL ||
        thingstream->server.deviceId == NULL) {
        XPLRZTP_CONSOLE(E, "NULL pointer detected!");
        ret = ESP_FAIL;
    } else {
        XPLRZTP_CONSOLE(D, "POST URL: %s", thingstream->server.serverUrl);
        //Initialize HTTP Client
        httpWifiClient = esp_http_client_init(&config_post);
        if (httpWifiClient == NULL) {
            XPLRZTP_CONSOLE(E, "HTTP client creation failed!");
            ret = ESP_FAIL;
        } else {
            //Set HTTP Headers
            ret = ztpWifiSetHeaders(httpWifiClient);
            if (ret != ESP_OK) {
                XPLRZTP_CONSOLE(E, "Setting POST headers failed!");
            } else {
                //Create ZTP message
                ret = ztpWifiSetPostData(post_data, 150, thingstream, httpWifiClient);
                if (ret != ESP_OK) {
                    XPLRZTP_CONSOLE(E, "Setting POST message failed!");
                } else {
                    //HTTP Post
                    ret = ztpWifiPostMsg(httpWifiClient, ztpData);
                }
            }
        }
    }
    return ret;
}

esp_err_t xplrZtpGetPayloadCell(const char *rootCaName,
                                xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData,
                                xplrCom_cell_config_t *cellConfig)
{
    esp_err_t ret;
    xplr_thingstream_error_t tsErr;
    int64_t timeNow, startTime;

    if (rootCaName == NULL ||
        thingstream == NULL ||
        ztpData == NULL ||
        cellConfig == NULL) {
        ret = ESP_FAIL;
        XPLRZTP_CONSOLE(E, "Null pointer! Cannot perform ZTP!");
    } else {
        //Config HTTP Client
        ztpCellClientConfig(&httpCellClient, thingstream, ztpData, rootCaName);
        //Connect to server
        ret = ztpCellHttpConnect(cellConfig, &httpCellClient);
        //Create ZTP message for POST
        if (ret == ESP_OK) {
            tsErr = xplrThingstreamApiMsgCreate(XPLR_THINGSTREAM_API_LOCATION_ZTP,
                                                ztpData->payload,
                                                &httpCellClient.session->data.bufferSizeOut,
                                                thingstream);
            if (tsErr == XPLR_THINGSTREAM_OK) {

                //Perform HTTP POST
                ret = ztpCellPostMsg(&httpCellClient, ztpData, cellConfig);
                //Get payload from the request (triggered by callback)
                startTime = esp_timer_get_time();
                timeNow = startTime;
                while (!httpCellClient.session->requestPending &&
                       (timeNow - startTime) <= XPLR_ZTP_HTTP_TIMEOUT_MS) {
                    // wait until a response to the POST request arrives
                    timeNow = esp_timer_get_time();
                }
                if ((timeNow - startTime) > XPLR_ZTP_HTTP_TIMEOUT_MS) {
                    XPLRZTP_CONSOLE(E, "HTTP POST timeout!");
                    ret = ESP_ERR_TIMEOUT;
                } else {
                    ret = ESP_OK;
                }
            } else {
                XPLRZTP_CONSOLE(E, "Could not create POST message for ZTP");
                ret = ESP_FAIL;
            }

        } else {
            XPLRZTP_CONSOLE(E, "Error in http client setup");
        }
        //Cleanup http client
        xplrCellHttpDisconnect(cellConfig->profileIndex, httpCellClient.id);
    }
    //Return OK or fail (esp_err_t)
    return ret;
}

int8_t xplrZtpInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_ZTP_DEFAULT_FILENAME,
                                   XPLRLOG_FILE_SIZE_INTERVAL,
                                   XPLRLOG_NEW_FILE_ON_BOOT);
        } else {
            /* logCfg contains the instance settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   logCfg->filename,
                                   logCfg->sizeInterval,
                                   logCfg->erasePrev);
        }
        ret = logIndex;
    } else {
        /* logIndex is positive so logging has been initialized before */
        logErr = xplrLogEnable(logIndex);
        if (logErr != XPLR_LOG_OK) {
            ret = -1;
        } else {
            ret = logIndex;
        }
    }

    return ret;
}

esp_err_t xplrZtpStopLogModule(void)
{
    esp_err_t ret;
    xplrLog_error_t logErr;

    logErr = xplrLogDisable(logIndex);
    if (logErr != XPLR_LOG_OK) {
        ret = ESP_FAIL;
    } else {
        ret = ESP_OK;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/* -------------------------------------------
 * WIFI
 * ------------------------------------------*/

/*
 * Sets the header Types and Data.
 * Check Thingstream and Swagger docs for more information.
 */
static esp_err_t ztpWifiSetHeaders(esp_http_client_handle_t client)
{
    esp_err_t ret;

    ret = esp_http_client_set_header(httpWifiClient,
                                     HTTP_POST_HEADER_TYPE_CONTENT,
                                     HTTP_POST_HEADER_TYPE_DATA_CONTENT);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed setting first header part!");
    } else {
        ret = esp_http_client_set_header(httpWifiClient,
                                         HTTP_POST_HEADER_TYPE_JSON,
                                         HTTP_POST_HEADER_TYPE_DATA_JSON);
        if (ret != ESP_OK) {
            XPLRZTP_CONSOLE(E, "Failed setting second header part!");
        } else {
            XPLRZTP_CONSOLE(D, "Successfully set headers for HTTP POST");
        }
    }
    return ret;
}

/*
 * Sets the POST data body.
 * Check Thingstream and Swagger docs for more information.
 */
static esp_err_t ztpWifiSetPostData(char *post_data,
                                    size_t maxLen,
                                    xplr_thingstream_t *thingstream,
                                    esp_http_client_handle_t httpWifiClient)
{
    esp_err_t ret;
    xplr_thingstream_error_t tsErr;

    if (thingstream == NULL ||
        thingstream->server.ppToken == NULL ||
        thingstream->server.deviceId == NULL) {
        XPLRZTP_CONSOLE(E, "NULL pointer detected!");
        ret = ESP_FAIL;
    } else {
        tsErr = xplrThingstreamApiMsgCreate(XPLR_THINGSTREAM_API_LOCATION_ZTP,
                                            post_data,
                                            &maxLen,
                                            thingstream);
        if (tsErr != XPLR_THINGSTREAM_OK) {
            XPLRZTP_CONSOLE(E, "Failed to create the ZTP Post message");
            ret = ESP_FAIL;
        } else {
            ret = esp_http_client_set_post_field(httpWifiClient, post_data, strlen(post_data));
            if (ret != ESP_OK) {
                XPLRZTP_CONSOLE(E, "Failed setting POST field");
            } else {
                XPLRZTP_CONSOLE(D, "Successfully set POST field");
            }
        }
    }

    return ret;
}

/**
 * Function responsible of the HTTP POST for WiFi ZTP
*/
static esp_err_t ztpWifiPostMsg(esp_http_client_handle_t httpWifiClient,
                                xplrZtpData_t *ztpData)
{
    esp_err_t ret;
    int length;
    // Blocking function no need for while loop or termination flag
    ret = esp_http_client_perform(httpWifiClient);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Error in HTTP POST");
        ztpWifiHttpCleanup(httpWifiClient);
    } else {
        ztpData->httpReturnCode = esp_http_client_get_status_code(httpWifiClient);
        length = esp_http_client_get_content_length(httpWifiClient);
        if (length > ztpData->payloadLength - 1) {
            ztpData->payload[0] = 0;
            XPLRZTP_CONSOLE(E, "HTTPS POST payload larger [%d] than buffer [%d]!",
                            length,
                            ztpData->payloadLength - 1);
            ztpWifiHttpCleanup(httpWifiClient);
        } else {
            //Do nothing
        }
    }

    if (ztpData->httpReturnCode == HttpStatus_Ok) {
        XPLRZTP_CONSOLE(D, "HTTPS POST request OK.");
        ret = ESP_OK;
    } else {
        XPLRZTP_CONSOLE(D, "HTTPS POST request failed: Code [%d]", ztpData->httpReturnCode);
        ret = ESP_FAIL;
    }

    XPLRZTP_CONSOLE(D, "HTTPS POST: Return Code - %d", ztpData->httpReturnCode);
    ztpWifiHttpCleanup(httpWifiClient);
    return ret;
}

/**
 * Function responsible to cleanup the HTTP WiFi client
*/
static void ztpWifiHttpCleanup(esp_http_client_handle_t httpWifiClient)
{
    esp_err_t err;
    err = esp_http_client_cleanup(httpWifiClient);
    if (err != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Client cleanup failed!");
    } else {
        XPLRZTP_CONSOLE(D, "Client cleanup succeeded!");
    }
}

/*
 * Populates the buffer from the HTTP EVENTS to be sent back as a result.
 */
static void ztpWifiPopulateBuffer(char *payload, char *source, uint16_t length)
{
    if (bufferStackPointer < max_buffer_size) {
        memcpy(payload + bufferStackPointer, source, length);
        bufferStackPointer += length;
        payload[bufferStackPointer] = 0;
    } else {
        XPLRZTP_CONSOLE(E, "Payload buffer not big enough. Could not copy all data from HTTP!");
    }
}

/* -------------------------------------------
 * CELL
 * ------------------------------------------*/

/**
 * Function that configures the HTTP client for the ZTP POST request
*/
static void ztpCellClientConfig(xplrCell_http_client_t *httpCellClient,
                                xplr_thingstream_t *thingstream,
                                xplrZtpData_t *ztpData,
                                const char *rootCaName)
{
    //Create and config a cell http client
    httpCellClient->credentials.token = thingstream->server.ppToken;
    httpCellClient->credentials.rootCa = thingstream->server.rootCa;
    httpCellClient->credentials.rootCaName = rootCaName;
    httpCellClient->session = &httpSessionCell;
    httpCellClient->session->data.buffer = ztpData->payload;
    httpCellClient->session->data.bufferSizeOut = ztpData->payloadLength;
    httpCellClient->session->data.path = thingstream->pointPerfect.urlPath;
    memcpy(httpCellClient->session->data.contentType, HTTP_POST_HEADER_TYPE_DATA_CONTENT, 17);
    httpCellClient->session->data.bufferSizeIn = ztpData->payloadLength;
    httpCellClient->responseCb = &httpCellCallback;
    //Config HTTP server settings
    httpCellClient->settings.errorOnBusy = false;
    httpCellClient->settings.timeoutSeconds = 30;
    httpCellClient->settings.serverAddress = thingstream->server.serverUrl;
    httpCellClient->settings.registerMethod = XPLR_CELL_HTTP_CERT_METHOD_ROOTCA;
    httpCellClient->settings.async = true;
}

/**
 * Function that handles the HTTP connection to a server (via Cellular)
*/
static esp_err_t ztpCellHttpConnect(xplrCom_cell_config_t *cellConfig,
                                    xplrCell_http_client_t *httpCellClient)
{
    esp_err_t ret;
    xplrCell_http_error_t err;
    xplrCom_cell_connect_t comState;

    comState = xplrComCellFsmConnectGetState(cellConfig->profileIndex);
    if (comState == XPLR_COM_CELL_CONNECTED) {
        err = xplrCellHttpConnect(cellConfig->profileIndex, httpCellClient->id, httpCellClient);
        if (err == XPLR_CELL_HTTP_ERROR) {
            XPLRZTP_CONSOLE(E, "Device %d, client %d (http) failed to Connect.\n",
                            cellConfig->profileIndex,
                            httpCellClient->id);
            ret = ESP_FAIL;
        } else {
            ret = ESP_OK;
            XPLRZTP_CONSOLE(D, "Device %d, client %d (http) connected ok.\n",
                            cellConfig->profileIndex,
                            httpCellClient->id);
        }
    } else {
        XPLRZTP_CONSOLE(E, "Could not get cell module's state from FSM!");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * Function that handles the POST request for ZTP (via Cellular)
*/
static esp_err_t ztpCellPostMsg(xplrCell_http_client_t *httpCellClient,
                                xplrZtpData_t *ztpData,
                                xplrCom_cell_config_t *cellConfig)
{
    esp_err_t ret;
    xplrCell_http_error_t err;
    xplrCom_cell_connect_t comState;

    comState = xplrComCellFsmConnectGetState(cellConfig->profileIndex);
    if (comState == XPLR_COM_CELL_CONNECTED) {
        err = xplrCellHttpPostRequest(cellConfig->profileIndex, httpCellClient->id, NULL);
        vTaskDelay(1);
        if (err == XPLR_CELL_HTTP_ERROR) {
            ret = ESP_FAIL;
            XPLRZTP_CONSOLE(E, "Device %d, client %d (http) POST REQUEST to %s, failed.\n",
                            cellConfig->profileIndex,
                            httpCellClient->id,
                            httpCellClient->session->data.path);
        } else {
            ret = ESP_OK;
            XPLRZTP_CONSOLE(D, "Device %d, client %d (http) POST REQUEST to %s, ok.\n",
                            cellConfig->profileIndex,
                            httpCellClient->id,
                            httpCellClient->session->data.path);
        }
    } else {
        ret = ESP_FAIL;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * CALLBACK FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/*
 * Handles HTTP(S) Events as they arrive after performing a POST request.
 * This function get called every time an esp_http_client_event_handle_t occurs.
 * Specific for Wi-Fi
 */
static esp_err_t httpWifiCallback(esp_http_client_event_handle_t evt)
{
    esp_err_t ret;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            XPLRZTP_CONSOLE(D, "HTTP_EVENT_ON_CONNECTED!");
            ret = ESP_OK;
            break;

        case HTTP_EVENT_HEADERS_SENT:
            ret = ESP_OK;
            break;

        case HTTP_EVENT_ON_HEADER:
            ret = ESP_OK;
            break;

        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ztpWifiPopulateBuffer((char *)evt->user_data, evt->data, evt->data_len);
                ret = ESP_OK;
            } else {
                XPLRZTP_CONSOLE(W, "HTTP_DATA_IS_CHUNKED!");
                ret = ESP_FAIL;
            }
            break;

        case HTTP_EVENT_ERROR:
            XPLRZTP_CONSOLE(E, "HTTP_EVENT_ERROR: %.*s", evt->data_len, (char *)evt->data);
            ret = ESP_FAIL;
            break;

        case HTTP_EVENT_ON_FINISH:
            XPLRZTP_CONSOLE(D, "HTTP_EVENT_ON_FINISH");
            ret = ESP_OK;
            break;

        default:
            ret = ESP_FAIL;
            break;
    }

    return ret;
}

/*
 * Handles HTTP(S) Events as they arrive after performing a POST request.
 * Specific for cellular.
 */
static void httpCellCallback(uDeviceHandle_t devHandle,
                             int32_t statusCodeOrError,
                             size_t responseSize,
                             void *pResponseCallbackParam)
{
    XPLRZTP_CONSOLE(I, "Http response callback fired with code (%d).", statusCodeOrError);
    XPLRZTP_CONSOLE(D, "Message size of %d bytes.\n", responseSize);

    httpCellClient.session->error = statusCodeOrError;
    if (statusCodeOrError > -1) {
        httpCellClient.session->statusCode = statusCodeOrError;
        httpCellClient.session->rspAvailable = true;
        httpCellClient.session->rspSize = responseSize;
        httpCellClient.session->data.bufferSizeOut = XPLRZTP_PAYLOAD_SIZE_MAX;
    }

    if (httpCellClient.session->requestPending) {
        httpCellClient.session->requestPending = false;
    }
}