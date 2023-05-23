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
#include "xplr_mqtt_client.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "u_port_os.h"      // Required by u_cell_private.h
#include "u_cell_net.h"     // Required by u_cell_private.
#include "./../../../components/ubxlib/cell/src/u_cell_private.h" // So that we can get at some innards
#include "u_at_client.h"
#include "u_cell_mqtt.h"

#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/com_service/xplr_com.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR      (3)
#define XPLRCELL_MQTT_TOKEN_LENGTH              (44-1)
#define XPLRCELL_MQTT_PP_TOKEN_LENGTH           (XPLRCELL_MQTT_TOKEN_LENGTH - 7)
#define XPLRCELL_MQTT_PP_MD5_LENGTH             (33)

#if (1 == XPLRCELL_MQTT_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRCELL_MQTT_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrMqttCell", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRCELL_MQTT_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

// *INDENT-OFF*
typedef struct xplrCell_Mqtt_type {
    int8_t                          *dvcProfile;    /**< hpglib device id */
    uDeviceHandle_t                 handler;        /**< ubxlib device handler */
    const uCellPrivateModule_t      *module;        /**< ubxlib private module of cell API */
    xplrCell_mqtt_client_t          *client[XPLRCELL_MQTT_NUMOF_CLIENTS]; /**< pointer to hpglib mqtt cell client module */
    uMqttClientContext_t            *clientContext[XPLRCELL_MQTT_NUMOF_CLIENTS]; /**< ubxlib private client context of mqtt */
    uMqttClientConnection_t         clientConnection[XPLRCELL_MQTT_NUMOF_CLIENTS]; /**< ubxlib private connection of mqtt */
    volatile bool                   msgAvailable[XPLRCELL_MQTT_NUMOF_CLIENTS];  /**< indicates if a message is available to read. */
    void                            (*msgReceived[XPLRCELL_MQTT_NUMOF_CLIENTS]) /**< array of function pointers to msg received callback. */
                                    (int32_t numUnread, void *received);
    void                            (*disconnected[XPLRCELL_MQTT_NUMOF_CLIENTS]) /**< array of function pointers to msg received callback. */
                                    (int32_t status, void *param);
} xplrCell_mqtt_t;
// *INDENT-ON*

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char nvsNamespace[] = "mqttCell_";
static xplrCell_mqtt_t mqtt[XPLRCOM_NUMOF_DEVICES];

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static xplrCell_mqtt_error_t mqttClientNvsInit(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientNvsLoad(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientNvsWriteDefaults(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientNvsReadConfig(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientNvsUpdate(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientNvsErase(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientCheckToken(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientCheckRoot(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientCheckCert(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientCheckKey(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientWriteRoot(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientWriteCert(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientWriteKey(int8_t dvcProfile, int8_t clientId);
static void mqttClientConfigTls(xplrCell_mqtt_client_t *client, uSecurityTlsSettings_t *settings);
static void mqttClientConfigBroker(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientStart(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientConnectTLS(int8_t dvcProfile,
                                                  int8_t clientId,
                                                  uSecurityTlsSettings_t *tlsSettings);
static xplrCell_mqtt_error_t mqttClientSubscribeToTopic(int8_t dvcProfile,
                                                        int8_t clientId,
                                                        xplrCell_mqtt_topic_t *topic);
static xplrCell_mqtt_error_t mqttClientSubscribeToTopicList(int8_t dvcProfile, int8_t clientId);
static xplrCell_mqtt_error_t mqttClientUnsubscribeFromTopic(int8_t dvcProfile,
                                                            int8_t clientId,
                                                            xplrCell_mqtt_topic_t *topic);
static xplrCell_mqtt_error_t mqttClientUnsubscribeFromTopicList(int8_t dvcProfile,
                                                                int8_t clientId);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCell_mqtt_error_t xplrCellMqttInit(int8_t dvcProfile,
                                       int8_t clientId,
                                       xplrCell_mqtt_client_t *client)
{
    xplrCell_mqtt_t *instance = &mqtt[clientId];
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_error_t ret;

    if (dvcProfile < XPLRCELL_MQTT_NUMOF_CLIENTS) {
        /* Check tha selected mqtt service is supported by the module */
        if (client->settings.useFlexService) {
            if (uCellMqttSnIsSupported(handler)) {
                XPLRCELL_MQTT_CONSOLE(D, "MQTT Flex is supported, continue...");
                ret = XPLR_CELL_MQTT_OK;
            } else {
                XPLRCELL_MQTT_CONSOLE(D, "MQTT Flex is not supported, Error.");
                ret = XPLR_CELL_MQTT_ERROR;
            }
        } else {
            if (uCellMqttIsSupported(handler)) {
                XPLRCELL_MQTT_CONSOLE(D, "MQTT is supported, continue...");
                ret = XPLR_CELL_MQTT_OK;
            } else {
                XPLRCELL_MQTT_CONSOLE(D, "MQTT is not supported, Error.");
                ret = XPLR_CELL_MQTT_ERROR;
            }
        }

        /* Continue init process if no error */
        if (ret == XPLR_CELL_MQTT_OK) {
            memset(instance, 0x00, sizeof(xplrCell_mqtt_t));
            /* Retrieve user settings to configure device */
            instance->dvcProfile = &dvcProfile;
            instance->module = pUCellPrivateGetModule(&handler);
            instance->client[clientId] = client;
            instance->client[clientId]->id = clientId;
            instance->client[clientId]->fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_CHECK_MODULE_CREDENTIALS;
            instance->msgReceived[clientId] = client->msgReceived;
            XPLRCELL_MQTT_CONSOLE(D, "init ok.");
            ret = XPLR_CELL_MQTT_OK;
        } else {
            XPLRCELL_MQTT_CONSOLE(E, "Unknown error during module initialization.");
            ret = XPLR_CELL_MQTT_ERROR;
        }

        /* init nvs of mqtt client */
        if (ret == XPLR_CELL_MQTT_OK) {
            ret = mqttClientNvsInit(dvcProfile, clientId);
        }

        /* load settings (if available) */
        if (ret == XPLR_CELL_MQTT_OK) {
            ret = mqttClientNvsLoad(dvcProfile, clientId);
        }
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "init error, profile %d out of index.", dvcProfile);
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

void xplrCellMqttDeInit(int8_t dvcProfile, int8_t clientId)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];

    uMqttClientClose(ubxClientPrv);
    XPLRCELL_MQTT_CONSOLE(D, "Client %d closed ok.", clientId);
}

xplrCell_mqtt_error_t xplrCellMqttDisconnect(int8_t dvcProfile, int8_t clientId)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    bool isConnected;
    xplrCell_mqtt_error_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);
    if (isConnected) {
        ubxErrorCode = uMqttClientDisconnect(ubxClientPrv);
        if (ubxErrorCode != 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Error disconnecting client %d from broker.", clientId);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            XPLRCELL_MQTT_CONSOLE(D, "Client %d disconnected ok.", clientId);
            ret = XPLR_CELL_MQTT_OK;
        }
    } else {
        XPLRCELL_MQTT_CONSOLE(W, "Client %d is not connected to a broker.", clientId);
        ret = XPLR_CELL_MQTT_OK;
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellMqttSubscribeToTopic(int8_t dvcProfile,
                                                   int8_t clientId,
                                                   xplrCell_mqtt_topic_t *topic)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    bool isConnected;
    xplrCell_mqtt_error_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);

    if (isConnected) {
        ret = mqttClientSubscribeToTopic(dvcProfile, clientId, topic);
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellMqttSubscribeToTopicList(int8_t dvcProfile,
                                                       int8_t clientId)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    bool isConnected;
    xplrCell_mqtt_error_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);

    if (isConnected) {
        ret = mqttClientSubscribeToTopicList(dvcProfile, clientId);
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopic(int8_t dvcProfile,
                                                       int8_t clientId,
                                                       xplrCell_mqtt_topic_t *topic)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    bool isConnected;
    xplrCell_mqtt_error_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);

    if (isConnected) {
        ret = mqttClientUnsubscribeFromTopic(dvcProfile, clientId, topic);
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellMqttUnsubscribeFromTopicList(int8_t dvcProfile,
                                                           int8_t clientId)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    bool isConnected;
    xplrCell_mqtt_error_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);

    if (isConnected) {
        ret = mqttClientUnsubscribeFromTopicList(dvcProfile, clientId);
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

int32_t xplrCellMqttGetNumofMsgAvailable(int8_t dvcProfile, int8_t clientId)
{
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxResult;

    ubxResult = uMqttClientGetUnread(ubxClientPrv);
    if (ubxResult < 0) {
        XPLRCELL_MQTT_CONSOLE(W, "Could not get number of unread messages from client %d. Error: %d",
                              clientId, ubxResult);
    } else {
        if (ubxResult > 0) {
            XPLRCELL_MQTT_CONSOLE(D, "There are %d messages unread for Client %d.",
                                  ubxResult, clientId);
        }
    }

    return ubxResult;
}

int32_t xplrCellMqttUpdateTopicList(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    bool isConnected;
    char name[XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_NAME] = {0};
    char buffer[XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_PAYLOAD] = {0};
    size_t  bufferSizeOut;
    uint32_t numOfBytesRead = 0;
    uint8_t numOfMsgAvailable = 0;
    int32_t ret;

    isConnected = uMqttClientIsConnected(ubxClientPrv);
    if (isConnected) {
        if (client->settings.useFlexService) {
            ret = 0;
        } else {
            numOfMsgAvailable = xplrCellMqttGetNumofMsgAvailable(dvcProfile, clientId);
            ret = 0;
            for (uint8_t msg = 0; msg < numOfMsgAvailable; msg++) {
                bufferSizeOut = XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_PAYLOAD;
                ubxErrorCode = uMqttClientMessageRead(ubxClientPrv,
                                                      name,
                                                      XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_NAME,
                                                      buffer,
                                                      &bufferSizeOut,
                                                      NULL);

                if (ubxErrorCode < 0) {
                    XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to read topic %s with code (%d).",
                                          clientId, name, ubxErrorCode);
                    ret = -1;
                } else {
                    numOfBytesRead = bufferSizeOut;
                    XPLRCELL_MQTT_CONSOLE(D, "Client %d read %d bytes from topic %s.",
                                          clientId, numOfBytesRead, name);
                    ret = numOfBytesRead;
                }

                if (numOfBytesRead > 0) {
                    for (uint8_t i = 0; i < client->numOfTopics; i++) {
                        if (strstr(client->topicList[i].name, name) != NULL) {
                            if (client->topicList[i].rxBufferSize >= numOfBytesRead) {
                                memcpy(client->topicList[i].rxBuffer, buffer, numOfBytesRead);
                                client->topicList[i].msgSize = numOfBytesRead;
                                client->topicList[i].msgAvailable = true;
                                XPLRCELL_MQTT_CONSOLE(D, "Client %d, topic %s updated. Msg size %d bytes",
                                                      clientId, client->topicList[i].name, numOfBytesRead);
                            } else {
                                XPLRCELL_MQTT_CONSOLE(W, "Client %d, topic %s is out of space.",
                                                      clientId, client->topicList[i].name);
                                ret = -2;
                            }
                            break;
                        }
                    }
                }
            }
        }
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "Client %d not connected to broker.", clientId);
        ret = -4;
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellFactoryReset(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    int32_t ubxRes;
    xplrCell_mqtt_error_t nvsRes;
    xplrCell_mqtt_error_t ret;

    ubxRes = uCellCfgFactoryReset(handler, 1, 0);

    ubxRes = uSecurityCredentialRemove(handler, U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                       client->credentials.rootCaName);
    ubxRes = uSecurityCredentialRemove(handler, U_SECURITY_CREDENTIAL_CLIENT_X509,
                                       client->credentials.certName);
    ubxRes = uSecurityCredentialRemove(handler, U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                       client->credentials.keyName);

    nvsRes = mqttClientNvsErase(dvcProfile, clientId);

    if ((ubxRes == 0) && (nvsRes == XPLR_CELL_MQTT_OK)) {
        ret = XPLR_CELL_MQTT_OK;
        XPLRCELL_MQTT_CONSOLE(W, "Factory reset completed OK, please restart the device.");
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
        XPLRCELL_MQTT_CONSOLE(E, "Factory reset error, please restart the device.");
    }

    return ret;
}

xplrCell_mqtt_error_t xplrCellMqttFsmRun(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_fsm_t *fsm = mqtt[dvcProfile].client[clientId]->fsm;
    xplrCell_mqtt_error_t res[4];
    xplrCell_mqtt_error_t ret;
    static int32_t retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;

    switch (fsm[0]) {
        case XPLR_CELL_MQTT_CLIENT_FSM_CHECK_MODULE_CREDENTIALS:
            XPLRCELL_MQTT_CONSOLE(D, "Checking module %d, client %d for credentials.", dvcProfile, clientId);
            fsm[1] = fsm[0];
            res[0] = mqttClientCheckToken(dvcProfile, clientId);
            res[1] = mqttClientCheckRoot(dvcProfile, clientId);
            res[2] = mqttClientCheckCert(dvcProfile, clientId);
            res[3] = mqttClientCheckKey(dvcProfile, clientId);

            /* Check for errors */
            for (int8_t i = 0; i < 4; i++) {
                if (res[i] != XPLR_CELL_MQTT_OK) {
                    ret = XPLR_CELL_MQTT_ERROR;
                    break;
                } else {
                    ret = XPLR_CELL_MQTT_OK;
                }
            }

            /* Advance to next state depending on former check result */
            if (ret == XPLR_CELL_MQTT_OK) {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE;
                XPLRCELL_MQTT_CONSOLE(D, "Credentials chain is OK.");
            } else {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_WRITE_MODULE_CREDENTIALS;
                XPLRCELL_MQTT_CONSOLE(W, "Credentials chain contains ERRORs.");
            }

            ret = XPLR_CELL_MQTT_OK; /* mask return result since there cannot be an actual error */
            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_WRITE_MODULE_CREDENTIALS:
            XPLRCELL_MQTT_CONSOLE(D, "Writing module %d, client %d credentials.", dvcProfile, clientId);
            fsm[1] = fsm[0];
            res[0] = mqttClientWriteRoot(dvcProfile, clientId);
            res[1] = mqttClientWriteKey(dvcProfile, clientId);
            res[2] = mqttClientWriteCert(dvcProfile, clientId);

            /* Check for errors */
            for (int8_t i = 0; i < 3; i++) {
                if (res[i] != XPLR_CELL_MQTT_OK) {
                    ret = XPLR_CELL_MQTT_ERROR;
                    break;
                } else {
                    ret = XPLR_CELL_MQTT_OK;
                }
            }

            /* update nvs */
            ret = mqttClientNvsUpdate(dvcProfile, clientId);

            /* Advance to next state depending on former check result */
            if (ret == XPLR_CELL_MQTT_OK) {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE;
                XPLRCELL_MQTT_CONSOLE(D, "Credentials chain stored OK.");
            } else {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
                XPLRCELL_MQTT_CONSOLE(W, "Credentials chain could not be stored, going to Error state.");
            }
            ret = XPLR_CELL_MQTT_OK;

            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE:
            XPLRCELL_MQTT_CONSOLE(D, "Load module %d, client %d credentials.", dvcProfile, clientId);
            fsm[1] = fsm[0];
            /* Check security mode and make the appropriate connection */
            ret = mqttClientStart(dvcProfile, clientId);

            if (ret == XPLR_CELL_MQTT_OK) {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_READY;
                XPLRCELL_MQTT_CONSOLE(I, "MQTT client is connected.");
            } else {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
                ret = XPLR_CELL_MQTT_BUSY;
                XPLRCELL_MQTT_CONSOLE(E, "MQTT client failed to connect, going to Error state.");
            }

            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_READY:
            fsm[1] = fsm[0];
            if (xplrCellMqttUpdateTopicList(dvcProfile, clientId) >= 0) {
            } else {
                fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
            }
            ret = XPLR_CELL_MQTT_OK;

            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_BUSY:
            ret = XPLR_CELL_MQTT_OK;
            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_TIMEOUT:
            ret = XPLR_CELL_MQTT_ERROR;
            break;
        case XPLR_CELL_MQTT_CLIENT_FSM_ERROR:
            if (fsm[1] == XPLR_CELL_MQTT_CLIENT_FSM_READY) {
                /*
                 * incase of msg parsing error, retry parse
                 * for XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR times.
                 * if error persists then disconnect and reconnect.
                 */
                if (retries > 0) {
                    fsm[1] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
                    fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_READY;
                    retries--;
                    XPLRCELL_MQTT_CONSOLE(W,
                                          "Client %d failed to read message from broker. Retrying...(%d).",
                                          clientId, retries + 1);
                } else {
                    /*
                     * failed to read message. might be connection problem.
                     * try disconnecting from broker and connecting back
                    */
                    retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
                    fsm[1] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
                    fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE;
                    xplrCellMqttDisconnect(dvcProfile, clientId);
                    XPLRCELL_MQTT_CONSOLE(W,
                                          "Client %d failed to communicate with broker. Reconnecting...",
                                          clientId);
                    ret = XPLR_CELL_MQTT_OK;
                }
                ret = XPLR_CELL_MQTT_OK;
            } else if (fsm[1] == XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE) {
                /*
                 * incase of connection error, retry connect for XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR
                 * and then fail.
                 */
                if (retries > 0) {
                    fsm[1] = XPLR_CELL_MQTT_CLIENT_FSM_ERROR;
                    fsm[0] = XPLR_CELL_MQTT_CLIENT_FSM_INIT_MODULE;
                    xplrCellMqttDisconnect(dvcProfile, clientId);
                    retries--;
                    ret = XPLR_CELL_MQTT_BUSY;
                    XPLRCELL_MQTT_CONSOLE(W,
                                          "Client %d failed to connect. Retrying to connect (%d).",
                                          clientId, retries + 1);
                } else {
                    retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
                    XPLRCELL_MQTT_CONSOLE(E,
                                          "Client %d failed to connect. Going to Error.",
                                          clientId);
                    ret = XPLR_CELL_MQTT_ERROR;
                }
            } else {
                retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
                XPLRCELL_MQTT_CONSOLE(E,
                                      "Client %d unexpected Error.",
                                      clientId);
                ret = XPLR_CELL_MQTT_ERROR;
            }
            break;
        default:
            retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
            ret = XPLR_CELL_MQTT_ERROR;
            break;
    }

    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrCell_mqtt_error_t mqttClientNvsInit(int8_t dvcProfile, int8_t clientId)
{
    char id[3] = {0x00};
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err;
    xplrCell_mqtt_error_t ret;

    /* create namespace tag for given client */
    sprintf(id, "%u", clientId);
    memset(storage->nvs.tag, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES);
    memset(storage->id, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES);
    strcat(storage->nvs.tag, nvsNamespace);
    strcat(storage->id, storage->nvs.tag);
    strcat(storage->id, id);
    /* init nvs */
    XPLRCELL_MQTT_CONSOLE(D, "Trying to init nvs namespace <%s>.", storage->id);
    err = xplrNvsInit(&storage->nvs, storage->id);

    if (err != XPLR_NVS_OK) {
        ret = XPLR_CELL_MQTT_ERROR;
        XPLRCELL_MQTT_CONSOLE(E, "Failed to init nvs namespace <%s>.", storage->id);
    } else {
        XPLRCELL_MQTT_CONSOLE(D, "nvs namespace <%s> for cell mqtt client, init ok", storage->id);
        ret = XPLR_CELL_MQTT_OK;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientNvsLoad(int8_t dvcProfile, int8_t clientId)
{
    char storedId[NVS_KEY_NAME_MAX_SIZE] = {0x00};
    bool writeDefaults;
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err;
    size_t size = NVS_KEY_NAME_MAX_SIZE;
    xplrCell_mqtt_error_t ret;

    /* try read id key */
    err = xplrNvsReadString(&storage->nvs, "id", storedId, &size);
    if ((err != XPLR_NVS_OK) || (strlen(storedId) < 1)) {
        writeDefaults = true;
        XPLRCELL_MQTT_CONSOLE(W, "id key not found in <%s>, write defaults", storage->id);
    } else {
        writeDefaults = false;
        XPLRCELL_MQTT_CONSOLE(D, "id key <%s> found in <%s>", storedId, storage->id);
    }

    if (writeDefaults) {
        ret = mqttClientNvsWriteDefaults(dvcProfile, clientId);
        if (ret == XPLR_CELL_MQTT_OK) {
            ret = mqttClientNvsReadConfig(dvcProfile, clientId);
        }
    } else {
        ret = mqttClientNvsReadConfig(dvcProfile, clientId);
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientNvsWriteDefaults(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[4];
    xplrCell_mqtt_error_t ret;

    XPLRCELL_MQTT_CONSOLE(D, "Writing default settings in NVS");
    err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
    err[1] = xplrNvsWriteString(&storage->nvs, "ppRootCa", "invalid");
    err[2] = xplrNvsWriteString(&storage->nvs, "ppCert", "invalid");
    err[3] = xplrNvsWriteString(&storage->nvs, "ppKey", "invalid");

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_MQTT_ERROR;
            XPLRCELL_MQTT_CONSOLE(E, "Error writing element %u of default settings in NVS", i);
            break;
        } else {
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientNvsReadConfig(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[4];
    size_t size[] = {U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES,
                     2 * U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES + 1,
                     2 * U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES + 1,
                     2 * U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES + 1
                    };
    xplrCell_mqtt_error_t ret;

    err[0] = xplrNvsReadString(&storage->nvs, "id", storage->id, &size[0]);
    err[1] = xplrNvsReadStringHex(&storage->nvs, "ppRootCa", storage->md5RootCa, &size[1]);
    err[2] = xplrNvsReadStringHex(&storage->nvs, "ppCert", storage->md5PpCert, &size[2]);
    err[3] = xplrNvsReadStringHex(&storage->nvs, "ppKey", storage->md5PpKey, &size[3]);

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_MQTT_ERROR;
            break;
        } else {
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    if (ret == XPLR_CELL_MQTT_OK) {
        XPLRCELL_MQTT_CONSOLE(D, "id: <%s>", storage->id);
        XPLRCELL_MQTT_CONSOLE(D, "ppRootCa: <0x%x>", storage->md5RootCa);
        XPLRCELL_MQTT_CONSOLE(D, "ppCert: <0x%x>", storage->md5PpCert);
        XPLRCELL_MQTT_CONSOLE(D, "ppKey: <0x%x>", storage->md5PpKey);
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientNvsUpdate(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[4];
    xplrCell_mqtt_error_t ret;

    if ((storage->id != NULL) &&
        (storage->md5RootCa != NULL) &&
        (storage->md5PpCert != NULL) &&
        (storage->md5PpKey != NULL)) {
        err[0] = xplrNvsWriteString(&storage->nvs, "id", storage->id);
        err[1] = xplrNvsWriteStringHex(&storage->nvs, "ppRootCa", storage->md5RootCa);
        err[2] = xplrNvsWriteStringHex(&storage->nvs, "ppCert", storage->md5PpCert);
        err[3] = xplrNvsWriteStringHex(&storage->nvs, "ppKey", storage->md5PpKey);

        for (int i = 0; i < 4; i++) {
            if (err[i] != XPLR_NVS_OK) {
                ret = XPLR_CELL_MQTT_ERROR;
                break;
            } else {
                ret = XPLR_CELL_MQTT_OK;
            }
        }
    } else {
        ret = XPLR_CELL_MQTT_ERROR;
        XPLRCELL_MQTT_CONSOLE(E, "Trying to write invalid config, error");
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientNvsErase(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    xplrNvs_error_t err[4];
    xplrCell_mqtt_error_t ret;

    err[0] = xplrNvsEraseKey(&storage->nvs, "id");
    err[1] = xplrNvsEraseKey(&storage->nvs, "ppRootCa");
    err[2] = xplrNvsEraseKey(&storage->nvs, "ppCert");
    err[3] = xplrNvsEraseKey(&storage->nvs, "ppKey");

    for (int i = 0; i < 4; i++) {
        if (err[i] != XPLR_NVS_OK) {
            ret = XPLR_CELL_MQTT_ERROR;
            break;
        } else {
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientCheckToken(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    xplrCell_mqtt_error_t ret;

    if ((strstr(client->credentials.token, "device:") != NULL) &&
        (strlen(client->credentials.token) == XPLRCELL_MQTT_TOKEN_LENGTH)) {
        XPLRCELL_MQTT_CONSOLE(D, "Token OK.");
        ret = XPLR_CELL_MQTT_OK;
    } else if (strlen(client->credentials.token) == XPLRCELL_MQTT_PP_TOKEN_LENGTH) {
        XPLRCELL_MQTT_CONSOLE(D, "PP Token OK.");
        ret = XPLR_CELL_MQTT_OK;
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "Token ERROR.");
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientCheckRoot(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    char cellMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1] = {0};
    char *nvsMd5 = storage->md5RootCa;
    char appMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES] = {0};
    int32_t res;
    xplrCell_mqtt_error_t ret;

    /* calculate md5 hash of rootCa provided by user */
    if (client->credentials.rootCa != NULL) {
        res = xplrCommonMd5Get((const unsigned char *)client->credentials.rootCa,
                               strlen(client->credentials.rootCa),
                               (unsigned char *)appMd5);
        XPLRCELL_MQTT_CONSOLE(D, "MD5 hash of rootCa (user) is <0x%x>", appMd5);

        /* fetch md5 hash from modules' memory (will be different).
        * needed to see if there is a certificate stored in modules memory.
        */
        res = uSecurityCredentialGetHash(handler,
                                         U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                         client->credentials.rootCaName,
                                         cellMd5);

        if (res != 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Error (%d) checking MD5 hash of RootCa in modules memory",
                                  res);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            /* compare user md5 with one stored in nvs */
            if (memcmp(appMd5, nvsMd5, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES) == 0) {
                XPLRCELL_MQTT_CONSOLE(D, "User and NVS Root certificate OK.");
                XPLRCELL_MQTT_CONSOLE(I, "Root Certificate verified OK.");
                ret = XPLR_CELL_MQTT_OK;
            } else {
                memset(nvsMd5, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1);
                memcpy(nvsMd5, appMd5, strlen(appMd5));
                XPLRCELL_MQTT_CONSOLE(W, "User and NVS Root Certificate mismatch.");
                ret = XPLR_CELL_MQTT_ERROR;
            }
        }
    } else {
        /* rootCa not available, error */
        res = -1;
        XPLRCELL_MQTT_CONSOLE(E, "Error (%d) calculating user MD5 hash", res);
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientCheckCert(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    char cellMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1] = {0};
    char *nvsMd5 = storage->md5PpCert;
    char appMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES] = {0};
    int32_t res;
    xplrCell_mqtt_error_t ret;

    /* calculate md5 hash of pp client cert provided by user */
    if (client->credentials.cert != NULL) {
        res = xplrCommonMd5Get((const unsigned char *)client->credentials.cert,
                               strlen(client->credentials.cert),
                               (unsigned char *)appMd5);
        XPLRCELL_MQTT_CONSOLE(D, "MD5 hash of client cert (user) is <0x%x>", appMd5);

        /* fetch md5 hash from modules' memory (will be different).
        * needed to see if there is a certificate stored in modules memory.
        */
        res = uSecurityCredentialGetHash(handler,
                                         U_SECURITY_CREDENTIAL_CLIENT_X509,
                                         client->credentials.certName,
                                         cellMd5);

        if (res != 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Error (%d) checking MD5 hash of client cert in modules memory",
                                  res);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            /* compare user md5 with one stored in nvs */
            if (memcmp(appMd5, nvsMd5, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES) == 0) {
                XPLRCELL_MQTT_CONSOLE(D, "User and NVS Client certificate OK.");
                XPLRCELL_MQTT_CONSOLE(I, "Client Certificate verified OK.");
                ret = XPLR_CELL_MQTT_OK;
            } else {
                memset(nvsMd5, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1);
                memcpy(nvsMd5, appMd5, strlen(appMd5));
                XPLRCELL_MQTT_CONSOLE(W, "User and NVS Client certificate mismatch.");
                ret = XPLR_CELL_MQTT_ERROR;
            }
        }
    } else {
        /* client certificate not available, error */
        res = -1;
        XPLRCELL_MQTT_CONSOLE(E, "Error (%d) calculating user MD5 hash", res);
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientCheckKey(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    xplrCell_mqtt_nvs_t *storage = &mqtt[dvcProfile].client[clientId]->storage;
    char cellMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1] = {0};
    char *nvsMd5 = storage->md5PpKey;
    char appMd5[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES] = {0};
    int32_t res;
    xplrCell_mqtt_error_t ret;

    /* calculate md5 hash of pp client key provided by user */
    if (client->credentials.key != NULL) {
        res = xplrCommonMd5Get((const unsigned char *)client->credentials.key,
                               strlen(client->credentials.key),
                               (unsigned char *)appMd5);
        XPLRCELL_MQTT_CONSOLE(D, "MD5 hash of client key (user) is <0x%x>", appMd5);

        /* fetch md5 hash from modules' memory (will be different).
        * needed to see if there is a certificate stored in modules memory.
        */
        res = uSecurityCredentialGetHash(handler,
                                         U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                         client->credentials.keyName,
                                         cellMd5);

        if (res != 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Error (%d) checking MD5 hash of client key in modules memory",
                                  res);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            /* compare user md5 with one stored in nvs */
            if (memcmp(appMd5, nvsMd5, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES) == 0) {
                XPLRCELL_MQTT_CONSOLE(D, "User and NVS Client key OK.");
                XPLRCELL_MQTT_CONSOLE(I, "Client key verified OK.");
                ret = XPLR_CELL_MQTT_OK;
            } else {
                memset(nvsMd5, 0x00, U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1);
                memcpy(nvsMd5, appMd5, strlen(appMd5));
                XPLRCELL_MQTT_CONSOLE(W, "User and NVS Client key mismatch.");
                ret = XPLR_CELL_MQTT_ERROR;
            }
        }
    } else {
        /* client key not available, error */
        res = -1;
        XPLRCELL_MQTT_CONSOLE(E, "Error (%d) calculating user MD5 hash", res);
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientWriteRoot(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    int8_t res;
    char md5[XPLRCELL_MQTT_PP_MD5_LENGTH] = {0};
    xplrCell_mqtt_error_t ret;

    /* Try deleting the key first */
    uSecurityCredentialRemove(handler,
                              U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                              client->credentials.rootCaName);
#if 0
    XPLRCELL_MQTT_CONSOLE(D, "Root certificate to be stored in memory:%s", client->credentials.rootCa);
#endif
    if ((client->credentials.rootCa != NULL) &&
        (client->credentials.rootCaName != NULL)) {
        res = uSecurityCredentialStore(handler,
                                       U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                       client->credentials.rootCaName,
                                       client->credentials.rootCa,
                                       strlen(client->credentials.rootCa),
                                       NULL, md5);

        if (res == 0) {
            XPLRCELL_MQTT_CONSOLE(D, "Root certificate stored in memory, md5 is <0x%x> ", md5);
            ret = XPLR_CELL_MQTT_OK;
        } else {
            XPLRCELL_MQTT_CONSOLE(E, "Error while storing Root certificate in memory.");
            ret = XPLR_CELL_MQTT_ERROR;
        }
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "RootCa not found. Error");
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientWriteCert(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    int8_t res;
    char md5[XPLRCELL_MQTT_PP_MD5_LENGTH] = {0};
    xplrCell_mqtt_error_t ret;

    /* Try deleting the key first */
    uSecurityCredentialRemove(handler,
                              U_SECURITY_CREDENTIAL_CLIENT_X509,
                              client->credentials.certName);
#if 0
    XPLRCELL_MQTT_CONSOLE(D, "Root certificate to be stored in memory:%s", client->credentials.cert);
#endif
    res = uSecurityCredentialStore(handler,
                                   U_SECURITY_CREDENTIAL_CLIENT_X509,
                                   client->credentials.certName,
                                   client->credentials.cert,
                                   strlen(client->credentials.cert),
                                   NULL, md5);

    if (res == 0) {
        XPLRCELL_MQTT_CONSOLE(D, "Client certificate stored in memory, md5 is <0x%x> ", md5);
        ret = XPLR_CELL_MQTT_OK;
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "Error while storing client certificate in memory.");
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientWriteKey(int8_t dvcProfile, int8_t clientId)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    int8_t res;
    char md5[XPLRCELL_MQTT_PP_MD5_LENGTH] = {0};
    xplrCell_mqtt_error_t ret;

    /* Try deleting the key first */
    uSecurityCredentialRemove(handler,
                              U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                              client->credentials.keyName);

    res = uSecurityCredentialStore(handler,
                                   U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                   client->credentials.keyName,
                                   client->credentials.key,
                                   strlen(client->credentials.key),
                                   NULL, md5);

    if (res == 0) {
        XPLRCELL_MQTT_CONSOLE(D, "Client key stored in memory, md5 is <0x%x> ", md5);
        ret = XPLR_CELL_MQTT_OK;
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "Error while storing client key in memory.");
        ret = XPLR_CELL_MQTT_ERROR;
    }

    return ret;
}

void mqttClientConfigTls(xplrCell_mqtt_client_t *client, uSecurityTlsSettings_t *settings)
{
    settings->tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;
    settings->pRootCaCertificateName = client->credentials.rootCaName;
    settings->pClientCertificateName = client->credentials.certName;
    settings->pClientPrivateKeyName = client->credentials.keyName;
    settings->pExpectedServerUrl = NULL;
    settings->pSni = NULL;
    settings->psk.pBin = NULL;
    settings->psk.size = 0;
    settings->pskId.pBin = NULL;
    settings->pskId.size = 0;
    settings->pClientPrivateKeyPassword = NULL;
    settings->certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_NONE;
    settings->cipherSuites.num = 1;
    settings->cipherSuites.suite[0] = U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
}

void mqttClientConfigBroker(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientConnection_t *ubxConnPrv = &mqtt[dvcProfile].clientConnection[clientId];

    /* configure ubxlib internal settings */
    ubxConnPrv->pBrokerNameStr = client->settings.brokerAddress;
    ubxConnPrv->pClientIdStr = client->credentials.token;
    /* check if we are using flex client*/
    if (client->settings.useFlexService) {
        ubxConnPrv->mqttSn = true;
    } else {
        ubxConnPrv->mqttSn = false;
    }
    /* check retain option. */
    if (client->settings.retainMsg) {
        ubxConnPrv->retain = true;
    } else {
        ubxConnPrv->retain = false;
    }
    /* set local port of mqtt client to -1. Not to be confused with brokers' port. */
    ubxConnPrv->localPort = -1;

    /* set keep alive option. */
    if (client->settings.keepAliveTime > 0) {
        ubxConnPrv->keepAlive = true;
    } else {
        ubxConnPrv->keepAlive = false;
    }
    /* set inactivity timeout option. */
    ubxConnPrv->inactivityTimeoutSeconds = client->settings.inactivityTimeout;
}

xplrCell_mqtt_error_t mqttClientStart(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uSecurityTlsSettings_t UNUSED_PARAM(securityTlsSettings) = U_SECURITY_TLS_SETTINGS_DEFAULT;
    xplrCell_mqtt_error_t ret;

    switch (client->credentials.registerMethod) {
        case XPLR_CELL_MQTT_CERT_METHOD_NONE:
            ret = XPLR_CELL_MQTT_OK;
            break;
        case XPLR_CELL_MQTT_CERT_METHOD_TLS:
            /* configure client settings and connect to the broker */
            ret = mqttClientConnectTLS(dvcProfile, clientId, &securityTlsSettings);

            if (ret == XPLR_CELL_MQTT_OK) {
                /* subscribe to topics */
                ret = mqttClientSubscribeToTopicList(dvcProfile, clientId);
            }
            break;
        case XPLR_CELL_MQTT_CERT_METHOD_PWD:
            ret = XPLR_CELL_MQTT_OK;
            break;

        default:
            ret = XPLR_CELL_MQTT_ERROR;
            break;
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientConnectTLS(int8_t dvcProfile,
                                           int8_t clientId,
                                           uSecurityTlsSettings_t *tlsSettings)
{
    uDeviceHandle_t  handler = xplrComGetDeviceHandler(dvcProfile);
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    uMqttClientConnection_t *ubxConnPrv = &mqtt[dvcProfile].clientConnection[clientId];
    int32_t ubxErrorCode;
    xplrCell_mqtt_error_t ret;

    /* reset private ubxlib structures */
    ubxClientPrv = NULL;
    memset(ubxConnPrv, 0x00, sizeof(uMqttClientConnection_t));

    /* config tls settings and open the ubxlib client instance */
    mqttClientConfigTls(client, tlsSettings);
    ubxClientPrv = pUMqttClientOpen(handler, tlsSettings);
    mqtt[dvcProfile].clientContext[clientId] = ubxClientPrv;   /* update client context */
    if (ubxClientPrv != NULL) {
        XPLRCELL_MQTT_CONSOLE(D, "Client config OK.");
        ret = XPLR_CELL_MQTT_OK;
    } else {
        XPLRCELL_MQTT_CONSOLE(E, "Client config Error.");
        ret = XPLR_CELL_MQTT_ERROR;
    }

    /* make actual connection to the broker */
    if (ret == XPLR_CELL_MQTT_OK) {
        mqttClientConfigBroker(dvcProfile, clientId);
        ubxErrorCode = uMqttClientConnect(ubxClientPrv, ubxConnPrv);
        if (ubxErrorCode == 0) {
            XPLRCELL_MQTT_CONSOLE(D, "Client connection established.");
            ret = XPLR_CELL_MQTT_OK;
        } else {
            XPLRCELL_MQTT_CONSOLE(E, "Client connection Error (%d).", ubxErrorCode);
            ret = XPLR_CELL_MQTT_ERROR;
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientSubscribeToTopic(int8_t dvcProfile,
                                                 int8_t clientId,
                                                 xplrCell_mqtt_topic_t *topic)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    xplrCell_mqtt_error_t ret;

    if (client->settings.useFlexService) {
        ret = XPLR_CELL_MQTT_OK;
    } else {
        ubxErrorCode = uMqttClientSubscribe(ubxClientPrv,
                                            topic->name,
                                            client->settings.qos);

        if (ubxErrorCode < 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to subscribe to topic %s with code (%d).",
                                  clientId, topic->name, ubxErrorCode);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            XPLRCELL_MQTT_CONSOLE(D, "Client %d subscribed to %s.",
                                  clientId, topic->name);
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientSubscribeToTopicList(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    xplrCell_mqtt_error_t ret;
    static int32_t retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;

    /* Set up the callback to be called when new messages are available. The callback is fired as long as
       there is a message available to any of the subscribed topics. */
    ubxErrorCode = uMqttClientSetMessageCallback(ubxClientPrv,
                                                 mqtt[dvcProfile].msgReceived[clientId],
                                                 (void *) &mqtt[dvcProfile].msgAvailable[clientId]);

    if (ubxErrorCode != 0) {
        XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to set message indication callback with code (%d).",
                              client->id, ubxErrorCode);
        ret = XPLR_CELL_MQTT_ERROR;
    } else {
        ret = XPLR_CELL_MQTT_OK;
    }

    ubxErrorCode = uMqttClientSetDisconnectCallback(ubxClientPrv,
                                                    mqtt[dvcProfile].disconnected[clientId],
                                                    NULL);

    if (ubxErrorCode != 0) {
        XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to set disconnect callback with code (%d).",
                              client->id, ubxErrorCode);
        ret = XPLR_CELL_MQTT_ERROR;
    } else {
        ret = XPLR_CELL_MQTT_OK;
    }

    if (ret == XPLR_CELL_MQTT_OK) {
        /* subscribe to topics. */
        if (client->settings.useFlexService) {
            ret = XPLR_CELL_MQTT_OK;
        } else {
            if (client->numOfTopics > 0) {
                for (uint8_t i = 0; i < client->numOfTopics; i++) {
                    ubxErrorCode = uMqttClientSubscribe(ubxClientPrv,
                                                        client->topicList[i].name,
                                                        client->settings.qos);
                    if (ubxErrorCode < 0) {
                        if (retries > 0) {
                            i--;
                            retries--;
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            XPLRCELL_MQTT_CONSOLE(W,
                                                  "Client %d failed to subscribe to topic %s with code (%d). Retrying to subscribe (%d).",
                                                  clientId, client->topicList[i].name, ubxErrorCode, retries + 1);
                        } else {
                            XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to subscribe to topic %s with code (%d).",
                                                  clientId, client->topicList[i].name, ubxErrorCode);
                            retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
                            ret = XPLR_CELL_MQTT_ERROR;
                            break;
                        }
                    } else {
                        XPLRCELL_MQTT_CONSOLE(D, "Client %d subscribed to %s.",
                                              clientId, client->topicList[i].name);
                        retries = XPLRCELL_MQTT_MAX_RETRIES_ON_ERROR;
                        ret = XPLR_CELL_MQTT_OK;
                    }
                }
            } else {
                XPLRCELL_MQTT_CONSOLE(W, "No topics found in client %d list to subscribe.",
                                      clientId);
                ret = XPLR_CELL_MQTT_OK;
            }
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientUnsubscribeFromTopic(int8_t dvcProfile,
                                                     int8_t clientId,
                                                     xplrCell_mqtt_topic_t *topic)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    xplrCell_mqtt_error_t ret;

    if (client->settings.useFlexService) {
        ret = XPLR_CELL_MQTT_OK;
    } else {
        ubxErrorCode = uMqttClientUnsubscribe(ubxClientPrv,
                                              topic->name);

        if (ubxErrorCode < 0) {
            XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to unsubscribe from %s with code (%d).",
                                  clientId, topic->name, ubxErrorCode);
            ret = XPLR_CELL_MQTT_ERROR;
        } else {
            XPLRCELL_MQTT_CONSOLE(D, "Client %d unsubscribed from %s.",
                                  clientId, topic->name);
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    return ret;
}

xplrCell_mqtt_error_t mqttClientUnsubscribeFromTopicList(int8_t dvcProfile, int8_t clientId)
{
    xplrCell_mqtt_client_t *client = mqtt[dvcProfile].client[clientId];
    uMqttClientContext_t *ubxClientPrv = mqtt[dvcProfile].clientContext[clientId];
    int32_t ubxErrorCode;
    xplrCell_mqtt_error_t ret;

    if (client->settings.useFlexService) {
        // TODO: Support flex topics
        ret = XPLR_CELL_MQTT_OK;
    } else {
        if (client->numOfTopics > 0) {
            for (uint8_t i = 0; i < client->numOfTopics; i++) {
                ubxErrorCode = uMqttClientUnsubscribe(ubxClientPrv,
                                                      client->topicList[i].name);
                if (ubxErrorCode < 0) {
                    XPLRCELL_MQTT_CONSOLE(E, "Client %d failed to unsubscribe from %s with code (%d).",
                                          clientId, client->topicList[i].name, ubxErrorCode);
                    ret = XPLR_CELL_MQTT_ERROR;
                    break;
                } else {
                    XPLRCELL_MQTT_CONSOLE(D, "Client %d unsubscribed from %s.",
                                          clientId, client->topicList[i].name);
                    ret = XPLR_CELL_MQTT_OK;
                }
            }
        } else {
            XPLRCELL_MQTT_CONSOLE(W, "No topics found in client %d list to unsubscribe.",
                                  clientId);
            ret = XPLR_CELL_MQTT_OK;
        }
    }

    return ret;
}
