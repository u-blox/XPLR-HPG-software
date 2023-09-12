/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * An example for LBAND data correction with Thingstream (U-blox broker) MQTT assistance for decryption keys and GNSS for location.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream, using the Zero Touch Provisioning (referred as ZTP from now on) to achieve a connection to the MQTT broker
 * subscribes to PointPerfect decryption keys topic, using hpg_mqtt component,
 * sets up LBAND and GNSS using location_Service component,
 * and finally feed correction data from LBAND to GNSS
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "xplr_wifi_starter.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "u_cfg_app_platform_specific.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/location_service/lband_service/xplr_lband.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA         0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED   1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED     0U /* used to log the debug messages to the sd card. Set to 0 for disabling*/
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

/*
 * Simple macros used to set buffer size in bytes
 */
#define APP_MQTT_PAYLOAD_BUF_SIZE (512U)

/**
 * Interval to print location (in seconds)
 */
#define APP_LOCATION_PRINT_PERIOD  5

#if 1 == APP_PRINT_IMU_DATA
/**
 * Period in seconds to print Dead Reckoning data
 */
#define APP_DEAD_RECKONING_PRINT_PERIOD 5
#endif

/**
 * GNSS and LBAND I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42
#define APP_LBAND_I2C_ADDR 0x43

/**
 * Decryption keys distribution topic
 */
#define APP_KEYS_TOPIC  "/pp/ubx/0236/Lb"
#define APP_FREQ_TOPIC  "/pp/frequencies/Lb"

/**
 * Correction Data Region
*/
#define APP_REGION_FREQUENCY   XPLR_LBAND_FREQUENCY_EU

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * EXTERNAL VARIABLES
 * -------------------------------------------------------------- */

/**
 * Populate files:
 * - client.crt
 * - client.key
 * - root.crt
 * according to your needs.
 * If you are using Thingstream then you can find all the needed
 * certificates inside your location thing settings.
 */
extern const uint8_t client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t client_crt_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");
extern const uint8_t server_root_crt_start[] asm("_binary_root_crt_start");
extern const uint8_t server_root_crt_end[] asm("_binary_root_crt_end");

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * Location modules configs
 */
static xplrGnssDeviceCfg_t dvcGnssConfig;
static xplrLbandDeviceCfg_t dvcLbandConfig;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/**
 * Gnss and lband device profiles id
 */
const uint8_t gnssDvcPrfId = 0;
const uint8_t lbandDvcPrfId = 0;

/**
 * Location data struct
 */
static xplrGnssLocation_t locData;

#if 1 == APP_PRINT_IMU_DATA
/**
 * Dead Reckoning data structs
 */
xplrGnssImuAlignmentInfo_t imuAlignmentInfo;
xplrGnssImuFusionStatus_t imuFusionStatus;
xplrGnssImuVehDynMeas_t imuVehicleDynamics;
#endif

/**
 * You can use KConfig to set up these values.
 * CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME are taken from Thingstream.
 * In any case you can chose to overwrite those settings
 * by replacing CONFIG_XPLR_MQTTWIFI_CLIENT_ID and CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME
 * with the strings of your choice.
 */
static const char mqttClientId[] = CONFIG_XPLR_MQTTWIFI_CLIENT_ID;
static const char mqttHost[] = CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME;

/**
 * GNSS device handler used by LBAND module
 * to send correction data
 */
uDeviceHandle_t *gnssHandler;

/**
 * Frequency read from LBAND module
 */
static uint32_t frequency;

/**
 * Keeps time at "this" point of variable
 * assignment in code
 * Used for periodic printing
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the
 * values as needed here.
 */
static const char  wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char  wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA,
    .webserver = false
};

/**
 * MQTT client configuration
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;
static char *topicArray[] = {APP_KEYS_TOPIC,
                             APP_FREQ_TOPIC
                            };

/**
 * A struct where we can store our received MQTT message
 */
static char data[APP_MQTT_PAYLOAD_BUF_SIZE];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
static xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = ELEMENTCNT(data)
};

/**
 * Boolean flags for different functions
 */
static bool requestDc;
static bool keysSent;

static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;
/*INDENT-OFF*/
/**
 * Static log configuration struct and variables
 */
#if (1 == APP_SD_LOGGING_ENABLED)
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_SMALL];
static char appLogFilename[] = "/APPLOG.TXT";                  /**< Follow the same format if changing the filename*/
static char errorLogFilename[] = "/ERRORLOG.TXT";              /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize = 100;                           /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType = XPLR_SIZE_MB;       /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif
/*INDENT-ON*/
/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitLog(void);
static void appInitBoard(void);
static void appInitLocationDevices(void);
static void appPrintDeviceInfos(void);
static void appInitWiFi(void);
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
static void appMqttInit(void);
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appDeInitLog(void);
static void appHaltExecution(void);

void app_main(void)
{
    appInitLog();
    appInitBoard();
    appInitWiFi();
    xplrMqttWifiInitState(&mqttClient);
    appInitLocationDevices();
    appPrintDeviceInfos();

    timePrevLoc = MICROTOSEC(esp_timer_get_time());
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = MICROTOSEC(esp_timer_get_time());
#endif
    keysSent = false;

    while (1) {
        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                if (dvcLbandConfig.destHandler == NULL) {
                    gnssHandler = xplrGnssGetHandler(gnssDvcPrfId);
                    if (gnssHandler != NULL) {
                        espRet = xplrLbandSetDestGnssHandler(lbandDvcPrfId, gnssHandler);
                        if (espRet == OK) {
                            espRet = xplrLbandSendCorrectionDataAsyncStart(lbandDvcPrfId);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to get start Lband Async sender!");
                                appHaltExecution();
                            } else {
                                APP_CONSOLE(D, "Successfully started Lband Async sender!");
                            }
                        }
                    } else {
                        APP_CONSOLE(E, "Failed to get GNSS handler!");
                        appHaltExecution();
                    }
                }
                appPrintLocation(APP_LOCATION_PRINT_PERIOD);
#if 1 == APP_PRINT_IMU_DATA
                appPrintDeadReckoning(APP_DEAD_RECKONING_PRINT_PERIOD);
#endif
                break;

            case XPLR_GNSS_STATE_DEVICE_RESTART:
                if (dvcLbandConfig.destHandler != NULL) {
                    espRet = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "Failed to get stop Lband Async sender!");
                        appHaltExecution();
                    } else {
                        APP_CONSOLE(D, "Successfully stoped Lband Async sender!");
                        gnssHandler = NULL;
                    }
                }
                break;

            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                gnssHandler = NULL;
                appHaltExecution();
                break;

            default:
                break;
        }

        wifistarterErr = xplrWifiStarterFsm();

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_UNINIT ||
                xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_DISCONNECTED_OK) {
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                requestDc = false;
            }
        }

        mqttErr = xplrMqttWifiFsm(&mqttClient);

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    espRet = xplrMqttWifiSubscribeToTopicArray(&mqttClient,
                                                               topicArray,
                                                               ELEMENTCNT(topicArray),
                                                               XPLR_MQTTWIFI_QOS_LVL_0);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArray failed!");
                        appHaltExecution();
                    }
                }
                break;

            /**
             * If we are subscribed to a topic we can start printing some messages
             * We only need the decryption keys since correction data will be fed from
             * the LBAND module to the GNSS module
            */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                /**
                 * The following function will digest messages store in the internal buffer.
                 * If the user does not use it it will be discarded.
                 */
                if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                    /**
                     * Do not send data if GNSS is not in ready state.
                     * The device might not be initialized and the device handler
                     * would be NULL
                     */
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        if (strcmp(mqttMessage.topic, APP_KEYS_TOPIC) == 0) {
                            espRet = xplrGnssSendDecryptionKeys(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                appHaltExecution();
                            } else {
                                APP_CONSOLE(I, "Decryption keys sent successfully!");
                                keysSent = true;
                            }
                        }
                    }

                    if (strcmp(mqttMessage.topic, APP_FREQ_TOPIC) == 0) {
                        espRet = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId, mqttMessage.data, APP_REGION_FREQUENCY);
                        if (espRet != ESP_OK) {
                            APP_CONSOLE(E, "Failed to set frequency!");
                            appHaltExecution();
                        } else {
                            frequency = xplrLbandGetFrequency(lbandDvcPrfId);
                            if (frequency == 0) {
                                APP_CONSOLE(I, "No LBAND frequency is set");
                            }
                            APP_CONSOLE(I, "Frequency %d Hz read from device successfully!", frequency);
                        }
                    }
                }
                break;

            default:
                break;
        }

        /**
         * We lost WiFi connection.
         * When we reconnect everything will be executed from the beginning:
         * - ZTP gets the settings again
         * - Try to re-connect to MQTT
         * We are using xplrMqttWifiHardDisconnect() because autoreconnect is enabled
         * by default in the client's settings. This is a default behaviour
         * of the component provided by ESP-IDF.
         * You can change this by setting
         *      disable_auto_reconnect = false
         * in esp_mqtt_client_config_t
         * When autoreconnect is enabled then the client will always try
         * to reconnect, even if the user requests a disconnect.
         * If you request a hard disconnect then the client handler
         * and callback are destroyed and so is autoreconnect.
         */
        if ((requestDc == false) &&
            ((xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_DISCONNECT_OK) ||
             (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_SCHEDULE_RECONNECT))) {

            if (mqttClient.handler != NULL) {
                xplrMqttWifiHardDisconnect(&mqttClient);
            }

            requestDc = true;
        }

        /**
         * A window so other tasks can run
         */
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

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

/*
 * Initialize XPLR-HPG kit using its board file
 */
static void appInitBoard(void)
{
    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    }
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    }
}

/**
 * Populates gnss settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
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
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_LBAND;
}

/**
 * Populates lband settings
 */
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg)
{
    /**
    * Pin numbers are those of the MCU: if you
    * are using an MCU inside a u-blox module the IO pin numbering
    * for the module is likely different that from the MCU: check
    * the data sheet for the module to determine the mapping
    * DEVICE i.e. module/chip configuration: in this case an lband
    * module connected via UART
    */
    lbandCfg->hwConf.dvcConfig.deviceType = U_DEVICE_TYPE_GNSS;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.moduleType      =  1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinEnablePower  = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.pinDataReady    = -1;
    lbandCfg->hwConf.dvcConfig.deviceCfg.cfgGnss.i2cAddress = APP_LBAND_I2C_ADDR;
    lbandCfg->hwConf.dvcConfig.transportType = U_DEVICE_TRANSPORT_TYPE_I2C;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.i2c = 0;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinSda = BOARD_IO_I2C_PERIPHERALS_SDA;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.pinScl = BOARD_IO_I2C_PERIPHERALS_SCL;
    lbandCfg->hwConf.dvcConfig.transportCfg.cfgI2c.clockHertz = 400000;

    lbandCfg->hwConf.dvcNetwork.type = U_NETWORK_TYPE_GNSS;
    lbandCfg->hwConf.dvcNetwork.moduleType = U_GNSS_MODULE_TYPE_M9;
    lbandCfg->hwConf.dvcNetwork.devicePinPwr = -1;
    lbandCfg->hwConf.dvcNetwork.devicePinDataReady = -1;

    lbandCfg->destHandler = NULL;

    lbandCfg->corrDataConf.freq = 0;
    lbandCfg->corrDataConf.region = APP_REGION_FREQUENCY;
}

/**
 * Makes all required initializations for location
 * modules/devices
 */
static void appInitLocationDevices(void)
{
    /**
     * Init ubxlib
     */
    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    appConfigGnssSettings(&dvcGnssConfig);
    espRet = xplrGnssStartDevice(gnssDvcPrfId, &dvcGnssConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    APP_CONSOLE(D, "Waiting for LBAND device to come online!");
    appConfigLbandSettings(&dvcLbandConfig);
    espRet = xplrLbandStartDevice(gnssDvcPrfId, &dvcLbandConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Lband device config failed!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Prints some info for the initialized devices
 */
static void appPrintDeviceInfos(void)
{
    espRet = xplrLbandPrintDeviceInfo(lbandDvcPrfId);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to print LBAND device info!");
        appHaltExecution();
    }
}

/**
 * Populate MQTT Wi-Fi client settings
 */
static void appMqttInit(void)
{
    esp_err_t ret;
    /**
     * We declare how many slots a ring buffer should have.
     * You can chose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    ret = xplrMqttWifiSetRingbuffSlotsCount(&mqttClient, 6);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set MQTT ringbuffer slots!");
        appHaltExecution();
    }

    /**
     * Settings for MQTT client
     */
    mqttClientConfig.uri = mqttHost;
    mqttClientConfig.client_id = mqttClientId;
    mqttClientConfig.client_cert_pem = (const char *)client_crt_start;
    mqttClientConfig.client_key_pem = (const char *)client_key_start;
    mqttClientConfig.cert_pem = (const char *)server_root_crt_start;

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Start MQTT Wi-Fi Client
     */
    ret = xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to initialize Mqtt client!");
        appHaltExecution();
    }
}

/**
 * Prints location data over a period (seconds).
 * Declare a period in Period in seconds to print location.
 */
static void appPrintLocation(uint8_t periodSecs)
{
    if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) >= periodSecs) &&
        xplrGnssHasMessage(gnssDvcPrfId)) {
        espRet = xplrGnssGetLocationData(gnssDvcPrfId, &locData);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
        } else {
            espRet = xplrGnssPrintLocationData(&locData);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
            }
        }

        espRet = xplrGnssPrintGmapsLocation(gnssDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }

        timePrevLoc = esp_timer_get_time();
    }
}

#if 1 == APP_PRINT_IMU_DATA
/**
 * Prints Dead Reckoning data over a period (seconds).
 * Declare a period in seconds to print location.
 */
static void appPrintDeadReckoning(uint8_t periodSecs)
{
    if ((MICROTOSEC(esp_timer_get_time() - timePrevDr) >= periodSecs) &&
        xplrGnssIsDrEnabled(gnssDvcPrfId)) {
        espRet = xplrGnssGetImuAlignmentInfo(gnssDvcPrfId, &imuAlignmentInfo);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment info!");
        }

        espRet = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment data!");
        }

        espRet = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        }
        espRet = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Imu alignment status!");
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            espRet = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            }

            espRet = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
            }
        }

        timePrevDr = esp_timer_get_time();
    }
}
#endif

static void appDeInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
#endif
}

/**
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    appDeInitLog();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

