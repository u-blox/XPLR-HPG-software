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
 * An example for MQTT connection to Thingstream (U-blox broker) using a configuration file from the SD card.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * connects to Thingstream, using the credentials of the configuration file to achieve a connection to the MQTT broker
 * subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * sets up GNSS module using location_service component,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "xplr_wifi_starter.h"
#include "xplr_thingstream.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
#include "xplr_ztp.h"
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
#include "./../../../components/hpglib/src/common/xplr_common.h"

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA          0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED    1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED      0U /* used to log the debug messages to the sd card. Set to 1 for enabling*/
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
#define KIB                         (1024U)
#define APP_JSON_PAYLOAD_BUF_SIZE   ((5U) * (KIB))

/**
 * Period in seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD  5

/*
 * Button for shutting down device
 */
#define APP_DEVICE_OFF_MODE_BTN        (BOARD_IO_BTN1)

/*
 * Device off press duration in sec
 */
#define APP_DEVICE_OFF_MODE_TRIGGER    (3U)

#if 1 == APP_PRINT_IMU_DATA
/**
 * Period in seconds to print Dead Reckoning data
 */
#define APP_DEAD_RECKONING_PRINT_PERIOD 5
#endif

/**
 * GNSS I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42

/**
 * Thingstream subscription plan region
 * for correction data
*/
#define APP_REGION  XPLR_THINGSTREAM_PP_REGION_EU

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/**
 * GNSS configuration.
 */
static xplrGnssDeviceCfg_t dvcConfig;

/**
 * Gnss FSM state
 */
xplrGnssStates_t gnssState;

/**
 * Gnss device profile id
 */
const uint8_t gnssDvcPrfId = 0;

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
 * Name of the config file in the SD card
*/
/*INDENT-OFF*/
static char configFilename[128] = "device-ID-ucenter-config.json";      /**< Copy and paste your config file's name*/
/*INDENT-ON*/

static xplr_thingstream_t thingstreamSettings;

/*
 * Fill this struct with your desired settings and try to connect
 * The data are taken from KConfig or you can overwrite the
 * values as needed here.
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA,
    .webserver = false
};

/**
 * Providing MQTT client configuration
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;

/**
 * A struct where we can store our received MQTT message
 */
static char data[XPLRCELL_MQTT_MAX_SIZE_OF_TOPIC_PAYLOAD];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = ELEMENTCNT(data)
};

/**
 * Keeps time at "this" point of code execution.
 * Used to calculate elapse time.
 */
static uint64_t timePrevLoc;
#if 1 == APP_PRINT_IMU_DATA
static uint64_t timePrevDr;
#endif

/**
 * SD card instance
*/
static xplrSd_t *sdCard;

/**
 * Static log configuration struct and variables
 */
/*INDENT-OFF*/
#if (1 == APP_SD_LOGGING_ENABLED)
static xplrLog_t appLog, errorLog;
static char appBuff2Log[XPLRLOG_BUFFER_SIZE_LARGE];
static char appLogFilename[] = "/APPLOG.TXT";               /**< Follow the same format if changing the filename*/
static char errorLogFilename[] = "/ERRORLOG.TXT";           /**< Follow the same format if changing the filename*/
static uint8_t logFileMaxSize = 100;                        /**< Max file size (e.g. if the desired max size is 10MBytes this value should be 10U)*/
static xplrLog_size_t logFileMaxSizeType = XPLR_SIZE_MB;    /**< Max file size type (e.g. if the desired max size is 10MBytes this value should be XPLR_SIZE_MB)*/
#endif
/*INDENT-ON*/

/**
 * Static variables and flags
*/
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;
static bool requestDc;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static void appInitLog(void);
static esp_err_t appInitBoard(void);
static void appInitWiFi(void);
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCf);
static void appInitGnssDevice(void);
static esp_err_t appGetSdCredentials(void);
static void appMqttInit(void);
static void appPrintLocation(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appDeInitLog(void);
static void appHaltExecution(void);
static void appDeviceOffTask(void *arg);

void app_main(void)
{
    bool isNeededTopic, gotJson = false;
    esp_err_t espErr;

    appInitLog();
    appInitBoard();
    appInitWiFi();
    appInitGnssDevice();
    xplrMqttWifiInitState(&mqttClient);

    timePrevLoc = MICROTOSEC(esp_timer_get_time());
#if 1 == APP_PRINT_IMU_DATA
    timePrevDr = MICROTOSEC(esp_timer_get_time());
#endif

    while (1) {
        xplrGnssFsm(gnssDvcPrfId);
        gnssState = xplrGnssGetCurrentState(gnssDvcPrfId);

        switch (gnssState) {
            case XPLR_GNSS_STATE_DEVICE_READY:
                appPrintLocation(APP_LOCATION_PRINT_PERIOD);
#if 1 == APP_PRINT_IMU_DATA
                appPrintDeadReckoning(APP_DEAD_RECKONING_PRINT_PERIOD);
#endif
                break;

            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                appHaltExecution();
                break;

            default:
                break;
        }

        wifistarterErr = xplrWifiStarterFsm();
        /**
         * If we connect to WiFi we can continue with JSON parsing from the SD and
         * then with MQTT
         */
        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (!gotJson) {
                espErr = appGetSdCredentials();

                if (espErr != ESP_OK) {
                    APP_CONSOLE(E, "Credential Fetch from SD card failed");
                    requestDc = true;
                    appHaltExecution();
                } else {
                    /**
                     * Since MQTT is supported we can initialize the MQTT broker and try to connect
                     */
                    gotJson = true;
                    appMqttInit();
                    xplrMqttWifiStart(&mqttClient);
                    requestDc = false;
                }
            }
        }

        /**
         * This example uses the credentials
         * stored in the config file in the SD card
         * to get all required settings to connect
         * to Thingstream services such as PointPerfect.
         */
        mqttErr = xplrMqttWifiFsm(&mqttClient);

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                /**
                 * Lets try to use ZTP format topics to subscribe
                 * We subscribe after the GNSS device is ready. This way we do not
                 * lose the first message which contains the decryption keys
                 */
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    /**
                     * Lets try to use ZTP format topics to subscribe
                     */
                    espErr = xplrMqttWifiSubscribeToTopicArrayZtp(&mqttClient, &thingstreamSettings.pointPerfect);
                    if (espErr != ESP_OK) {
                        APP_CONSOLE(E, "xplrMqttWifiSubscribeToTopicArrayZtp failed");
                        appHaltExecution();
                    }
                }
                break;

            /**
             * If we are subscribed to a topic we can start sending messages to our
             * GNSS module: decryption keys and correction data
             */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                /**
                 * The following function will digest messages and store them in the internal buffer.
                 * If the user does not use it it will be discarded.
                 */
                if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                    /**
                     * Do not send data if GNSS is not in ready state.
                     * The device might not be initialized and the device handler
                     * would be NULL
                     */
                    if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                        isNeededTopic = xplrThingstreamPpMsgIsKeyDist(mqttMessage.topic, &thingstreamSettings);
                        if (isNeededTopic) {
                            espErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                            if (espErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                appHaltExecution();
                            }
                        }
                        /*INDENT-OFF*/
                        isNeededTopic = xplrThingstreamPpMsgIsCorrectionData(mqttMessage.topic, &thingstreamSettings);
                        /*INDENT-ON*/
                        if (isNeededTopic) {
                            espErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                            if (espErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send correction data!");
                            }
                        }
                    }
                }
                break;

            default:
                break;
        }

        /**
        * Print location every APP_LOCATION_PRINT_PERIOD secs
        */
        appPrintLocation(APP_LOCATION_PRINT_PERIOD);

        /**
         * We lost WiFi connection.
         * When we reconnect everything will be executed from the beginning:
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

/**
 * Initialize logging to the SD card
*/
static void appInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLog_error_t err = xplrLogInit(&errorLog, XPLR_LOG_DEVICE_ERROR, errorLogFilename,
                                      logFileMaxSize, logFileMaxSizeType);
    if (err == XPLR_LOG_OK) {
        errorLog.logEnable = true;
        err = xplrLogInit(&appLog, XPLR_LOG_DEVICE_INFO, appLogFilename, logFileMaxSize,
                          logFileMaxSizeType);
    }
    if (err == XPLR_LOG_OK) {
        appLog.logEnable = true;
    } else {
        appLog.logEnable = false;
        APP_CONSOLE(E, "Error initializing logging...");
    }
#endif
}

/*
 * Initialize XPLR-HPG kit using its board file
 */
static esp_err_t appInitBoard(void)
{
    gpio_config_t io_conf = {0};
    esp_err_t espRet;

    APP_CONSOLE(I, "Initializing board.");
    espRet = xplrBoardInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_DEVICE_OFF_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        espRet = gpio_config(&io_conf);
    }

    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        if (xTaskCreate(appDeviceOffTask, "deviceOffTask", 2 * 2048, NULL, 10, NULL) == pdPASS) {
            APP_CONSOLE(D, "Boot0 pin configured as button OK");
            APP_CONSOLE(D, "Board Initialized");
        } else {
            APP_CONSOLE(D, "Failed to start deviceOffTask task");
            APP_CONSOLE(E, "Board initialization failed!");
            espRet = ESP_FAIL;
        }
    }

    return espRet;
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
    gnssCfg->corrData.source = XPLR_GNSS_CORRECTION_FROM_IP;
}

/*
 * Trying to start a WiFi connection in station mode
 */
static void appInitWiFi(void)
{
    esp_err_t espRet;
    APP_CONSOLE(I, "Starting WiFi in station mode.");
    espRet = xplrWifiStarterInitConnection(&wifiOptions);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    } else {
        APP_CONSOLE(D, "Wifi station mode Initialized");
    }
}

/**
 * Makes all required initializations
 */
static void appInitGnssDevice(void)
{
    esp_err_t espRet;

    espRet = xplrGnssUbxlibInit();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    appConfigGnssSettings(&dvcConfig);

    espRet = xplrGnssStartDevice(0, &dvcConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Gets the credentials located in the config file of the SD card
*/
static esp_err_t appGetSdCredentials(void)
{
    esp_err_t ret;
    xplrSd_error_t err;
    xplr_thingstream_error_t tsErr;
    char payload[APP_JSON_PAYLOAD_BUF_SIZE];
    char fullFilepath[256];

    err = xplrSdInit(&sdCard);
    if (err == XPLR_SD_OK) {
        xplrSdFormatFilename(configFilename);
        sprintf(fullFilepath, "/sdcard/%s", configFilename);
        err = xplrSdReadFileString(fullFilepath, payload, APP_JSON_PAYLOAD_BUF_SIZE);
        if (err == XPLR_SD_OK) {
            tsErr = xplrThingstreamPpConfigFromFile(payload, APP_REGION, &thingstreamSettings);
            if (tsErr == XPLR_THINGSTREAM_OK) {
                APP_CONSOLE(I, "Successfully parsed configuration file");
                ret = ESP_OK;
            } else {
                ret = ESP_FAIL;
                APP_CONSOLE(E, "Error in parsing");
            }
        } else {
            APP_CONSOLE(E, "Error fetching payload from SD");
            ret = ESP_FAIL;
        }
    } else {
        APP_CONSOLE(E, "Error initializing SD");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * Populate some example settings
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
     * Settings for client
     * If Json parse is successful then all the following settings
     * will be populated
     */
    mqttClientConfig.uri = thingstreamSettings.pointPerfect.brokerAddress;
    mqttClientConfig.client_id = thingstreamSettings.pointPerfect.deviceId;
    mqttClientConfig.client_cert_pem = thingstreamSettings.pointPerfect.clientCert;
    mqttClientConfig.client_key_pem = thingstreamSettings.pointPerfect.clientKey;
    mqttClientConfig.cert_pem = thingstreamSettings.server.rootCa;

    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Let's start our client.
     * In case of multiple clients an array of clients can be used.
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/**
 * Declare a period in seconds to print location
 */
static void appPrintLocation(uint8_t periodSecs)
{
    esp_err_t espRet;

    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) && xplrGnssHasMessage(0)) {
        espRet = xplrGnssGetLocationData(0, &locData);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not get gnss location data!");
        } else {
            espRet = xplrGnssPrintLocationData(&locData);
            if (espRet != ESP_OK) {
                APP_CONSOLE(W, "Could not print gnss location data!");
            } else {
                //Do nothing
            }
        }

        espRet = xplrGnssPrintGmapsLocation(0);
        if (espRet != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        } else {
            //Do nothing
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
        } else {
            ret = xplrGnssPrintImuAlignmentInfo(&imuAlignmentInfo);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment data!");
            } else {
                //Do nothing
            }
        }

        ret = xplrGnssGetImuAlignmentStatus(gnssDvcPrfId, &imuFusionStatus);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not get Imu alignment status!");
        } else {
            ret = xplrGnssPrintImuAlignmentStatus(&imuFusionStatus);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not print Imu alignment status!");
            } else {
                //Do nothing
            }
        }

        if (xplrGnssIsDrCalibrated(gnssDvcPrfId)) {
            ret = xplrGnssGetImuVehicleDynamics(gnssDvcPrfId, &imuVehicleDynamics);
            if (ret != ESP_OK) {
                APP_CONSOLE(W, "Could not get Imu vehicle dynamic data!");
            } else {
                ret = xplrGnssPrintImuVehicleDynamics(&imuVehicleDynamics);
                if (ret != ESP_OK) {
                    APP_CONSOLE(W, "Could not print Imu vehicle dynamic data!");
                } else {
                    //Do nothing
                }
            }
        } else {
            //Do nothing
        }

        timePrevDr = MICROTOSEC(esp_timer_get_time());
    }
}
#endif

/**
 * DeInitialize logging to the SD card
*/
static void appDeInitLog(void)
{
#if (1 == APP_SD_LOGGING_ENABLED)
    xplrLogDeInit(&appLog);
    xplrLogDeInit(&errorLog);
#endif
}

/*
 * Function to halt app execution
 */
static void appHaltExecution(void)
{
    /* De-init log so that all the messages of log struct's internal buffer are flushed to the SD card*/
    APP_CONSOLE(E, "Halting execution");
    appDeInitLog();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appDeviceOffTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_DEVICE_OFF_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;

            if (btnPressDuration >= APP_DEVICE_OFF_MODE_TRIGGER) {
                APP_CONSOLE(W, "Device OFF triggered");
                xplrGnssHaltLogModule(XPLR_GNSS_LOG_MODULE_ALL);
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
                btnPressDuration = 0;
                appHaltExecution();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}