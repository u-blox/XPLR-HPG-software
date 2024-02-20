/*
 * Copyright 2023 u-blox
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

#include "string.h"
#include "xplr_at_server.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "../../../xplr_hpglib_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if (1 == XPLRATSERVER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRATSERVER_LOG_ACTIVE))
#define XPLRATSERVER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtServer", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRATSERVER_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRATSERVER_LOG_ACTIVE)
#define XPLRATSERVER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtServer", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRATSERVER_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRATSERVER_LOG_ACTIVE)
#define XPLRATSERVER_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgAtServer", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRATSERVER_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/**
 * Settings and data struct for server profiles
 */
typedef struct {
    bool configured; // false value means uninitialized profile
    int32_t uartHandle;
    uAtClientHandle_t uAtclientHandle;
    xplr_at_server_error_t error;
} xplr_at_server_profile_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * An array of server profiles
 */
static xplr_at_server_profile_t srv[XPLRATSERVER_NUMOF_SERVERS] = {{.configured = false}};

/**
 * Number of AT servers currently in use
 */
static uint8_t numberOfServers = 0;

static int8_t logIndex = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void getuAtClientDeviceError(xplr_at_server_t *server);

xplr_at_server_error_t xplrAtServerInit(xplr_at_server_t *server)
{
    int32_t errorCode;
    xplr_at_server_profile_t *instance;
    xplr_at_server_error_t error;

    // find an empty element
    if (numberOfServers < XPLRATSERVER_NUMOF_SERVERS) {
        for (size_t x = 0; x < XPLRATSERVER_NUMOF_SERVERS; x++) {
            if (!srv[x].configured) {
                server->profile = x;
                numberOfServers++;
                break;
            }
        }

        instance = &srv[server->profile];

        errorCode = uPortInit();
        if (errorCode != 0) {
            XPLRATSERVER_CONSOLE(E, "Error initializing uPort");
        } else {
            instance->uartHandle = uPortUartOpen(server->uartCfg->uart,
                                                 server->uartCfg->baudRate,
                                                 NULL,
                                                 server->uartCfg->rxBufferSize,
                                                 server->uartCfg->pinTxd,
                                                 server->uartCfg->pinRxd,
                                                 -1, -1);
            if (instance->uartHandle < 0) {
                XPLRATSERVER_CONSOLE(E, "Error opening UART %d", server->uartCfg->uart);
                instance->error = XPLR_AT_SERVER_ERROR;
            } else {
                instance->error = XPLR_AT_SERVER_OK;
            }
        }

        if (instance->error == XPLR_AT_SERVER_OK) {
            errorCode = uAtClientInit();
            if (errorCode != 0) {
                XPLRATSERVER_CONSOLE(E, "Error %d initializing ubxlib at_client library", errorCode);
                instance->error = XPLR_AT_SERVER_ERROR;
            } else {
                XPLRATSERVER_CONSOLE(D, "ubxlib at_client library initialized");
                instance->uAtclientHandle = uAtClientAdd(instance->uartHandle,
                                                         U_AT_CLIENT_STREAM_TYPE_UART,
                                                         NULL,
                                                         U_AT_CLIENT_BUFFER_LENGTH_BYTES);
                if (instance->uAtclientHandle == NULL) {
                    XPLRATSERVER_CONSOLE(E, "Error adding ubxlib at_client on UART");
                    instance->error = XPLR_AT_SERVER_ERROR;
                } else {
                    instance->error = XPLR_AT_SERVER_OK;
                    instance->configured = true;
                }
            }
        }
        error = instance->error;
    } else {
        XPLRATSERVER_CONSOLE(E, "Max number of AT server profiles reached");
        error = XPLR_AT_SERVER_ERROR;
    }

    return error;
}

// At filter name "AT+$(NAME):"
xplr_at_server_error_t xplrAtServerSetCommandFilter(xplr_at_server_t *server,
                                                    const char *strFilter,
                                                    void (*callback)(void *, void *),
                                                    void *callbackArg)
{
    int32_t errorCode;
    xplr_at_server_profile_t *instance = &srv[server->profile];
    errorCode = uAtClientSetUrcHandler(instance->uAtclientHandle,
                                       strFilter,
                                       callback,
                                       callbackArg);
    if (errorCode != 0) {
        XPLRATSERVER_CONSOLE(E, "Error setting uAtClientSetUrcHandler");
        instance->error = XPLR_AT_SERVER_ERROR;
    } else {
        instance->error = XPLR_AT_SERVER_OK;
    }

    return instance->error;
}


void xplrAtServerRemoveCommandFilter(xplr_at_server_t *server,
                                     const char *strFilter)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uAtClientRemoveUrcHandler(instance->uAtclientHandle, strFilter);
    XPLRATSERVER_CONSOLE(D, "Removed uAt handler with name %s", strFilter);
}

xplr_at_server_error_t xplrAtServerCallback(xplr_at_server_t *server,
                                            void (*callback)(void *, void *),
                                            void *callbackArg)
{
    int32_t errorCode;
    xplr_at_server_profile_t *instance = &srv[server->profile];

    errorCode = uAtClientCallback(instance->uAtclientHandle,
                                  callback,
                                  callbackArg);
    if (errorCode != 0) {
        instance->error = XPLR_AT_SERVER_ERROR;
    } else {
        instance->error = XPLR_AT_SERVER_OK;
    }

    return instance->error;
}

void xplrAtServerDeInit(xplr_at_server_t *server)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uAtClientRemove(instance->uAtclientHandle);
    uPortUartClose(instance->uartHandle);
    uAtClientDeinit();
    uPortDeinit();

    memset(server, 0x00, sizeof(xplr_at_server_t));
    numberOfServers--;
}

int32_t xplrAtServerReadString(xplr_at_server_t *server,
                               char *buffer,
                               size_t lengthBytes,
                               bool ignoreStopTag)
{
    int32_t errorCode;
    xplr_at_server_profile_t *instance = &srv[server->profile];

    errorCode = uAtClientReadString(instance->uAtclientHandle,
                                    buffer,
                                    lengthBytes,
                                    ignoreStopTag);
    if (errorCode < 0) {
        XPLRATSERVER_CONSOLE(E, "Error %d reading uAt String", errorCode);
    }

    return errorCode;
}

int32_t xplrAtServerReadBytes(xplr_at_server_t *server,
                              char *buffer,
                              size_t lengthBytes,
                              bool standalone)
{
    int32_t errorCode;
    xplr_at_server_profile_t *instance = &srv[server->profile];

    errorCode = uAtClientReadBytes(instance->uAtclientHandle,
                                   buffer,
                                   lengthBytes,
                                   standalone);
    if (errorCode < 0) {
        XPLRATSERVER_CONSOLE(E, "Error reading uAt String");
    }

    return errorCode;
}

size_t xplrAtServerWrite(xplr_at_server_t *server,
                         const char *buffer,
                         size_t lengthBytes)
{
    size_t writeLength;
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uAtClientCommandStart(instance->uAtclientHandle, "");
    writeLength = uAtClientWriteBytes(instance->uAtclientHandle,
                                      buffer,
                                      lengthBytes,
                                      true);
    uAtClientWriteBytes(instance->uAtclientHandle, XPLR_ATSERVER_EOF, XPLR_ATSERVER_EOF_SIZE, true);

    // check for uAtClient errors
    getuAtClientDeviceError(server);

    return writeLength;
}

size_t xplrAtServerWriteString(xplr_at_server_t *server,
                               const char *buffer,
                               size_t lengthBytes,
                               xplr_at_server_response_type_t responseType)
{
    size_t writeLength;
    xplr_at_server_profile_t *instance = &srv[server->profile];
    uAtClientCommandStart(instance->uAtclientHandle, "");

    if (responseType == XPLR_AT_SERVER_RESPONSE_START) {
        uAtClientWriteBytes(instance->uAtclientHandle, "", 0, true);
    }

    writeLength = uAtClientWriteBytes(instance->uAtclientHandle,
                                      buffer,
                                      lengthBytes,
                                      true);

    if (responseType == XPLR_AT_SERVER_RESPONSE_MID) {
        (void)uAtClientWriteBytes(instance->uAtclientHandle,
                                  ",",
                                  1,
                                  true);
    }

    if (responseType == XPLR_AT_SERVER_RESPONSE_END) {
        uAtClientWriteBytes(instance->uAtclientHandle, XPLR_ATSERVER_EOF, XPLR_ATSERVER_EOF_SIZE, true);
    }
    // check for uAtClient errors
    getuAtClientDeviceError(server);

    return writeLength;
}

void xplrAtServerWriteInt(xplr_at_server_t *server,
                          int32_t integer,
                          xplr_at_server_response_type_t responseType)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];
    uAtClientCommandStart(instance->uAtclientHandle, "");

    if (responseType == XPLR_AT_SERVER_RESPONSE_START) {
        uAtClientWriteBytes(instance->uAtclientHandle, "", 0, true);
    }

    uAtClientWriteInt(instance->uAtclientHandle, integer);

    if (responseType == XPLR_AT_SERVER_RESPONSE_MID) {
        (void)uAtClientWriteBytes(instance->uAtclientHandle,
                                  ",",
                                  1,
                                  true);
    }

    if (responseType == XPLR_AT_SERVER_RESPONSE_END) {
        uAtClientWriteBytes(instance->uAtclientHandle, XPLR_ATSERVER_EOF, XPLR_ATSERVER_EOF_SIZE, true);
    }
    // check for uAtClient errors
    getuAtClientDeviceError(server);
}

void xplrAtServerWriteUint(xplr_at_server_t *server,
                           uint64_t uinteger,
                           xplr_at_server_response_type_t responseType)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];
    uAtClientCommandStart(instance->uAtclientHandle, "");

    if (responseType == XPLR_AT_SERVER_RESPONSE_START) {
        uAtClientWriteBytes(instance->uAtclientHandle, "", 0, true);
    }

    uAtClientWriteUint64(instance->uAtclientHandle, uinteger);

    if (responseType == XPLR_AT_SERVER_RESPONSE_MID) {
        (void)uAtClientWriteBytes(instance->uAtclientHandle,
                                  ",",
                                  1,
                                  true);
    }

    if (responseType == XPLR_AT_SERVER_RESPONSE_END) {
        uAtClientWriteBytes(instance->uAtclientHandle, XPLR_ATSERVER_EOF, XPLR_ATSERVER_EOF_SIZE, true);
    }
    // check for uAtClient errors
    getuAtClientDeviceError(server);
}

void xplrAtServerFlushRx(xplr_at_server_t *server)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uAtClientFlush(instance->uAtclientHandle);
}

xplr_at_server_error_t xplrAtServerGetError(xplr_at_server_t *server)
{
    return srv[server->profile].error;
}

xplr_at_server_error_t xplrAtServerUartReconfig(xplr_at_server_t *server)
{
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uPortUartClose(instance->uartHandle);

    instance->uartHandle = uPortUartOpen(server->uartCfg->uart,
                                         server->uartCfg->baudRate,
                                         NULL,
                                         server->uartCfg->rxBufferSize,
                                         server->uartCfg->pinTxd,
                                         server->uartCfg->pinRxd,
                                         -1, -1);

    if (instance->uartHandle < 0) {
        XPLRATSERVER_CONSOLE(E, "Error opening UART %d", server->uartCfg->uart);
        instance->error = XPLR_AT_SERVER_ERROR;
    } else {
        instance->error = XPLR_AT_SERVER_OK;
    }

    return instance->error;
}

int8_t xplrAtServerInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_AT_SERVER_DEFAULT_FILENAME,
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

esp_err_t xplrAtServerStopLogModule(void)
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

/**
 * get uAtClient error
 */
static void getuAtClientDeviceError(xplr_at_server_t *server)
{
    uAtClientDeviceError_t error;
    xplr_at_server_profile_t *instance = &srv[server->profile];

    uAtClientDeviceErrorGet(instance->uAtclientHandle, &error);
    if (error.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) {
        instance->error = XPLR_AT_SERVER_OK;
    } else {
        instance->error = XPLR_AT_SERVER_ERROR;
    }
}