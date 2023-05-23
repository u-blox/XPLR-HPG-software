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
#include "./../../../components/xplr_ztp/include/xplr_ztp.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRZTP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRZTP_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrZtp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRZTP_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static uint32_t bufferStackPointer;
static uint32_t max_buffer_size;
static const char *post_body_parts[] = {"{\"token\":\"",
                                        "\",\"givenName\":\"",
                                        "\",\"hardwareId\":\"",
                                        "\",\"tags\": []}"
                                       };

/* ----------------------------------------------------------------
 * CALLBACK FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt);

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void xplrZtpPrivatePopulateBuffer(char *payload, char *source, uint16_t length);
static esp_err_t xplrZtpPrivateSetHeaders(esp_http_client_handle_t client);
static esp_err_t xplrZtpPrivateSetPostData(char *post_data, 
                                           int maxLen,
                                           xplrZtpDevicePostData_t *dvcPostData,
                                           esp_http_client_handle_t client);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

esp_err_t xplrZtpGetPayload(const char *rootCert,
                            const char *url,
                            xplrZtpDevicePostData_t *dvcPostData, 
                            xplrZtpData_t *ztpData)
{
    esp_http_client_handle_t client;
    esp_err_t ret;
    char post_data[150] = {0};

    esp_http_client_config_t config_post = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = client_event_post_handler,
        .cert_pem = rootCert,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = ztpData->payload
    };

    bufferStackPointer = 0;
    max_buffer_size = ztpData->payloadLength;

    if (url == NULL || 
        ztpData->payload == NULL || 
        ztpData == NULL || 
        dvcPostData == NULL ||
        dvcPostData->dvcToken == NULL ||
        dvcPostData->dvcName == NULL) {
        XPLRZTP_CONSOLE(E, "NULL pointer detected!");
        return ESP_FAIL;
    }

    XPLRZTP_CONSOLE(D, "POST URL: %s", url);

    client = esp_http_client_init(&config_post);
    if (client == NULL) {
        XPLRZTP_CONSOLE(E, "HTTP client creation failed!");
        return ESP_FAIL;
    }

    ret = xplrZtpPrivateSetHeaders(client);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Setting POST headers failed!");
        return ret;
    }

    ret = xplrZtpPrivateSetPostData(post_data, ELEMENTCNT(post_data), dvcPostData, client);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Setting POST data body failed!");
        return ret;
    }

    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Client POST request failed!");
        ret = esp_http_client_cleanup(client);
        if (ret != ESP_OK) {
            XPLRZTP_CONSOLE(E, "Client clean-up failed.");
            return ret;
        }
        return ret;
    }

    ztpData->httpReturnCode = esp_http_client_get_status_code(client);

    if (esp_http_client_get_content_length(client) > ztpData->payloadLength - 1) {
        ztpData->payload[0] = 0;
        XPLRZTP_CONSOLE(E, "HTTPS POST payload larger [%d] than buffer [%d]!",
                        esp_http_client_get_content_length(client),
                        ztpData->payloadLength - 1);
        ret = esp_http_client_cleanup(client);
        if (ret != ESP_OK) {
            XPLRZTP_CONSOLE(E, "Client clean-up failed.");
            return ret;
        }
        return ESP_FAIL;
    }

    if (ztpData->httpReturnCode == HttpStatus_Ok) {
        XPLRZTP_CONSOLE(D, "HTTPS POST request OK.");
    } else {
        XPLRZTP_CONSOLE(D, "HTTPS POST request failed: Code [%d]", ztpData->httpReturnCode);
    }

    XPLRZTP_CONSOLE(D, "HTTPS POST: Return Code - %d", ztpData->httpReturnCode);
    ret = esp_http_client_cleanup(client);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(D, "Client cleanup failed!");
        return ret;
    }

    return ESP_OK;
}

esp_err_t xplrZtpGetDeviceID(char *deviceID, uint8_t maxLen, esp_mac_type_t macType)
{
    esp_err_t ret;
    int writeLen;
    uint8_t derived_mac_addr[6] = {0};

    ret = esp_read_mac(derived_mac_addr, macType);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed to get device ID!");
        return ret;
    }

    writeLen = snprintf(deviceID, 
                        maxLen, 
                        "hpg-%02x%02x%02x", 
                        derived_mac_addr[3], 
                        derived_mac_addr[4],
                        derived_mac_addr[5]);

    if (writeLen < 0) {
        XPLRZTP_CONSOLE(E, "Getting device ID failed with error code[%d]!", writeLen);
        return ESP_FAIL;
    } else if (writeLen == 0) {
        XPLRZTP_CONSOLE(E, "Getting device ID failed!");
        XPLRZTP_CONSOLE(E, "Nothing was written in the buffer");
        return ESP_FAIL;
    } else if (writeLen >= maxLen) {
        XPLRZTP_CONSOLE(E, "Getting device ID failed!");
        XPLRZTP_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
        return ESP_FAIL;
    } else {
        XPLRZTP_CONSOLE(I, "Got device ID successfully.");
    }

    XPLRZTP_CONSOLE(D, "Device ID: %s", deviceID);

    return ESP_OK;
}

/* ----------------------------------------------------------------
 * CALLBACK FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/*
 * Handles HTTP(S) Events as they arrive after performing a POST request.
 * This function get called every time an esp_http_client_event_handle_t occurs.
 */
static esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            XPLRZTP_CONSOLE(D, "HTTP_EVENT_ON_CONNECTED!");
            break;

        case HTTP_EVENT_HEADERS_SENT:
            break;

        case HTTP_EVENT_ON_HEADER:
            break;

        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                xplrZtpPrivatePopulateBuffer((char *)evt->user_data, evt->data, evt->data_len);
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
            XPLRZTP_CONSOLE(E, "HTTP_EVENT_ERROR: %.*s", evt->data_len, (char *)evt->data);
            break;

        case HTTP_EVENT_ON_FINISH:
            XPLRZTP_CONSOLE(D, "HTTP_EVENT_ON_FINISH");
            break;

        default:
            break;
    }

    return ESP_OK;
}

esp_err_t xplrZtpGetPostBody(char *res, uint32_t maxLen, xplrZtpDevicePostData_t *dvcPostData)
{
    int writeLen;
    esp_err_t ret;
    char deviceID[11] = {0};

    ret = xplrZtpGetDeviceID(deviceID, ELEMENTCNT(deviceID), ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed getting Device ID!");
        return ret;
    }

    writeLen = snprintf(res, 
                        maxLen, 
                        "%s%s%s%s%s%s%s", 
                        post_body_parts[0], 
                        dvcPostData->dvcToken,
                        post_body_parts[1], 
                        dvcPostData->dvcName, 
                        post_body_parts[2], 
                        deviceID, 
                        post_body_parts[3]);

    if (writeLen < 0) {
        XPLRZTP_CONSOLE(E, "Post body creation failed with error code[%d]!", writeLen);
        return ESP_FAIL;
    } else if (writeLen == 0) {
        XPLRZTP_CONSOLE(E, "Post body creation failed!");
        XPLRZTP_CONSOLE(E, "Nothing was written in the buffer");
        return ESP_FAIL;
    } else if (writeLen >= maxLen) {
        XPLRZTP_CONSOLE(E, "Post body creation failed!");
        XPLRZTP_CONSOLE(E, "Write length %d is larger than buffer size %d", writeLen, maxLen);
        return ESP_FAIL;
    } else {
        XPLRZTP_CONSOLE(I, "Post body created successfully.");
    }

    return ESP_OK;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

/*
 * Populates the buffer from the HTTP EVENTS to be sent back as a result.
 */
static void xplrZtpPrivatePopulateBuffer(char *payload, char *source, uint16_t length)
{
    if (bufferStackPointer < max_buffer_size) {
        memcpy(payload + bufferStackPointer, source, length);
        bufferStackPointer += length;
        payload[bufferStackPointer] = 0;
    } else {
        XPLRZTP_CONSOLE(E, "Payload buffer not big enough. Could not copy all data from HTTP!");
    }
}

/*
 * Sets the POST data body.
 * Check Thingstream and Swagger docs for more information.
 */
static esp_err_t xplrZtpPrivateSetPostData(char *post_data, 
                                           int maxLen,
                                           xplrZtpDevicePostData_t *dvcPostData,
                                           esp_http_client_handle_t client)
{
    esp_err_t ret;

    if (dvcPostData == NULL || dvcPostData->dvcToken == NULL || dvcPostData->dvcName == NULL) {
        XPLRZTP_CONSOLE(E, "NULL pointer detected!");
        return ESP_FAIL;
    }

    ret = xplrZtpGetPostBody(post_data, maxLen, dvcPostData);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Error getting POST body!");
        return ret;
    }

    XPLRZTP_CONSOLE(D, "Post data: %s", post_data);

    ret = esp_http_client_set_post_field(client, post_data, strlen(post_data));
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed setting POST field!");
        return ret;
    }

    return ESP_OK;
}

/*
 * Sets the header Types and Data.
 * Check Thingstream and Swagger docs for more information.
 */
static esp_err_t xplrZtpPrivateSetHeaders(esp_http_client_handle_t client)
{
    esp_err_t ret;

    ret = esp_http_client_set_header(client, HTTP_POST_HEADER_TYPE_CONTENT,
                                     HTTP_POST_HEADER_TYPE_DATA_CONTENT);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed setting first header part!");
        return ret;
    }

    ret = esp_http_client_set_header(client, HTTP_POST_HEADER_TYPE_JSON,
                                     HTTP_POST_HEADER_TYPE_DATA_JSON);
    if (ret != ESP_OK) {
        XPLRZTP_CONSOLE(E, "Failed setting second header part!");
        return ret;
    }

    return ESP_OK;
}