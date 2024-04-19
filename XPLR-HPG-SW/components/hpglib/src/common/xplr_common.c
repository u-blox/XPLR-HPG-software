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

#include "xplr_common.h"
#include "string.h"
#include "time.h"
#include "esp_mac.h"
#include "esp_err.h"
#include "mbedtls/md.h"
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "otp_defs.h"
#include "otp_reader.h"
#include "cJSON.h"

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t parseAppConfig(char *payload, xplr_cfg_app_t *appCfg);
static esp_err_t parseCellConfig(char *payload, xplr_cfg_cell_t *cellCfg);
static esp_err_t parseWifiConfig(char *payload, xplr_cfg_wifi_t *wifiCfg);
static esp_err_t parseTsConfig(char *payload, xplr_cfg_thingstream_t *tsCfg);
static esp_err_t parseNtripConfig(char *payload, xplr_cfg_ntrip_t *ntripCfg);
static esp_err_t parseLogConfig(char *payload, xplr_cfg_log_t *logCfg);
static esp_err_t parseDrConfig(char *payload, xplr_cfg_dr_t *drCfg);
static esp_err_t parseGnssConfig(char *payload, xplr_cfg_gnss_t *gnssCfg);
static bool hexToBin(const char *pHex, char *pBin);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

bool xplrCommonConvertHash(const char *pHex, char *pBin)
{
    bool success = true;

    for (size_t x = 0; (x < 16) && success; x++) {
        success = hexToBin(pHex, pBin);
        pHex += 2;
        pBin++;
    }

    return success;
}

int32_t xplrCommonMd5Get(const unsigned char *pInput, size_t size, unsigned char *pOut)
{
    mbedtls_md_context_t md5Ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
    //mbedtls_md_info_t md5Info;
    int32_t err[4];
    int32_t ret;

    mbedtls_md_init(&md5Ctx);
    err[0] = mbedtls_md_setup(&md5Ctx, mbedtls_md_info_from_type(md_type), 0);
    err[1] = mbedtls_md_starts(&md5Ctx);
    err[2] = mbedtls_md_update(&md5Ctx, pInput, size);
    err[3] = mbedtls_md_finish(&md5Ctx, pOut);
    mbedtls_md_free(&md5Ctx);

    for (int i = 0; i < 4; i++) {
        if (err != 0) {
            ret = err[i];
            break;
        } else {
            ret = 0;
        }
    }

    if (ret != 0) {
        memset(pOut, 0x00, 16);
    }

    return ret;
}

int32_t xplrRemovePortInfo(const char *serverUrl, char *serverName, size_t serverNameSize)
{
    int32_t urlSize = strlen(serverUrl);
    char *portInfo;
    int32_t portInfoIndex, charsToRemove;
    int32_t ret;

    if (serverNameSize < urlSize) {
        ret = -1; /* given buffer may not fit */
    } else {
        memcpy(serverName, serverUrl, urlSize);
        portInfo = strchr(serverUrl, ':');
        if (portInfo != NULL) {
            portInfoIndex = (int)(portInfo - serverUrl);
            charsToRemove = urlSize - portInfoIndex;
            memset(&serverName[portInfoIndex], 0x00, charsToRemove);
            ret = charsToRemove;
        } else {
            ret = 0;
        }
    }

    return ret;
}

int32_t xplrAddPortInfo(char *str, uint16_t port)
{
    char portStr[12];
    char *pStr;
    int32_t ret;

    sprintf(portStr, "%d", port);

    if (portStr != NULL) {
        pStr = strcat(str, ":");
        pStr = strcat(str, portStr);
        if (pStr != NULL) {
            ret = 0;
        } else {
            ret = -1;
        }
    } else {
        ret = -1;
    }

    return ret;
}

int32_t xplrRemoveChar(char *str, const char ch)
{
    int32_t i, j;
    int32_t len = strlen(str);
    int32_t ret = 0;
    for (i = 0; i < len; i++) {
        if (str[i] == ch) {
            for (j = i; j < len; j++) {
                str[j] = str[j + 1];
            }
            len--;
            i--;
            ret++;
        }
    }

    if (ret == 0) {
        ret = -1;
    }

    return ret;
}

int32_t xplrGetDeviceMac(uint8_t *mac)
{
    uint8_t dvcMac[6] = {0};
    esp_err_t err;
    int32_t ret;

    err = esp_read_mac(dvcMac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ret = -1;
    } else {
        memcpy(mac, dvcMac, 6);
        ret = 0;
    }

    return ret;
}

esp_err_t xplrSetDeviceMacToUblox(void)
{
    uint8_t mac_wifi_sta[cbOTP_SIZE_MAC];
    esp_err_t ret;

    if (otp_probe() == ESP_OK) {
        // Read the ublox MAC
        ret = otp_read_mac_wifi_sta(mac_wifi_sta);
        if (ret == ESP_OK) {
            // Set MCU base MAC to ublox MAC
            ret = esp_base_mac_addr_set(mac_wifi_sta);
            if (ret == ESP_OK) {
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
            }
        } else {
            ret = ESP_FAIL;
        }
    } else {
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrTimestampToDate(int64_t timeStamp, char *res, uint8_t maxLen)
{
    esp_err_t ret = ESP_OK;
    uint8_t intRet;
    time_t rawTimeStamp = timeStamp;
    struct tm ts = *localtime(&rawTimeStamp);

    intRet = strftime(res, maxLen, "%d.%m.%Y", &ts);
    if (intRet == 0) {
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrTimestampToTime(int64_t timeStamp, char *res, uint8_t maxLen)
{
    esp_err_t ret = ESP_OK;
    uint8_t intRet;
    time_t rawTimeStamp = timeStamp;
    struct tm ts = *localtime(&rawTimeStamp);

    intRet = strftime(res, maxLen, "%H:%M:%S", &ts);
    if (intRet == 0) {
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t xplrTimestampToDateTime(int64_t timeStamp, char *res, uint8_t maxLen)
{
    esp_err_t ret = ESP_OK;
    uint8_t intRet;
    time_t rawTimeStamp = timeStamp;
    struct tm ts = *localtime(&rawTimeStamp);

    intRet = strftime(res, maxLen, "%a %d.%m.%Y %H:%M:%S", &ts);
    if (intRet == 0) {
        ret = ESP_FAIL;
    }

    return ret;
}

int8_t xplrTimestampToDateTimeForFilename(int64_t timeStamp, char *res, uint8_t maxLen)
{
    int8_t ret;
    time_t rawTimeStamp = timeStamp;
    struct tm ts = *localtime(&rawTimeStamp);

    ret = strftime(res, maxLen, "%Y_%m_%d_%H_%M_%S_", &ts);
    if (ret == 0) {
        ret = -1;
    } else {
        /* Do nothing */
    }

    return ret;
}

void xplrMemUsagePrint(uint8_t periodSecs)
{
    static uint64_t prevTime;
    char taskList[720];
    uint32_t free, minFree, total, numOfTasks, maxBlock;
    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs)) {
        free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        minFree = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
        numOfTasks = uxTaskGetNumberOfTasks();
        printf("heap: min %d cur %d size %d maxBlock %d tasks: %d\n",
               minFree,
               free,
               total,
               maxBlock,
               numOfTasks);
        vTaskList(taskList);
        printf("Task List:\n%s\n", taskList);
        prevTime = MICROTOSEC(esp_timer_get_time());
    }
}

esp_err_t xplrParseConfigSettings(char *payload, xplr_cfg_t *settings)
{
    esp_err_t ret;
    esp_err_t err[8];

    if (payload == NULL || settings == NULL) {
        printf("Invalid Parameters. Cannot Parse Configuration\n");
        ret = ESP_FAIL;
    } else {
        err[0] = parseAppConfig(payload, &settings->appCfg);
        err[1] = parseCellConfig(payload, &settings->cellCfg);
        err[2] = parseWifiConfig(payload, &settings->wifiCfg);
        err[3] = parseTsConfig(payload, &settings->tsCfg);
        err[4] = parseNtripConfig(payload, &settings->ntripCfg);
        err[5] = parseLogConfig(payload, &settings->logCfg);
        err[6] = parseDrConfig(payload, &settings->drCfg);
        err[7] = parseGnssConfig(payload, &settings->gnssCfg);
        for (int i = 0; i < 8; i++) {
            if (err[i] != ESP_OK) {
                ret = ESP_FAIL;
                break;
            } else {
                ret = ESP_OK;
            }
        }
    }

    return ret;
}

esp_err_t xplrPpConfigFileFormatCert(char *cert, xplr_common_cert_type_t type, bool addNewLines)
{
    esp_err_t ret;
    char buf[2048] = {0};
    char footer[32] = {0};
    uint16_t times, timesMod;
    switch (type) {
        case XPLR_COMMON_CERT_KEY:
            strcpy(buf, "-----BEGIN RSA PRIVATE KEY-----");
            strcpy(footer, "-----END RSA PRIVATE KEY-----");
            if (addNewLines == true) {
                strcat(buf, "\n");
                strcat(footer, "\n");
            } else {
                //do nothing
            }
            ret = ESP_OK;
            break;
        case XPLR_COMMON_CERT_ROOTCA:
        case XPLR_COMMON_CERT:
            strcpy(buf, "-----BEGIN CERTIFICATE-----");
            strcpy(footer, "-----END CERTIFICATE-----");
            if (addNewLines == true) {
                strcat(buf, "\n");
                strcat(footer, "\n");
            } else {
                //do nothing
            }
            ret = ESP_OK;
            break;
        default:
            ret = ESP_FAIL;
            break;
    }
    if (cert != NULL && ret == ESP_OK) {
        times = strlen(cert) / 64;
        timesMod = strlen(cert) % 64;
        for (int i = 0; i <= times; i++) {
            memcpy(buf + strlen(buf), cert + i * 64, 64);
            if (addNewLines == true) {
                if ((timesMod != 0) || (i != times)) {
                    strcat(buf + strlen(buf), "\n");
                } else {
                    //Do nothing
                }
            }
        }
        strcat(buf, footer);
        memset(cert, 0x00, XPLR_COMMON_CERT_SIZE_MAX);
        memcpy(cert, buf, XPLR_COMMON_CERT_SIZE_MAX);
    } else {
        printf("Pointer to Certificate is NULL!\n");
        ret = ESP_FAIL;
    }
    return ret;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

bool hexToBin(const char *pHex, char *pBin)
{
    bool success = true;
    char y[2];

    y[0] = *pHex - '0';
    pHex++;
    y[1] = *pHex - '0';
    for (size_t x = 0; (x < sizeof(y)) && success; x++) {
        if (y[x] > 9) {
            // Must be A to F or a to f
            y[x] -= 'A' - '0';
            y[x] += 10;
        }
        if (y[x] > 15) {
            // Must be a to f
            y[x] -= 'a' - 'A';
        }
        // Cast here to shut-up a warning under ESP-IDF
        // which appears to have chars as unsigned and
        // hence thinks the first condition is always true
        success = ((signed char) y[x] >= 0) && (y[x] <= 15);
    }

    if (success) {
        *pBin = (char)(((y[0] & 0x0f) << 4) | y[1]);
    }

    return success;
}

static esp_err_t parseAppConfig(char *payload, xplr_cfg_app_t *appCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *appSettings;
    bool settingsFound, abort;

    if (payload == NULL || appCfg == NULL) {
        printf("Invalid Parameters. Cannot parse application configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (cJSON_HasObjectItem(root, "AppSettings")) {
            appSettings = cJSON_GetObjectItem(root, "AppSettings");
            /* Check for individual settings */
            settingsFound = cJSON_HasObjectItem(appSettings, "RunTimeUtc");
            settingsFound &= cJSON_HasObjectItem(appSettings, "LocationPrintInterval");
            settingsFound &= cJSON_HasObjectItem(appSettings, "StatisticsPrintInterval");
            settingsFound &= cJSON_HasObjectItem(appSettings, "MQTTClientWatchdogEnable");
            if (settingsFound) {
                abort = false;
                /* Parse individual settings */
                if (!abort) {
                    element = cJSON_GetObjectItem(appSettings, "RunTimeUtc");
                    if (cJSON_IsNumber(element)) {
                        appCfg->runTime = element->valuedouble;
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(appSettings, "LocationPrintInterval");
                    if (cJSON_IsNumber(element)) {
                        appCfg->locInterval = element->valuedouble;
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(appSettings, "StatisticsPrintInterval");
                    if (cJSON_IsNumber(element)) {
                        appCfg->statInterval = element->valuedouble;
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(appSettings, "MQTTClientWatchdogEnable");
                    if (cJSON_IsBool(element)) {
                        appCfg->mqttWdgEnable = (bool) element->valueint;
                    } else {
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Application configuration contains invalid value types");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete application settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find AppSettings\n");
            ret = ESP_FAIL;
        }

        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseCellConfig(char *payload, xplr_cfg_cell_t *cellCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *cellSettings;
    bool settingsFound;

    if (payload == NULL || cellCfg == NULL) {
        printf("Invalid Parameters. Cannot parse cell configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (cJSON_HasObjectItem(root, "CellSettings")) {
            cellSettings = cJSON_GetObjectItem(root, "CellSettings");
            settingsFound = cJSON_HasObjectItem(cellSettings, "APN");
            if (settingsFound) {
                element = cJSON_GetObjectItem(cellSettings, "APN");
                if (cJSON_IsString(element)) {
                    strncpy(cellCfg->apn, element->valuestring, 31);
                    ret = ESP_OK;
                } else {
                    printf("Cell configuration contains invalid value types");
                    ret = ESP_FAIL;
                }
            } else {
                printf("Incomplete cell module settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find CellSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseWifiConfig(char *payload, xplr_cfg_wifi_t *wifiCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *wifiSettings;
    bool settingsFound, abort;

    if (payload == NULL || wifiCfg == NULL) {
        printf("Invalid Parameters. Cannot parse wifi configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (cJSON_HasObjectItem(root, "WifiSettings")) {
            wifiSettings = cJSON_GetObjectItem(root, "WifiSettings");
            settingsFound = cJSON_HasObjectItem(wifiSettings, "SSID");
            settingsFound &= cJSON_HasObjectItem(wifiSettings, "Password");
            if (settingsFound) {
                /* Parse individual settings */
                abort = false;

                if (!abort) {
                    element = cJSON_GetObjectItem(wifiSettings, "SSID");
                    if (cJSON_IsString(element)) {
                        strncpy(wifiCfg->ssid, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(wifiSettings, "Password");
                    if (cJSON_IsString(element)) {
                        strncpy(wifiCfg->pwd, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Wifi configuration contains invalid value types");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete wifi module settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find WifiSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseTsConfig(char *payload, xplr_cfg_thingstream_t *tsCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *tsSettings;
    bool settingsFound, abort;

    if (payload == NULL || tsCfg == NULL) {
        printf("Invalid Parameters. Cannot parse thingstream configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (cJSON_HasObjectItem(root, "ThingstreamSettings")) {
            tsSettings = cJSON_GetObjectItem(root, "ThingstreamSettings");
            settingsFound = cJSON_HasObjectItem(tsSettings, "Region");
            settingsFound &= cJSON_HasObjectItem(tsSettings, "ConfigFilename");
            settingsFound &= cJSON_HasObjectItem(tsSettings, "ZTPToken");
            if (settingsFound) {
                /* Parse individual settings */
                abort = false;

                if (!abort) {
                    element = cJSON_GetObjectItem(tsSettings, "Region");
                    if (cJSON_IsString(element)) {
                        strncpy(tsCfg->region, element->valuestring, 31);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(tsSettings, "ConfigFilename");
                    if (cJSON_IsString(element)) {
                        strncpy(tsCfg->uCenterConfigFilename, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(tsSettings, "ZTPToken");
                    if (cJSON_IsString(element)) {
                        strncpy(tsCfg->ztpToken, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Thingstream module configuration contains invalid value types\n");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete Thingstream module settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find ThingstreamSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseNtripConfig(char *payload, xplr_cfg_ntrip_t *ntripCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *ntripSettings;
    bool settingsFound, abort;

    if (payload == NULL || ntripCfg == NULL) {
        printf("Invalid Parameters. Cannot parse NTRIP Client configuration.\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (cJSON_HasObjectItem(root, "NTRIPSettings")) {
            ntripSettings = cJSON_GetObjectItem(root, "NTRIPSettings");
            settingsFound = cJSON_HasObjectItem(ntripSettings, "Host");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "Port");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "MountPoint");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "UserAgent");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "SendGGA");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "UseAuthentication");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "Username");
            settingsFound &= cJSON_HasObjectItem(ntripSettings, "Password");
            if (settingsFound) {
                /* Parse individual settings */
                abort = false;

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "Host");
                    if (cJSON_IsString(element)) {
                        strncpy(ntripCfg->host, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "Port");
                    if (cJSON_IsNumber(element)) {
                        ntripCfg->port = (uint16_t)element->valueint;
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "MountPoint");
                    if (cJSON_IsString(element)) {
                        strncpy(ntripCfg->mountpoint, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "UserAgent");
                    if (cJSON_IsString(element)) {
                        strncpy(ntripCfg->userAgent, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "SendGGA");
                    if (cJSON_IsBool(element)) {
                        ntripCfg->sendGGA = (bool)element->valueint;
                    } else {
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(ntripSettings, "UseAuthentication");
                    if (cJSON_IsBool(element)) {
                        ntripCfg->useAuth = (bool)element->valueint;
                    } else {
                        abort = true;
                    }
                }

                if (!abort && ntripCfg->useAuth) {
                    element = cJSON_GetObjectItem(ntripSettings, "Username");
                    if (cJSON_IsString(element)) {
                        strncpy(ntripCfg->username, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (!abort && ntripCfg->useAuth) {
                    element = cJSON_GetObjectItem(ntripSettings, "Password");
                    if (cJSON_IsString(element)) {
                        strncpy(ntripCfg->password, element->valuestring, 63);
                    } else {
                        abort = true;
                    }
                }

                if (abort) {
                    printf("NTRIP Client configuration contains invalid value types\n");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete NTRIP client settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find NTRIPSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseLogConfig(char *payload, xplr_cfg_log_t *logCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *logSettings, *instances;
    bool settingsFound, abort;

    if (payload == NULL || logCfg == NULL) {
        printf("Invalid Parameters. Cannot parse logging configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (root != NULL && cJSON_HasObjectItem(root, "LogSettings")) {
            logSettings = cJSON_GetObjectItem(root, "LogSettings");
            /* Check for individual settings */
            settingsFound = cJSON_HasObjectItem(logSettings, "Instances");
            settingsFound &= cJSON_HasObjectItem(logSettings, "FilenameUpdateInterval");
            settingsFound &= cJSON_HasObjectItem(logSettings, "HotPlugEnable");
            if (settingsFound) {
                /* All settings exist we need to parse the data */
                abort = false;
                /* Parse log instances */
                instances = cJSON_GetObjectItem(logSettings, "Instances");
                if (cJSON_IsArray(instances)) {
                    logCfg->numOfInstances = cJSON_GetArraySize(instances);
                    if ((logCfg->numOfInstances > 0) && (logCfg->numOfInstances <= XPLR_LOG_MAX_INSTANCES)) {
                        for (int i = 0; i < logCfg->numOfInstances; i++) {
                            element = cJSON_GetArrayItem(instances, i);
                            if (element != NULL) {
                                strncpy(logCfg->instance[i].description,
                                        cJSON_GetObjectItem(element, "Description")->valuestring,
                                        63);
                                strncpy(logCfg->instance[i].filename, cJSON_GetObjectItem(element, "Filename")->valuestring, 63);
                                logCfg->instance[i].enable = (bool)cJSON_GetObjectItem(element, "Enable")->valueint;
                                logCfg->instance[i].erasePrev = (bool)cJSON_GetObjectItem(element, "ErasePrev")->valueint;
                                logCfg->instance[i].sizeInterval = (uint64_t)(cJSON_GetObjectItem(element,
                                                                                                  "SizeIntervalKBytes")->valueint) * 1024;
                            } else {
                                abort = true;
                                break;
                            }
                        }
                    } else {
                        printf("Invalid log instance number\n");
                        abort = true;
                    }
                } else {
                    abort = true;
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(logSettings, "FilenameUpdateInterval");
                    if (cJSON_IsNumber(element)) {
                        logCfg->filenameInterval = (uint64_t) element->valueint;
                    } else {
                        printf("Invalid filename increment interval value\n");
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(logSettings, "HotPlugEnable");
                    if (cJSON_IsBool(element)) {
                        logCfg->hotPlugEnable = (bool)element->valueint;
                    } else {
                        printf("Invalid hot plug enable option\n");
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Invalid log module configuration options\n");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete Log module settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find LogSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseDrConfig(char *payload, xplr_cfg_dr_t *drCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *drSettings;
    bool settingsFound, abort;

    if (payload == NULL || drCfg == NULL) {
        printf("Invalid Parameters. Cannot parse dead reckoning configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (root != NULL && cJSON_HasObjectItem(root, "DeadReckoningSettings")) {
            drSettings = cJSON_GetObjectItem(root, "DeadReckoningSettings");
            settingsFound = cJSON_HasObjectItem(drSettings, "Enable");
            settingsFound &= cJSON_HasObjectItem(drSettings, "PrintIMUData");
            if (settingsFound) {
                abort = false;
                element = cJSON_GetObjectItem(drSettings, "Enable");
                if (cJSON_IsBool(element)) {
                    drCfg->enable = (bool)element->valueint;
                } else {
                    printf("Could not find DR enable option\n");
                    abort = true;
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(drSettings, "PrintIMUData");
                    if (cJSON_IsBool(element)) {
                        drCfg->printImuData = (bool)element->valueint;
                    } else {
                        printf("Could not find print IMU data option\n");
                        abort = true;
                    }
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(drSettings, "PrintInterval");
                    if (cJSON_IsNumber(element)) {
                        drCfg->printInterval = (uint32_t)element->valueint;
                    } else {
                        printf("Could not find print IMU data interval option\n");
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Invalid DR module configuration options\n");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete Dead Reckoning settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find DrSettings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}

static esp_err_t parseGnssConfig(char *payload, xplr_cfg_gnss_t *gnssCfg)
{
    esp_err_t ret;
    cJSON *root, *element, *gnssSettings;
    bool settingsFound, abort;

    if (payload == NULL || gnssCfg == NULL) {
        printf("Invalid Parameters. Cannot parse dead reckoning configuration\n");
        ret = ESP_FAIL;
    } else {
        root = cJSON_Parse(payload);
        if (root != NULL && cJSON_HasObjectItem(root, "GNSSModuleSettings")) {
            gnssSettings = cJSON_GetObjectItem(root, "GNSSModuleSettings");
            settingsFound = cJSON_HasObjectItem(gnssSettings, "Module");
            settingsFound &= cJSON_HasObjectItem(gnssSettings, "CorrectionDataSource");
            if (settingsFound) {
                abort = false;
                element = cJSON_GetObjectItem(gnssSettings, "Module");
                if (cJSON_IsString(element)) {
                    if (strcmp(element->valuestring, "F9R") == 0) {
                        gnssCfg->module = 0;
                    } else if (strcmp(element->valuestring, "F9P") == 0) {
                        gnssCfg->module = 1;
                    } else {
                        printf("Invalid GNSS module option\n");
                        gnssCfg->module = -1;
                        abort = true;
                    }
                } else {
                    printf("Could not find GNSS module option\n");
                    abort = true;
                }

                if (!abort) {
                    element = cJSON_GetObjectItem(gnssSettings, "CorrectionDataSource");
                    if (cJSON_IsString(element)) {
                        if (strcmp(element->valuestring, "IP") == 0) {
                            gnssCfg->corrDataSrc = 0;
                        } else if (strcmp(element->valuestring, "LBAND") == 0) {
                            gnssCfg->corrDataSrc = 1;
                        } else {
                            printf("Invalid correction data source option\n");
                            abort = true;
                        }
                    } else {
                        printf("Could not find correction data source option\n");
                        abort = true;
                    }
                }

                if (abort) {
                    printf("Invalid GNSS module configuration options\n");
                    ret = ESP_FAIL;
                } else {
                    ret = ESP_OK;
                }
            } else {
                printf("Incomplete GNSS module settings in configuration file\n");
                ret = ESP_FAIL;
            }
        } else {
            printf("Cannot find GNSS Module Settings\n");
            ret = ESP_FAIL;
        }
        /* Free the memory */
        cJSON_Delete(root);
    }

    return ret;
}