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
#include "esp_task_wdt.h"
#include "xplr_http_client.h"
#include "u_cell_http.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#if (1 == XPLRCELL_HTTP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRCELL_HTTP_LOG_ACTIVE))
#define XPLRCELL_HTTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgCellHttp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRCELL_HTTP_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_HTTP_LOG_ACTIVE)
#define XPLRCELL_HTTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgCellHttp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRCELL_HTTP_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRCELL_HTTP_LOG_ACTIVE)
#define XPLRCELL_HTTP_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgCellHttp", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRCELL_HTTP_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

// *INDENT-OFF*
typedef struct xplrCell_Http_type {
    int8_t                          *dvcProfile;                                                    /**< hpglib device id */
    uDeviceHandle_t                 handler;                                                        /**< ubxlib device handler */
    xplrCell_http_client_t          *client[XPLRCELL_MQTT_NUMOF_CLIENTS];                           /**< pointer to hpglib http cell client module */
    uHttpClientContext_t            *clientContext[XPLRCELL_MQTT_NUMOF_CLIENTS];                    /**< ubxlib private client context of http api */
    uHttpClientConnection_t         clientConnection[XPLRCELL_MQTT_NUMOF_CLIENTS];                  /**< ubxlib private connection of http api */
    uSecurityTlsSettings_t          clientTlsSettings[XPLRCELL_MQTT_NUMOF_CLIENTS];                 /**< ubxlib private tls settings of http client */

    void                            (*uHttpClientResponseCallback_t[XPLRCELL_MQTT_NUMOF_CLIENTS]);  /**< array of function pointers to msg received callback. */
} xplrCell_Http_t;
// *INDENT-ON*

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char nvsNamespace[] = "httpCell_";
static xplrCell_Http_t http[XPLRCOM_NUMOF_DEVICES];
static int8_t logIndex = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplrCell_http_error_t clientNvsInit(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientNvsLoad(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientNvsWriteDefaults(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientNvsReadConfig(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientNvsUpdate(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientNvsErase(int8_t dvcProfile, int8_t clientId);
static xplrCell_http_error_t clientConnect(int8_t dvcProfile, int8_t clientId);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCell_http_error_t xplrCellHttpConnect(int8_t dvcProfile,
                                          int8_t clientId,
                                          xplrCell_http_client_t *client)
{
    xplrCell_Http_t  *instance = &http[dvcProfile];
    xplrCell_http_error_t ret;

    if (dvcProfile < XPLRCELL_MQTT_NUMOF_CLIENTS) {
        memset(instance, 0x00, sizeof(xplrCell_Http_t));
        /* Retrieve user settings to configure device */
        instance->dvcProfile = &dvcProfile;
        instance->client[clientId] = client;
        instance->client[clientId]->id = clientId;
        instance->client[clientId]->fsm[0] = XPLR_CELL_HTTP_CLIENT_FSM_CONNECT;
        if (client->settings.async) {
            instance->uHttpClientResponseCallback_t[clientId] = client->responseCb;
        } else {
            instance->uHttpClientResponseCallback_t[clientId] = NULL;
        }

        /* init nvs of mqtt client */
        ret = clientNvsInit(dvcProfile, clientId);

        /* load settings (if available) */
        if (ret == XPLR_CELL_HTTP_OK) {
            ret = clientNvsLoad(dvcProfile, clientId);
        }

        if (ret != XPLR_CELL_HTTP_OK) {
            XPLRCELL_HTTP_CONSOLE(E, "Http client init error (%d)", ret);
            ret = XPLR_CELL_HTTP_ERROR;
        } else {
            XPLRCELL_HTTP_CONSOLE(D, "Http client init ok.");
            XPLRCELL_HTTP_CONSOLE(D, "Device %d, client %d connecting to %s.",
                                  dvcProfile,
                                  clientId,
                                  http[dvcProfile].client[clientId]->settings.serverAddress);
            ret = clientConnect(dvcProfile, clientId);
        }
    } else {
        XPLRCELL_HTTP_CONSOLE(E, "HTTP init error, profile %d out of index.", dvcProfile);
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

void xplrCellHttpDeInit(int8_t dvcProfile, int8_t clientId)
{
    uHttpClientConnection_t *connection = &http[dvcProfile].clientConnection[clientId];
    uSecurityTlsSettings_t *tlsSettings = &http[dvcProfile].clientTlsSettings[clientId];
    xplrCellHttpDisconnect(dvcProfile, clientId);
    memset(connection, 0x00, sizeof(uHttpClientConnection_t));
    memset(tlsSettings, 0x00, sizeof(uSecurityTlsSettings_t));

}

void xplrCellHttpDisconnect(int8_t dvcProfile, int8_t clientId)
{
    uHttpClientContext_t *context = http[dvcProfile].clientContext[clientId];
    uHttpClientClose(context);
}

xplrCell_http_error_t xplrCellHttpCertificateSaveRootCA(int8_t dvcProfile,
                                                        int8_t clientId,
                                                        char *md5)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    int8_t res;
    char md5Stored[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1] = {0};
    xplrCell_http_error_t ret;

    /* Try deleting the key first */
    res = uSecurityCredentialRemove(handler,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    client->credentials.rootCaName);
    if (res == 0) {
        XPLRCELL_HTTP_CONSOLE(D, "Previous Root CA certificate removed from memory");
    }

    res = uSecurityCredentialStore(handler,
                                   U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                   client->credentials.rootCaName,
                                   client->credentials.rootCa,
                                   strlen(client->credentials.rootCa),
                                   NULL, md5Stored);

    if (res == 0) {
        if (md5 != NULL) {
            memcpy(md5, md5Stored, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1);
        }
        clientNvsUpdate(dvcProfile, clientId);
        XPLRCELL_HTTP_CONSOLE(D, "Certificate %s stored in memory (md5:0x%08x).",
                              client->credentials.rootCaName, (unsigned int)md5Stored);
        ret = XPLR_CELL_HTTP_OK;
    } else {
        XPLRCELL_HTTP_CONSOLE(E, "Error while storing %s certificate in memory.",
                              client->credentials.rootCaName);
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

xplrCell_http_error_t xplrCellHttpCertificateEraseRootCA(int8_t dvcProfile,
                                                         int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    int32_t ubxRes;
    xplrCell_http_error_t err;
    xplrCell_http_error_t ret;

    err = clientNvsErase(dvcProfile, clientId);

    ubxRes = uSecurityCredentialRemove(handler, U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                       client->credentials.rootCaName);

    if ((ubxRes == 0) && (err == XPLR_CELL_HTTP_OK)) {
        ret = XPLR_CELL_HTTP_OK;
        XPLRCELL_HTTP_CONSOLE(W, "Factory reset completed OK, please restart the device.");
    } else {
        ret = XPLR_CELL_HTTP_ERROR;
        XPLRCELL_HTTP_CONSOLE(E, "Factory reset error, please restart the device.");
    }

    return ret;
}

xplrCell_http_error_t xplrCellHttpCertificateCheckRootCA(int8_t dvcProfile,
                                                         int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    char cellMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1] = {0};
    char *nvsMd5 = storage->md5RootCa;
    char appMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES + 1] = {0};
    int32_t res;
    xplrCell_http_error_t ret;

    /* calculate md5 hash of rootCa provided by user */
    if (client->credentials.rootCa != NULL) {
        res = xplrCommonMd5Get((const unsigned char *)client->credentials.rootCa,
                               strlen(client->credentials.rootCa),
                               (unsigned char *)appMd5);

        /* fetch md5 hash from modules' memory (will be different).
        * needed to see if there is a certificate stored in modules memory.
        */
        res = uSecurityCredentialGetHash(handler,
                                         U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                         client->credentials.rootCaName,
                                         cellMd5);

        if (res != 0) {
            XPLRCELL_HTTP_CONSOLE(E, "Error (%d) checking MD5 hash of RootCa in modules memory",
                                  res);
            ret = XPLR_CELL_HTTP_ERROR;
        } else {
            /* compare user md5 with one stored in nvs */
            if (memcmp(appMd5, nvsMd5, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES) == 0) {
                XPLRCELL_HTTP_CONSOLE(D, "User and NVS Root certificate OK.");
                XPLRCELL_HTTP_CONSOLE(I, "Root Certificate verified OK.");
                ret = XPLR_CELL_HTTP_OK;
            } else {
                memset(nvsMd5, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1);
                memcpy(nvsMd5, appMd5, strlen(appMd5));
                XPLRCELL_HTTP_CONSOLE(W, "User and NVS Root Certificate mismatch.");
                ret = XPLR_CELL_HTTP_ERROR;
            }
        }
    } else {
        /* rootCa not available, error */
        res = -1;
        XPLRCELL_HTTP_CONSOLE(E, "Error (%d) calculating user MD5 hash", res);
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

xplrCell_http_error_t xplrCellHttpPostRequest(int8_t dvcProfile,
                                              int8_t clientId,
                                              xplrCell_http_dataTransfer_t *data)
{
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    uHttpClientContext_t *context = http[dvcProfile].clientContext[clientId];
    int32_t ubxResult;
    xplrCell_http_error_t ret;

    if (data != NULL) {
        client->session->data = *data;
    }

    if (client->session->data.path != NULL) {
        ubxResult = uHttpClientPostRequest(context,
                                           client->session->data.path,
                                           &client->session->data.buffer[0],
                                           client->session->data.bufferSizeOut,
                                           &client->session->data.contentType[0],
                                           &client->session->data.buffer[0],
                                           &client->session->data.bufferSizeIn,
                                           &client->session->data.contentType[0]);
        if (ubxResult > -1) {
            client->session->statusCode = ubxResult;
            if (client->settings.async) {
                client->session->requestPending = true;
            } else {
                client->session->rspAvailable = true;
                client->session->rspSize = client->session->data.bufferSizeIn;
                client->session->requestPending = false;
            }

            XPLRCELL_HTTP_CONSOLE(D, "Device %d, Http client %d GET REQUEST from %s returned %d.",
                                  dvcProfile, clientId, client->session->data.path, ubxResult);
            ret = XPLR_CELL_HTTP_OK;
        } else {
            XPLRCELL_HTTP_CONSOLE(E, "Device %d, Http client %d failed to GET REQUEST from %s with code %d.",
                                  dvcProfile, clientId, client->session->data.path, ubxResult);
            ret = XPLR_CELL_HTTP_ERROR;
        }

    } else {
        XPLRCELL_HTTP_CONSOLE(W, "Device %d, Http client %d dataPath is NULL.",
                              dvcProfile, clientId);
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

xplrCell_http_error_t xplrCellHttpGetRequest(int8_t dvcProfile,
                                             int8_t clientId,
                                             xplrCell_http_dataTransfer_t *data)
{
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    uHttpClientContext_t *context = http[dvcProfile].clientContext[clientId];
    int32_t ubxResult;
    xplrCell_http_error_t ret;

    if (data != NULL) {
        client->session->data = *data;
    }

    if (client->session->data.path != NULL) {
        ubxResult = uHttpClientGetRequest(context,
                                          client->session->data.path,
                                          &client->session->data.buffer[0],
                                          &client->session->data.bufferSizeOut,
                                          &client->session->data.contentType[0]);
        if (ubxResult > -1) {
            client->session->statusCode = ubxResult;
            if (client->settings.async) {
                client->session->requestPending = true;
            } else {
                client->session->rspAvailable = true;
                client->session->rspSize = client->session->data.bufferSizeOut;
                client->session->requestPending = false;
            }

            XPLRCELL_HTTP_CONSOLE(D, "Device %d, Http client %d GET REQUEST from %s returned %d.",
                                  dvcProfile, clientId, client->session->data.path, ubxResult);
            ret = XPLR_CELL_HTTP_OK;
        } else {
            XPLRCELL_HTTP_CONSOLE(E, "Device %d, Http client %d failed to GET REQUEST from %s with code %d.",
                                  dvcProfile, clientId, client->session->data.path, ubxResult);
            ret = XPLR_CELL_HTTP_ERROR;
        }

    } else {
        XPLRCELL_HTTP_CONSOLE(W, "Device %d, Http client %d dataPath is NULL.",
                              dvcProfile, clientId);
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

int8_t xplrCellHttpInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLRCELL_HTTP_DEFAULT_FILENAME,
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

xplrCell_http_error_t xplrCellHttpStopLogModule(void)
{
    xplrCell_http_error_t ret;
    xplrLog_error_t logErr;

    logErr = xplrLogDisable(logIndex);
    if (logErr == XPLR_LOG_OK) {
        ret = XPLR_CELL_HTTP_OK;
    } else {
        ret = XPLR_CELL_HTTP_ERROR;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCell_http_error_t clientNvsInit(int8_t dvcProfile, int8_t clientId)
{
    char id[3] = {0x00};
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err;
    xplrCell_http_error_t ret;

    /* create namespace tag for given client */
    sprintf(id, "%u", clientId);
    memset(storage->nvs.tag, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES);
    memset(storage->id, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES);
    strcat(storage->nvs.tag, nvsNamespace);
    strcat(storage->id, storage->nvs.tag);
    strcat(storage->id, id);
    /* init nvs */
    XPLRCELL_HTTP_CONSOLE(D, "Trying to init nvs namespace <%s>.", storage->id);
    err = xplrNvsInit(&storage->nvs, storage->id);

    if (err != XPLR_NVS_OK) {
        ret = XPLR_CELL_HTTP_ERROR;
        XPLRCELL_HTTP_CONSOLE(E, "Failed to init nvs namespace <%s>.", storage->id);
    } else {
        XPLRCELL_HTTP_CONSOLE(D, "nvs namespace <%s> for cell http client, init ok", storage->id);
        ret = XPLR_CELL_HTTP_OK;
    }

    return ret;
}
xplrCell_http_error_t clientNvsLoad(int8_t dvcProfile, int8_t clientId)
{
    char storedId[NVS_KEY_NAME_MAX_SIZE] = {0x00};
    bool writeDefaults;
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err;
    size_t size = NVS_KEY_NAME_MAX_SIZE;
    xplrCell_http_error_t ret;

    /* try read id key */
    err = xplrNvsReadString(&storage->nvs, "id", storedId, &size);
    if ((err != XPLR_NVS_OK) || (strlen(storedId) < 1)) {
        writeDefaults = true;
        XPLRCELL_HTTP_CONSOLE(W, "id key not found in <%s>, write defaults", storage->id);
    } else {
        writeDefaults = false;
        XPLRCELL_HTTP_CONSOLE(D, "id key <%s> found in <%s>", storedId, storage->id);
    }

    if (writeDefaults) {
        ret = clientNvsWriteDefaults(dvcProfile, clientId);
        if (ret == XPLR_CELL_HTTP_OK) {
            ret = clientNvsReadConfig(dvcProfile, clientId);
        }
    } else {
        ret = clientNvsReadConfig(dvcProfile, clientId);
    }

    return ret;
}
xplrCell_http_error_t clientNvsWriteDefaults(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[2];
    xplrCell_http_error_t ret;

    XPLRCELL_HTTP_CONSOLE(D, "Writing default settings in NVS");
    err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
    err[1] = xplrNvsWriteString(&storage->nvs, "rootCa", "invalid");

    for (int i = 0; i < 2; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_HTTP_ERROR;
            XPLRCELL_HTTP_CONSOLE(E, "Error writing element %u of default settings in NVS", i);
            break;
        } else {
            ret = XPLR_CELL_HTTP_OK;
        }
    }

    return ret;
}
xplrCell_http_error_t clientNvsReadConfig(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[2];
    size_t size[] = {U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES,
                     2 * U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES + 2
                    };
    xplrCell_http_error_t ret;

    err[0] = xplrNvsReadString(&storage->nvs, "id", storage->id, &size[0]);
    err[1] = xplrNvsReadStringHex(&storage->nvs, "rootCa", storage->md5RootCa, &size[1]);

    for (int i = 0; i < 2; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_HTTP_ERROR;
            break;
        } else {
            ret = XPLR_CELL_HTTP_OK;
        }
    }

    if (ret == XPLR_CELL_HTTP_OK) {
        XPLRCELL_HTTP_CONSOLE(D, "id: <%s>", storage->id);
        XPLRCELL_HTTP_CONSOLE(D, "rootCa: <0x%x>", (unsigned int)storage->md5RootCa);
    }

    return ret;
}
xplrCell_http_error_t clientNvsUpdate(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[2];
    xplrCell_http_error_t ret;

    if ((storage->id != NULL) &&
        (storage->md5RootCa != NULL)) {
        err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
        err[1] = xplrNvsWriteStringHex(&storage->nvs, "rootCa", storage->md5RootCa);

        for (int i = 0; i < 2; i++) {
            if (err[i] != XPLR_NVS_OK) {
                ret = XPLR_CELL_HTTP_ERROR;
                break;
            } else {
                ret = XPLR_CELL_HTTP_OK;
            }
        }
    } else {
        ret = XPLR_CELL_HTTP_ERROR;
        XPLRCELL_HTTP_CONSOLE(E, "Trying to write invalid config, error");
    }

    return ret;
}
xplrCell_http_error_t clientNvsErase(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_http_nvs_t *storage = &http[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[2];
    xplrCell_http_error_t ret;

    err[0] = xplrNvsEraseKey(&storage->nvs, "id");
    err[1] = xplrNvsEraseKey(&storage->nvs, "rootCa");

    for (int i = 0; i < 2; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_HTTP_ERROR;
            break;
        } else {
            ret = XPLR_CELL_HTTP_OK;
        }
    }

    return ret;
}
xplrCell_http_error_t clientConnect(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_http_client_t *client = http[dvcProfile].client[clientId];
    uHttpClientContext_t *context = http[dvcProfile].clientContext[clientId];
    uHttpClientConnection_t *connection = &http[dvcProfile].clientConnection[clientId];
    uSecurityTlsSettings_t *tlsSettings = &http[dvcProfile].clientTlsSettings[clientId];
    char sni[128] = {0};
    int32_t res;
    xplrCell_http_error_t ret;

    context = NULL;
    memset(connection, 0x00, sizeof(uHttpClientConnection_t));
    memset(tlsSettings, 0x00, sizeof(uSecurityTlsSettings_t));

    connection->pServerName = client->settings.serverAddress;
    connection->timeoutSeconds = client->settings.timeoutSeconds;
    connection->errorOnBusy = client->settings.errorOnBusy;
    if (client->settings.async) {
        connection->pResponseCallback = client->responseCb;
        connection->pResponseCallbackParam = &client->msgAvailable;
    } else {
        connection->pResponseCallback = NULL;
        connection->pResponseCallbackParam = NULL;
    }

    res = xplrRemovePortInfo(client->settings.serverAddress, sni, 128);

    switch (client->settings.registerMethod) {
        case XPLR_CELL_HTTP_CERT_METHOD_NONE:
            if (res > -1) {
                tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_ANY;
                tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_NONE;
                tlsSettings->pSni = sni;
                tlsSettings->cipherSuites.num = 0;
                ret = XPLR_CELL_HTTP_OK;
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;
        case XPLR_CELL_HTTP_CERT_METHOD_PSWD:
            if (res > -1) {
                if ((client->credentials.user != NULL) &&
                    (client->credentials.password != NULL)) {
                    connection->pUserName = client->credentials.user;
                    connection->pPassword = client->credentials.password;
                    tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_ANY;
                    tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_NONE;
                    tlsSettings->pSni = sni;
                    tlsSettings->cipherSuites.num = 0;
                    ret = XPLR_CELL_HTTP_OK;
                } else {
                    ret = XPLR_CELL_HTTP_ERROR;
                }
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;
        case XPLR_CELL_HTTP_CERT_METHOD_ROOTCA:
            if (res > -1) {
                if (client->credentials.rootCaName != NULL) {
                    tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;
                    tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA;
                    tlsSettings->pRootCaCertificateName = client->credentials.rootCaName;
                    tlsSettings->pSni = sni;
                    tlsSettings->cipherSuites.num = 0;
                    ret = XPLR_CELL_HTTP_OK;
                } else {
                    ret = XPLR_CELL_HTTP_ERROR;
                }
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;
        case XPLR_CELL_HTTP_CERT_METHOD_TLS:
            if (res > -1) {
                if ((client->credentials.rootCaName != NULL) &&
                    (client->credentials.certName != NULL)) {
                    tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;
                    tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA;
                    tlsSettings->pRootCaCertificateName = client->credentials.rootCaName;
                    tlsSettings->pClientCertificateName = client->credentials.certName;
                    tlsSettings->pSni = sni;
                    tlsSettings->cipherSuites.num = 0;
                    ret = XPLR_CELL_HTTP_OK;
                } else {
                    ret = XPLR_CELL_HTTP_ERROR;
                }
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;
        case XPLR_CELL_HTTP_CERT_METHOD_TLS_KEY:
            if (res > -1) {
                if ((client->credentials.rootCaName != NULL) &&
                    (client->credentials.certName != NULL) &&
                    (client->credentials.keyName != NULL)) {
                    tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;
                    tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA;
                    tlsSettings->pRootCaCertificateName = client->credentials.rootCaName;
                    tlsSettings->pClientCertificateName = client->credentials.certName;
                    tlsSettings->pClientPrivateKeyName = client->credentials.keyName;
                    tlsSettings->pSni = sni;
                    tlsSettings->cipherSuites.num = 0;
                    ret = XPLR_CELL_HTTP_OK;
                } else {
                    ret = XPLR_CELL_HTTP_ERROR;
                }
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;
        case XPLR_CELL_HTTP_CERT_METHOD_TLS_KEY_PSWD:
            if (res > -1) {
                if ((client->credentials.rootCaName != NULL) &&
                    (client->credentials.certName != NULL) &&
                    (client->credentials.keyName != NULL) &&
                    (client->credentials.keyPassword != NULL)) {
                    tlsSettings->tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;

                    tlsSettings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA;
                    tlsSettings->pRootCaCertificateName = client->credentials.rootCaName;
                    tlsSettings->pClientCertificateName = client->credentials.certName;
                    tlsSettings->pClientPrivateKeyName = client->credentials.keyName;
                    tlsSettings->pClientPrivateKeyPassword = client->credentials.keyPassword;
                    tlsSettings->pSni = sni;
                    tlsSettings->cipherSuites.num = 0;
                    ret = XPLR_CELL_HTTP_OK;
                } else {
                    ret = XPLR_CELL_HTTP_ERROR;
                }
            } else {
                ret = XPLR_CELL_HTTP_ERROR;
            }
            break;

        default:
            ret = XPLR_CELL_HTTP_ERROR;
            break;
    }

    if (ret == XPLR_CELL_HTTP_OK) {
        context = pUHttpClientOpen(handler, connection, tlsSettings);
        if (context != NULL) {
            http[dvcProfile].clientContext[clientId] = context;   /* update client context */
            XPLRCELL_HTTP_CONSOLE(D, "Device %d, Http client %d connected to %s ok.",
                                  dvcProfile, clientId, connection->pServerName);
            ret = XPLR_CELL_HTTP_OK;
        } else {
            XPLRCELL_HTTP_CONSOLE(E, "Device %d, Http client %d connection to %s failed.",
                                  dvcProfile, clientId, connection->pServerName);
            ret = XPLR_CELL_HTTP_ERROR;
        }
    } else {
        XPLRCELL_HTTP_CONSOLE(E, "Device %d, Http client %d failed while connecting to %s.",
                              dvcProfile, clientId, connection->pServerName);
    }

    return ret;
}

// End of file
