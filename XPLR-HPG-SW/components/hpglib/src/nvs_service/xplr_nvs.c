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
#include "xplr_nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "xplr_log.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * Debugging print macro
 */
#if (1 == XPLRNVS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && ((0 == XPLR_HPGLIB_LOG_ENABLED) || (0 == XPLRNVS_LOG_ACTIVE))
#define XPLRNVS_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_PRINT_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgNvs", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == XPLRNVS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRNVS_LOG_ACTIVE)
#define XPLRNVS_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_AND_PRINT, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgNvs", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif ((0 == XPLRNVS_DEBUG_ACTIVE) || (0 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)) && (1 == XPLR_HPGLIB_LOG_ENABLED) && (1 == XPLRNVS_LOG_ACTIVE)
#define XPLRNVS_CONSOLE(tag, message, ...) XPLRLOG(logIndex, XPLR_LOG_SD_ONLY, XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "hpgNvs", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRNVS_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
const char nvsPartitionName[] = "nvs";  /* partition name to store configuration settings */
static int8_t logIndex = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* opens nvs namespace in given mode */
static xplrNvs_error_t nvsOpen(xplrNvs_t *nvs, nvs_open_mode_t mode);
/* closes nvs namespace */
static xplrNvs_error_t nvsClose(xplrNvs_t *nvs);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

xplrNvs_error_t xplrNvsInit(xplrNvs_t *nvs, const char *nvsNamespace)
{
    const esp_partition_t *nvs_partition;
    esp_err_t err;
    xplrNvs_error_t ret;

    /* initialize nvs */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /*
         * NVS partition was truncated and needs to be erased
         * Retry nvs_flash_init
        */
        XPLRNVS_CONSOLE(W, "nvs flash init error(%d)", err);
        XPLRNVS_CONSOLE(W, "nvs erase and re-init");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            XPLRNVS_CONSOLE(E, "nvs erase error");
        } else {
            /* try init again */
            err = nvs_flash_init();
        }
    }

    err = nvs_flash_init_partition(nvsPartitionName);

    if (err != ESP_OK) {
        XPLRNVS_CONSOLE(E, "nvs init error");
    } else {
        XPLRNVS_CONSOLE(D, "nvs init ok");
        /* check NVS data partition present */
        nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                 ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                 nvsPartitionName);
#if 0   //activate for formatting nvs partition on boot 
        XPLRNVS_CONSOLE(W, "Formatting partition...");
        esp_partition_erase_range(nvs_partition, 0, nvs_partition->size);
#endif

        if (!nvs_partition) {
            err = ESP_FAIL;
            XPLRNVS_CONSOLE(E, "NVS data partition not found");
        } else {
            err = ESP_OK;
            XPLRNVS_CONSOLE(D, "NVS data partition found");
        }
    }

    /* check namespace and create it if not present */
    if (err == ESP_OK) {
        /* check namespace not null */
        if (nvsNamespace != NULL) {
            /* check namespace length */
            if (strlen(nvsNamespace) >= NVS_KEY_NAME_MAX_SIZE - 1) {
                err = ESP_FAIL;
                XPLRNVS_CONSOLE(E, "namespace <%s> too long (%u), max size is <%u>",
                                nvsNamespace,
                                strlen(nvsNamespace),
                                NVS_KEY_NAME_MAX_SIZE);
            } else {
                /* get namespace to handle */
                memset(nvs->tag, 0x00, 16);
                memcpy(nvs->tag, nvsNamespace, strlen(nvsNamespace));
                XPLRNVS_CONSOLE(D, "namespace set: <%s>", nvs->tag);
                /* create namespace if not present */
                err = nvs_open_from_partition(nvsPartitionName, nvs->tag, NVS_READWRITE, &nvs->handler);
                //nvs_close(nvs->handler);
            }
        } else {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "namespace is <NULL>");
        }
    }



    if (err != ESP_OK) {
        ret = XPLR_NVS_ERROR;
        XPLRNVS_CONSOLE(E, "namespace <%s> could not be initialized (%d)", nvs->tag, err);
    } else {
        ret = XPLR_NVS_OK;
        XPLRNVS_CONSOLE(D, "nvs <%s> initialized ok", nvs->tag);
    }

    return ret;
}

xplrNvs_error_t xplrNvsDeInit(xplrNvs_t *nvs)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* attempt close of used namespace / handler */
    nvs_close(nvs->handler);
    /* de-init nvs */
    err = nvs_flash_deinit();

    if (err != ESP_OK) {
        ret = XPLR_NVS_ERROR;
        XPLRNVS_CONSOLE(E, "nvs de-init error");
    } else {
        ret = XPLR_NVS_OK;
        XPLRNVS_CONSOLE(D, "nvs de-init ok");
    }

    return ret;
}

xplrNvs_error_t xplrNvsEraseAll()
{
    xplrNvs_error_t ret;
    esp_err_t espRet;

    espRet = nvs_flash_erase_partition(nvsPartitionName);
    if (espRet != ESP_OK) {
        XPLRNVS_CONSOLE(E, "nvs erase all error");
        ret = XPLR_NVS_ERROR;
    } else {
        XPLRNVS_CONSOLE(D, "erased all namespaces");
        ret = XPLR_NVS_OK;
    }
    return ret;
}

xplrNvs_error_t xplrNvsErase(xplrNvs_t *nvs)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != ESP_OK) {
        XPLRNVS_CONSOLE(E, "failed to open <%s> in r/w mode", nvs->tag);
    } else {
        err = nvs_erase_all(nvs->handler);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "failed to erase <%s> namespace", nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "<%s> namespace erased ok", nvs->tag);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsEraseKey(xplrNvs_t *nvs, const char *key)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != ESP_OK) {
        XPLRNVS_CONSOLE(E, "failed to open <%s> in r/w mode", nvs->tag);
    } else {
        err = nvs_erase_key(nvs->handler, key);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "failed to erase <%s> from <%s> namespace", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "<%s> key from namespace <%s> erased ok", key,  nvs->tag);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadU8(xplrNvs_t *nvs, const char *key, uint8_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_u8(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%u>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadU16(xplrNvs_t *nvs, const char *key, uint16_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_u16(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%u>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadU32(xplrNvs_t *nvs, const char *key, uint32_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_u32(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%u>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadU64(xplrNvs_t *nvs, const char *key, uint64_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_u64(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%llu>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadI8(xplrNvs_t *nvs, const char *key, int8_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_i8(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadI16(xplrNvs_t *nvs, const char *key, int16_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_i16(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadI32(xplrNvs_t *nvs, const char *key, int32_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_i32(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadI64(xplrNvs_t *nvs, const char *key, int64_t *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_i64(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error reading key <%s> from namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%lld>", key,  nvs->tag, *value);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadString(xplrNvs_t *nvs, const char *key, char *value, size_t *size)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_str(nvs->handler, key, value, size);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error (0x%04x) reading key <%s> from namespace <%s>", (int32_t)err, key,
                            nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "Read key <%s> in namespace <%s>", key,  nvs->tag);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsReadStringHex(xplrNvs_t *nvs, const char *key, char *value, size_t *size)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in readonly */
    ret = nvsOpen(nvs, NVS_READONLY);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_get_str(nvs->handler, key, value, size);
        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error (0x%04x) reading key <%s> from namespace <%s>", (int32_t)err, key,
                            nvs->tag);
            (void)nvsClose(nvs);
        } else {
            XPLRNVS_CONSOLE(D, "Read key <%s> in namespace <%s>", key,  nvs->tag);
            ret = nvsClose(nvs);
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteU8(xplrNvs_t *nvs, const char *key, uint8_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_u8(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%u>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteU16(xplrNvs_t *nvs, const char *key, uint16_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_u16(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%u>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteU32(xplrNvs_t *nvs, const char *key, uint32_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_u32(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%lu>", key,  nvs->tag, (long unsigned int)value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteU64(xplrNvs_t *nvs, const char *key, uint64_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_u64(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%llu>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteI8(xplrNvs_t *nvs, const char *key, int8_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_i8(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteI16(xplrNvs_t *nvs, const char *key, int16_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_i16(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteI32(xplrNvs_t *nvs, const char *key, int32_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_i32(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%d>", key,  nvs->tag, value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteI64(xplrNvs_t *nvs, const char *key, int64_t value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read-write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_i64(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "key <%s> in namespace <%s> is <%lld>", key,  nvs->tag, (int64_t)value);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteString(xplrNvs_t *nvs, const char *key, const char *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read/write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_str(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "Wrote key <%s> in namespace <%s>", key,  nvs->tag);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

xplrNvs_error_t xplrNvsWriteStringHex(xplrNvs_t *nvs, const char *key, const char *value)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* open nvs in read/write */
    ret = nvsOpen(nvs, NVS_READWRITE);
    if (ret != XPLR_NVS_OK) {
        ret = XPLR_NVS_ERROR;
    } else {
        err = nvs_set_str(nvs->handler, key, value);

        if (err != ESP_OK) {
            ret = XPLR_NVS_ERROR;
            XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
            (void)nvsClose(nvs);
        } else {
            err = nvs_commit(nvs->handler);
            vTaskDelay(pdMS_TO_TICKS(100));
            if (err != ESP_OK) {
                ret = XPLR_NVS_ERROR;
                XPLRNVS_CONSOLE(E, "Error writing key <%s> to namespace <%s>", key,  nvs->tag);
                (void)nvsClose(nvs);
            } else {
                XPLRNVS_CONSOLE(D, "Wrote key <%s> in namespace <%s>", key,  nvs->tag);
                ret = nvsClose(nvs);
            }
        }
    }

    return ret;
}

int8_t xplrNvsInitLogModule(xplr_cfg_logInstance_t *logCfg)
{
    int8_t ret;
    xplrLog_error_t logErr;

    if (logIndex < 0) {
        /* logIndex is negative so logging has not been initialized before */
        if (logCfg == NULL) {
            /* logCfg is NULL so we will use the default module settings */
            logIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                   XPLR_NVS_DEFAULT_FILENAME,
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

esp_err_t xplrNvsStopLogModule(void)
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
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static xplrNvs_error_t nvsOpen(xplrNvs_t *nvs, nvs_open_mode_t mode)
{
    esp_err_t err;
    xplrNvs_error_t ret;

    /* check if already init. tag should be present */
    if (nvs->tag != NULL) {
        XPLRNVS_CONSOLE(D, "Opening nvs namespace <%s> with permissions (%d).", nvs->tag, (int32_t)mode);

        err = nvs_open_from_partition(nvsPartitionName, nvs->tag, mode, &nvs->handler);

        if (err != ESP_OK) {
            XPLRNVS_CONSOLE(E, "Error (0x%04x) opening nvs namespace <%s>", (int32_t)err, nvs->tag);
            ret = XPLR_NVS_ERROR;
        } else {
            XPLRNVS_CONSOLE(D, "nvs namespace <%s> opened ok", nvs->tag);
            ret = XPLR_NVS_OK;
        }
    } else {
        XPLRNVS_CONSOLE(E, "Error, nvs not initialized");
        ret = XPLR_NVS_ERROR;
    }

    return ret;
}

static xplrNvs_error_t nvsClose(xplrNvs_t *nvs)
{
    xplrNvs_error_t ret;

    /* check if alread init. tag should be present */
    if (nvs->tag != NULL) {
        XPLRNVS_CONSOLE(D, "Closing nvs namespace <%s>.", nvs->tag);
        nvs_close(nvs->handler);
        XPLRNVS_CONSOLE(D, "nvs namespace <%s> closed ok", nvs->tag);
        ret = XPLR_NVS_OK;
    } else {
        XPLRNVS_CONSOLE(E, "Error, nvs not initialized");
        ret = XPLR_NVS_ERROR;
    }

    return ret;
}
