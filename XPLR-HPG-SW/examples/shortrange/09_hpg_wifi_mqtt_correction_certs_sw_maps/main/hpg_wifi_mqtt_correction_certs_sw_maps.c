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
 * An example for MQTT connection to Thingstream (U-blox broker) using certificates and correction data for the GNSS module.
 *
 * In the current example U-blox XPLR-HPG-1/XPLR-HPG-2 kit,
 * is setup using KConfig,
 * connects to WiFi network using wifi_starter component,
 * uses certificated downloaded from Thingstream to achieve a connection to the Thingstream MQTT broker
 * subscribes to PointPerfect correction data topic, as well as a decryption key topic, using hpg_mqtt component,
 * sets up GNSS module using location_service component,
 * sets up LBAND module (in our case the NEO-D9S module), if the Thingstream plan supports it,
 * and finally feeds the correction data to the GNSS module which displays the current location.
 */

/**
 * If paths not found in VScode:
 *      press keys --> <ctrl+shift+p>
 *      and select --> ESP-IDF: Add vscode configuration folder
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
#include "./../../../components/hpglib/src/bluetooth_service/xplr_bluetooth.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define APP_PRINT_IMU_DATA          0U /* Disables/Enables imu data printing*/
#define APP_SERIAL_DEBUG_ENABLED    1U /* used to print debug messages in console. Set to 0 for disabling */
#define APP_SD_LOGGING_ENABLED      0U /* used to log the debug messages to the sd card. Set to 1 for enabling
                                          When selected correction data source is LBAND, it is recommended to have SD logging disabled, due to memory constraints */
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#if (1 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_AND_PRINT, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (1 == APP_SERIAL_DEBUG_ENABLED && 0 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)  XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_PRINT_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif (0 == APP_SERIAL_DEBUG_ENABLED && 1 == APP_SD_LOGGING_ENABLED)
#define APP_CONSOLE(tag, message, ...)XPLRLOG(appLogCfg.appLogIndex, XPLR_LOG_SD_ONLY, APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

/*
 * Simple macros used to set buffer size in bytes
 */
#define KIB (1024U)
#define APP_MQTT_PAYLOAD_BUF_SIZE ((10U) * (KIB))

/**
 * Seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD  1

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
 * GNSS and LBAND I2C address in hex
 */
#define APP_GNSS_I2C_ADDR  0x42
#define APP_LBAND_I2C_ADDR 0x43

/**
 * Valid values
 * EU, US, KR, AU, JP
 * KR (no LBAND)
 * AU (no LBAND)
 * JP (no LBAND)
 */
#define APP_ORIGIN_COUNTRY "EU"

/**
 * Valid values
 * IP
 * IPLBAND
 * LBAND
 */
#define APP_CORRECTION_TYPE "IP"

#define APP_MAX_TOPICLEN 64

/**
 * Print bluetooth connected devices interval
 */
#define APP_DEVICES_PRINT_INTERVAL 10

/**
 * The size of the allocated Bluetooth buffer
 */
#define APP_BT_BUFFER_SIZE XPLRBLUETOOTH_MAX_MSG_SIZE * XPLRBLUETOOTH_NUMOF_DEVICES

/**
 * Time in seconds to trigger an inactivity timeout and cause a restart
 */
#define APP_INACTIVITY_TIMEOUT 30

/**
 * Trigger soft reset if device in error state
 */
#define APP_RESTART_ON_ERROR            (1U)

/*
 * Option to enable/disable the hot plug functionality for the SD card
 * It is recommended to disable the SD card Hot Plug functionality in this example, due to memory constraints.
 */
#define APP_SD_HOT_PLUG_FUNCTIONALITY   (0U) & APP_SD_LOGGING_ENABLED
/**
 * Option to enable the correction message watchdog mechanism.
 * When set to 1, if no correction data are forwarded to the GNSS
 * module (either via IP or SPARTN) for a defined amount of time
 * (MQTT_MESSAGE_TIMEOUT macro defined in hpglib/xplr_mqtt/include/xplr_mqtt.h)
 * an error event will be triggered
*/
#define APP_ENABLE_CORR_MSG_WDG (1U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef union appLog_Opt_type {
    struct {
        uint8_t appLog          : 1;
        uint8_t nvsLog          : 1;
        uint8_t mqttLog         : 1;
        uint8_t gnssLog         : 1;
        uint8_t gnssAsyncLog    : 1;
        uint8_t lbandLog        : 1;
        uint8_t locHelperLog    : 1;
        uint8_t wifistarterLog  : 1;
        uint8_t bluetoothLog    : 1;
    } singleLogOpts;
    uint16_t allLogOpts;
} appLog_Opt_t;

typedef struct appLog_type {
    appLog_Opt_t    logOptions;
    int8_t          appLogIndex;
    int8_t          nvsLogIndex;
    int8_t          mqttLogIndex;
    int8_t          gnssLogIndex;
    int8_t          gnssAsyncLogIndex;
    int8_t          lbandLogIndex;
    int8_t          locHelperLogIndex;
    int8_t          wifiStarterLogIndex;
    int8_t          bluetoothLogIndex;
} appLog_t;

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
 * Frequency read from LBAND module
 */
static uint32_t frequency;

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
 * Keeps time at "this" point of variable
 * assignment in code
 * Used for periodic printing
 */
static uint64_t timePrevLoc;
static uint64_t gnssLastAction;
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
static char appKeysTopic[APP_MAX_TOPICLEN];
static char appCorrectionDataTopic[APP_MAX_TOPICLEN];
static char appFrequencyTopic[APP_MAX_TOPICLEN];
static char *topicArray[] = {appKeysTopic, appCorrectionDataTopic, appFrequencyTopic};

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
static bool deviceOffRequested = false;
static bool isPlanLband = false;

/**
 * Error return types
 */
static esp_err_t espRet;
static xplrWifiStarterError_t wifistarterErr;
static xplrMqttWifiError_t mqttErr;

/**
 * Bluetooth client
 */
xplrBluetooth_client_t xplrBtClient;
SemaphoreHandle_t btSemaphore;
uint16_t timeNow;
char xplrBluetoothMessageBuffer[APP_BT_BUFFER_SIZE];
xplrBluetooth_error_t btError;
bool btIsInit = false;

/**
 * Static log configuration struct and variables
 */
static appLog_t appLogCfg = {
    .logOptions.allLogOpts = ~0, // All modules selected to log
    .appLogIndex = -1,
    .nvsLogIndex = -1,
    .mqttLogIndex = -1,
    .gnssLogIndex = -1,
    .gnssAsyncLogIndex = -1,
    .lbandLogIndex = -1,
    .locHelperLogIndex = -1,
    .wifiStarterLogIndex = -1,
    .bluetoothLogIndex = -1
};

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
TaskHandle_t cardDetectTaskHandler;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t appInitBoard(void);
#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void);
static void appDeInitLogging(void);
#endif
static void appInitWiFi(void);
static void appInitBt(void);
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg);
static void appConfigLbandSettings(xplrLbandDeviceCfg_t *lbandCfg);
static void appInitGnssDevice(void);
static void appInitLbandDevice(void);
static void appMqttInit(void);
static void appSendLocationToBt(uint8_t periodSecs);
#if 1 == APP_PRINT_IMU_DATA
static void appPrintDeadReckoning(uint8_t periodSecs);
#endif
static void appHaltExecution(void);
static void appTerminate(void);
static esp_err_t appConfigTopics(char **subTopics,
                                 const char *region,
                                 const char *corrType);
static void appDeviceOffTask(void *arg);
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appCardDetectTask(void *arg);
#endif

void app_main(void)
{
    xplrGnssError_t gnssErr;

#if (APP_SD_LOGGING_ENABLED == 1)
    espRet = appInitLogging();
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Logging failed to initialize");
    } else {
        APP_CONSOLE(I, "Logging initialized!");
    }
#endif
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
                gnssLastAction = esp_timer_get_time();
                if ((dvcLbandConfig.destHandler == NULL) && isPlanLband) {
                    dvcLbandConfig.destHandler = xplrGnssGetHandler(gnssDvcPrfId);
                    if (dvcLbandConfig.destHandler != NULL) {
                        espRet = xplrLbandSetDestGnssHandler(lbandDvcPrfId, dvcLbandConfig.destHandler);
                        if (espRet == ESP_OK) {
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
#if 1 == APP_PRINT_IMU_DATA
                appPrintDeadReckoning(APP_DEAD_RECKONING_PRINT_PERIOD);
#endif
                break;
            case XPLR_GNSS_STATE_DEVICE_RESTART:
                if ((dvcLbandConfig.destHandler != NULL) && isPlanLband) {
                    espRet = xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "Failed to get stop Lband Async sender!");
                        appHaltExecution();
                    } else {
                        APP_CONSOLE(D, "Successfully stoped Lband Async sender!");
                        dvcLbandConfig.destHandler = NULL;
                    }
                }
                break;
            case XPLR_GNSS_STATE_ERROR:
                APP_CONSOLE(E, "GNSS in error state");
                if (isPlanLband) {
                    xplrLbandSendCorrectionDataAsyncStop(lbandDvcPrfId);
                    dvcLbandConfig.destHandler = NULL;
                }
                appTerminate();
                break;

            default:
                if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                    appTerminate();
                }
                break;
        }

        wifistarterErr = xplrWifiStarterFsm();

        if (xplrWifiStarterGetCurrentFsmState() == XPLR_WIFISTARTER_STATE_CONNECT_OK) {
            if (xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_UNINIT ||
                xplrMqttWifiGetCurrentState(&mqttClient) == XPLR_MQTTWIFI_STATE_DISCONNECTED_OK) {
                if (appConfigTopics(topicArray, APP_ORIGIN_COUNTRY, APP_CORRECTION_TYPE) != ESP_OK) {
                    APP_CONSOLE(E, "appConfigTopics failed!");
                    appHaltExecution();
                }
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                requestDc = false;
            }
        }

        mqttErr = xplrMqttWifiFsm(&mqttClient);

        switch (xplrMqttWifiGetCurrentState(&mqttClient)) {
            /**
             * Subscribe to some topics
             * We subscribe after the GNSS device is ready. This way we do not
             * lose the first message which contains the decryption keys
             */
            case XPLR_MQTTWIFI_STATE_CONNECTED:
                if (gnssState == XPLR_GNSS_STATE_DEVICE_READY) {
                    gnssLastAction = esp_timer_get_time();
                    espRet = xplrMqttWifiSubscribeToTopicArray(&mqttClient,
                                                               topicArray,
                                                               ELEMENTCNT(topicArray),
                                                               XPLR_MQTTWIFI_QOS_LVL_0);
                    if (espRet != ESP_OK) {
                        APP_CONSOLE(E, "Subscribing to %s failed!", appCorrectionDataTopic);
                        appHaltExecution();
                    }
                } else {
                    if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                        appTerminate();
                    }
                }
                break;

            /**
             * If we are subscribed to a topic we can start sending messages to our
             * GNSS module: decryption keys and correction data
             */
            case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                if (!btIsInit) {
                    appInitBt();
                    btIsInit = true;
                }
                appSendLocationToBt(APP_LOCATION_PRINT_PERIOD);
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
                        gnssLastAction = esp_timer_get_time();
                        if (strcmp(mqttMessage.topic, topicArray[0]) == 0) {
                            espRet = xplrGnssSendDecryptionKeys(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                            if (espRet != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                appHaltExecution();
                            }
                        }
                        if (strcmp(mqttMessage.topic, topicArray[1]) == 0) {
                            if (!isPlanLband) {
                                espRet = xplrGnssSendCorrectionData(gnssDvcPrfId, mqttMessage.data, mqttMessage.dataLength);
                                if (espRet != ESP_OK) {
                                    APP_CONSOLE(E, "Failed to send correction data!");
                                }
                            } else {
                                // Correction data source is LBAND no need to send IP correction data
                            }
                        }
                        if (strcmp(mqttMessage.topic, topicArray[2]) == 0) {
                            if (isPlanLband) {
                                espRet = xplrLbandSetFrequencyFromMqtt(lbandDvcPrfId,
                                                                       mqttMessage.data,
                                                                       dvcLbandConfig.corrDataConf.region);
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
                    } else {
                        if (MICROTOSEC(esp_timer_get_time() - gnssLastAction) > APP_INACTIVITY_TIMEOUT) {
                            appTerminate();
                        }
                    }
                }
                break;
            case XPLR_MQTTWIFI_STATE_DISCONNECTED_OK:
                // We have a disconnect event (probably from the watchdog). Let's reconnect
                appMqttInit();
                xplrMqttWifiStart(&mqttClient);
                break;
            default:
                break;
        }

        /**
         * Check if any LBAND messages have been forwarded to the GNSS module
         * and if there are feed the MQTT module's watchdog.
        */
        if (xplrLbandHasFrwdMessage()) {
            xplrMqttWifiFeedWatchdog(&mqttClient);
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
                if (mqttClient.handler != NULL) {
                    xplrMqttWifiHardDisconnect(&mqttClient);
                }
                requestDc = true;
            }
        }

        if (deviceOffRequested) {
            xplrBluetoothDisconnectAllDevices();
            xplrBluetoothDeInit();
            xplrMqttWifiUnsubscribeFromTopicArray(&mqttClient, topicArray, ELEMENTCNT(topicArray));
            xplrMqttWifiHardDisconnect(&mqttClient);
            if (isPlanLband) {
                (void) xplrLbandStopDevice(lbandDvcPrfId);
            }
            xplrGnssStopAllAsyncs(gnssDvcPrfId);
            espRet = xplrGnssStopDevice(gnssDvcPrfId);
            timePrevLoc = esp_timer_get_time();
            do {
                gnssErr = xplrGnssFsm(gnssDvcPrfId);
                vTaskDelay(pdMS_TO_TICKS(10));
                if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
                    gnssErr == XPLR_GNSS_ERROR &&
                    espRet != ESP_OK) {
                    break;
                }
            } while (gnssErr != XPLR_GNSS_STOPPED);
#if (APP_SD_LOGGING_ENABLED == 1)
            appDeInitLogging();
#endif
            appHaltExecution();
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
#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    if (xTaskCreate(appCardDetectTask,
                    "hotPlugTask",
                    4 * 1024,
                    NULL,
                    20,
                    &cardDetectTaskHandler) == pdPASS) {
        APP_CONSOLE(D, "Hot plug for SD card OK");
    } else {
        APP_CONSOLE(W, "Hot plug for SD card failed");
    }
#endif

    return espRet;
}

#if (APP_SD_LOGGING_ENABLED == 1)
static esp_err_t appInitLogging(void)
{
    esp_err_t ret;
    xplrSd_error_t sdErr;

    /* Configure the SD card */
    sdErr = xplrSdConfigDefaults();
    if (sdErr != XPLR_SD_OK) {
        APP_CONSOLE(E, "Failed to configure the SD card");
        ret = ESP_FAIL;
    } else {
        /* Create the card detect task */
        sdErr = xplrSdStartCardDetectTask();
        /* A time window so that the card gets detected*/
        vTaskDelay(pdMS_TO_TICKS(50));
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Failed to start the card detect task");
            ret = ESP_FAIL;
        } else {
            /* Initialize the SD card */
            sdErr = xplrSdInit();
            if (sdErr != XPLR_SD_OK) {
                APP_CONSOLE(E, "Failed to initialize the SD card");
                ret = ESP_FAIL;
            } else {
                APP_CONSOLE(D, "SD card initialized");
                ret = ESP_OK;
            }
        }
    }

    if (ret == ESP_OK) {
        /* Start logging for each module (if selected in configuration) */
        if (appLogCfg.logOptions.singleLogOpts.appLog == 1) {
            appLogCfg.appLogIndex = xplrLogInit(XPLR_LOG_DEVICE_INFO,
                                                "main_app.log",
                                                XPLRLOG_FILE_SIZE_INTERVAL,
                                                XPLRLOG_NEW_FILE_ON_BOOT);
            if (appLogCfg.appLogIndex >= 0) {
                APP_CONSOLE(D, "Application logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.nvsLog == 1) {
            appLogCfg.nvsLogIndex = xplrNvsInitLogModule(NULL);
            if (appLogCfg.nvsLogIndex >= 0) {
                APP_CONSOLE(D, "NVS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.mqttLog == 1) {
            appLogCfg.mqttLogIndex = xplrMqttWifiInitLogModule(NULL);
            if (appLogCfg.mqttLogIndex >= 0) {
                APP_CONSOLE(D, "MQTT WiFi logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssLog == 1) {
            appLogCfg.gnssLogIndex = xplrGnssInitLogModule(NULL);
            if (appLogCfg.gnssLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.gnssAsyncLog == 1) {
            appLogCfg.gnssAsyncLogIndex = xplrGnssAsyncLogInit(NULL);
            if (appLogCfg.gnssAsyncLogIndex >= 0) {
                APP_CONSOLE(D, "GNSS Async logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.lbandLog == 1) {
            appLogCfg.lbandLogIndex = xplrLbandInitLogModule(NULL);
            if (appLogCfg.lbandLogIndex >= 0) {
                APP_CONSOLE(D, "LBAND service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.locHelperLog == 1) {
            appLogCfg.locHelperLogIndex = xplrHlprLocSrvcInitLogModule(NULL);
            if (appLogCfg.locHelperLogIndex >= 0) {
                APP_CONSOLE(D, "Location Helper Service logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.wifistarterLog == 1) {
            appLogCfg.wifiStarterLogIndex = xplrWifiStarterInitLogModule(NULL);
            if (appLogCfg.wifiStarterLogIndex >= 0) {
                APP_CONSOLE(D, "WiFi Starter logging instance initialized");
            }
        }
        if (appLogCfg.logOptions.singleLogOpts.bluetoothLog == 1) {
            appLogCfg.bluetoothLogIndex = xplrBluetoothInitLogModule(NULL);
            if (appLogCfg.bluetoothLogIndex >= 0) {
                APP_CONSOLE(D, "Bluetooth service logging instance initialized");
            }
        }
    }

    return ret;
}

static void appDeInitLogging(void)
{
    xplrLog_error_t logErr;
    xplrSd_error_t sdErr = XPLR_SD_ERROR;
    esp_err_t espErr;

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
    vTaskDelete(cardDetectTaskHandler);
#endif
    logErr = xplrLogDisableAll();
    if (logErr != XPLR_LOG_OK) {
        APP_CONSOLE(E, "Error disabling logging");
    } else {
        logErr = xplrLogDeInitAll();
        if (logErr != XPLR_LOG_OK) {
            APP_CONSOLE(E, "Error de-initializing logging");
        } else {
            espErr = xplrGnssAsyncLogDeInit();
            if (espErr != XPLR_LOG_OK) {
                APP_CONSOLE(E, "Error de-initializing async logging");
                logErr = XPLR_LOG_ERROR;
            } else {
                //Do nothing
            }
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdStopCardDetectTask();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error stopping the the SD card detect task");
        }
    }

    if (logErr == XPLR_LOG_OK) {
        sdErr = xplrSdDeInit();
        if (sdErr != XPLR_SD_OK) {
            APP_CONSOLE(E, "Error de-initializing the SD card");
        }
    } else {
        //Do nothing
    }

    if (logErr == XPLR_LOG_OK && sdErr == XPLR_SD_OK) {
        APP_CONSOLE(I, "Logging service de-initialized successfully");
    }
}
#endif

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

void appInitBt(void)
{
    btSemaphore = xSemaphoreCreateMutex();
    strcpy(xplrBtClient.configuration.deviceName, CONFIG_XPLR_BLUETOOTH_DEVICE_NAME);
    xplrBluetoothInit(&xplrBtClient,
                      btSemaphore,
                      xplrBluetoothMessageBuffer,
                      APP_BT_BUFFER_SIZE);
}

/**
 * Populates gnss settings
 */
static void appConfigGnssSettings(xplrGnssDeviceCfg_t *gnssCfg)
{
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
    gnssCfg->corrData.source = (xplrGnssCorrDataSrc_t)CONFIG_XPLR_CORRECTION_DATA_SOURCE;
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
    * module connected via I2C
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
}

/**
 * Makes all required initializations
 */
static void appInitGnssDevice(void)
{
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

    APP_CONSOLE(I, "Successfully initialized all GNSS related devices/functions!");
}

/**
 * Function that initializes the LBAND device
*/
static void appInitLbandDevice(void)
{
    esp_err_t espRet;

    APP_CONSOLE(D, "Waiting for LBAND device to come online!");
    appConfigLbandSettings(&dvcLbandConfig);
    espRet = xplrLbandStartDevice(lbandDvcPrfId, &dvcLbandConfig);
    if (espRet != ESP_OK) {
        APP_CONSOLE(E, "Lband device config failed!");
        appHaltExecution();
    } else {
        espRet = xplrLbandPrintDeviceInfo(lbandDvcPrfId);
        if (espRet != ESP_OK) {
            APP_CONSOLE(E, "Failed to print LBAND device info!");
            appHaltExecution();
        }
    }
}

/**
 * Populate MQTT Wi-Fi client settings
 */
static void appMqttInit(void)
{
    esp_err_t ret;

    mqttClient.ucd.enableWatchdog = (bool) APP_ENABLE_CORR_MSG_WDG;

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
     * Settings for mqtt client
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
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to initialize Mqtt client!");
        appHaltExecution();
    }
}

/**
 * Prints location in console and sends to SW maps via Bt
 */
static void appSendLocationToBt(uint8_t periodSecs)
{
    char GgaMsg[256];
    char *GgaMsgPtr = GgaMsg;
    char **GgaMsgPtrPtr = &GgaMsgPtr;
    int32_t len;


    if ((MICROTOSEC(esp_timer_get_time()) - timePrevLoc >= periodSecs) &&
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

        switch (xplrBluetoothGetState()) {
            case XPLR_BLUETOOTH_CONN_STATE_CONNECTED:
                len = xplrGnssGetGgaMessage(0, GgaMsgPtrPtr, ELEMENTCNT(GgaMsg));
                btError = xplrBluetoothWrite(&xplrBtClient.devices[0], GgaMsg, len);
                if (btError != XPLR_BLUETOOTH_OK) {
                    APP_CONSOLE(W, "Couldn't send location to Bluetooth device with handle -> [%d]",
                                xplrBtClient.devices[0].handle);
                } else {
                    APP_CONSOLE(I, "Sent location successfully to Bluetooth device with handle -> [%d]",
                                xplrBtClient.devices[0].handle);
                }
                break;
            case XPLR_BLUETOOTH_CONN_STATE_READY:
                APP_CONSOLE(D, "No bluetooth device connected");
                break;
            default:
                break;
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
    if ((MICROTOSEC(esp_timer_get_time()) - timePrevDr >= periodSecs) &&
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

        timePrevDr = MICROTOSEC(esp_timer_get_time());
    }
}
#endif

/*
 * Function to halt app execution
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appTerminate(void)
{
    xplrGnssError_t gnssErr;
    esp_err_t espErr;

    APP_CONSOLE(E, "Unrecoverable error in application. Terminating and restarting...");

    (void) xplrMqttWifiUnsubscribeFromTopicArray(&mqttClient, topicArray, ELEMENTCNT(topicArray));
    xplrMqttWifiHardDisconnect(&mqttClient);
    (void) xplrLbandStopDevice(lbandDvcPrfId);
    espErr = xplrGnssStopDevice(gnssDvcPrfId);
    timePrevLoc = esp_timer_get_time();
    do {
        gnssErr = xplrGnssFsm(gnssDvcPrfId);
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((MICROTOSEC(esp_timer_get_time() - timePrevLoc) <= APP_INACTIVITY_TIMEOUT) &&
            gnssErr == XPLR_GNSS_ERROR &&
            espErr != ESP_OK) {
            break;
        }
    } while (gnssErr != XPLR_GNSS_STOPPED);
#if (APP_SD_LOGGING_ENABLED == 1)
    appDeInitLogging();
#endif
#if (APP_RESTART_ON_ERROR == 1)
    esp_restart();
#else
    appHaltExecution();
#endif
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
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrBoardSetPower(XPLR_PERIPHERAL_LTE_ID, false);
                btnPressDuration = 0;
                deviceOffRequested = true;
                appHaltExecution();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}

#if (APP_SD_HOT_PLUG_FUNCTIONALITY == 1)
static void appCardDetectTask(void *arg)
{
    bool prvState = xplrSdIsCardOn();
    bool currState;
    esp_err_t espErr;

    for (;;) {
        currState = xplrSdIsCardOn();

        /* Check if state has changed */
        if (currState ^ prvState) {
            if (currState) {
                if (!xplrSdIsCardInit()) {
                    espErr = appInitLogging();
                    if (espErr == ESP_OK) {
                        APP_CONSOLE(I, "Logging is enabled!");
                    } else {
                        APP_CONSOLE(E, "Failed to enable logging");
                    }
                }
                /* Enable all log instances (the ones enabled during configuration) */
                if (xplrLogEnableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is re-enabled!");
                } else {
                    APP_CONSOLE(E, "Failed to re-enable logging");
                }
            } else {
                if (xplrSdIsCardInit()) {
                    xplrSdDeInit();
                }
                if (xplrLogDisableAll() == XPLR_LOG_OK) {
                    APP_CONSOLE(I, "Logging is disabled!");
                } else {
                    APP_CONSOLE(E, "Failed to disable logging");
                }
            }
        } else {
            // Do nothing
        }
        prvState = currState;
        /* Window for other tasks to run */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif

static esp_err_t appConfigTopics(char **subTopics,
                                 const char *region,
                                 const char *corrType)
{
    esp_err_t ret = ESP_OK;

    memset(subTopics[0], 0x00, APP_MAX_TOPICLEN);
    memset(subTopics[1], 0x00, APP_MAX_TOPICLEN);
    memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);

    if (strcmp(corrType, "IP") == 0) {
        isPlanLband = false;
        strcpy(subTopics[0], "/pp/ubx/0236/ip");
        strcpy(subTopics[1], "/pp/ip/");
    } else if ((strcmp(corrType, "IPLBAND") == 0)) {
        isPlanLband = (bool)CONFIG_XPLR_CORRECTION_DATA_SOURCE;
        strcpy(subTopics[0], "/pp/ubx/0236/Lb");
        strcpy(subTopics[1], "/pp/Lb/");
        strcpy(subTopics[2], "/pp/frequencies/Lb");
    } else if ((strcmp(corrType, "LBAND") == 0)) {
        isPlanLband = (bool)CONFIG_XPLR_CORRECTION_DATA_SOURCE;
        strcpy(subTopics[0], "/pp/ubx/0236/Lb");
        strcpy(subTopics[2], "/pp/frequencies/Lb");
    } else {
        APP_CONSOLE(E, "Invalid Thingstream plan!");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        if (strcmp(region, "EU") == 0) {
            if ((strcmp(corrType, "LBAND") == 0)) {
                // Plan is LBAND do nothing
            } else {
                strcat(subTopics[1], "eu");
            }
            if (isPlanLband) {
                dvcLbandConfig.corrDataConf.region = XPLR_LBAND_FREQUENCY_EU;
            }
        } else if (strcmp(region, "US") == 0) {
            if ((strcmp(corrType, "LBAND") == 0)) {
                // Plan is LBAND do nothing
            } else {
                strcat(subTopics[1], "us");
            }
            if (isPlanLband) {
                dvcLbandConfig.corrDataConf.region = XPLR_LBAND_FREQUENCY_US;
            }
        } else if (strcmp(region, "KR") == 0) {
            if (strcmp(corrType, "IPLBAND") == 0) {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                APP_CONSOLE(E, "IP+LBAND plan is not supported in Korea region");
                ret = ESP_FAIL;
            } else if ((strcmp(corrType, "LBAND") == 0)) {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                APP_CONSOLE(E, "LBAND plan is not supported in Korea region");
                dvcLbandConfig.corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
                ret = ESP_FAIL;
            } else {
                strcat(subTopics[1], "kr");
            }
        } else if (strcmp(region, "AU") == 0) {
            if ((strcmp(corrType, "LBAND") == 0)) {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                APP_CONSOLE(E, "LBAND plan is not supported in Australia region");
                dvcLbandConfig.corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
                ret = ESP_FAIL;
            } else {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                strcat(subTopics[1], "au");
            }
        } else if (strcmp(region, "JP") == 0) {
            if ((strcmp(corrType, "LBAND") == 0)) {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                APP_CONSOLE(E, "LBAND plan is not supported in Japan region");
                dvcLbandConfig.corrDataConf.region = XPLR_LBAND_FREQUENCY_INVALID;
                ret = ESP_FAIL;
            } else {
                isPlanLband = false;
                memset(subTopics[2], 0x00, APP_MAX_TOPICLEN);
                strcat(subTopics[1], "jp");
            }
        } else {
            APP_CONSOLE(E, "Invalid region!");
            ret = ESP_FAIL;
        }
    }

    if ((dvcLbandConfig.destHandler == NULL) && isPlanLband) {
        appInitLbandDevice();
    }

    return ret;
}