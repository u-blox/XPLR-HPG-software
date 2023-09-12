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

#include "xplr_wifi_ntrip_client.h"

#include <stdbool.h>
#include "./../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRCELL_NTRIP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRWIFI_NTRIP_LOG_ACTIVE))
#define XPLRWIFI_NTRIP_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrWifiNtrip", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRCELL_NTRIP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
#define XPLRWIFI_NTRIP_CONSOLE(tag, message, ...)  esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrWifiNtrip", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    if (ntripLog.logEnable){\
        snprintf(&ntripLogBuf[0], ELEMENTCNT(ntripLogBuf), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrWifiNtrip", __FUNCTION__, __LINE__, ## __VA_ARGS__);\
        XPLRLOG(&ntripLog,ntripLogBuf);}
#elif ((0 == XPLRCELL_NTRIP_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
#define XPLRWIFI_NTRIP_CONSOLE(tag, message, ...) if (ntripLog.logEnable){\
        snprintf(&ntripLogBuf[0], ELEMENTCNT(ntripLogBuf), "[(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrWifiNtrip", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
        XPLRLOG(&ntripLog,ntripLogBuf);}
#else
#define XPLRWIFI_NTRIP_CONSOLE(message, ...) do{} while(0)
#endif

#define XPLRWIFI_NTRIP_FSM_TIMEOUT_S (30U)
#define XPLRWIFI_NTRIP_SEMAPHORE_WAIT_MS (200U)

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */
// Struct used to encode username & password to Base64 (used by base64Encode)
typedef struct xplrBase64 {
    char encoded[256];
    size_t encodedLen;
} xplrBase64_t;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
static xplrLog_t ntripLog;
static char ntripLogBuf[XPLRLOG_BUFFER_SIZE_SMALL];
#endif
/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

const char *ntripWifiResponseIcy = "ICY 200 OK\r\n";                  //!< correction data response
const char *ntripWifiResponseSourcetable = "SOURCETABLE 200 OK\r\n";  //!< source table response

TaskHandle_t xHandle;
SemaphoreHandle_t ntripSemaphore;
/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void ntripLoop(xplrWifi_ntrip_client_t *client);
static xplrWifi_ntrip_error_t ntripCleanup(xplrWifi_ntrip_client_t *client);
static xplrBase64_t ntripBase64Encode(char *data, size_t input_length);
static void ntripFormatRequest(xplrWifi_ntrip_client_t *client, char *request);
static xplrWifi_ntrip_error_t ntripCreateSocket(xplrWifi_ntrip_client_t *client);
static xplrWifi_ntrip_error_t ntripSetTimeout(xplrWifi_ntrip_client_t *client);
static xplrWifi_ntrip_error_t ntripCreateTask(xplrWifi_ntrip_client_t *client);
static xplrWifi_ntrip_error_t ntripCheckConfig(xplrWifi_ntrip_client_t *client);
static xplrWifi_ntrip_error_t ntripHandleResponse(xplrWifi_ntrip_client_t *client,
                                                  bool icy,
                                                  bool sourcetable);
static xplrWifi_ntrip_error_t ntripCasterHandshake(xplrWifi_ntrip_client_t *client);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrWifi_ntrip_error_t xplrWifiNtripInit(xplrWifi_ntrip_client_t *client,
                                         SemaphoreHandle_t xplrNtripSemaphore)
{
    xplrWifi_ntrip_error_t ret;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
    /* initialize logging*/
    xplrLog_error_t logErr;
    logErr = xplrLogInit(&ntripLog, XPLR_LOG_DEVICE_INFO, "/ntrip.log", 100, XPLR_SIZE_MB);
    if(logErr == XPLR_LOG_OK){
        ntripLog.logEnable = true;
    }else{
        ntripLog.logEnable = false;
    }
    client->logCfg = &ntripLog;
#endif

    // Keep a copy of the APP semaphore
    ntripSemaphore = xplrNtripSemaphore;

    // Check configuration / credentials
    ret = ntripCheckConfig(client);

    // Begin the NTRIP init
    if (ret != XPLR_WIFI_NTRIP_ERROR) {
        ret = ntripCreateSocket(client);
        if (ret != XPLR_WIFI_NTRIP_OK) {
            XPLRWIFI_NTRIP_CONSOLE(E, "ntripCreateSocket failed");
        } else {
            ret = ntripCasterHandshake(client);
        }
    } else {
        // Do nothing
    }

    if (ret != XPLR_WIFI_NTRIP_OK) {
        XPLRWIFI_NTRIP_CONSOLE(E, "NTRIP failed to initialize");
        XPLRWIFI_NTRIP_CONSOLE(E, "Running cleanup");
        ret = ntripCleanup(client);
        if (ret == XPLR_WIFI_NTRIP_ERROR) {
            XPLRWIFI_NTRIP_CONSOLE(E, "ntripCleanup failed");
        } else {
            // make the return value ERROR to indicate the init failed
            ret = XPLR_WIFI_NTRIP_ERROR;
        }
    } else {
        // NTRIP init successful no need for cleanup
    }

    return ret;
}

xplrWifi_ntrip_error_t xplrWifiNtripSendGGA(xplrWifi_ntrip_client_t *client,
                                            char *buffer,
                                            uint32_t ggaSize)
{
    xplrWifi_ntrip_error_t ret;
    int32_t writeSize;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRWIFI_NTRIP_SEMAPHORE_WAIT_MS));
    if (semaphoreRet == pdTRUE) {
        writeSize = write(client->socket, buffer, ggaSize);
        client->ggaInterval = MICROTOSEC(esp_timer_get_time());
        if (writeSize == ggaSize) {
            XPLRWIFI_NTRIP_CONSOLE(I, "Sent GGA message to caster [%d] bytes", ggaSize);
            ret = XPLR_WIFI_NTRIP_OK;
            client->state = XPLR_WIFI_NTRIP_STATE_READY;
            client->ggaInterval = MICROTOSEC(esp_timer_get_time());
        } else if (writeSize < 0) {
            XPLRWIFI_NTRIP_CONSOLE(E,
                                   "Encountered error while sending GGA message to caster, socket errno -> [%d]",
                                   errno);
            ret = XPLR_WIFI_NTRIP_ERROR;
            client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
            client->error = XPLR_WIFI_NTRIP_SOCKET_ERROR;
        } else {
            XPLRWIFI_NTRIP_CONSOLE(E,
                                   "Encountered error while sending GGA message to caster [%d] bytes",
                                   writeSize);
            ret = XPLR_WIFI_NTRIP_ERROR;
            client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
            client->error = XPLR_WIFI_NTRIP_SOCKET_ERROR;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
        client->error = XPLR_WIFI_NTRIP_SEMAPHORE_ERROR;
    }

    return ret;
}

xplrWifi_ntrip_error_t xplrWifiNtripGetCorrectionData(xplrWifi_ntrip_client_t *client,
                                                      char *buffer,
                                                      uint32_t bufferSize,
                                                      uint32_t *corrDataSize)
{
    xplrWifi_ntrip_error_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRWIFI_NTRIP_SEMAPHORE_WAIT_MS));
    if (semaphoreRet == pdTRUE) {
        if (bufferSize < XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE) {
            XPLRWIFI_NTRIP_CONSOLE(I, "Buffer provided is too small");
            ret = XPLR_WIFI_NTRIP_ERROR;
            client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
            client->error = XPLR_WIFI_NTRIP_BUFFER_TOO_SMALL_ERROR;
        } else {
            memcpy(buffer, client->transfer.corrData, XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE);
            *corrDataSize = client->transfer.corrDataSize;
            ret = XPLR_WIFI_NTRIP_OK;
            client->state = XPLR_WIFI_NTRIP_STATE_READY;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
        client->error = XPLR_WIFI_NTRIP_SEMAPHORE_ERROR;
    }

    return ret;
}

xplrWifi_ntrip_state_t xplrWifiNtripGetClientState(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_state_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(1000));
    if (semaphoreRet == pdTRUE) {
        ret = client->state;
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_WIFI_NTRIP_STATE_BUSY;
    }
    return ret;
}

xplrWifi_ntrip_detailed_error_t xplrWifiNtripGetDetailedError(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_detailed_error_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(1000));
    if (semaphoreRet == pdTRUE) {
        ret = client->error;
        switch (ret) {
            case XPLR_WIFI_NTRIP_UKNOWN_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_UKNOWN_ERROR");
                break;
            case XPLR_WIFI_NTRIP_BUSY_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_BUSY_ERROR");
                break;
            case XPLR_WIFI_NTRIP_CONNECTION_RESET_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_CONNECTION_RESET_ERROR");
                break;
            case XPLR_WIFI_NTRIP_BUFFER_TOO_SMALL_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_BUFFER_TOO_SMALL_ERROR");
                break;
            case XPLR_WIFI_NTRIP_NO_GGA_TIMEOUT_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_NO_GGA_TIMEOUT_ERROR");
                break;
            case XPLR_WIFI_NTRIP_CORR_DATA_TIMEOUT_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_CORR_DATA_TIMEOUT_ERROR");
                break;
            case XPLR_WIFI_NTRIP_SOCKET_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_SOCKET_ERROR");
                break;
            case XPLR_WIFI_NTRIP_UNABLE_TO_CREATE_TASK_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_UNABLE_TO_CREATE_TASK_ERROR");
                break;
            case XPLR_WIFI_NTRIP_SEMAPHORE_ERROR:
                XPLRWIFI_NTRIP_CONSOLE(E, "Detailed error -> XPLR_WIFI_NTRIP_SEMAPHORE_ERROR");
                break;
            default:
                break;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_WIFI_NTRIP_STATE_BUSY;
    }
    return ret;
}

xplrWifi_ntrip_error_t xplrWifiNtripDeInit(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, portMAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        vTaskDelete(xHandle);
        ret = ntripCleanup(client);
        // Invalidate configuration and credentials
        client->config_set = false;
        client->credentials_set = false;
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_WIFI_NTRIP_ERROR;
    }
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
    xplrLogDeInit(&ntripLog);
#endif

    return ret;
}

void xplrWifiNtripSetConfig(xplrWifi_ntrip_client_t *client,
                            const char *host,
                            uint16_t port,
                            const char *mountpoint,
                            bool ggaNecessary)
{
    if (client->config_set) {
        XPLRWIFI_NTRIP_CONSOLE(W, "Configuration have already been set, overwriting with new one");
    } else {
        // No action needed
    }

    memset(client->config.host, 0x00, strlen(client->config.host));
    memset(client->config.mountpoint, 0x00, strlen(client->config.mountpoint));

    client->config.ggaNecessary = ggaNecessary;
    strcpy(client->config.host, host);
    client->config.port = port;
    strcpy(client->config.mountpoint, mountpoint);

    client->config_set = true;
}

void xplrWifiNtripSetCredentials(xplrWifi_ntrip_client_t *client,
                                 bool useAuth,
                                 const char *username,
                                 const char *password,
                                 const char *userAgent)
{
    if (client->credentials_set) {
        XPLRWIFI_NTRIP_CONSOLE(W, "Credentials have already been set, overwriting with new ones");
    }

    memset(client->credentials.username, 0x00, strlen(client->credentials.username));
    memset(client->credentials.password, 0x00, strlen(client->credentials.password));
    memset(client->credentials.userAgent, 0x00, strlen(client->credentials.userAgent));
    client->credentials.useAuth = useAuth;

    if (useAuth) {
        strcpy(client->credentials.username, username);
        strcpy(client->credentials.password, password);
    } else {
        // No action needed
    }

    strcpy(client->credentials.userAgent, userAgent);

    client->credentials_set = true;
}

bool xplrWifiNtripHaltLogModule(xplrWifi_ntrip_client_t *client)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
    if(client->logCfg != NULL) {
        client->logCfg->logEnable = false;
        ret = true;
    } else {
        /* log module is not initialized thus do nothing and return false*/
        ret = false;
    }
#else
    ret = false;
#endif
    return ret;
}

bool xplrWifiNtripStartLogModule(xplrWifi_ntrip_client_t *client)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRWIFI_NTRIP_LOG_ACTIVE)
    if(client->logCfg != NULL) {
        client->logCfg->logEnable = true;
        ret = true;
    } else {
        /* log module is not initialized thus don't enable logging
           and return false*/
        ret = false; 
    }
#else 
    ret = false;
#endif
    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrBase64_t ntripBase64Encode(char *data, size_t input_length)
{
    xplrBase64_t encodedBase64;

    static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                    'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                    '4', '5', '6', '7', '8', '9', '+', '/'
                                   };
    static int mod_table[] = {0, 2, 1};

    encodedBase64.encodedLen = 4 * ((input_length + 2) / 3);

    for (int i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encodedBase64.encoded[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encodedBase64.encoded[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encodedBase64.encoded[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encodedBase64.encoded[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++) {
        encodedBase64.encoded[encodedBase64.encodedLen - 1 - i] = '=';
    }

    return encodedBase64;
}

void ntripFormatRequest(xplrWifi_ntrip_client_t *client, char *request)
{
    xplrBase64_t encodedBase64;
    char buff[128];

    memset(buff, 0x00, strlen(buff));
    sprintf(buff, "%s:%s", client->credentials.username,
            client->credentials.password);
    encodedBase64 = ntripBase64Encode(buff, strlen(buff));

    if (client->credentials.useAuth) {
        sprintf(request,
                "GET /%s HTTP/1.0\r\n"
                "User-Agent: %s\r\n"
                "Accept: */*\r\n"
                "Authorization: Basic %.*s\r\n"
                "Connection: close\r\n"
                "\r\n",
                client->config.mountpoint, client->credentials.userAgent, encodedBase64.encodedLen,
                encodedBase64.encoded);
    } else {
        sprintf(request,
                "GET /%s HTTP/1.0\r\n"
                "User-Agent: %s\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n"
                "\r\n",
                client->config.mountpoint, client->credentials.userAgent);
    }
}

// Main NTRIP task
void ntripLoop(xplrWifi_ntrip_client_t *client)
{
    int size;
    BaseType_t semaphoreRet;

    while (1) {
        semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRWIFI_NTRIP_SEMAPHORE_WAIT_MS));
        if (semaphoreRet == pdTRUE) {
            switch (client->state) {
                case XPLR_WIFI_NTRIP_STATE_READY:
                    client->error = XPLR_WIFI_NTRIP_NO_ERROR;
                    if ((MICROTOSEC(esp_timer_get_time()) - client->ggaInterval) > XPLRWIFI_NTRIP_GGA_INTERVAL_S
                        &&
                        client->config.ggaNecessary) {
                        // Signal APP to give GGA to NTRIP client
                        client->state = XPLR_WIFI_NTRIP_STATE_REQUEST_GGA;
                        client->timeout = MICROTOSEC(esp_timer_get_time());
                    } else {
                        memset(client->transfer.corrData, 0x00, XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE);
                        // Read from the socket the data sent by the caster
                        size = read(client->socket, client->transfer.corrData, XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE);
                        if (size > 0) {
                            // Signal APP to read correction data from buffer
                            client->state = XPLR_WIFI_NTRIP_STATE_CORRECTION_DATA_AVAILABLE;
                            client->transfer.corrDataSize = size;
                            client->timeout = MICROTOSEC(esp_timer_get_time());
                        } else {
                            if (errno == 11) {
                                // Nothing to read
                                client->state = XPLR_WIFI_NTRIP_STATE_READY;
                            } else if (errno == 5) {
                                client->state = XPLR_WIFI_NTRIP_STATE_CONNECTION_RESET;
                            } else {
                                client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
                                client->error = XPLR_WIFI_NTRIP_SOCKET_ERROR;
                                XPLRWIFI_NTRIP_CONSOLE(E,
                                                       "Failed to get correction data, client going to error state (socket errno -> [%d])",
                                                       errno);
                            }
                        }
                    }
                    break;
                case XPLR_WIFI_NTRIP_STATE_REQUEST_GGA:
                    // APP hasn't provided GGA yet
                    if (MICROTOSEC(esp_timer_get_time()) - client->timeout
                        >=
                        pdMS_TO_TICKS(XPLRWIFI_NTRIP_FSM_TIMEOUT_S)) {
                        client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
                        client->error = XPLR_WIFI_NTRIP_NO_GGA_TIMEOUT_ERROR;
                    }
                    break;
                case XPLR_WIFI_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                    // APP hasn't read correction data yet
                    if (MICROTOSEC(esp_timer_get_time()) - client->timeout
                        >=
                        pdMS_TO_TICKS(XPLRWIFI_NTRIP_FSM_TIMEOUT_S)) {
                        client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
                        client->error = XPLR_WIFI_NTRIP_CORR_DATA_TIMEOUT_ERROR;
                    }
                    break;
                default:
                    // Do nothing
                    break;
            }
            xSemaphoreGive(ntripSemaphore);
            vTaskDelay(pdMS_TO_TICKS(25));
        } else {
            XPLRWIFI_NTRIP_CONSOLE(E, "Failed to get semaphore");
            client->state = XPLR_WIFI_NTRIP_STATE_BUSY;
        }

    }
}

xplrWifi_ntrip_error_t ntripCreateSocket(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    int err;
    char buff[32];
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    memset(buff, 0x00, strlen(buff));
    sprintf(buff, "%d", client->config.port);
    err = getaddrinfo(client->config.host, buff, &hints, &res);

    if (err != 0 || res == NULL) {
        XPLRWIFI_NTRIP_CONSOLE(E, "DNS lookup failed");
        ret = XPLR_WIFI_NTRIP_ERROR;
    } else {
        client->socket = socket(res->ai_family, res->ai_socktype, 0);
        if (client->socket < 0) {
            XPLRWIFI_NTRIP_CONSOLE(E, "Failed to allocate socket.");
            ret = XPLR_WIFI_NTRIP_ERROR;
        } else {
            err = connect(client->socket, res->ai_addr, res->ai_addrlen);
            if (err != 0) {
                XPLRWIFI_NTRIP_CONSOLE(E, "socket connect failed errno=%d", errno);
                ret = XPLR_WIFI_NTRIP_ERROR;
            } else {
                XPLRWIFI_NTRIP_CONSOLE(I, "Socket connected");
                ret = XPLR_WIFI_NTRIP_OK;
            }
        }
    }

    return ret;
}

xplrWifi_ntrip_error_t ntripCleanup(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    int intRet;

    intRet = close(client->socket);
    if (intRet) {
        XPLRWIFI_NTRIP_CONSOLE(W, "Error closing socket");
        ret = XPLR_WIFI_NTRIP_ERROR;
    } else {
        ret = XPLR_WIFI_NTRIP_OK;
    }
    client->socketIsValid = false;

    return ret;
}

xplrWifi_ntrip_error_t ntripSetTimeout(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    int32_t len;
    // Struct used to set the socket timeout
    struct timeval receivingTimeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };

    len = setsockopt(client->socket,
                     SOL_SOCKET, SO_RCVTIMEO,
                     &receivingTimeout,
                     sizeof(receivingTimeout));
    if (len < 0) {
        XPLRWIFI_NTRIP_CONSOLE(E, "failed to set socket receive timeout");
        ret = XPLR_WIFI_NTRIP_ERROR;
    } else {
        ret = XPLR_WIFI_NTRIP_OK;
    }

    return ret;
}

xplrWifi_ntrip_error_t ntripCreateTask(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    BaseType_t taskRet, semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, portMAX_DELAY);
    if (semaphoreRet == pdTRUE) {
        taskRet = xTaskCreate((TaskFunction_t)ntripLoop, "NtripTask",
                              2048,
                              client,
                              10,
                              &xHandle);
        xSemaphoreGive(ntripSemaphore);
        if (taskRet != pdPASS) {
            client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
            client->error = XPLR_WIFI_NTRIP_UNABLE_TO_CREATE_TASK_ERROR;
            XPLRWIFI_NTRIP_CONSOLE(I, "failed to create NTRIP task");
            client->socketIsValid = false;
            ret = XPLR_WIFI_NTRIP_ERROR;
        } else {
            if (client->config.ggaNecessary) {
                client->state = XPLR_WIFI_NTRIP_STATE_REQUEST_GGA;
                client->timeout = MICROTOSEC(esp_timer_get_time());
            } else {
                client->state = XPLR_WIFI_NTRIP_STATE_READY;
            }
            client->socketIsValid = true;
            ret = XPLR_WIFI_NTRIP_OK;
            XPLRWIFI_NTRIP_CONSOLE(I, "NTRIP task created");
        }
    } else {
        client->state = XPLR_WIFI_NTRIP_STATE_ERROR;
        client-> error = XPLR_WIFI_NTRIP_SEMAPHORE_ERROR;
        client->socketIsValid = false;
        ret = XPLR_WIFI_NTRIP_ERROR;
        XPLRWIFI_NTRIP_CONSOLE(I, "failed to create NTRIP task");
    }

    return ret;
}

xplrWifi_ntrip_error_t ntripCheckConfig(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;

    if (!client->config_set) {
        // Check if NTRIP configuration is set
        XPLRWIFI_NTRIP_CONSOLE(E, "NTRIP configuration not set");
        ret = XPLR_WIFI_NTRIP_ERROR;
    } else if (!client->credentials_set) {
        // Check if NTRIP credentials are set
        XPLRWIFI_NTRIP_CONSOLE(E, "NTRIP credentials not set");
        ret = XPLR_WIFI_NTRIP_ERROR;
    } else {
        if (client->socketIsValid) {
            // Cleanup socket if it has already been initialized
            ret = ntripCleanup(client);
            if (ret == XPLR_WIFI_NTRIP_ERROR) {
                XPLRWIFI_NTRIP_CONSOLE(E, "ntripCleanup failed");
            } else {
                // No action needed
            }
        } else {
            ret = XPLR_WIFI_NTRIP_OK;
        }
    }

    return ret;
}

xplrWifi_ntrip_error_t ntripHandleResponse(xplrWifi_ntrip_client_t *client,
                                           bool icy,
                                           bool sourcetable)
{
    xplrWifi_ntrip_error_t ret;

    if (icy) {
        // The caster responded with ICY (i see you) which means that the client configuration is correct
        XPLRWIFI_NTRIP_CONSOLE(I, "Connected to caster");
        XPLRWIFI_NTRIP_CONSOLE(I, "NTRIP client initialization successful");
        ret = ntripSetTimeout(client);
        if (ret != XPLR_WIFI_NTRIP_ERROR) {
            ret = ntripCreateTask(client);
        }

    } else if (sourcetable) {
        // The caster responded with SOURCETABLE which means that the mountpoint in the client configuration is probably incorrect
        XPLRWIFI_NTRIP_CONSOLE(W, "Got source table, please provide a mountpoint");
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->socketIsValid = false;
    } else if (errno == EHOSTUNREACH) {
        XPLRWIFI_NTRIP_CONSOLE(E, "Host unreachable");
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->socketIsValid = false;
    } else if (errno == ECONNRESET) {
        XPLRWIFI_NTRIP_CONSOLE(E, "Connection reset by peer");
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->socketIsValid = false;
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Error reading from socket, socket errno -> [%d]", errno);
        ret = XPLR_WIFI_NTRIP_ERROR;
        client->socketIsValid = false;
    }

    return ret;
}

xplrWifi_ntrip_error_t ntripCasterHandshake(xplrWifi_ntrip_client_t *client)
{
    xplrWifi_ntrip_error_t ret;
    int32_t len;
    char *strRet;
    // Buffers to send request and receive the response
    char request[256], response[64];
    // Flags for ICY (I see you) and SOURCETABLE response from caster
    bool sourcetable = false;
    bool icy = false;

    memset(response, 0x00, strlen(response));
    memset(request, 0x00, strlen(request));

    ntripFormatRequest(client, request);
    // Send the initial request to the NTRIP caster
    len = write(client->socket, request, strlen(request));
    if (len == strlen(request)) {
        XPLRWIFI_NTRIP_CONSOLE(I, "Request sent [%d] bytes", len);
        // Look for ICY 200 or SOURCETABLE 200 response
        len = read(client->socket, response, sizeof(response));
        if (len > 0) {
            strRet = strstr(response, ntripWifiResponseSourcetable);
            if (strRet != NULL) {
                // Got sourcetable
                sourcetable = true;
            } else {
                strRet = strstr(response, ntripWifiResponseIcy);
                if (strRet != NULL) {
                    // Got ICY from caster
                    icy = true;
                } else {
                    // No action needed
                }
            }
            // Handle response from server
            ret = ntripHandleResponse(client, icy, sourcetable);
        } else {
            XPLRWIFI_NTRIP_CONSOLE(E, "Socket read failed, errno [%d]", errno);
            ret = XPLR_WIFI_NTRIP_ERROR;
        }
    } else {
        XPLRWIFI_NTRIP_CONSOLE(E, "Request failed, sent [%d] bytes, socket errno -> [%d]", len, errno);
        ret = XPLR_WIFI_NTRIP_ERROR;
    }

    return ret;
}

/* -------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

// End of file
