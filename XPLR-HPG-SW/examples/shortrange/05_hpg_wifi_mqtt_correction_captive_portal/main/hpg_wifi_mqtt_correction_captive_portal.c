/*
 * Copyright 2022 u-blox Ltd
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

/*Captive portal with live map tracking implementation.
 *
 * Current example demonstrates a captive portal implementation to configure U-blox XPLR-HPG-1/2 kits
 * with Wi-Fi and Thingstream credentials in order to acquire correction data from Thingstream's Point Perfect
 * location service, over Wi-Fi.
 *
 * The web interface also provides a map tracking application to visualize kit's position in real time.
 * To configure required credentials, the device boots in access point mode providing to the user an easy
 * configuration interface. As soon as the device is configured, credentials are stored in MCU's non-volatile memory
 * and switches to station mode for connecting to provisioned Wi-Fi.
 * If for any reason the kit fails to connect to a Wi-Fi network using the provisioned credentials then it switches back to
 * access point mode thus, allowing the user to reconfigure it. The device tries for at least 5 minutes to connect to a Wi-Fi network
 * before switching to access point again.
 *
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "xplr_wifi_starter.h"
#include "xplr_mqtt.h"
#include "mqtt_client.h"
#include "xplr_common.h"
#include "./../../../components/ubxlib/ubxlib.h"
#include "u_cfg_app_platform_specific.h"
#include "./../../../../components/hpglib/xplr_hpglib_cfg.h"
#include "./../../../components/hpglib/src/location_service/gnss_service/xplr_gnss.h"
#include "./../../../components/hpglib/src/common/xplr_common.h"
#if defined(XPLR_BOARD_SELECTED_IS_C214)
#include "./../../../../../components/boards/xplr-hpg2-c214/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_C213)
#include "./../../../../../components/boards/xplr-hpg1-c213/board.h"
#elif defined(XPLR_BOARD_SELECTED_IS_MAZGCH)
#include "./../../../../../components/boards/mazgch-hpg-solution/board.h"
#else
#error "No board selected in xplr_hpglib_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define  APP_SERIAL_DEBUG_ENABLED 1U /* used to print debug messages in console. Set to 0 for disabling */
#if defined (APP_SERIAL_DEBUG_ENABLED)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [(%u) %s|%s|%ld|: " format LOG_RESET_COLOR "\n"
#define APP_CONSOLE(tag, message, ...)   esp_rom_printf(APP_LOG_FORMAT(tag, message), esp_log_timestamp(), "app", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define APP_CONSOLE(message, ...) do{} while(0)
#endif

#define MAJOR_APP_VER 0
#define MINOR_APP_VER 1
#define INTERNAL_APP_VER 0


/*
 * Simple macros used to set buffer size in bytes
 */
#define KIB                         (1024U)
#define APP_MQTT_PAYLOAD_BUF_SIZE   ((10U) * (KIB))

/*
 * Seconds to print location
 */
#define APP_LOCATION_PRINT_PERIOD   (5U)

/*
 * Seconds to update location in webserver
 */
#define APP_LOCATION_UPDATE_PERIOD  (1U)

/*
 * Max topic count
 */
#define APP_MAX_TOPIC_CNT           (2U)

/*
 * GNSS I2C address in hex
 */
#define XPLR_GNSS_I2C_ADDR          (0x42)

/*
 * Button for factory reset
 */
#define APP_FACTORY_MODE_BTN        (BOARD_IO_BTN1)

/*
 * Factory reset button press duration in sec
 */
#define APP_FACTORY_MODE_TRIGGER    (5U)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char mqttHost[] = "mqtts://pp.services.u-blox.com";
static const char mqttTopicKeys[] = "/pp/ubx/0236/Lb";
static const char mqttTopicCorrectionDataEu[] = "/pp/Lb/eu";
static const char mqttTopicCorrectionDataUs[] = "/pp/Lb/us";

/*
 * Fill this struct with your desired settings and try to connect
 * This is used only if you wish to override the setting from KConfig
 * and use the function with custom options
 */
static const char wifiSsid[] = CONFIG_XPLR_WIFI_SSID;
static const char wifiPassword[] = CONFIG_XPLR_WIFI_PASSWORD;
static xplrWifiStarterOpts_t wifiOptions = {
    .ssid = wifiSsid,
    .password = wifiPassword,
    .mode = XPLR_WIFISTARTER_MODE_STA_AP,
    .webserver = true
};

/*
 * A struct where we can store our received MQTT message
 */
static char data[APP_MQTT_PAYLOAD_BUF_SIZE];
static char topic[XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN];
xplrMqttWifiPayload_t mqttMessage = {
    .data = data,
    .topic = topic,
    .dataLength = 0,
    .maxDataLength = APP_MQTT_PAYLOAD_BUF_SIZE
};

/*
 * MQTT client config and handler
 */
static esp_mqtt_client_config_t mqttClientConfig;
static xplrMqttWifiClient_t mqttClient;

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static esp_err_t appInitBoard(void);
static esp_err_t appInitWiFi(void);
static esp_err_t appInitGnssDevice(void);
static void appMqttInit(void);
static esp_err_t appPrintLocation(uint8_t periodSecs);
static esp_err_t appUpdateServerLocation(uint8_t periodSecs);
static char *appTimeFromBoot(void);
static char *appTimeToFix();
static void appVersionUpdate();
static void appHaltExecution(void);
static void appFactoryResetTask(void *arg);

void app_main(void)
{
    bool coldStart = true;
    xplrWifiStarterFsmStates_t wifiState;

    bool isMqttInitialized = false;
    xplrMqttWifiClientStates_t  mqttState;
    esp_err_t mqttRes[APP_MAX_TOPIC_CNT];
    esp_err_t brokerErr = ESP_FAIL;
    char *region = NULL;

    esp_err_t gnssErr;

    int8_t tVal;
    uint32_t mqttStats[1][2] = { {0, 0} }; //num of msgs and total bytes received
    char mqttStatsStr[64];

    appInitBoard();
    appInitWiFi();
    appInitGnssDevice();
    xplrGnssPrintDeviceInfo(0);
    xplrMqttWifiInitState(&mqttClient);

    if(coldStart) {
        appVersionUpdate(); //update fw version shown in webserver
        coldStart = false;
    }

    while (1) {
        xplrWifiStarterFsm();
        wifiState = xplrWifiStarterGetCurrentFsmState();

        if ((wifiState == XPLR_WIFISTARTER_STATE_CONNECT_OK) && (!isMqttInitialized)) {
            /*
            * We have connected to user's AP thus all config data should be available.
            * Lets configure the MQTT client to communicate with thingstream's point perfect service.
            */
            appMqttInit();
            xplrMqttWifiStart(&mqttClient);
            isMqttInitialized = true;
        }

        if ((wifiState == XPLR_WIFISTARTER_STATE_CONNECT_OK) && (isMqttInitialized)) {
            /*
             * Module has access to the web and mqtt client is ready/initialized.
             * Lets connect to the Thingstream's point perfect broker and subscribe to
             * correction topics according to our region.
             */

            xplrMqttWifiFsm(&mqttClient);
            mqttState = xplrMqttWifiGetCurrentState(&mqttClient);

            switch (mqttState) {
                case XPLR_MQTTWIFI_STATE_CONNECTED:
                    /* ok, we are connected to the broker. Lets subscribe to correction topics */
                    region = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_CLIENTREGION);
                    mqttRes[0] = xplrMqttWifiSubscribeToTopic(&mqttClient,
                                                              (char *)mqttTopicKeys,
                                                              XPLR_MQTTWIFI_QOS_LVL_0);
                    if (memcmp(region, "EU", strlen("EU")) == 0) {
                        mqttRes[1] = xplrMqttWifiSubscribeToTopic(&mqttClient,
                                                                  (char *)mqttTopicCorrectionDataEu,
                                                                  XPLR_MQTTWIFI_QOS_LVL_0);
                    } else if (memcmp(region, "US", strlen("US")) == 0) {
                        mqttRes[1] = xplrMqttWifiSubscribeToTopic(&mqttClient,
                                                                  (char *)mqttTopicCorrectionDataUs,
                                                                  XPLR_MQTTWIFI_QOS_LVL_0);
                    }

                    for (int i = 0; i < APP_MAX_TOPIC_CNT; i++) {
                        if (mqttRes[i] != ESP_OK) {
                            brokerErr = ESP_FAIL;
                            break;
                        }
                        brokerErr = ESP_OK;
                    }

                    if (brokerErr != ESP_OK) {
                        APP_CONSOLE(E, "Failed to subscribe to required topics. Correction data will not be available.");
                    } else {
                        tVal = 1;
                        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
                        APP_CONSOLE(D, "Subscribed to required topics successfully.");
                    }
                    break;
                case XPLR_MQTTWIFI_STATE_SUBSCRIBED:
                    /* check if we have a msg in any of the topics subscribed */
                    if (xplrMqttWifiReceiveItem(&mqttClient, &mqttMessage) == XPLR_MQTTWIFI_ITEM_OK) {
                        /* we have a message available. check origin of topic and fwd it to gnss */
                        mqttStats[0][0]++;
                        mqttStats[0][1] += mqttMessage.dataLength;
                        snprintf(mqttStatsStr, 64, "Messages: %u (%u bytes)", mqttStats[0][0], mqttStats[0][1]);
                        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_MQTTSTATS,
                                                               (void *)mqttStatsStr);
                        if (strcmp(mqttMessage.topic, mqttTopicKeys) == 0) {
                            gnssErr = xplrGnssSendDecryptionKeys(0, mqttMessage.data, mqttMessage.dataLength);
                            if (gnssErr != ESP_OK) {
                                APP_CONSOLE(E, "Failed to send decryption keys!");
                                appHaltExecution();
                            }
                        } else { //should be correction data
                            if (strcmp(region, "EU") == 0) {
                                if (strcmp(mqttMessage.topic, mqttTopicCorrectionDataEu) == 0) {
                                    gnssErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                                    if (gnssErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to send correction data!");
                                    }
                                }
                            } else if (strcmp(region, "US") == 0) {
                                if (strcmp(mqttMessage.topic, mqttTopicCorrectionDataUs) == 0) {
                                    gnssErr = xplrGnssSendCorrectionData(0, mqttMessage.data, mqttMessage.dataLength);
                                    if (gnssErr != ESP_OK) {
                                        APP_CONSOLE(E, "Failed to send correction data!");
                                    }
                                }
                            } else {
                                APP_CONSOLE(E, "Region selected not supported...");
                            }
                        }
                    }
                    break;

                default:
                    tVal = 0;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_CONFIGURED, (void *)&tVal);
                    break;
            }
        }

        /* Print (to console) and update (webserver) location data */
        appPrintLocation(APP_LOCATION_PRINT_PERIOD);
        appUpdateServerLocation(APP_LOCATION_UPDATE_PERIOD);
        appTimeFromBoot();
        appTimeToFix();

        /*
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
    esp_err_t ret;

    APP_CONSOLE(I, "Initializing board.");
    ret = xplrBoardInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Board initialization failed!");
        appHaltExecution();
    } else {
        /* config boot0 pin as input */
        io_conf.pin_bit_mask = 1ULL << APP_FACTORY_MODE_BTN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;
        ret = gpio_config(&io_conf);
    }

    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set boot0 pin in input mode");
    } else {
        xTaskCreate(appFactoryResetTask, "factoryResetTask", 2048, NULL, 10, NULL);
        APP_CONSOLE(D, "Boot0 pin configured as button OK");
    }
    return ret;
}

/*
 * Initialize WiFi
 */
static esp_err_t appInitWiFi(void)
{
    esp_err_t ret;

    APP_CONSOLE(I, "Starting WiFi in station mode.");
    ret = xplrWifiStarterInitConnection(&wifiOptions);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "WiFi station mode initialization failed!");
        appHaltExecution();
    }

    return ret;
}

/*
 * Initialize GNSS
 */
static esp_err_t appInitGnssDevice(void)
{
    esp_err_t ret;
    ret = xplrGnssUbxlibInit();
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "UbxLib init failed!");
        appHaltExecution();
    }

    APP_CONSOLE(I, "Waiting for devices to come online!");

    ret = xplrGnssStartDeviceDefaultSettings(0, XPLR_GNSS_I2C_ADDR);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to start GNSS device!");
        appHaltExecution();
    }

    ret = xplrGnssSetCorrectionDataSource(0, XPLR_GNSS_CORRECTION_FROM_IP);
    if (ret != ESP_OK) {
        APP_CONSOLE(E, "Failed to set correction data source!");
        appHaltExecution();
    }

    APP_CONSOLE(D, "GNSS init OK.");

    return ret;
}

/*
 * Initialize MQTT client
 */
static void appMqttInit(void)
{
    /**
     * We declare how many slots a ring buffer should have.
     * You can choose to have more depending on the traffic of your broker.
     * If the ring buffer cannot keep up then you can increase this number
     * to better suit your case.
     */
    mqttClient.ucd.ringBufferSlotsNumber = 3;

    /**
     * Settings for mqtt client
     */
    mqttClientConfig.uri = mqttHost;
    mqttClientConfig.client_id = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_CLIENTID);
    mqttClientConfig.client_cert_pem = xplrWifiStarterWebserverDataGet(
                                           XPLR_WIFISTARTER_SERVERDATA_CLIENTCERT);
    mqttClientConfig.client_key_pem = xplrWifiStarterWebserverDataGet(
                                          XPLR_WIFISTARTER_SERVERDATA_CLIENTKEY);
    mqttClientConfig.cert_pem = xplrWifiStarterWebserverDataGet(XPLR_WIFISTARTER_SERVERDATA_ROOTCA);
    mqttClientConfig.user_context = &mqttClient.ucd;

    /**
     * Initialize mqtt client
     */
    xplrMqttWifiInitClient(&mqttClient, &mqttClientConfig);
}

/*
 * Periodic console print location (in seconds)
 */
static esp_err_t appPrintLocation(uint8_t periodSecs)
{
    static uint64_t prevTime = 0;
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs) && xplrGnssHasMessage(0)) {
        ret = xplrGnssPrintLocation(0);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print gnss location!");
        }

        ret = xplrGnssPrintGmapsLocation(0);
        if (ret != ESP_OK) {
            APP_CONSOLE(W, "Could not print Gmaps location!");
        }

        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        ret = ESP_ERR_NOT_FINISHED;
    }

    return ret;
}

/*
 * Periodic webserver update function with location data (in seconds)
 */
static esp_err_t appUpdateServerLocation(uint8_t periodSecs)
{
    xplrGnssLocation_t gnssInfo;
    char gMapStr[256] = {0};
    char timestamp[32] = {0};
    cJSON *jDoc;
    char *jBuff = NULL;
    int8_t i8Val;

    static uint64_t prevTime = 0;
    esp_err_t err[2];
    esp_err_t ret;

    if ((MICROTOSEC(esp_timer_get_time()) - prevTime >= periodSecs) && xplrGnssHasMessage(0)) {
        err[0] = xplrGnssGetLocation(0, &gnssInfo);
        if (err[0] != ESP_OK) {
            APP_CONSOLE(E, "Could not get gnss location");
        } else {
            err[1] = xplrGnssGetGmapsLocation(0, gMapStr, 256);
            if (err[1] != ESP_OK) {
                APP_CONSOLE(E, "Could not build Gmap string");
            }
        }

        for (int i = 0; i < 2; i++) {
            if (err[i] != ESP_OK) {
                ret = ESP_FAIL;
                break;
            }
            ret = ESP_OK;
        }

        if (ret != ESP_FAIL) {
            timestampToTime(gnssInfo.location.timeUtc,
                            timestamp,
                            32);

            jDoc = cJSON_CreateObject();
            if (jDoc != NULL) {
                cJSON_AddStringToObject(jDoc, "rsp", "dvcLocation");
                cJSON_AddNumberToObject(jDoc, "lat", (double)gnssInfo.location.latitudeX1e7 * (1e-7));
                cJSON_AddNumberToObject(jDoc, "lon", (double)gnssInfo.location.longitudeX1e7 * (1e-7));
                cJSON_AddNumberToObject(jDoc, "alt", (double)gnssInfo.location.altitudeMillimetres * (1e-3));
                cJSON_AddNumberToObject(jDoc, "speed", (double)gnssInfo.location.speedMillimetresPerSecond);
                cJSON_AddNumberToObject(jDoc, "accuracy", (double)gnssInfo.accuracy.horizontal * (1e-4));
                cJSON_AddNumberToObject(jDoc, "type", (double)gnssInfo.locFixType);
                cJSON_AddStringToObject(jDoc, "timestamp", timestamp);
                cJSON_AddStringToObject(jDoc, "gMap", gMapStr);

                jBuff = cJSON_Print(jDoc);
                if (jBuff != NULL) {
                    xplrWifiStarterWebserverLocationSet(jBuff);
                    i8Val = (int8_t)gnssInfo.locFixType;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_READY,
                                                           (void *)&i8Val);

                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_GNSS_ACCURACY,
                                                           (void *)&gnssInfo.accuracy.horizontal);
                    cJSON_free(jBuff);
                    jBuff = NULL;
                } else {
                    ret = ESP_FAIL;
                    APP_CONSOLE(E, "Failed to create json buffer");
                }
                cJSON_Delete(jDoc);
            } else {
                ret = ESP_FAIL;
                APP_CONSOLE(E, "Failed to create json doc");
            }
        }

        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        ret = ESP_ERR_NOT_FINISHED;
    }

    return ret;
}

static char *appTimeFromBoot(void)
{
    static char uptime[32];
    static uint32_t prevTime = 0;
    int sec;
    int min;
    int hour;
    uint64_t timeNow = MICROTOSEC(esp_timer_get_time());
    char *ret;

    if (timeNow - prevTime >= 1) {
        sec = timeNow % 60;
        min = (timeNow / 60) % 60;
        hour = (timeNow / 60) / 60;

        memset(uptime, 0x00, 32);
        snprintf(uptime, 32, "%d:%02d:%02d", hour, min, sec);

        prevTime = MICROTOSEC(esp_timer_get_time());

        xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_UPTIME,
                                               (void *)&uptime);

        ret = uptime;
    } else {
        ret = NULL;
    }

    return ret;
}

static char *appTimeToFix()
{
    static char fixTime[32];
    static bool coldBoot = true;
    static bool gotFix = false;
    static uint64_t bootTime = 0;
    static uint32_t prevTime = 0;
    xplrGnssLocation_t gnssInfo;
    uint32_t timeToFix;
    int8_t i8Val;
    esp_err_t err;
    int sec;
    int min;
    int hour;
    uint64_t timeNow = MICROTOSEC(esp_timer_get_time());
    char *ret;

    if (coldBoot) {
        bootTime = MICROTOSEC(esp_timer_get_time());
        coldBoot = false;
    }

    if (timeNow - prevTime >= 1) {
        err = xplrGnssGetLocation(0, &gnssInfo);
        if (err != ESP_OK) {
            ret = NULL;
            APP_CONSOLE(E, "Could not get gnss location");
        } else {
            if (gnssInfo.locFixType != XPLR_GNSS_LOCFIX_INVALID && !gotFix) {
                timeToFix = timeNow - bootTime;
                gotFix = true;

                sec = timeToFix % 60;
                min = (timeToFix / 60) % 60;
                hour = (timeToFix / 60) / 60;

                memset(fixTime, 0x00, 32);
                snprintf(fixTime, 32, "%d:%02d:%02d", hour, min, sec);

                xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_FIXTIME,
                                                       (void *)&fixTime);

                ret = fixTime;
                APP_CONSOLE(I, "Device got FIX");
            } else {
                if (gnssInfo.locFixType == XPLR_GNSS_LOCFIX_INVALID && gotFix) {
                    // we had a fix but its gone... reset;
                    gotFix = false;
                    bootTime = MICROTOSEC(esp_timer_get_time());
                    i8Val = (int8_t)gnssInfo.locFixType;
                    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_READY,
                                                           (void *)&i8Val);
                    APP_CONSOLE(E, "Device lost FIX");
                }
                ret = NULL;
            }
        }

        prevTime = MICROTOSEC(esp_timer_get_time());
    } else {
        ret = NULL;
    }

    return ret;
}

static void appVersionUpdate()
{
    static char version[32];

    memset(version, 0x00, 32);
    if(INTERNAL_APP_VER > 0) {
        snprintf(version, 32, "%u.%u.%u", MAJOR_APP_VER, MINOR_APP_VER, INTERNAL_APP_VER);
    } else {
        snprintf(version, 32, "%u.%u", MAJOR_APP_VER, MINOR_APP_VER);
    }
    
    xplrWifiStarterWebserverDiagnosticsSet(XPLR_WIFISTARTER_SERVERDIAG_FWVERSION,
                                           (void *)&version);
}

/*
 * A dummy function to pause on error
 */
static void appHaltExecution(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void appFactoryResetTask(void *arg)
{
    uint32_t btnStatus;
    uint32_t currTime, prevTime;
    uint32_t btnPressDuration = 0;

    for (;;) {
        btnStatus = gpio_get_level(APP_FACTORY_MODE_BTN);
        currTime = MICROTOSEC(esp_timer_get_time());

        if (btnStatus != 1) { //check if pressed
            prevTime = MICROTOSEC(esp_timer_get_time());
            while (btnStatus != 1) { //wait for btn release.
                btnStatus = gpio_get_level(APP_FACTORY_MODE_BTN);
                vTaskDelay(pdMS_TO_TICKS(10));
                currTime = MICROTOSEC(esp_timer_get_time());
            }

            btnPressDuration = currTime - prevTime;

            if (btnPressDuration >= APP_FACTORY_MODE_TRIGGER) {
                APP_CONSOLE(W, "Factory reset triggered");
                vTaskDelay(pdMS_TO_TICKS(1000));
                xplrWifiStarterDeviceErase();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); //wait for btn release.
    }
}