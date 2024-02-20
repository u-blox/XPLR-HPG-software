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

#include <stdbool.h>
#include "./../log_service/xplr_log.h"
#include "./../common/xplr_common.h"

#ifndef _XPLR_AT_SERVER_H_
#define _XPLR_AT_SERVER_H_

/** @file
 * @brief AT command server, hpglib library based on ubxlib.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define XPLR_ATSERVER_EOF "\r\n"
#define XPLR_ATSERVER_EOF_SIZE 2

/* ----------------------------------------------------------------
 * PUBLIC TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to xplr_at_server module. */
typedef enum {
    XPLR_AT_SERVER_ERROR = -1,            /**< process returned with errors. */
    XPLR_AT_SERVER_OK,                    /**< indicates success of returning process. */
} xplr_at_server_error_t;

typedef enum {
    XPLR_AT_SERVER_RESPONSE_START = 0,    /**< Start of response */
    XPLR_AT_SERVER_RESPONSE_MID,          /**< mid response, a delimiter follows */
    XPLR_AT_SERVER_RESPONSE_END,          /**< End of response, line termination follows*/
} xplr_at_server_response_type_t;

/** UART configuration struct */
typedef struct xplr_at_server_uartCfg_type {
    int32_t uart;                         /** The UART HW block to use. */
    int32_t baudRate;                     /** Uart speed value */
    int32_t pinTxd;                       /** The output pin that sends UART data to
                                              the module. */
    int32_t pinRxd;                       /** The input pin that receives UART data from
                                              the module. */
    size_t rxBufferSize;                  /** the amount of memory to allocate
                                              for the receive buffer */
} xplr_at_server_uartCfg_t;

typedef struct xplr_at_server_type {
    uint8_t                         profile;
    xplr_at_server_uartCfg_t        *uartCfg;
} xplr_at_server_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize an AT server profile for handling
 * AT commands.
 *
 * @param server      struct for storing public server data/settings.
 * @return            XPLR_AT_SERVER_OK on success,
 *                    XPLR_AT_SERVER_ERROR on failure.
 */
xplr_at_server_error_t xplrAtServerInit(xplr_at_server_t *server);

/**
 * @brief Deinitialize an AT server profile
 *
 * @param server      struct storing public server data/settings
 */
void xplrAtServerDeInit(xplr_at_server_t *server);

/**
 * @brief Set an AT command filter, for executing a handler
 * function when a specific AT command is received.
 *
 * IMPORTANT: don't do anything heavy in a handler, e.g. don't
 * printf() or, at most, print a few characters; handlers
 * have to run quickly as they are interleaved with everything
 * else handling incoming data and any delay may result in
 * buffer overflows.  If you need to do anything heavy then
 * have your handler call xplrAtServerCallback().
 *
 * @param server        struct storing public server data/settings.
 * @param strFilter     AT command filter, "AT+COMMANDNAME:".
 * @param callback      handler function to be executed upon receiving strFilter.
 * @param callbackArg   Optional variable to be passed as a second argument
 *                      on the handler function.
 * @return              XPLR_AT_SERVER_OK on success,
 *                      XPLR_AT_SERVER_ERROR on failure.
 */
xplr_at_server_error_t xplrAtServerSetCommandFilter(xplr_at_server_t *server,
                                                    const char *strFilter,
                                                    void (*callback)(void *, void *),
                                                    void *callbackArg);

/**
 * @brief Remove an AT command filter.
 *
 * @param server      struct storing public server data/settings.
 * @param strFilter   AT command filter whose handler function to be removed.
 */
void xplrAtServerRemoveCommandFilter(xplr_at_server_t *server,
                                     const char *strFilter);

/**
 * @brief Make an asynchronous callback that is run in its own task
 * context
 *
 * @param server        struct storing public server data/settings.
 * @param strFilter     callback function to execute.
 * @param callbackArg   optional second argument to be passed to the
 *                      callback function
 * @return              XPLR_AT_SERVER_OK on success,
 *                      XPLR_AT_SERVER_ERROR on failure.
 */
xplr_at_server_error_t xplrAtServerCallback(xplr_at_server_t *server,
                                            void (*callback)(void *, void *),
                                            void *callbackArg);

/**
 * @brief Read characters from the received AT stream until the delimiter ","
 * or the stop tag "CRLF" is found.
 *
 * @param server            struct storing public server data/settings.
 * @param strFilter         buffer for storing the received string.
 * @param lengthBytes       the maximum number of chars to read
 *                          including the null terminator.
 * @param ignoreStopTag     if true then continue reading even
 *                          if the stop tag is found. The stop tag is
 *                          CRLF string
 * @return                  the length of the string stored in
 *                          buffer (as in the value that strlen()
 *                          would return) or negative error code
 *                          if a read timeout occurs before the
 *                          delimiter or the stop tag is found.
 */
int32_t xplrAtServerReadString(xplr_at_server_t *server,
                               char *buffer, size_t lengthBytes,
                               bool ignoreStopTag);

/**
 * @brief Read bytes from the received AT stream until
 * the stop tag "CRLF" is found.
 *
 * @param server            struct storing public server data/settings.
 * @param strFilter         buffer for storing the received bytes.
 * @param lengthBytes       the number of bytes to read
 *                          including the null terminator.
 * @param standalone        set this to true if the bytes form
 *                          a standalone sequence, e.g. when reading
 *                          a specific number of bytes of an IP
 *                          socket AT command.  If this is false
 *                          then a delimiter (or the stop tag
 *                          for a response line) will be expected
 *                          following the sequence of bytes.
 * @return                  the number of bytes read or negative
 *                          error code.
 */
int32_t xplrAtServerReadBytes(xplr_at_server_t *server,
                              char *buffer, size_t lengthBytes,
                              bool standalone);

/**
 * @brief               Write an AT response back to the sender.
 *
 * @param server        struct storing public server data/settings.
 * @param buffer        buffer containing the bytes to be written.
 * @param lengthBytes   the number of bytes in buffer.
 * @return              the number of bytes written.
 */
size_t xplrAtServerWrite(xplr_at_server_t *server,
                         const char *buffer,
                         size_t lengthBytes);

/**
 * @brief               Write a string formatted AT response back to the sender,
 * containing delimiters between values. An appropriate  xplr_at_server_response_type_t
 * value is needed for the start of the string response, the inbetween values for
 * inserting delimiters and the last value for line termination.
 *
 * @param server        struct storing public server data/settings.
 * @param buffer        buffer containing the bytes to be written.
 * @param lengthBytes   the number of bytes in buffer.
 * @param responseType  The response type
 * @return              the number of bytes written from the buffer.
 */
size_t xplrAtServerWriteString(xplr_at_server_t *server,
                               const char *buffer,
                               size_t lengthBytes,
                               xplr_at_server_response_type_t responseType);

/**
 * @brief               Write an integer AT response back to the sender,
 * containing delimiters between values. An appropriate  xplr_at_server_response_type_t
 * value is needed for the start of the string response, the inbetween values for
 * inserting delimiters and the last value for line termination.
 *
 * @param server        struct storing public server data/settings.
 * @param integer       integer to be written.
 * @param responseType  The response type
 */
void xplrAtServerWriteInt(xplr_at_server_t *server,
                          int32_t integer,
                          xplr_at_server_response_type_t responseType);

/**
 * @brief               Write an unsigned integer AT response back to the sender,
 * containing delimiters between values. An appropriate  xplr_at_server_response_type_t
 * value is needed for the start of the string response, the inbetween values for
 * inserting delimiters and the last value for line termination.
 *
 * @param server        struct storing public server data/settings.
 * @param uinteger      unsigned integer to be written.
 * @param responseType  The response type
 */
void xplrAtServerWriteUint(xplr_at_server_t *server,
                           uint64_t uinteger,
                           xplr_at_server_response_type_t responseType);

/**
 * @brief            Reconfigure the Uart interface according to the updated
 * uartCfg struct located in server->uartCfg
 *
 * @param server     struct storing public server data/settings.
 * @return           XPLR_AT_SERVER_OK or XPLR_AT_SERVER_ERROR.
 */
xplr_at_server_error_t xplrAtServerUartReconfig(xplr_at_server_t *server);

/**
 * @brief            Empty the underlying receive buffer.
 *
 * @param server     struct storing public server data/settings.
 */
void xplrAtServerFlushRx(xplr_at_server_t *server);

/**
 * @brief            Return the error state from the previous function
 * called. Usefeul for functions not returning an error or success variable.
 *
 * @param server     struct storing public server data/settings.
 * @return           XPLR_AT_SERVER_OK or XPLR_AT_SERVER_ERROR.
 */
xplr_at_server_error_t xplrAtServerGetError(xplr_at_server_t *server);

/**
 * @brief Function that initializes logging of the module with user-selected configuration
 *
 * @param logCfg    Pointer to a xplr_cfg_logInstance_t configuration struct.
 *                  If NULL, the instance will be initialized using the default settings
 *                  (located in xplr_hpglib_cfg.h file)
 * @return          index of the logging instance in success, -1 in failure.
*/
int8_t xplrAtServerInitLogModule(xplr_cfg_logInstance_t *logCfg);

/**
 * @brief   Function that stops logging of the module.
 *
 * @return  ESP_OK on success, ESP_FAIL otherwise.
*/
esp_err_t xplrAtServerStopLogModule(void);

#ifdef __cplusplus
}
#endif
#endif //_AT_SERVER_H

// End of file