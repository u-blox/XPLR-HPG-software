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

#include "xplr_cell_ntrip_client.h"

#include <stdbool.h>
#include <string.h>
#include "./../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "errno.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#if (1 == XPLRCELL_NTRIP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRCELL_NTRIP_LOG_ACTIVE))
#define XPLRCELL_NTRIP_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrCellNtrip", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRCELL_NTRIP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
#define XPLRCELL_NTRIP_CONSOLE(tag, message, ...)  esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrCellNtrip", __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    if (ntripLog.logEnable){\
        snprintf(&ntripLogBuf[0], ELEMENTCNT(ntripLogBuf), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrCellNtrip", __FUNCTION__, __LINE__, ## __VA_ARGS__);\
        XPLRLOG(&ntripLog,ntripLogBuf);}
#elif ((0 == XPLRCELL_NTRIP_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
#define XPLRCELL_NTRIP_CONSOLE(tag, message, ...) if (ntripLog.logEnable){\
        snprintf(&ntripLogBuf[0], ELEMENTCNT(ntripLogBuf), "[(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "xplrCellNtrip", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
        XPLRLOG(&ntripLog,ntripLogBuf);}
#else
#define XPLRCELL_NTRIP_CONSOLE(message, ...) do{} while(0)
#endif


#define XPLRCELL_NTRIP_FSM_TIMEOUT_S (30U)
#define XPLRCELL_NTRIP_SEMAPHORE_WAIT_MS (200U)

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */
// Struct used to encode username & password to Base64 (used by ntripBase64Encode)

typedef struct xplrBase64 {
    char encoded[256];
    size_t encodedLen;
} xplrBase64_t;

#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
static xplrLog_t ntripLog;
static char ntripLogBuf[XPLRLOG_BUFFER_SIZE_SMALL];
#endif

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
const char *ntripCellResponseIcy = "ICY 200 OK\r\n";                  //!< correction data response
const char *ntripCellResponseSourcetable = "SOURCETABLE 200 OK\r\n";  //!< source table response

TaskHandle_t xHandle;
SemaphoreHandle_t ntripSemaphore;
/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void ntripLoop(xplrCell_ntrip_client_t *client);
static xplrCell_ntrip_error_t ntripCleanup(xplrCell_ntrip_client_t *client);
static xplrBase64_t ntripBase64Encode(char *data, size_t input_length);
static void ntripFormatRequest(xplrCell_ntrip_client_t *client, char *request);
static xplrCell_ntrip_error_t ntripCreateSocket(xplrCell_ntrip_client_t *client);
static xplrCell_ntrip_error_t ntripSetTimeout(xplrCell_ntrip_client_t *client);
static xplrCell_ntrip_error_t ntripCreateTask(xplrCell_ntrip_client_t *client);
static xplrCell_ntrip_error_t ntripCheckConfig(xplrCell_ntrip_client_t *client);
static xplrCell_ntrip_error_t ntripHandleResponse(xplrCell_ntrip_client_t *client,
                                                  bool icy,
                                                  bool sourcetable);
static xplrCell_ntrip_error_t ntripCasterHandshake(xplrCell_ntrip_client_t *client);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCell_ntrip_error_t xplrCellNtripInit(xplrCell_ntrip_client_t *client,
                                         SemaphoreHandle_t xplrNtripSemaphore)
{
    xplrCell_ntrip_error_t ret;

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
    if (ret != XPLR_CELL_NTRIP_ERROR) {

        ret = ntripCreateSocket(client);
        if (ret != XPLR_CELL_NTRIP_OK) {
            XPLRCELL_NTRIP_CONSOLE(E, "ntripCreateSocket failed");
        } else {
            ret = ntripCasterHandshake(client);
        }
    } else {
        // Do nothing
    }

    if (ret != XPLR_CELL_NTRIP_OK) {
        XPLRCELL_NTRIP_CONSOLE(E, "NTRIP failed to initialize");
        XPLRCELL_NTRIP_CONSOLE(E, "Running cleanup");
        ret = ntripCleanup(client);
        if (ret == XPLR_CELL_NTRIP_ERROR) {
            XPLRCELL_NTRIP_CONSOLE(E, "ntripCleanup failed");
        } else {
            // make the return value ERROR to indicate the init failed
            ret = XPLR_CELL_NTRIP_ERROR;
        }
    } else {
        // Do nothing
    }

    return ret;
}

xplrCell_ntrip_error_t xplrCellNtripSendGGA(xplrCell_ntrip_client_t *client,
                                            char *buffer,
                                            uint32_t ggaSize)
{
    xplrCell_ntrip_error_t ret;
    int32_t writeSize;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRCELL_NTRIP_SEMAPHORE_WAIT_MS));
    if (semaphoreRet == pdTRUE) {
        writeSize = uSockWrite(client->socket, buffer, ggaSize);
        client->ggaInterval = MICROTOSEC(esp_timer_get_time());
        if (writeSize == ggaSize) {
            XPLRCELL_NTRIP_CONSOLE(I, "Sent GGA message to caster [%d] bytes", ggaSize);
            ret = XPLR_CELL_NTRIP_OK;
            client->state = XPLR_CELL_NTRIP_STATE_READY;
            client->ggaInterval = MICROTOSEC(esp_timer_get_time());
        } else if (writeSize < 0) {
            XPLRCELL_NTRIP_CONSOLE(E,
                                   "Encountered error while sending GGA message to caster, socket errno -> [%d]",
                                   errno);
            ret = XPLR_CELL_NTRIP_ERROR;
            client->state = XPLR_CELL_NTRIP_STATE_ERROR;
            client->error = XPLR_CELL_NTRIP_SOCKET_ERROR;
        } else {
            XPLRCELL_NTRIP_CONSOLE(E,
                                   "Encountered error while sending GGA message to caster [%d] bytes",
                                   writeSize);
            ret = XPLR_CELL_NTRIP_ERROR;
            client->state = XPLR_CELL_NTRIP_STATE_ERROR;
            client->error = XPLR_CELL_NTRIP_SOCKET_ERROR;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_CELL_NTRIP_ERROR;
        client->state = XPLR_CELL_NTRIP_STATE_ERROR;
        client->error = XPLR_CELL_NTRIP_SEMAPHORE_ERROR;
    }

    return ret;
}

xplrCell_ntrip_error_t xplrCellNtripGetCorrectionData(xplrCell_ntrip_client_t *client,
                                                      char *buffer,
                                                      uint32_t bufferSize,
                                                      uint32_t *corrDataSize)
{
    xplrCell_ntrip_error_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRCELL_NTRIP_SEMAPHORE_WAIT_MS));
    if (semaphoreRet == pdTRUE) {
        if (bufferSize < XPLRCELL_NTRIP_RECEIVE_DATA_SIZE) {
            XPLRCELL_NTRIP_CONSOLE(I, "Buffer provided is too small");
            ret = XPLR_CELL_NTRIP_ERROR;
            client->state = XPLR_CELL_NTRIP_STATE_ERROR;
            client->error = XPLR_CELL_NTRIP_BUFFER_TOO_SMALL_ERROR;
        } else {
            memcpy(buffer, client->transfer.corrData, XPLRCELL_NTRIP_RECEIVE_DATA_SIZE);
            *corrDataSize = client->transfer.corrDataSize;
            ret = XPLR_CELL_NTRIP_OK;
            client->state = XPLR_CELL_NTRIP_STATE_READY;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_CELL_NTRIP_ERROR;
        client->state = XPLR_CELL_NTRIP_STATE_ERROR;
        client->error = XPLR_CELL_NTRIP_SEMAPHORE_ERROR;
    }

    return ret;
}

xplrCell_ntrip_state_t xplrCellNtripGetClientState(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_state_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(1000));
    if (semaphoreRet == pdTRUE) {
        ret = client->state;
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore, %s has it",
                               pcTaskGetName(xSemaphoreGetMutexHolder(ntripSemaphore)));
        ret = XPLR_CELL_NTRIP_STATE_BUSY;
    }
    return ret;
}

xplrCell_ntrip_detailed_error_t xplrCellNtripGetDetailedError(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_detailed_error_t ret;
    BaseType_t semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(1000));
    if (semaphoreRet == pdTRUE) {
        ret = client->error;
        switch (ret) {
            case XPLR_CELL_NTRIP_UKNOWN_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_UKNOWN_ERROR");
                break;
            case XPLR_CELL_NTRIP_BUSY_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_BUSY_ERROR");
                break;
            case XPLR_CELL_NTRIP_CONNECTION_RESET_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_CONNECTION_RESET_ERROR");
                break;
            case XPLR_CELL_NTRIP_BUFFER_TOO_SMALL_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_BUFFER_TOO_SMALL_ERROR");
                break;
            case XPLR_CELL_NTRIP_NO_GGA_TIMEOUT_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_NO_GGA_TIMEOUT_ERROR");
                break;
            case XPLR_CELL_NTRIP_CORR_DATA_TIMEOUT_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_CORR_DATA_TIMEOUT_ERROR");
                break;
            case XPLR_CELL_NTRIP_SOCKET_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_SOCKET_ERROR");
                break;
            case XPLR_CELL_NTRIP_UNABLE_TO_CREATE_TASK_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_UNABLE_TO_CREATE_TASK_ERROR");
                break;
            case XPLR_CELL_NTRIP_SEMAPHORE_ERROR:
                XPLRCELL_NTRIP_CONSOLE(E, "Detailed error -> XPLR_CELL_NTRIP_SEMAPHORE_ERROR");
                break;
            default:
                break;
        }
        xSemaphoreGive(ntripSemaphore);
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore, %s has it",
                               pcTaskGetName(xSemaphoreGetMutexHolder(ntripSemaphore)));
        ret = XPLR_CELL_NTRIP_STATE_BUSY;
    }
    return ret;
}

xplrCell_ntrip_error_t xplrCellNtripDeInit(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;
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
        XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore");
        ret = XPLR_CELL_NTRIP_ERROR;
    }
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
    xplrLogDeInit(&ntripLog);
#endif

    return ret;
}

void xplrCellNtripSetConfig(xplrCell_ntrip_client_t *client,
                            const char *host,
                            uint16_t port,
                            const char *mountpoint,
                            uint8_t cellDvcProfile,
                            bool ggaNecessary)
{
    if (client->config_set) {
        XPLRCELL_NTRIP_CONSOLE(W, "Configuration have already been set, overwriting with new one");
    } else {
        // No action needed
    }

    memset(client->config.host, 0x00, strlen(client->config.host));
    memset(client->config.mountpoint, 0x00, strlen(client->config.mountpoint));

    client->config.ggaNecessary = ggaNecessary;
    strcpy(client->config.host, host);
    client->config.port = port;
    strcpy(client->config.mountpoint, mountpoint);

    client->cellDvcProfile = cellDvcProfile;

    client->config_set = true;
}

void xplrCellNtripSetCredentials(xplrCell_ntrip_client_t *client,
                                 bool useAuth,
                                 const char *username,
                                 const char *password,
                                 const char *userAgent)
{
    if (client->credentials_set) {
        XPLRCELL_NTRIP_CONSOLE(W, "Credentials have already been set, overwriting with new ones");
    } else {
        // No action needed
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

bool xplrCellNtripHaltLogModule(xplrCell_ntrip_client_t *client)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
    if(client->logCfg != NULL) {
        client->logCfg->logEnable = false;
        ret = true;
    } else {
        ret = false;
        /* log module is not initialized thus do nothing*/
    }
#else
    ret = false;
#endif
    return ret;
}

bool xplrCellNtripStartLogModule(xplrCell_ntrip_client_t *client)
{
    bool ret;
#if (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_NTRIP_LOG_ACTIVE)
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

void ntripFormatRequest(xplrCell_ntrip_client_t *client, char *request)
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
void ntripLoop(xplrCell_ntrip_client_t *client)
{
    int size;
    BaseType_t semaphoreRet;

    while (1) {
        semaphoreRet = xSemaphoreTake(ntripSemaphore, pdMS_TO_TICKS(XPLRCELL_NTRIP_SEMAPHORE_WAIT_MS));
        if (semaphoreRet == pdTRUE) {
            switch (client->state) {
                case XPLR_CELL_NTRIP_STATE_READY:
                    client->error = XPLR_CELL_NTRIP_NO_ERROR;
                    if ((MICROTOSEC(esp_timer_get_time()) - client->ggaInterval) > XPLRCELL_NTRIP_GGA_INTERVAL_S
                        &&
                        client->config.ggaNecessary) {
                        // Signal APP to give GGA to NTRIP client
                        client->state = XPLR_CELL_NTRIP_STATE_REQUEST_GGA;
                        client->timeout = MICROTOSEC(esp_timer_get_time());
                    } else {
                        memset(client->transfer.corrData, 0x00, XPLRCELL_NTRIP_RECEIVE_DATA_SIZE);
                        // Read from the socket the data sent by the caster
                        size = uSockRead(client->socket, client->transfer.corrData, XPLRCELL_NTRIP_RECEIVE_DATA_SIZE);
                        if (size > 0) {
                            // Signal APP to read correction data from buffer
                            client->state = XPLR_CELL_NTRIP_STATE_CORRECTION_DATA_AVAILABLE;
                            client->transfer.corrDataSize = size;
                            client->timeout = MICROTOSEC(esp_timer_get_time());
                        } else {
                            if (errno == 11) {
                                // Nothing to read
                                client->state = XPLR_CELL_NTRIP_STATE_READY;
                            } else if (errno == 5) {
                                client->state = XPLR_CELL_NTRIP_STATE_CONNECTION_RESET;
                            } else {
                                client->state = XPLR_CELL_NTRIP_STATE_ERROR;
                                client->error = XPLR_CELL_NTRIP_SOCKET_ERROR;
                                XPLRCELL_NTRIP_CONSOLE(E,
                                                       "Failed to get correction data, client going to error state (socket errno -> [%d])",
                                                       errno);
                            }
                        }
                    }
                    break;
                case XPLR_CELL_NTRIP_STATE_REQUEST_GGA:
                    // APP hasn't provided GGA yet
                    if (MICROTOSEC(esp_timer_get_time()) - client->timeout >= pdMS_TO_TICKS(
                            XPLRCELL_NTRIP_FSM_TIMEOUT_S)) {
                        client->state = XPLR_CELL_NTRIP_STATE_ERROR;
                        client->error = XPLR_CELL_NTRIP_NO_GGA_TIMEOUT_ERROR;
                    }
                    break;
                case XPLR_CELL_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                    // APP hasn't read correction data yet
                    if (MICROTOSEC(esp_timer_get_time()) - client->timeout >= pdMS_TO_TICKS(
                            XPLRCELL_NTRIP_FSM_TIMEOUT_S)) {
                        client->state = XPLR_CELL_NTRIP_STATE_ERROR;
                        client->error = XPLR_CELL_NTRIP_CORR_DATA_TIMEOUT_ERROR;
                    }
                    break;
                default:
                    // Do nothing
                    break;
            }
            xSemaphoreGive(ntripSemaphore);
            vTaskDelay(pdMS_TO_TICKS(25));
        } else {
            XPLRCELL_NTRIP_CONSOLE(E, "Failed to get semaphore, %s has it",
                                   pcTaskGetName(xSemaphoreGetMutexHolder(ntripSemaphore)));
            client->state = XPLR_CELL_NTRIP_STATE_BUSY;
        }

    }
}

xplrCell_ntrip_error_t ntripCreateSocket(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;
    int intRet;
    uSockAddress_t address;

    intRet = uSockGetHostByName(xplrComGetDeviceHandler(client->cellDvcProfile),
                                client->config.host,
                                &(address.ipAddress));
    if (intRet != U_ERROR_COMMON_SUCCESS) {
        XPLRCELL_NTRIP_CONSOLE(E, "uSockGetHostByName failed");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else {
        address.port = client->config.port;
        client->socket = uSockCreate(xplrComGetDeviceHandler(client->cellDvcProfile),
                                     U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);

        intRet = uSockConnect(client->socket, &address);
        if (intRet != U_ERROR_COMMON_SUCCESS) {
            XPLRCELL_NTRIP_CONSOLE(E, "uSockConnect failed");
            ret = XPLR_CELL_NTRIP_ERROR;
        } else {
            XPLRCELL_NTRIP_CONSOLE(I, "Socket connected");
            ret = XPLR_CELL_NTRIP_OK;
        }
    }

    return ret;
}

xplrCell_ntrip_error_t ntripCleanup(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret = XPLR_CELL_NTRIP_OK;
    int32_t intRet;

    client->socketIsValid = false;

    intRet = uSockShutdown(client->socket, U_SOCK_SHUTDOWN_READ_WRITE);
    if (intRet != U_ERROR_COMMON_SUCCESS) {
        XPLRCELL_NTRIP_CONSOLE(W, "Error shutting down socket");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else {
        ret = XPLR_CELL_NTRIP_OK;
    }

    intRet = uSockClose(client->socket);
    if (intRet != U_ERROR_COMMON_SUCCESS) {
        XPLRCELL_NTRIP_CONSOLE(W, "Error closing socket");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else {
        ret = XPLR_CELL_NTRIP_OK;
    }

    uSockCleanUp();
    client->socketIsValid = false;

    return ret;
}

xplrCell_ntrip_error_t ntripSetTimeout(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;
    int32_t len;
    // Struct used to set the socket timeout
    struct timeval receivingTimeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };

    len = uSockOptionSet(client->socket,
                         U_SOCK_OPT_LEVEL_SOCK,
                         U_SOCK_OPT_RCVTIMEO,
                         &receivingTimeout,
                         sizeof(receivingTimeout));
    if (len < 0) {
        XPLRCELL_NTRIP_CONSOLE(E, "failed to set socket receive timeout");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else {
        ret = XPLR_CELL_NTRIP_OK;
    }

    return ret;
}

xplrCell_ntrip_error_t ntripCreateTask(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;
    BaseType_t taskRet, semaphoreRet;

    semaphoreRet = xSemaphoreTake(ntripSemaphore,
                                  pdMS_TO_TICKS(XPLRCELL_NTRIP_SEMAPHORE_WAIT_MS));
    if (semaphoreRet == pdTRUE) {
        taskRet = xTaskCreate((TaskFunction_t)ntripLoop, "NtripTask",
                              2048,
                              client,
                              10,
                              &xHandle);
        xSemaphoreGive(ntripSemaphore);
        if (taskRet != pdPASS) {
            client->state = XPLR_CELL_NTRIP_STATE_ERROR;
            client->error = XPLR_CELL_NTRIP_UNABLE_TO_CREATE_TASK_ERROR;
            XPLRCELL_NTRIP_CONSOLE(I, "failed to create NTRIP task");
            client->socketIsValid = false;
            ret = XPLR_CELL_NTRIP_ERROR;
        } else {
            if (client->config.ggaNecessary) {
                client->state = XPLR_CELL_NTRIP_STATE_REQUEST_GGA;
                client->timeout = MICROTOSEC(esp_timer_get_time());
            } else {
                client->state = XPLR_CELL_NTRIP_STATE_READY;
            }
            client->socketIsValid = true;
            ret = XPLR_CELL_NTRIP_OK;
            XPLRCELL_NTRIP_CONSOLE(I, "NTRIP task created");
        }
    } else {
        client->state = XPLR_CELL_NTRIP_STATE_ERROR;
        client-> error = XPLR_CELL_NTRIP_SEMAPHORE_ERROR;
        client->socketIsValid = false;
        ret = XPLR_CELL_NTRIP_ERROR;
        XPLRCELL_NTRIP_CONSOLE(I, "failed to create NTRIP task");
    }

    return ret;
}

xplrCell_ntrip_error_t ntripCheckConfig(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;

    if (!client->config_set) {
        // Check if NTRIP configuration is set
        XPLRCELL_NTRIP_CONSOLE(E, "NTRIP configuration not set");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else if (!client->credentials_set) {
        // Check if NTRIP credentials are set
        XPLRCELL_NTRIP_CONSOLE(E, "NTRIP credentials not set");
        ret = XPLR_CELL_NTRIP_ERROR;
    } else {
        if (client->socketIsValid) {
            // Cleanup socket if it has already been initialized
            ret = ntripCleanup(client);
            if (ret == XPLR_CELL_NTRIP_ERROR) {
                XPLRCELL_NTRIP_CONSOLE(E, "ntripCleanup failed");
            } else {
                // No action needed
            }
        } else {
            ret = XPLR_CELL_NTRIP_OK;
        }
    }

    return ret;
}

xplrCell_ntrip_error_t ntripHandleResponse(xplrCell_ntrip_client_t *client,
                                           bool icy,
                                           bool sourcetable)
{
    xplrCell_ntrip_error_t ret;

    if (icy) {
        // The caster responded with ICY (i see you) which means that the client configuration is correct
        XPLRCELL_NTRIP_CONSOLE(I, "Connected to caster");
        XPLRCELL_NTRIP_CONSOLE(I, "NTRIP client initialization successful");
        ret = ntripSetTimeout(client);
        if (ret != XPLR_CELL_NTRIP_ERROR) {
            ret = ntripCreateTask(client);
        }

    } else if (sourcetable) {
        // The caster responded with SOURCETABLE which means that the mountpoint in the client configuration is probably incorrect
        XPLRCELL_NTRIP_CONSOLE(W, "Got source table, please provide a mountpoint");
        ret = XPLR_CELL_NTRIP_ERROR;
        client->socketIsValid = false;
    } else if (errno == U_SOCK_EHOSTUNREACH) {
        XPLRCELL_NTRIP_CONSOLE(E, "Host unreachable");
        ret = XPLR_CELL_NTRIP_ERROR;
        client->socketIsValid = false;
    } else if (errno == U_SOCK_ECONNRESET) {
        XPLRCELL_NTRIP_CONSOLE(E, "Connection reset by peer");
        ret = XPLR_CELL_NTRIP_ERROR;
        client->socketIsValid = false;
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Error reading from socket, socket errno -> [%d]", errno);
        ret = XPLR_CELL_NTRIP_ERROR;
        client->socketIsValid = false;
    }

    return ret;
}

xplrCell_ntrip_error_t ntripCasterHandshake(xplrCell_ntrip_client_t *client)
{
    xplrCell_ntrip_error_t ret;
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
    len = uSockWrite(client->socket, request, strlen(request));
    if (len == strlen(request)) {
        XPLRCELL_NTRIP_CONSOLE(I, "Request sent [%d] bytes", len);
        // Look for ICY 200 or SOURCETABLE 200 response
        len = uSockRead(client->socket, response, sizeof(response));
        if (len > 0) {
            strRet = strstr(response, ntripCellResponseSourcetable);
            if (strRet != NULL) {
                // Got sourcetable
                sourcetable = true;
            } else {
                strRet = strstr(response, ntripCellResponseIcy);
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
            XPLRCELL_NTRIP_CONSOLE(E, "Socket read failed, errno [%d]", errno);
            ret = XPLR_CELL_NTRIP_ERROR;
        }
    } else {
        XPLRCELL_NTRIP_CONSOLE(E, "Request failed, sent [%d] bytes, socket errno -> [%d]", len, errno);
        ret = XPLR_CELL_NTRIP_ERROR;
    }

    return ret;
}

/* -------------------------------------------------------------
 * STATIC CALLBACK FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

// End of file