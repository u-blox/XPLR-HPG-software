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

/** Macros to configure SD card and Filesystem. User should pick the values that fit the use case or leave them at default*/
#define CONFIG_FORMAT_IF_FAILED     1U                          //If set to 1, the sd card will be formatted ,in case of failed mount attempt
#define CONFIG_MAX_FILES_OPEN       2                           //Maximum number of files allowed to be open at the same time
#define CONFIG_ALLOC_UNIT_SIZE      8                           //Minimum file size (in Kbytes). Will change if card is formatted
#define CONFIG_MAXIMUM_FILES        20                          //Maximum files allowed in filesystem
#define MOUNT_POINT                 "/sdcard"                   //Filesystem's default mount point. 
#define DEL_EXCEPTION               MOUNT_POINT"/SYSTEM~1"      //File name that is protected in full erase of sd's filesystem
#define MAX_TIMEOUT_SEC             (1*1)                       //Maximum timeout in seconds before an erase all operation is aborted

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
static xplrSd_file_t files[CONFIG_MAXIMUM_FILES];

/* Local instance of SD*/
static xplrSd_t locSd = {0};

/* Local mutex to ensure atomic access to the locSd instance*/
SemaphoreHandle_t xSdMutex = NULL;

/**
 * ------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * ------------------------------------------------------
*/

/**
 * Function that populates the sd struct with the current
 * stats of free, total and used space in the card.
*/
xplrSd_error_t sdGetStats(xplrSd_t *sd);

/**
 * -----------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------
*/

xplrSd_error_t xplrSdConfig(xplrSd_t *sd)
{
    xplrSd_error_t ret;
    xplr_board_error_t boardErr;
    //Mount Configuration
    xplrSd_mount_config_t mountCfg = {
#if(CONFIG_FORMAT_IF_FAILED)
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = CONFIG_MAX_FILES_OPEN,
        .allocation_unit_size = (CONFIG_ALLOC_UNIT_SIZE * 2 * 1024)
    };
    xplrSd_device_config_t slotCfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    xplrSd_spi_config_t busCfg = {
        .mosi_io_num = SPI_SD_MOSI,
        .miso_io_num = SPI_SD_MISO,
        .sclk_io_num = SPI_SD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    boardErr = xplrBoardDetectSd();
    if (boardErr == XPLR_BOARD_ERROR) {
        sd->isDetected = false;
        ret = XPLR_SD_NOT_FOUND;
    } else {
        sd->isDetected = true;
        sd->mountConfig = mountCfg;
        memcpy(sd->mountPoint, MOUNT_POINT, strlen(MOUNT_POINT));
        sd->card.host.flags = SDMMC_HOST_FLAG_SPI | SDMMC_HOST_FLAG_DEINIT_ARG;
        sd->card.host.slot = SDSPI_DEFAULT_HOST;
        sd->card.host.max_freq_khz = SDMMC_FREQ_DEFAULT;
        sd->card.host.io_voltage = 3.3f;
        sd->card.host.init = &sdspi_host_init;
        sd->card.host.set_bus_width = NULL;
        sd->card.host.get_bus_width = NULL;
        sd->card.host.set_bus_ddr_mode = NULL;
        sd->card.host.set_card_clk = &sdspi_host_set_card_clk;
        sd->card.host.do_transaction = &sdspi_host_do_transaction;
        sd->card.host.deinit_p = &sdspi_host_remove_device;
        sd->card.host.io_int_enable = &sdspi_host_io_int_enable;
        sd->card.host.io_int_wait = &sdspi_host_io_int_wait;
        sd->card.host.command_timeout_ms = 0;
        sd->spiConfig = busCfg;
        slotCfg.gpio_cs = SPI_SD_nCS;
        slotCfg.gpio_cd = SDSPI_SLOT_NO_CD;
        slotCfg.host_id = sd->card.host.slot;
        sd->devConfig = slotCfg;
        sd->maxTimeout = MAX_TIMEOUT_SEC;
        memcpy(sd->fileSystem.protectedFilename, DEL_EXCEPTION, strlen(DEL_EXCEPTION));
        sd->fileSystem.files = files;
        sd->spaceConfig.sizeUnit = XPLR_SIZE_KB;
        ret = XPLR_SD_OK;
    }
    return ret;
}

xplrSd_error_t xplrSdInit(xplrSd_t **sd)
{
    xplrSd_error_t ret;
    esp_err_t err;
    xplrSd_card_t *card = &locSd.card;

    if (!locSd.isInit) {
        /* Local instance is not initialized so it is the first SD initialization*/
        xSdMutex = xSemaphoreCreateMutex();
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            ret = xplrSdConfig(&locSd);
            if (ret == XPLR_SD_OK) {
                XPLRSD_CONSOLE(D, "Starting Initialization of SD card in mountPoint = %s", locSd.mountPoint);
                err = spi_bus_initialize(locSd.devConfig.host_id, &locSd.spiConfig, SDSPI_DEFAULT_DMA);
                if (err != ESP_OK) {
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
                        memcpy(&locSd.card, card, sizeof(locSd.card));
                        locSd.isInit = true;
                        ret = XPLR_SD_OK;
                    }
                }
            } else {
                XPLRSD_CONSOLE(E, "Card is not inserted! Cannot initialize");
            }
            /* This is the point where we want to give the mutex back*/
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            ret = XPLR_SD_BUSY;
        }
    } else {
        ret = XPLR_SD_OK;
    }
    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        if (ret == XPLR_SD_OK) {
            *sd = &locSd;
            XPLRSD_CONSOLE(D, "Filesystem mounted in mountPoint = %s", locSd.mountPoint);
        } else {
            XPLRSD_CONSOLE(E, "SD not initialized.");
            *sd = NULL;
        }
        timer_init(TIMER_GROUP_0, TIMER_1, &timerCfg);
        timer_pause(TIMER_GROUP_0, TIMER_1);
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;

}

xplrSd_error_t xplrSdDeInit(xplrSd_t *sd)
{
    xplrSd_error_t ret;
    esp_err_t err;

    timer_deinit(TIMER_GROUP_0, TIMER_1);
    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        if (sd->isInit) {
            err = esp_vfs_fat_sdmmc_unmount();
            if (err == ESP_OK) {
                XPLRSD_CONSOLE(D, "Filesystem unmounted sucessfully");
                ret = XPLR_SD_OK;
                err = spi_bus_free(sd->card.host.slot);
                if (err == ESP_OK) {
                    XPLRSD_CONSOLE(D, "SPI bus successfully deinitialized");
                    sd->isInit = false;
                } else {
                    XPLRSD_CONSOLE(E, "SPI could not be unitialized");
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

void xplrSdPrintInfo(xplrSd_t *sd)
{
#if (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED && 1 == XPLRSD_DEBUG_ACTIVE)
    sdmmc_card_print_info(stdout, &sd->card);
#endif
}


FILE *xplrSdOpenFile(const char *filepath, xplrSd_file_mode_t filemode)
{
    FILE *f;

    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
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
                f = NULL;
                break;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        f = NULL;
    }
    return f;
}

xplrSd_error_t xplrSdCloseFile(FILE *file, const char *filepath, bool erase)
{
    xplrSd_error_t ret;

    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        int err = fclose(file);
        if (err == 0) {
            XPLRSD_CONSOLE(D, "File %s closed successfully", filepath);
            ret = XPLR_SD_OK;
        } else {
            XPLRSD_CONSOLE(E, "Error in closing file %s", filepath);
            ret = XPLR_SD_ERROR;
        }
        if (err == XPLR_SD_OK) {
            if (erase) {
                err = unlink(filepath);
                if (err == 0) {
                    XPLRSD_CONSOLE(D, "File %s deleted successfully", filepath);
                } else {
                    XPLRSD_CONSOLE(E, "Error in deletion of file %s", filepath);
                    ret = XPLR_SD_ERROR;
                }
            }
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }

    return ret;
}


int xplrSdFileList(xplrSd_t *sd)
{
    DIR *dp;
    struct dirent *ep;
    double opTime = 0;
    int index = 0;
    char temp[512];

    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0);
        dp = opendir(sd->mountPoint);
        if (dp != NULL) {
            timer_start(TIMER_GROUP_0, TIMER_1);
            while (((ep = readdir(dp)) != NULL) && (opTime <= sd->maxTimeout)) {
                timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_1, &opTime);
                memset(sd->fileSystem.files[index].filename, 0x00, 256);
                sprintf(temp, "%s/%s", sd->mountPoint, ep->d_name);
                memcpy(sd->fileSystem.files[index].filename, temp, 256);
                memset(temp, 0x00, strlen(temp));
                sd->fileSystem.files[index].isEmpty = false;
                XPLRSD_CONSOLE(D, "file found: %s , index = %d", sd->fileSystem.files[index].filename, index);
                index++;
            }
            (void) closedir(dp);
        }
        timer_pause(TIMER_GROUP_0, TIMER_1);
        sd->fileSystem.existingFiles = index;
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        index = -1;
    }
    return index;
}

int xplrSdSeekFile(xplrSd_t *sd, const char *filename)
{
    int index;
    int i = 0;

    index = xplrSdFileList(sd);
    if (index >= 0) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            index = sd->fileSystem.existingFiles;
            if (index >= 0) {
                for (i = 0; i < index; i++) {
                    if (strcasecmp(sd->fileSystem.files[i].filename, filename) == 0) {
                        break;
                    }
                }
                if (i <= index) {
                    index = i;
                } else {
                    index = -1;
                }
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            index = -1;
        }
    } else {
        XPLRSD_CONSOLE(E, "Could not list files in order to seek <%s>", filename);
        index = -1;
    }
    return index;
}

xplrSd_error_t xplrSdRenameFile(xplrSd_t *sd, const char *original, const char *renamed)
{
    xplrSd_error_t ret;
    struct stat st;

    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        XPLRSD_CONSOLE(D, "Renaming file %s to %s", original, renamed);
        /* Check if the filename is occupied by another file*/
        if (stat(renamed, &st) == 0) {
            XPLRSD_CONSOLE(W, "File %s found in filesystem, deleting...", renamed);
            unlink(renamed);
        }

        if (stat(original, &st) == 0) {
            if (rename(original, renamed) != 0) {
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
    /* Update File List*/
    xplrSdFileList(sd);


    return ret;
}

xplrSd_error_t xplrSdEraseFile(xplrSd_t *sd, const char *filepath)
{
    xplrSd_error_t ret;
    int err, index;

    index = xplrSdSeekFile(sd, filepath);
    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        if (index >= 0) {
            if (strcasecmp(filepath, sd->fileSystem.protectedFilename) == 0) {
                ret = XPLR_SD_OK;
            } else {
                err = unlink(filepath);
                if (err == 0) {
                    XPLRSD_CONSOLE(D, "File %s deleted", filepath);
                    ret = XPLR_SD_OK;
                } else {
                    XPLRSD_CONSOLE(E, "Error in deletion of file %s", filepath);
                    ret = XPLR_SD_ERROR;
                }
            }
        } else {
            XPLRSD_CONSOLE(E, "File %s not found in filesystem. Deletion could not be executed.", filepath);
            ret = XPLR_SD_NOT_FOUND;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }
    /* Update File List*/
    xplrSdFileList(sd);

    return ret;
}

xplrSd_error_t xplrSdEraseAll(xplrSd_t *sd)
{
    xplrSd_error_t ret = XPLR_SD_OK;
    int err = xplrSdFileList(sd);

    if (err >= 0) {
        ret = XPLR_SD_OK;
        for (uint8_t i = 0; i < err; i++) {
            ret = xplrSdEraseFile(sd, sd->fileSystem.files[i].filename);
            if (ret != XPLR_SD_OK) {
                break;
            }
        }
        if (ret == XPLR_SD_OK) {
            /* Update File List*/
            err = xplrSdFileList(sd);
        }
    } else {
        ret = XPLR_SD_ERROR;
    }

    return ret;
}

xplrSd_error_t xplrSdReadFileString(const char *filepath, char *value, size_t length)
{
    xplrSd_error_t ret;
    int err;
    FILE *fp = xplrSdOpenFile(filepath, XPLR_FILE_MODE_READ);

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            err = fread(value, 1, length, fp);
            if (err <= length) {
                XPLRSD_CONSOLE(D, "Read successfully %d bytes oy of the required %d", strlen(value), length);
                ret = XPLR_SD_OK;
            } else {
                XPLRSD_CONSOLE(E, "Error in reading from file");
                ret = XPLR_SD_ERROR;
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        }
        ret = xplrSdCloseFile(fp, filepath, false);
    } else {
        XPLRSD_CONSOLE(E, "File %s not found in filesystem", filepath);
        ret = XPLR_SD_NOT_FOUND;
    }

    return ret;
}

int xplrSdReadFileU8(const char *filepath, uint8_t *value, size_t length)
{
    FILE *fp = xplrSdOpenFile(filepath, XPLR_FILE_MODE_READ);
    int ret = 0;
    xplrSd_error_t sdErr;

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            ret = fread(value, 1, length, fp);
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
            ret = -1;
        }
        XPLRSD_CONSOLE(D, "Read %d bytes out of the %d requested from file %s", ret, length, filepath);
        sdErr = xplrSdCloseFile(fp, filepath, false);
        if (sdErr != XPLR_SD_OK) {
            ret = -1;
        }
    } else {
        XPLRSD_CONSOLE(E, "File %s not found in filesystem", filepath);
        ret = -1;
    }
    return ret;
}

xplrSd_error_t xplrSdWriteFileString(xplrSd_t *sd,
                                     const char *filepath,
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
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_APPEND:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_READ_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_WRITE_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_APPEND_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        default:
            XPLRSD_CONSOLE(E, "File mode unexpected error");
            break;
    }

    if (fp != NULL) {
        if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
            if (fputs(value, fp) != EOF) {
                XPLRSD_CONSOLE(D, "Write operation in file %s was successful", filepath);
                ret = XPLR_SD_OK;
            } else {
                XPLRSD_CONSOLE(E, "Write operation in file %s was unsuccessful", filepath);
                ret = XPLR_SD_ERROR;
            }
            xSemaphoreGive(xSdMutex);
        } else {
            XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        }
        ret = xplrSdCloseFile(fp, filepath, false);
        XPLRSD_CONSOLE(D, " Successfully wrote %d bytes in file %s", strlen(value), filepath);
    } else {
        ret = XPLR_SD_ERROR;
        XPLRSD_CONSOLE(E, "Could not open file %s for writing", filepath);
    }

    return ret;
}

int xplrSdWriteFileU8(xplrSd_t *sd,
                      const char *filepath,
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
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_APPEND:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_READ_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_WRITE_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
            break;
        case XPLR_FILE_MODE_APPEND_PLUS:
            fp = xplrSdOpenFile(filepath, mode);
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
            XPLRSD_CONSOLE(D, " Successfully wrote %d bytes in file %s", ret, filepath);
            sdErr = xplrSdCloseFile(fp, filepath, false);
            if (sdErr != XPLR_SD_OK) {
                XPLRSD_CONSOLE(W, "Error closing file <%s> after logging!", filepath);
            } else {
                //Do nothing
            }
        } else {
            XPLRSD_CONSOLE(E, "Write operation unsuccessful!");
        }
    } else {
        ret = -1;
        XPLRSD_CONSOLE(E, "Could not open file %s for writing", filepath);
    }

    return ret;
}

void xplrSdFormatFilename(char *filename)
{
    const char s[2] = ".";
    char *name, *fileType;
    bool isLong = false;
    size_t nameLength, fileTypeLength;
    char fatFilename[13] = {0};
    /* Null pointer check*/
    if (filename != NULL) {
        /* get the first token */
        name = strtok(filename, s);
        fileType = strtok(NULL, s);
        nameLength = strlen(name);
        if (fileType != NULL) {
            fileTypeLength = strlen(fileType);
        } else {
            fileTypeLength = 0;
        }
        if (nameLength + fileTypeLength > 11 || fileTypeLength > 3) {
            isLong = true;
        }
        if (fileTypeLength > 3) {
            fileTypeLength = 3;
        }
        if (isLong && fileTypeLength == 0) {
            snprintf(fatFilename, 11, "%s", name);
            strncat(fatFilename, "~1", 3);
        } else if (isLong && fileTypeLength != 0) {
            snprintf(fatFilename, 10 - fileTypeLength, "%s", name);
            strncat(fatFilename, "~1.", 4);
            strncat(fatFilename, fileType, fileTypeLength);
        } else if (!isLong && fileTypeLength != 0) {
            snprintf(fatFilename, 13, "%s.%s", name, fileType);
        } else {
            snprintf(fatFilename, 13, "%s", name);
        }
        strncpy(filename, fatFilename, 13);
        XPLRSD_CONSOLE(I, "Formatted filename: <%s>", fatFilename);
    } else {
        XPLRSD_CONSOLE(E, "Filename to be formatted is an NULL pointer!");
    }
}

uint64_t xplrSdGetTotalSpace(xplrSd_t *sd)
{
    uint64_t totalSpace;
    xplrSd_error_t err;

    err = sdGetStats(sd);

    if (err == XPLR_SD_OK) {
        totalSpace = sd->spaceConfig.totalSpace;
    } else {
        totalSpace = 0;
    }

    return totalSpace;
}

uint64_t xplrSdGetFreeSpace(xplrSd_t *sd)
{
    uint64_t freeSpace;
    xplrSd_error_t err;

    err = sdGetStats(sd);

    if (err == XPLR_SD_OK) {
        freeSpace = sd->spaceConfig.freeSpace;
    } else {
        freeSpace = 0;
    }

    return freeSpace;
}

uint64_t xplrSdGetUsedSpace(xplrSd_t *sd)
{
    uint64_t usedSpace;
    xplrSd_error_t err;

    err = sdGetStats(sd);

    if (err == XPLR_SD_OK) {
        usedSpace = sd->spaceConfig.usedSpace;
    } else {
        usedSpace = 0;
    }

    return usedSpace;
}

/**
 * ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ----------------------------------------------------------------
*/

xplrSd_error_t sdGetStats(xplrSd_t *sd)
{
    xplrSd_error_t ret;
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    uint64_t divider;
    char UNUSED_PARAM(*size2print);

    switch (sd->spaceConfig.sizeUnit) {
        case XPLR_SIZE_KB:
            divider = 2;
            size2print = "KBytes";
            break;

        case XPLR_SIZE_MB:
            divider = 1024 * 2;
            size2print = "MBytes";
            break;

        case XPLR_SIZE_GB:
            divider = 1024 * 1024 * 2;
            size2print = "GBytes";
            break;

        default:
            size2print = "KBytes";
            divider = 2;
            XPLRSD_CONSOLE(W, "No correct size given. Will print size in Kbytes");
            break;
    }
    if (xSemaphoreTake(xSdMutex, portMAX_DELAY) == pdTRUE) {
        /* Get volume information and free clusters of drive 0 */
        if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
            /* Get total sectors and free sectors */
            tot_sect = ((fs->n_fatent - 2) * fs->csize);
            fre_sect = (fre_clust * fs->csize);

            sd->spaceConfig.totalSpace = tot_sect / divider;
            sd->spaceConfig.freeSpace = fre_sect / divider;
            sd->spaceConfig.usedSpace = (tot_sect - fre_sect) / divider;

            /* Print the free space (assuming 512 bytes/sector) */
            XPLRSD_CONSOLE(D,
                           "%" PRIu64 " %s total drive space. %" PRIu64 " %s available.",
                           sd->spaceConfig.totalSpace,
                           size2print,
                           sd->spaceConfig.freeSpace,
                           size2print);

            ret = XPLR_SD_OK;
        } else {
            ret = XPLR_SD_ERROR;
        }
        xSemaphoreGive(xSdMutex);
    } else {
        XPLRSD_CONSOLE(E, "Could not take mutex to be able to access the SD card...");
        ret = XPLR_SD_BUSY;
    }
    return ret;
}