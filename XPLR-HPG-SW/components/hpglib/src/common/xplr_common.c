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

#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "string.h"
#include "time.h"
#include "esp_mac.h"
#include "esp_err.h"
#include "mbedtls/md.h"
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

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

void xplrMemUsagePrint(uint8_t periodSecs)
{
    static uint64_t prevTime;
    char taskList[720];
    uint32_t free, minFree, total, numOfTasks;
    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs)) {
        free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        minFree = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
        numOfTasks = uxTaskGetNumberOfTasks();
        printf("heap: min %d cur %d size %d tasks: %d\n",
               minFree,
               free,
               total,
               numOfTasks);
        vTaskList(taskList);
        printf("Task List:\n%s\n", taskList);
        prevTime = MICROTOSEC(esp_timer_get_time());
    }
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