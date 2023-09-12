#include <stdio.h>
#include <string.h>
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "./../../../components/hpglib/src/com_service/xplr_com.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/ntripCellClient_service/xplr_cell_ntrip_client.h"
#include "driver/timer.h"

/*
 * An example utilizing NTRIP Cellular Client module to fetch correction data.
 *
 * In the current example U-blox XPLR-HPG kit is initialized using boards component,
 * connects to the cellular network using xplr_com component,
 * connects to the NTRIP caster, using xplr_ntrip component.
 *
 */

/* if paths not found run: <ctrl+shift+p> ESP-IDF: Add vscode configuration folder */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA         0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)  esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)\
    snprintf(&appBuff2Log[0], ELEMENTCNT(appBuff2Log), #tag " [(%u) %s|%s|%d|: " message "\n", esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    if(strcmp(#tag, "E") == 0)  XPLRLOG(&errorLog,appBuff2Log); \
    else XPLRLOG(&appLog,appBuff2Log);
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

#define APP_GNSS_LOC_INTERVAL           (1 * 1)         /* frequency of location info logging to console in seconds */
#if 1 == APP_PRINT_IMU_DATA
#define APP_GNSS_DR_INTERVAL            (5 * 1)         /* frequency of dead reckoning info logging to console in seconds */
#endif
#define APP_NTRIP_STATE_INTERVAL_SEC    (15)            /* frequency of NTRIP client state logging to console in seconds */
#define APP_RUN_TIME_SEC                (120)           /* period of app (in seconds) before exiting */
#define APP_DEVICE_OFF_MODE_BTN         (BOARD_IO_BTN1) /* Button for shutting down device */
#define APP_DEVICE_OFF_MODE_TRIGGER     (3U)            /* Device off press duration in sec */

#define APP_GNSS_I2C_ADDR 0x42

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* application errors */
typedef enum {
    APP_ERROR_UNKNOWN = -6,
    APP_ERROR_CELL_INIT,
    APP_ERROR_GNSS_INIT,
    APP_ERROR_NTRIP_INIT,
    APP_ERROR_NTRIP_TASK,
    APP_ERROR_NETWORK_OFFLINE,
    APP_ERROR_OK = 0,
} app_error_t;

/* application states */
typedef enum {
    APP_FSM_INACTIVE = -2,
    APP_FSM_ERROR,
    APP_FSM_INIT_HW = 0,
    APP_FSM_INIT_PERIPHERALS,
    APP_FSM_CONFIG_GNSS,
    APP_FSM_CHECK_NETWORK,
    APP_FSM_INIT_NTRIP_CLIENT,
    APP_FSM_RUN,
    APP_FSM_TERMINATE,
} app_fsm_t;

typedef struct app_type {
    app_error_t error;
    app_fsm_t state[2];
    uint64_t time;
    uint64_t timeOut;
} app_t;

char ntripBuffer[XPLRCELL_NTRIP_RECEIVE_DATA_SIZE];
uint32_t ntripSize;
SemaphoreHandle_t ntripSemaphore;

/**
 * You can use KConfig to set up these values.
 */
static const char ntripHost[] = CONFIG_XPLR_CELL_NTRIP_HOST;
static const int ntripPort = CONFIG_XPLR_CELL_NTRIP_PORT;
static const char ntripMountpoint[] = CONFIG_XPLR_CELL_NTRIP_MOUNTPOINT;
static const char ntripUserAgent[] = CONFIG_XPLR_CELL_NTRIP_USERAGENT;
#ifdef CONFIG_XPLR_CELL_NTRIP_GGA_MSG
static const bool ntripSendGga = true;
#else
static const bool ntripSendGga = false;
#endif
#ifdef CONFIG_XPLR_CELL_NTRIP_USE_AUTH
static const bool ntripUseAuth = true;
#else
static const bool ntripUseAuth = false;
#endif
static const char ntripUser[] = CONFIG_XPLR_CELL_NTRIP_USERNAME;
static const char ntripPass[] = CONFIG_XPLR_CELL_NTRIP_PASSWORD;

xplrCell_ntrip_client_t ntripClient;
xplrCell_ntrip_error_t ntripClientError;
xplrCell_ntrip_detailed_error_t ntripClientDetailedError;

/*INDENT-OFF*/
/**
 * Static log configuration struct and variables
 */
#if (1 == APP_SD_LOGGING_ENABLED)
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_LARGE];
static char appLogFilename[] = "/APPLOG.TXT";               /**< Follow the same format if changing the filename*/
static char errorLogFilename[] = "/ERRORLOG.TXT";           /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize = 100;                        /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType = XPLR_SIZE_MB;    /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif
/*INDENT-ON*/

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

app_t app;

/* ubxlib configuration structs.
 * Configuration parameters are passed by calling  configCellSettings()
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static uDeviceCfgCell_t cellHwConfig;
static uDeviceCfgUart_t cellComConfig;
static uNetworkCfgCell_t netConfig;
/* hpg com service configuration struct  */
static xplrCom_cell_config_t cellConfig;
/* gnss device id */
const uint8_t gnssDvcPrfId = 0;
/* location modules */
xplrGnssStates_t gnssState;
xplrLocDvcInfo_t gnssDvcInfo;
xplrGnssLocation_t gnssLocation;
#if 1 == APP_PRINT_IMU_DATA
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif
/**
 * Keeps time at "this" point of variable
 * assignment in code
 * Used for periodic printing
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

char GgaMsg[256];
char *pGgaMsg = GgaMsg;
char **ppGgaMsg = &pGgaMsg;
int32_t len;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* initialize logging to the SD card*/
static void appInitLog(void);
/* initialize hw */
static esp_err_t appInitBoard(void);
/* configure free running timer for calculating time intervals in app */
static void timerInit(void);
/* configure gnss related settings*/
static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
/* configure cellular related settings */
static void configCellSettings(xplrCom_cell_config_t *cfg);
/* initialize cellular module */
static app_error_t cellInit(void);
/* runs the fsm for the GNSS module */
static app_error_t gnssRunFsm(void);
/* register cellular module to the network */
static app_error_t cellNetworkRegister(void);
/* check if cellular module is connected to the network */
static app_error_t cellNetworkConnected(void);
/* initialize the GNSS module */
static app_error_t gnssInit(void);
/* print location info to console */
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
/* print dead reckoning info to console */
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
/* initialize app */
static void appInit(void);
/* initialize ntrip client */
static app_error_t ntripInit(void);
/* terminate/deinitialize logging to the SD card*/
static void appDeInitLog(void);
/* terminate app */
static app_error_t appTerminate(void);
/* powerdown device modules */
static void appDeviceOffTask(void *arg);
/* A dummy function to pause on error */
static void appHaltExecution(void);

/* ----------------------------------------------------------------
 * MAIN APP
 * -------------------------------------------------------------- */
void app_main(void)
{
    timePrevLoc = 0;
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = 0;
#endif

    appInitLog();

    APP_CONSOLE(I, "XPLR-HPG-SW Demo: NTRIP Cellular Client\n");

    while (1) {
        switch (app.state[0]) {
            case APP_FSM_INIT_HW:
                app.state[1] = app.state[0];
                appInitBoard();
                appInit();
                app.timeOut = MICROTOSEC(esp_timer_get_time());
                app.state[0] = APP_FSM_INIT_PERIPHERALS;
                break;
            case APP_FSM_INIT_PERIPHERALS:
                app.state[1] = app.state[0];
                app.error = gnssInit();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.error = cellInit();
                    app.state[0] = APP_FSM_CONFIG_GNSS;
                }
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_CONFIG_GNSS;
                }
                break;
            case APP_FSM_CONFIG_GNSS:
                app.state[1] = app.state[0];
                app.error = gnssRunFsm();
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        app.state[0] = APP_FSM_CHECK_NETWORK;
                    } else {
                        /* module still configuring. do nothing */
                    }
                }
                break;
            case APP_FSM_CHECK_NETWORK:
                app.state[1] = app.state[0];
                app.error = cellNetworkRegister();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_INIT_NTRIP_CLIENT;
                } else if (app.error == APP_ERROR_NETWORK_OFFLINE) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    /* module still trying to connect. do nothing */
                }
                break;
            case APP_FSM_INIT_NTRIP_CLIENT:

                app.state[1] = app.state[0];
                app.error = ntripInit();
                if (app.error == APP_ERROR_OK) {
                    app.state[0] = APP_FSM_RUN;
                } else {
                    app.state[0] = APP_FSM_ERROR;
                }
                break;
            case APP_FSM_RUN:
                app.state[1] = app.state[0];

                /* run GNSS FSM */
                app.error = gnssRunFsm();
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    appPrintLocation(APP_GNSS_LOC_INTERVAL);
#if 1 == APP_PRINT_IMU_DATA
                    appPrintDeadReckoning(APP_GNSS_DR_INTERVAL);
#endif
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        switch (xplrCellNtripGetClientState(&ntripClient)) {
                            case XPLR_CELL_NTRIP_STATE_READY:
                                // NTRIP client operates normally no action needed from APP
                                break;
                            case XPLR_CELL_NTRIP_STATE_CORRECTION_DATA_AVAILABLE:
                                // NTRIP client has received correction data
                                xplrCellNtripGetCorrectionData(&ntripClient,
                                                               ntripBuffer,
                                                               XPLRCELL_NTRIP_RECEIVE_DATA_SIZE,
                                                               &ntripSize);
                                APP_CONSOLE(I, "Received correction data [%d B]", ntripSize);
                                xplrGnssSendRtcmCorrectionData(gnssDvcPrfId, ntripBuffer, ntripSize);
                                break;
                            case XPLR_CELL_NTRIP_STATE_REQUEST_GGA:
                                // NTRIP client requires GGA to send back to server
                                memset(GgaMsg, 0x00, strlen(GgaMsg));
                                len = xplrGnssGetGgaMessage(gnssDvcPrfId, ppGgaMsg, ELEMENTCNT(GgaMsg));
                                xplrCellNtripSendGGA(&ntripClient, GgaMsg, len);
                                break;
                            case XPLR_CELL_NTRIP_STATE_ERROR:
                                // NTRIP client encountered an error
                                APP_CONSOLE(E, "NTRIP Client encountered error");
                                // Check the detailer error code
                                ntripClientDetailedError = xplrCellNtripGetDetailedError(&ntripClient);
                                // Handle specific error
                                // ...
                                app.state[0] = APP_FSM_ERROR;
                                break;
                            case XPLR_CELL_NTRIP_STATE_BUSY:
                                // NTRIP client busy, retry until state changes
                                break;
                            default:
                                break;
                        }
                    } else {
                        //do nothing
                    }

                    vTaskDelay(pdMS_TO_TICKS(25));
                    /* Check if its time to terminate the app */
                    if (MICROTOSEC(esp_timer_get_time()) - app.timeOut >= APP_RUN_TIME_SEC) {
                        app.state[0] = APP_FSM_TERMINATE;
                    }
                }
                break;
            case APP_FSM_TERMINATE:
                app.state[1] = app.state[0];
                app.error = appTerminate();
                if (app.error != APP_ERROR_OK) {
                    app.state[0] = APP_FSM_ERROR;
                } else {
                    app.state[0] = APP_FSM_INACTIVE;
                }
                break;
            case APP_FSM_INACTIVE:
                APP_CONSOLE(I, "ALL DONE!!!");
                appHaltExecution();
                break;
            case APP_FSM_ERROR:
                APP_CONSOLE(E, "Halting execution");
                appHaltExecution();
                break;
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DEFINITIONS
 * -------------------------------------------------------------- */

static void configGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
{
    /**
     * Pin numbers are those of the MCU: if you
     * are using an MCU inside a u-blox module the IO pin numbering
     * for the module is likely different that from the MCU: check
     * the data sheet for the module to determine the mapping
     * DEVICE i.e. module/chip configuration: in this case a gnss
     * module connected via UART
     */
    gnssCfg->hw.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    gnssCfg->hw.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_GNSS_I2C_ADDR;
    gnssCfg->hw.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    gnssCfg->hw.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    gnssCfg->hw.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    gnssCfg->hw.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    gnssCfg->hw.dvcNetwork.devicePinPwr = -1;
    gnssCfg->hw.dvcNetwork.devicePinDataReady = -1;

    gnssCfg->dr.enable = CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE;
    gnssCfg->dr.mode = XPLR_GNSS_IMU_CALIBRATION_AUTO;
    gnssCfg->dr.vehicleDynMode = XPLR_GNSS_DYNMODE_AUTOMOTIVE;

    gnssCfg->corrData.keys.size = 0;
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
}

static void appInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLog_error_t err = xplrLogInit(&errorLog,
                                      XPLR_LOG_DEVICE_ERROR,
                                      errorLogFilename,
                                      logFileMaxSize,
                                      logFileMaxSizeType);
    if (err == XPLR_LOG_OK) {
        errorLog.logEnable = true;
        err = xplrLogInit(&appLog,
                          XPLR_LOG_DEVICE_INFO,
                          appLogFilename,
                          logFileMaxSize,
                          logFileMaxSizeType);
    }
    if (err == XPLR_LOG_OK) {
        appLog.logEnable = true;
    } else {
        APP_CONSOLE(E, "Error initializing logging...");
    }
#endif
}

static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t ret;

    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_DEVICE_OFF_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        ret = gpio_config(&io_conf);
    }

    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        if (xTaskCreate(appDeviceOffTask, "deviceOffTask", 2 * 2048, NULL, 10, NULL) == pdPASS) {
            APP_CONSOLE(D, "Boot0 pin configured as button OK");
            APP_CONSOLE(D, "Board Initialized");
        } else {
            APP_CONSOLE(D, "Failed to start deviceOffTask task");
            APP_CONSOLE(E, "Board initialization failed!");
            ret = ESP_FAIL;
        }
    }

    return ret;
}

static void configCellSettings(xplrCom_cell_config_t *cfg)
{
    /* Config hardware pins connected to cellular module */
    cfg->hwSettings = &cellHwConfig;
    cfg->comSettings = &cellComConfig;
    cfg->netSettings = &netConfig;

    /*
     * Pin numbers are those of the MCU: if you
     * are using an MCU inside a u-blox module the IO pin numbering
     * for the module is likely different that from the MCU: check
     * the data sheet for the module to determine the mapping
     * DEVICE i.e. module/chip configuration: in this case a cellular
     * module connected via UART
     */

    cfg->hwSettings->moduleType = U_CELL_MODULE_TYPE_LARA_R6;
    cfg->hwSettings->pSimPinCode = NULL;
    cfg->hwSettings->pinEnablePower = -1;
    cfg->hwSettings->pinPwrOn = BOARD_IO_LTE_PWR_ON;

    cfg->hwSettings->pinVInt = BOARD_IO_LTE_ON_nSENSE;
    cfg->hwSettings->pinDtrPowerSaving = -1;

    cfg->comSettings->uart = 1;
    cfg->comSettings->baudRate = U_CELL_UART_BAUD_RATE;
    cfg->comSettings->pinTxd = BOARD_IO_UART_LTE_TX;
    cfg->comSettings->pinRxd = BOARD_IO_UART_LTE_RX;
    cfg->comSettings->pinCts = BOARD_IO_UART_LTE_CTS;
    cfg->comSettings->pinRts = BOARD_IO_UART_LTE_RTS;

    cfg->netSettings->type = U_NETWORK_TYPE_CELL;
    cfg->netSettings->pApn = CONFIG_XPLR_CELL_APN;
    cfg->netSettings->timeoutSeconds = 240; /* Connection timeout in seconds */
    cfg->mno = 100;

    cfg->ratList[0] = U_CELL_NET_RAT_LTE;
    cfg->ratList[1] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    cfg->ratList[2] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    cfg->bandList[0] = 0;
    cfg->bandList[1] = 0;
    cfg->bandList[2] = 0;
    cfg->bandList[3] = 0;
    cfg->bandList[4] = 0;
    cfg->bandList[5] = 0;
}

static void timerInit(void)
{
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
    timer_init(TIMER_GROUP_0, TIMER_0, &timerCfg);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
}

static app_error_t cellInit(void)
{
    /* initialize ubxlib and cellular module */

    app_error_t ret;
    xplrCom_error_t err;

    /* initialize ubxlib and cellular module */
    err = xplrUbxlibInit(); /* Initialise the ubxlib APIs we will need */
    if (err == XPLR_COM_OK) {
        configCellSettings(&cellConfig);    /* Setup configuration parameters for hpg com */
        err = xplrComCellInit(&cellConfig); /* Initialize hpg com */
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_CELL_INIT;
        APP_CONSOLE(E, "Cell setting init failed with code %d.\n", err);
    }

    return ret;
}

static app_error_t gnssRunFsm(void)
{
    app_error_t ret;
    xplrGnssStates_t state;

    xplrGnssFsm(gnssDvcPrfId);
    state = xplrGnssGetCurrentState(gnssDvcPrfId);

    switch (state) {
        case XPLR_GNSS_STATE_DEVICE_READY:
            ret = APP_ERROR_OK;
            break;

        case XPLR_GNSS_STATE_ERROR:
            ret = APP_ERROR_GNSS_INIT;
            break;

        default:
            ret = APP_ERROR_OK;
    }

    return ret;
}

static app_error_t cellNetworkRegister(void)
{
    app_error_t ret;
    xplrCom_cell_connect_t comState;

    xplrComCellFsmConnect(cellConfig.profileIndex);
    comState = xplrComCellFsmConnectGetState(cellConfig.profileIndex);

    switch (comState) {
        case XPLR_COM_CELL_CONNECTED:
            APP_CONSOLE(I, "Cell module is Online.");
            /* quick blink 5 times*/
            for (int i = 0; i < 5; i++) {
                xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            xplrBoardSetLed(XPLR_BOARD_LED_ON);
            ret = APP_ERROR_OK;
            break;
        case XPLR_COM_CELL_CONNECT_TIMEOUT:
        case XPLR_COM_CELL_CONNECT_ERROR:
            APP_CONSOLE(W, "Cell module is Offline.");
#if (APP_SHUTDOWN_CELL_AFTER_REGISTRATION == 1)
            APP_CONSOLE(E, "Cellular registration not completed. Shutting down cell dvc.");
            xplrComCellPowerDown(cellConfig.profileIndex);
            xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
#endif
            /* slow blink 5 times*/
            for (int i = 0; i < 5; i++) {
                xplrBoardSetLed(XPLR_BOARD_LED_TOGGLE);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            xplrBoardSetLed(XPLR_BOARD_LED_ON);
            ret = APP_ERROR_NETWORK_OFFLINE;
            break;

        default:
            ret = APP_ERROR_UNKNOWN;
            break;
    }

    return ret;
}

static app_error_t cellNetworkConnected(void)
{
    int8_t id = cellConfig.profileIndex;
    xplrCom_cell_connect_t comState;
    app_error_t ret;

    xplrComCellFsmConnect(id);
    comState = xplrComCellFsmConnectGetState(id);
    if (comState == XPLR_COM_CELL_CONNECTED) {
        ret = APP_ERROR_OK;
    } else {
        ret = APP_ERROR_NETWORK_OFFLINE;
    }

    return ret;
}

static app_error_t gnssInit(void)
{
    esp_err_t err;
    app_error_t ret;

    err = xplrGnssUbxlibInit();
    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "UbxLib init (GNSS) failed!");
    } else {
        APP_CONSOLE(W, "Waiting for GNSS device to come online!");
        configGnssSettings(&dvcGnssConfig);
        err = xplrGnssStartDevice(gnssDvcPrfId, &dvcGnssConfig);
    }

    if (err != ESP_OK) {
        ret = APP_ERROR_GNSS_INIT;
        APP_CONSOLE(E, "Failed to start GNSS");
    } else {
        ret = APP_ERROR_OK;
    }

    return ret;
}

static app_error_t ntripInit(void)
{
    xplrCell_ntrip_error_t err;
    app_error_t ret = APP_ERROR_OK;

    ret = cellNetworkConnected();

    if (ret == APP_ERROR_OK) {
        xplrCellNtripSetConfig(&ntripClient, ntripHost, ntripPort, ntripMountpoint, 0, ntripSendGga);
        xplrCellNtripSetCredentials(&ntripClient, ntripUseAuth, ntripUser, ntripPass, ntripUserAgent);

        ntripSemaphore = xSemaphoreCreateMutex();
        err = xplrCellNtripInit(&ntripClient, ntripSemaphore);

        if (err != XPLR_CELL_NTRIP_OK) {
            APP_CONSOLE(E, "NTRIP client initialization failed!");
            ret = APP_ERROR_NTRIP_INIT;
        }
    } else if (ret == APP_ERROR_NETWORK_OFFLINE) {
        APP_CONSOLE(E, "Cellular network offline");
    }

    return ret;
}

/**
 * Prints locations according to period
 */
static void appPrintLocation(uint8_t periodSecs)
{
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) && xplrGnssHasMessage(0)) {
        ret = xplrGnssGetLocationData(0, &gnssLocation);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
        } else {
            ret = xplrGnssPrintLocationData(&gnssLocation);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
            }
        }

        ret = xplrGnssPrintGmapsLocation(0);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }

        timePrevLoc = MICROTOSEC(esp_timer_get_time());
    }
}

#if 1 == APP_PRINT_IMU_DATA
/**
 * Prints Dead Reckoning data over a period (seconds).
 * Declare a period in seconds to print location.
 */
static void appPrintDeadReckoning(uint8_t periodSecs)
{
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevDr >= periodSecs) &&
        xplrGnssIsDrEnabled(gnssDvcPrfId)) {
        ret = xplrGnssGetImuAlignmentInfo(gnssDvcPrfId, &imuAlignmentInfo);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment info!");
        }

        ret = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment data!");
        }

        ret = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        }

        ret = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment status!");
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            ret = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            }

            ret = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
            }
        }

        timePrevDr = MICROTOSEC(esp_timer_get_time());
    }
}
#endif

static void appInit(void)
{
    app.state[0] = APP_FSM_INIT_HW;
    timerInit();
    app.state[0] = APP_FSM_INIT_PERIPHERALS;
}

static void appDeInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
#endif
}

static app_error_t appTerminate(void)
{
    app_error_t ret;
    esp_err_t gnssErr;
    xplrCell_ntrip_error_t ntripRet;

    ntripRet = xplrCellNtripDeInit(&ntripClient);
    if (ntripRet != XPLR_CELL_NTRIP_ERROR) {
        gnssErr = xplrGnssStopDevice(gnssDvcPrfId);
        if (gnssErr != ESP_OK) {
            APP_CONSOLE(E, "App could not stop gnss device.");
            ret = APP_ERROR_GNSS_INIT;
        } else {
            ret = APP_ERROR_OK;
        }
    } else {
        APP_CONSOLE(E, "App could not de-init the NTRIP client.");
        ret = APP_ERROR_NTRIP_INIT;
    }

    APP_CONSOLE(W, "App disconnected the NTRIP client.");
    xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
    appDeInitLog();

    return ret;
}


static void appDeviceOffTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;
    esp_err_t gnssErr;


    for (;;) {
        btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        //Check button button state
        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;
        } else {
            //reset hold duration on release
            btnPressDuration = 0;
        }

        /*
         *  Check button hold duration.
         * Power down device if:
         *  button hold duration >= APP_DEVICE_OFF_MODE_TRIGGER
         *  and
         *  not already powered down by the app
        */
        if (btnPressDuration >= APP_DEVICE_OFF_MODE_TRIGGER) {
            if (app.state[0] != APP_FSM_INACTIVE) {
                APP_CONSOLE(W, "Device OFF triggered");
                xplrGnssHaltLogModule(XPLR_GNSS_LOG_MODULE_ALL);
                gnssErr = xplrGnssStopDevice(gnssDvcPrfId);
                if (gnssErr != ESP_OK) {
                    APP_CONSOLE(E, "Couldn't stop gnss device");
                } else {
                    //nothing to do...
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
            } else {
                APP_CONSOLE(D, "Device is powered down, nothing to do...");
            }
        } else {
            //nothing to do...
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}

static void appHaltExecution(void)
{
    appDeInitLog();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}