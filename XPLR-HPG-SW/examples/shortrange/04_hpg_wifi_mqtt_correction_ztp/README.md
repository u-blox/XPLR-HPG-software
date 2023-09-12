![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for your **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. You will have to provide a Wi-Fi connection with internet access.

This example uses Zero Touch Provisioning (referred as ZTP from now on) to achieve a connection to the MQTT broker in contrast to **[Correction data via Wi-Fi MQTT to ZED-F9R using certificates](../03_hpg_wifi_mqtt_correction_certs/)** which uses manually downloaded certificates and MQTT settings.<br>
This example is recommended as the go to method to achieve an MQTT connection. It is also the method with the least set-up required.<br>
ZTP will guarantee that you will always get the latest required settings for all your MQTT needs.<br>

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user.
If ZTP is successful an MQTT connection will open with the PointPerfect Broker. After both previous steps go succeed a data a set of diagnostics are printed similar to the ones below:

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

**Dead Reckoning disabled**
```
I (9135) esp_netif_handlers: sta ip: ***, mask: ***, gw: ***
I [(9137) xplrWifiStarter|wifiStaPrintInfo|1651|: Station connected with following settings:
SSID: ***
Password: ***
IP: ***
...
D [(9179) xplrThingstream|xplrThingstreamInit|255|: Server url:https://api.thingstream.io.
D [(9187) xplrThingstream|xplrThingstreamInit|256|: PP credentials path:/ztp/pointperfect/credentials.
...
I (9804) esp-x509-crt-bundle: Certificate validated
D [(11024) app|httpClientEventCB|844|: HTTP_EVENT_ON_CONNECTED!
D [(11148) app|httpClientEventCB|884|: HTTP_EVENT_ON_FINISH
I [(11148) app|appGetRootCa|608|: HTTPS GET request OK: code [200] - payload size [1188].
D [(11155) xplrZtp|xplrZtpGetPayloadWifi|143|: POST URL: https://api.thingstream.io/ztp/pointperfect/credentials
D [(11163) xplrZtp|ztpWifiSetHeaders|267|: Successfully set headers for HTTP POST
D [(11169) xplrThingstream|tsApiMsgCreatePpZtp|1075|: Thingstream API PP ZTP POST of 124 bytes created:
{
        "tags": ["***"],
        "token":        "***",
        "hardwareId":   "***",
        "givenName":    "***"
}
D [(11190) xplrZtp|ztpWifiSetPostData|303|: Successfully set POST field
D [(12966) xplrZtp|httpWifiCallback|492|: HTTP_EVENT_ON_CONNECTED!
D [(13589) xplrZtp|httpWifiCallback|520|: HTTP_EVENT_ON_FINISH
D [(13589) xplrZtp|ztpWifiPostMsg|339|: HTTPS POST request OK.
D [(13589) xplrZtp|ztpWifiPostMsg|346|: HTTPS POST: Return Code - 200
D [(13600) xplrZtp|ztpWifiHttpCleanup|361|: Client cleanup succeeded!
D [(13607) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1575|: Tag <brokerHost> found.
D [(13616) xplrThingstream|tsApiMsgParsePpZtpBrokerAddress|1108|: Broker address (22 bytes): pp.services.u-blox.com.
...
I [(13707) xplrThingstream|tsPpGetPlanType|1596|: Your current Thingstream plan is : PointPerfect L-band and IP, thus, valid to receive correction data via MQTT
D [(13725) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1575|: Tag <dynamickeys> found.
...
D [(13887) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1575|: Tag <subscriptions> found.
D [(13896) xplrThingstream|tsApiMsgParsePpZtpTopic|1487|: Parsed L-band + IP GNSS clock topic for EU region @ /pp/Lb/eu/clk.
I [(13903) app|app_main|340|: ZTP Successful!
D ...
D [(13968) xplrMqttWifi|xplrMqttWifiFsm|220|: MQTT event register successful!
D [(14000) xplrMqttWifi|xplrMqttWifiFsm|235|: MQTT client start successful!
D [(17678) xplrMqttWifi|xplrMqttWifiEventHandlerCb|713|: MQTT event connected!
...
D [(17743) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|409|: Successfully subscribed to topic: /pp/Lb/eu/clk with id: 33354
D [(17877) xplrMqttWifi|xplrMqttWifiEventHandlerCb|726|: MQTT event subscribed!
D [(17901) xplrGnss|xplrGnssSendDecryptionKeys|1384|: Saved keys into config struct.
D [(17903) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|477|: Sent UBX data [60] bytes.
...
D [(18110) xplrMqttWifi|xplrMqttWifiEventHandlerCb|726|: MQTT event subscribed!
...
I [(254200) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [267] bytes.
I [(255481) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [5349] bytes.
I [(255521) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [607] bytes.
I [(257571) xplrGnss|xplrGnssPrintLocation|771|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FIXED
Location latitude: 38.048038 (raw: 380480379)
Location longitude: 23.809122 (raw: 238091221)
Location altitude: 233.311000 (m) | 233311 (mm)
Location radius: 0.031000 (m) | 31 (mm)
Speed: 0.014400 (km/h) | 0.004000 (m/s) | 4 (mm/s)
Estimated horizontal accuracy: 0.0311 (m) | 31.10 (mm)
Estimated vertical accuracy: 0.0410 (m) | 41.00 (mm)
Satellite number: 32
Time UTC: 13:12:11
Date UTC: 08.08.2023
Calendar Time UTC: Tue 08.08.2023 13:12:11
===============================
```
**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
```
I [(131302) xplrGnss|xplrGnssPrintImuAlignmentInfo|1621|: Printing Imu Alignment Info.
====== Imu Alignment Info ======
Calibration Mode: Manual
Calibration Status: user-defined
Aligned yaw: 31.39
Aligned pitch: 5.04
Aligned roll: -178.85
-------------------------------
I [(131328) xplrGnss|xplrGnssPrintImuAlignmentStatus|1696|: Printing Imu Alignment Statuses.
===== Imu Alignment Status ====
Fusion mode: 0
Number of sensors: 7
-------------------------------
Sensor type: Gyroscope Z Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Wheel Tick Single Tick
Used: 0 | Ready: 0
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Gyroscope Y Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Gyroscope X Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer X Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Y Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Z Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 50 Hz
Sensor faults: No Errors
-------------------------------
I [(131441) xplrGnss|xplrGnssPrintImuVehicleDynamics|1778|: Printing vehicle dynamics
======= Vehicle Dynamics ======
----- Meas Validity Flags -----
Gyro  X: 0 | Gyro  Y: 0 | Gyro  Z: 0
Accel X: 0 | Accel Y: 0 | Accel Z: 0
- Dynamics Compensated Values -
X-axis angular rate: 0.000 deg/s
Y-axis angular rate: 0.000 deg/s
Z-axis angular rate: 0.000 deg/s
X-axis acceleration (gravity-free): 0.00 m/s^2
Y-axis acceleration (gravity-free): 0.00 m/s^2
Z-axis acceleration (gravity-free): 0.00 m/s^2
===============================
```
**NOTE:** Vehicle dynamics will only be printed if the module has been calibrated.

<br>

## Build instructions
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi, ZTP and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_mqtt_correction_ztp` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
   set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    1U
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRZTP_DEBUG_ACTIVE               (1U)
   #define XPLRZTPJSONPARSER_DEBUG_ACTIVE     (1U)
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   #define XPLRMQTTWIFI_DEBUG_ACTIVE          (1U)
   ```
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED             1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   ...
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRMQTTWIFI_LOG_ACTIVE            (1U)
   #define XPLRZTP_LOG_ACTIVE                 (1U)
   #define XPLRZTPJSONPARSER_LOG_ACTIVE       (1U)
   ...

   ```
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
9. `XPLR HPG Options -> AWS Root CA Settings -> AWS Root CA Url`. Amazon Url in order to fetch the Root CA certificate. If not using a different Root CA certificate leave to default value.
10.  Zero Touch provisioning [**Token** and **URL**](./../../../docs/README_thingstream_ztp.md):
    - Get your **device token** from **Thingstream**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream token` and configure it as needed.
11. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure your **Access Point's SSID** as needed.
12. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure your **Access Point's Password** as needed.
13. Change variable `ppRegion` defined in [app wifi ztp](./../04_hpg_wifi_mqtt_correction_ztp/main/hpg_wifi_mqtt_correction_ztp.c) according to your **[Thingstream](https://developer.thingstream.io/home)** service location.\
Possible values are **XPLR_THINGSTREAM_PP_REGION_EU** and **XPLR_THINGSTREAM_PP_REGION_US** as they are the only supported regions at the moment.
14. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.

<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.

<br>
<br>

## KConfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[KConfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[KConfig](./../../../docs/README_kconfig.md)** before building.\
**[KConfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_TS_PP_ZTP_TOKEN`** | "ztp-token" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | A device token taken from **Thingstream** devices. | You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.

<br>
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB `** | Helper definition to denote a size of 1 KByte.
**`APP_ZTP_PAYLOAD_BUF_SIZE `** | A 10 KByte buffer size to store the POST response body from Zero Touch Provisioning.
**`APP_KEYCERT_PARSE_BUF_SIZE  `** | A 2 KByte buffer size used for both key and cert/pem key parsed data from ZTP.
**`APP_MQTT_PAYLOAD_BUF_SIZE `** | Definition of MQTT buffer size of 10 KBytes.
**`APP_MQTT_CLIENT_ID_BUF_SIZE `** | A 128 Bytes buffer size to store the parsed MQTT Client ID from ZTP Json Parser.
**`APP_MQTT_HOST_BUF_SIZE `** | A 128 Bytes buffer size to store the parsed MQTT host URL/URI from ZTP Json Parser.
**`APP_LOCATION_PRINT_PERIOD `** | Period in seconds on how often we want our print location function \[**`appPrintLocation(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_DEAD_RECKONING_PRINT_PERIOD `** | Period in seconds on how often we want our print dead reckoning data function \[**`appPrintDeadReckoning(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_MAX_TOPIC_CNT `** | Maximum number of MQTT topics to subscribe to.
**`APP_GNSS_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

<br>
<br>


## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[hpglib/ztp_service](./../../../components/hpglib/src/ztp_service/)** | Performs Zero Touch Provisioning POST and gets necessary data for MQTT.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.