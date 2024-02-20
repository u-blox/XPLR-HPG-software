/**
 * Import libraries
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdmmc_cmd.h"
#include "xplr_sd.h"
#include <inttypes.h>
#include <sys/types.h>
#include <dirent.h>
#include "driver/timer.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

#if (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED && 1 == XPLRSD_DEBUG_ACTIVE)
#define XPLRSD_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrSd", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRSD_CONSOLE(message, ...) do{} while(0)
#endif

/** Macros based on the board selection from Kconfig values. Should not be modified*/
#define SPI_SD_SCK  BOARD_IO_SPI_SD_SCK
#define SPI_SD_MISO BOARD_IO_SPI_SD_MISO
#define SPI_SD_MOSI BOARD_IO_SPI_SD_MOSI
#define SPI_SD_nCS  BOARD_IO_SPI_SD_nCS
#if defined (BOARD_IO_SD_DETECT)
#define SPI_SD_DET  BOARD_IO_SD_DETECT
#endif

/* initialize timer
     * no irq or alarm.
     * timer in free running mode.
     * timer remains halted after config. */
timer_config_t timerCfg = {
    .divider = 16,
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE,
    .alarm_en = TIMER_ALARM_DIS,
    .auto_reload = TIMER_AUTORELOAD_EN
};

/* Local instance of SD*/
static xplrSd_t locSd;

/* Local mutex to ensure atomic access to the locSd instance*/
static SemaphoreHandle_t xSdMutex = NULL;

/* Flag indicating Card Detect Task is created */
static bool isCDTaskCreated = false;

/* Handler for the Card Detect Task */
static TaskHandle_t cdTaskHandler = NULL;

/**
 * ------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * ------------------------------------------------------
*/

static xplrSd_error_t sdStartSizeTask(void);
static xplrSd_error_t sdStopSizeTask(void);
static xplrSd_error_t sdStartDetectTask(void);
static xplrSd_error_t sdStopDetectTask(void);
static xplrSd_error_t sdEraseFile(const char *filename);

/**
 * ------------------------------------------------------
 * TASK CALLBACK FUNCTIONS
 * ------------------------------------------------------
*/
static void sdSizeTaskCB(void *pvParameters);
static void sdDetectTaskCB(void *pvParameters);

/**
 * -----------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------
*/

xplrSd_error_t xplrSdStartCardDetectTask(void)
{
    xplrSd_error_t ret;

    if (isCDTaskCreated) {
        ret = XPLR_SD_OK;
    } else {
        ret = sdStartDetectTask();
    }

    return ret;
}

xplrSd_error_t xplrSdStopCardDetectTask(void)
{
    return sdStopDetectTask();
}

xplrSd_error_t xplrSdConfigDefaults(void)
{
    xplrSd_error_t ret;
    xplrSd_mount_config_t mountCfg = XPLR_SD_MOUNT_CFG_DEFAULT();
    xplrSd_device_config_t slotCfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    xplrSd_spi_config_t busCfg = XPLR_SD_SPI_BUS_CFG_DEFAULT();

    locSd.mountConfig = mountCfg;
    memcpy(locSd.mountPoint, DEFAULT_MOUNT_POINT, strlen(DEFAULT_MOUNT_POINT));
    locSd.card.host.flags = SDMMC_HOST_FLAG_SPI | SDMMC_HOST_FLAG_DEINIT_ARG;
    locSd.card.host.slot = SDSPI_DEFAULT_HOST;
    locSd.card.host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    locSd.card.host.io_voltage = 3.3f;
    locSd.card.host.init = &sdspi_host_init;
    locSd.card.host.set_bus_width = NULL;
    locSd.card.host.get_bus_width = NULL;
    locSd.card.host.set_bus_ddr_mode = NULL;
    locSd.card.host.set_card_clk = &sdspi_host_set_card_clk;
    locSd.card.host.do_transaction = &sdspi_host_do_transaction;
    locSd.card.host.deinit_p = &sdspi_host_remove_device;
    locSd.card.host.io_int_enable = &sdspi_host_io_int_enable;
    locSd.card.host.io_int_wait = &sdspi_host_io_int_wait;
    locSd.card.host.command_timeout_ms = 0;
    locSd.spiConfig = busCfg;
    slotCfg.gpio_cs = SPI_SD_nCS;
    slotCfg.gpio_cd = SDSPI_SLOT_NO_CD;
    slotCfg.host_id = locSd.card.host.slot;
    locSd.devConfig = slotCfg;
    locSd.maxTimeout = 2;
    strncpy(locSd.protectFilename, XPLR_SD_SVI_FILENAME, 64);
    ret = XPLR_SD_OK;

    return ret;
}

xplrSd_error_t xplrSdConfig(xplrSd_mount_config_t *mountCfg,
                            xplrSd_device_config_t *slotCfg,
                            xplrSd_spi_config_t *busCfg,
                            xplrSd_card_t *card,
                            const char *mountPoint)
{
    xplrSd_error_t ret;

    memset(&locSd, 0x00, sizeof(xplrSd_t));
    memcpy(&locSd.mountConfig, mountCfg, sizeof(xplrSd_mount_config_t));
    memcpy(&locSd.devConfig, slotCfg, sizeof(xplrSd_device_config_t));
    memcpy(&locSd.spiConfig, busCfg, sizeof(xplrSd_spi_config_t));
    if (strlen(mountPoint) > 63) {
        XPLRSD_CONSOLE(E, "Mount Point longer than 64 characters!");
        ret = XPLR_SD_ERROR;
    } else {
        memcpy(locSd.mountPoint, mountPoint, strlen(mountPoint));
        ret = XPLR_SD_OK;
    }

    return ret;
}

xplrSd_error_t xplrSdInit(void)
{
    xplrSd_error_t ret;
    esp_err_t err;
    xplrSd_card_t *card = &locSd.card;

    if (!locSd.isDetected) {
        ret = XPLR_SD_NOT_FOUND;
    } else {
        if (!locSd.isInit) {
            /* Local instance is not initialized so it is the first SD initialization */
            /* Check if semaphore has been created */
            if (!locSd.semaphoreCreated) {
                xSdMutex = xSemaphoreCreateMutex();
                locSd.semaphoreCreated = true;
            }

            if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
                XPLRSD_CONSOLE(D, "Starting Initialization of SD card in mountPoint = %s", locSd.mountPoint);
                err = spi_bus_initialize(locSd.devConfig.host_id, &locSd.spiConfig, SDSPI_DEFAULT_DMA);
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    XPLRSD_CONSOLE(E, "Error initializing SPI bus");
                    ret = XPLR_SD_ERROR;
                } else {
                    XPLRSD_CONSOLE(D, "SPI bus initialization successful");
                    XPLRSD_CONSOLE(D, "Mounting Filesystem");
                    err = esp_vfs_fat_sdspi_mount(locSd.mountPoint,
                                                  &locSd.card.host,
                                                  &locSd.devConfig,
                                                  &locSd.mountConfig,
                                                  &card);
                    if (err != ESP_OK) {
                        if (err == ESP_FAIL) {
                            XPLRSD_CONSOLE(E, "Failed to mount filesystem");
                            ret = XPLR_SD_ERROR;
                        } else {
                            XPLRSD_CONSOLE(E, "Failed to initialized the card with error code %s", esp_err_to_name(err));
                            ret = XPLR_SD_ERROR;
                        }
                    } else {
                        locSd.isInit = true;
                        memcpy(&locSd.card, card, sizeof(locSd.card));
                        XPLRSD_CONSOLE(D, "Filesystem mounted in mountPoint = %s", locSd.mountPoint);
                        /* SD is initialized so we initialize the async tasks */
                        ret = sdStartSizeTask();
                        timer_init(TIMER_GROUP_0, TIMER_1, &timerCfg);
                        timer_pause(TIMER_GROUP_0, TIMER_1);
                    }
                }
                /* This is the point where we want to give the mutex back */
                xSemaphoreGive(xSdMutex);
            } else {
                XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
                ret = XPLR_SD_BUSY;
            }
        } else {
            /* Local instance is initialized and populated */
            ret = XPLR_SD_OK;
        }
    }

    return ret;
}

xplrSd_error_t xplrSdDeInit(void)
{
    xplrSd_error_t ret;
    esp_err_t err;

    sdStopSizeTask();
    timer_deinit(TIMER_GROUP_0, TIMER_1);
    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        if (locSd.isInit) {
            err = esp_vfs_fat_sdmmc_unmount();
            if (err == ESP_OK) {
                XPLRSD_CONSOLE(D, "Filesystem unmounted successfully");
                ret = XPLR_SD_OK;
                err = spi_bus_free(locSd.card.host.slot);
                if (err == ESP_OK) {
                    XPLRSD_CONSOLE(D, "SPI bus successfully de-initialized");
                    memset(&locSd, 0x00, sizeof(xplrSd_t));
                    locSd.isInit = false;
                } else {
                    XPLRSD_CONSOLE(E, "SPI could not be uninitialized");
                    ret = XPLR_SD_ERROR;
                }
            } else {
                XPLRSD_CONSOLE(E, "Filesystem could not be unmounted with error code %s", esp_err_to_name(err));
                ret = XPLR_SD_ERROR;
            }
        } else {
            XPLRSD_CONSOLE(E, "SD card is not initialized");
            ret = XPLR_SD_NOT_INIT;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;

}

void xplrSdPrintInfo(void)
{
#if (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED && 1 == XPLRSD_DEBUG_ACTIVE)
    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        sdmmc_card_print_info(stdout, &locSd.card);
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
    }
#endif
}


FILE *xplrSdOpenFile(const char *filename, xplrSd_file_mode_t filemode)
{
    FILE *f = NULL;
    char filepath[128] = {0};

    if (filename != NULL) {
        if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
            /* Create full filepath to the file */
            snprintf(filepath, 128, "%s/%s", locSd.mountPoint, filename);
            switch (filemode) {
                case XPLR_FILE_MODE_READ:
                    f = fopen(filepath, "r");
                    break;
                case XPLR_FILE_MODE_WRITE:
                    f = fopen(filepath, "w");
                    break;
                case XPLR_FILE_MODE_APPEND:
                    f = fopen(filepath, "a");
                    break;
                case XPLR_FILE_MODE_READ_PLUS:
                    f = fopen(filepath, "r+");
                    break;
                case XPLR_FILE_MODE_WRITE_PLUS:
                    f = fopen(filepath, "w+");
                    break;
                case XPLR_FILE_MODE_APPEND_PLUS:
                    f = fopen(filepath, "a+");
                    break;
                default:
                    break;
            }
            if (f != NULL) {
                XPLRSD_CONSOLE(D, "Opened file <%s> with file descriptor <%p>", filepath, f);
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        }
    } else {
        XPLRSD_CONSOLE(E, "NULL filename");
    }

    return f;
}

xplrSd_error_t xplrSdCloseFile(FILE *file)
{
    xplrSd_error_t ret;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        int err = fclose(file);
        if (err == 0) {
            XPLRSD_CONSOLE(D, "File descriptor <%p> closed successfully", file);
            ret = XPLR_SD_OK;
        } else {
            XPLRSD_CONSOLE(E, "Error in closing file descriptor <%p>", file);
            ret = XPLR_SD_ERROR;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;
}


xplrSd_error_t xplrSdRenameFile(const char *original, const char *renamed)
{
    xplrSd_error_t ret;
    struct stat st;
    char originalFilepath[128], renamedFilepath[128];

    memset(originalFilepath, 0x00, 128);
    memset(renamedFilepath, 0x00, 128);
    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        snprintf(originalFilepath, 128, "%s/%s", locSd.mountPoint, original);
        snprintf(renamedFilepath, 128, "%s/%s", locSd.mountPoint, renamed);
        XPLRSD_CONSOLE(D, "Renaming file %s to %s", original, renamed);
        /* Check if the filename is occupied by another file*/
        if (stat(renamedFilepath, &st) == 0) {
            XPLRSD_CONSOLE(W, "File %s found in filesystem, deleting...", renamed);
            unlink(renamedFilepath);
        }

        if (stat(originalFilepath, &st) == 0) {
            if (rename(originalFilepath, renamedFilepath) != 0) {
                XPLRSD_CONSOLE(E, "File renaming failed");
                ret = XPLR_SD_ERROR;
            } else {
                XPLRSD_CONSOLE(D, "Renaming completed successfully");
                ret = XPLR_SD_OK;
            }
        } else {
            XPLRSD_CONSOLE(E, "File %s not found in filesystem", original);
            ret = XPLR_SD_NOT_FOUND;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;
}

xplrSd_error_t xplrSdEraseFile(const char *filename)
{
    xplrSd_error_t ret;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        ret = sdEraseFile(filename);
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }
    return ret;
}

xplrSd_error_t xplrSdEraseAll(void)
{
    xplrSd_error_t err, ret;
    DIR *dp;
    struct dirent *ep;
    double opTime = 0;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        /* Set timer value to 0 */
        timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0);
        dp = opendir(locSd.mountPoint);
        ret = XPLR_SD_NOT_FOUND;
        if (dp != NULL) {
            timer_start(TIMER_GROUP_0, TIMER_1);
            while (((ep = readdir(dp)) != NULL) && (opTime <= locSd.maxTimeout)) {
                timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_1, &opTime);
                err = sdEraseFile((const char *)ep->d_name);
                if (err != XPLR_SD_OK) {
                    ret = XPLR_SD_ERROR;
                } else {
                    //Do nothing
                }
            }
            (void) closedir(dp);
        }
        if (opTime > locSd.maxTimeout) {
            XPLRSD_CONSOLE(W, "Timeout triggered during Erase All function!");
            ret = XPLR_SD_ERROR;
        }
        timer_pause(TIMER_GROUP_0, TIMER_1);

        if (ret != XPLR_SD_ERROR) {
            ret = XPLR_SD_OK;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;
}

xplrSd_error_t xplrSdReadFileString(const char *filename, char *value, size_t length)
{
    xplrSd_error_t ret;
    int err;
    FILE *fp = xplrSdOpenFile(filename, XPLR_FILE_MODE_READ);

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            err = fread(value, 1, length, fp);
            if (err <= length) {
                XPLRSD_CONSOLE(D, "Read successfully %d bytes oy of the required %d", strlen(value), length);
                ret = XPLR_SD_OK;
            } else {
                printf("#####fread failed, ret = <%d>\n", err);
                XPLRSD_CONSOLE(E, "Error in reading from file");
                ret = XPLR_SD_ERROR;
            }
            xSemaphoreGive(xSdMutex);
        } else {
            printf("#####Failed to take semaphore\n");
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        }
        ret = xplrSdCloseFile(fp);
    } else {
        printf("#####File not found, fp == NULL\n");
        XPLRSD_CONSOLE(E, "File %s not found in filesystem", filename);
        ret = XPLR_SD_NOT_FOUND;
    }

    return ret;
}

int xplrSdReadFileU8(const char *filename, uint8_t *value, size_t length)
{
    FILE *fp = xplrSdOpenFile(filename, XPLR_FILE_MODE_READ);
    int ret = 0;
    xplrSd_error_t sdErr;

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            ret = fread(value, 1, length, fp);
            xSemaphoreGive(xSdMutex);
            /* We need to first give the semaphore and then call xplrSdCloseFile */
            sdErr = xplrSdCloseFile(fp);
            if (sdErr != XPLR_SD_OK) {
                ret = -1;
            }
            XPLRSD_CONSOLE(D, "Read %d bytes out of the %d requested from file %s", ret, length, filename);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            ret = -1;
        }
    } else {
        XPLRSD_CONSOLE(E, "File %s not found in filesystem", filename);
        ret = -1;
    }

    return ret;
}

xplrSd_error_t xplrSdWriteFileString(const char *filename,
                                     char *value,
                                     xplrSd_file_mode_t mode)
{
    xplrSd_error_t ret;
    FILE *fp = NULL;

    switch (mode) {
        case XPLR_FILE_MODE_UNKNOWN:
            XPLRSD_CONSOLE(E, "XPLR_FILE_MODE Unknown");
            break;
        case XPLR_FILE_MODE_READ:
            XPLRSD_CONSOLE(E, "Cannot write to file in read mode");
            break;
        case XPLR_FILE_MODE_WRITE:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_APPEND:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_READ_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_WRITE_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_APPEND_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        default:
            XPLRSD_CONSOLE(E, "File mode unexpected error");
            break;
    }

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            if (fputs(value, fp) != EOF) {
                XPLRSD_CONSOLE(D, "Write operation in file %s was successful", filename);
                ret = XPLR_SD_OK;
            } else {
                XPLRSD_CONSOLE(E, "Write operation in file %s was unsuccessful", filename);
                ret = XPLR_SD_ERROR;
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        }
        ret = xplrSdCloseFile(fp);
        XPLRSD_CONSOLE(D, " Successfully wrote %d bytes in file %s", strlen(value), filename);
    } else {
        ret = XPLR_SD_ERROR;
        XPLRSD_CONSOLE(E, "Could not open file %s for writing", filename);
    }

    return ret;
}

int xplrSdWriteFileU8(const char *filename,
                      uint8_t *value,
                      size_t length,
                      xplrSd_file_mode_t mode)
{
    int ret = 0;
    FILE *fp = NULL;
    xplrSd_error_t sdErr;

    switch (mode) {
        case XPLR_FILE_MODE_UNKNOWN:
            XPLRSD_CONSOLE(E, "XPLR_FILE_MODE Unknown");
            break;
        case XPLR_FILE_MODE_READ:
            XPLRSD_CONSOLE(E, "Cannot write to file in read mode");
            break;
        case XPLR_FILE_MODE_WRITE:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_APPEND:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_READ_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_WRITE_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        case XPLR_FILE_MODE_APPEND_PLUS:
            fp = xplrSdOpenFile(filename, mode);
            break;
        default:
            XPLRSD_CONSOLE(E, "File mode unexpected error");
            break;
    }

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            ret = fwrite(value, 1, length, fp);
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            ret = -1;
        }
        if (ret >= 0) {
            XPLRSD_CONSOLE(D, " Successfully wrote %d bytes in file %s", ret, filename);
            sdErr = xplrSdCloseFile(fp);
            if (sdErr != XPLR_SD_OK) {
                XPLRSD_CONSOLE(W, "Error closing file <%s> after logging!", filename);
            } else {
                //Do nothing
            }
        } else {
            XPLRSD_CONSOLE(E, "Write operation unsuccessful!");
        }
    } else {
        ret = -1;
        XPLRSD_CONSOLE(E, "Could not open file %s for writing", filename);
    }

    return ret;
}

int64_t xplrSdGetFileSize(const char *filename)
{
    int64_t ret;
    struct stat st;
    char filepath[128];

    if (filename == NULL || strlen(filename) > 63) {
        ret = -1;
    } else {
        if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
            /* Create full filepath to the file */
            snprintf(filepath, 128, "%s/%s", locSd.mountPoint, filename);
            /* Check if the file exists */
            if (stat(filepath, &st) == 0) {
                ret = (int64_t) st.st_size;
            } else {
                /* We could not find the file, so the size is 0 */
                XPLRSD_CONSOLE(W, "Could not find the file <%s>", filename);
                ret = 0;
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            ret = -1;
        }
    }

    return ret;
}

uint64_t xplrSdGetTotalSpace(void)
{
    uint64_t totalSpace;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        totalSpace = locSd.spaceConfig.totalSpace;
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(W, "Could not take semaphore to access the SD card...");
        totalSpace = 0;
    }

    return totalSpace;
}

uint64_t xplrSdGetFreeSpace(void)
{
    uint64_t freeSpace;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        freeSpace = locSd.spaceConfig.freeSpace;
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(W, "Could not take semaphore to access the SD card...");
        freeSpace = 0;
    }

    return freeSpace;
}

uint64_t xplrSdGetUsedSpace(void)
{
    uint64_t usedSpace;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        usedSpace = locSd.spaceConfig.usedSpace;
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(W, "Could not take semaphore to access the SD card...");
        usedSpace = 0;
    }

    return usedSpace;
}

bool xplrSdIsCardOn(void)
{
    bool ret;

    if (xSdMutex == NULL) {
        /* Mutex not created yet do nothing */
        ret = false;
    } else {
        if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
            ret = locSd.isDetected;
            xSemaphoreGive(xSdMutex);
        } else {
            ret = false;
        }
    }

    return ret;
}

bool xplrSdIsCardInit(void)
{
    bool ret;

    if (xSdMutex == NULL) {
        /* Mutex not created yet do nothing */
        ret = false;
    } else {
        if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
            ret = locSd.isInit;
            xSemaphoreGive(xSdMutex);
        } else {
            ret = false;
        }
    }

    return ret;
}

/**
 * ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ----------------------------------------------------------------
*/

static xplrSd_error_t sdStartSizeTask(void)
{
    xplrSd_error_t ret;

    xTaskCreate(sdSizeTaskCB,
                "SDSizeCheckTask",
                2 * 1024,
                NULL,
                20,
                &locSd.spaceConfig.sizeTaskHandler);
    if (locSd.spaceConfig.sizeTaskHandler != NULL) {
        ret = XPLR_SD_OK;
        XPLRSD_CONSOLE(D, "Created Check Size Task");
    } else {
        ret = XPLR_SD_ERROR;
    }

    return ret;
}

static xplrSd_error_t sdStopSizeTask(void)
{
    xplrSd_error_t ret;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        if (locSd.spaceConfig.sizeTaskHandler != NULL) {
            vTaskDelete(locSd.spaceConfig.sizeTaskHandler);
            ret = XPLR_SD_OK;
            XPLRSD_CONSOLE(D, "Stopped Check Size Task");
        } else {
            ret = XPLR_SD_ERROR;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        ret = XPLR_SD_BUSY;
    }

    return ret;
}

static xplrSd_error_t sdStartDetectTask(void)
{
    xplrSd_error_t ret;

    xTaskCreate((void *)sdDetectTaskCB,
                "SDCardDetectTask",
                2 * 1024,
                NULL,
                20,
                &cdTaskHandler);
    if (cdTaskHandler != NULL) {
        ret = XPLR_SD_OK;
        isCDTaskCreated = true;
        XPLRSD_CONSOLE(D, "Created Card Detect Task");
    } else {
        XPLRSD_CONSOLE(E, "Failed to create Card Detect Task");
        ret = XPLR_SD_ERROR;
    }

    return ret;
}

static xplrSd_error_t sdStopDetectTask(void)
{
    xplrSd_error_t ret;

    if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
        if (cdTaskHandler != NULL) {
            vTaskDelete(cdTaskHandler);
            isCDTaskCreated = false;
            ret = XPLR_SD_OK;
            XPLRSD_CONSOLE(D, "Stopped Card Detect Task");
        } else {
            ret = XPLR_SD_ERROR;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(W, "Could not take semaphore");
        ret = XPLR_SD_BUSY;
    }

    return ret;
}

static xplrSd_error_t sdEraseFile(const char *filename)
{
    xplrSd_error_t ret;
    struct stat st;
    char filepath[128];

    /* Create full filepath to the file */
    snprintf(filepath, 128, "%s/%s", locSd.mountPoint, filename);
    /* Check if the file is protected */
    if (strcmp(locSd.protectFilename, filename) == 0) {
        XPLRSD_CONSOLE(D, "File <%s> is protected from erase", filename);
        ret = XPLR_SD_OK;
    } else {
        /* Check if the file exists */
        if (stat(filepath, &st) == 0) {
            XPLRSD_CONSOLE(D, "File %s found in filesystem, deleting...", filepath);
            unlink(filepath);
            ret = XPLR_SD_OK;
        } else {
            XPLRSD_CONSOLE(W, "File %s not found in filesystem", filepath);
            ret = XPLR_SD_NOT_FOUND;
        }
    }

    return ret;
}

/**
 * ----------------------------------------------------------------
 * CALLBACK FUNCTIONS
 * ----------------------------------------------------------------
*/

static void sdSizeTaskCB(void *pvParameters)
{
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    uint64_t divider = 2;

    while (1) {
        if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
            /* Check that SD is on */
            if (locSd.isDetected && locSd.isInit) {
                /* Get volume information and free clusters of drive 0 */
                if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
                    /* Get total sectors and free sectors */
                    tot_sect = ((fs->n_fatent - 2) * fs->csize);
                    fre_sect = (fre_clust * fs->csize);

                    locSd.spaceConfig.totalSpace = tot_sect / divider;
                    locSd.spaceConfig.freeSpace = fre_sect / divider;
                    locSd.spaceConfig.usedSpace = (tot_sect - fre_sect) / divider;
                }
            } else {
                //Do nothing
            }
            /* Window for other tasks to run*/
            xSemaphoreGive(xSdMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            XPLRSD_CONSOLE(W, "Could not take semaphore");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static void sdDetectTaskCB(void *pvParameters)
{
    xplr_board_error_t err;

    while (1) {
        if (xSdMutex != NULL) {
            if (xSemaphoreTake(xSdMutex, XPLR_SD_MAX_TIMEOUT) == pdTRUE) {
                /* Check Card Detect pin */
                err = xplrBoardDetectSd();
                if (err == XPLR_BOARD_ERROR_OK) {
                    locSd.isDetected = true;
                } else {
                    locSd.isDetected = false;
                }
                xSemaphoreGive(xSdMutex);
            } else {
                XPLRSD_CONSOLE(W, "Could not take semaphore");
            }
            /* Window for other tasks to run*/
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            xSdMutex = xSemaphoreCreateMutex();
            locSd.semaphoreCreated = true;
            // vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}