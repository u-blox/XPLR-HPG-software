![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning

## Description

The example provided demonstrates how to connect to the **Thingstream** platform and access the **[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for your **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. You will have to provide a Wi-Fi connection with internet access.

This example uses Zero Touch Provisioning (referred as ZTP from now on) to achieve a connection to the MQTT broker in contrast to **[Correction data via Wi-Fi MQTT to ZED-F9R using certificates](../03_hpg_wifi_mqtt_correction_certs/)** which uses manually downloaded certificates and MQTT settings.<br>
This example is recommended as the go to method to achieve an MQTT connection. It is also the method with the least set-up required.<br>
ZTP will guarantee that you will always get the latest required settings for all your MQTT needs.<br>

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user.
If ZTP is successful an MQTT connection will open with the Point Perfect Broker. After both previous steps go succeed a data a set of diagnostics are printed similar to the ones below:

**NOTE**: In the current implementation the GNSS module does **NOT** support **Dead Reckoning** since the internal accelerometer needs to go through a calibration procedure before being able to use that feature. This will be fixed in future release.

```
D [(20825) xplrZtp|xplrZtpGetPayload|107|: POST URL: https://api.thingstream.io/ztp/pointperfect/credentials
D [(20835) xplrZtp|xplrZtpGetDeviceID|190|: Device ID: **************
D [(20839) xplrZtp|xplrZtpPrivateSetPostData|334|: Post data: {"token":"**************","givenName":"device-name","hardwareId":"**************","tags": []}
D [(22542) xplrZtp|client_event_post_handler|207|: HTTP_EVENT_ON_CONNECTED!
D [(22960) xplrZtp|client_event_post_handler|242|: HTTP_EVENT_ON_FINISH
D [(22960) xplrZtp|xplrZtpGetPayload|152|: HTTPS POST request OK.
D [(22962) xplrZtp|xplrZtpGetPayload|157|: HTTPS POST: Return Code - 200
I [(22977) app|appDeallocateJSON|571|: Deallocating JSON object.
D [(22979) xplrMqttWifi|xplrMqttWifiFsm|171|: MQTT config successful!
I [(22982) xplrGnss|xplrGnssPrintLocation|771|: Printing location info.
...
D [(23059) xplrMqttWifi|xplrMqttWifiFsm|180|: MQTT event register successful!
D [(23084) xplrMqttWifi|xplrMqttWifiFsm|195|: MQTT client start successful!
D [(26325) xplrMqttWifi|xplrMqttWifiEventHandlerCb|628|: MQTT event connected!
D [(26346) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|375|: Successfully subscribed to topic: /pp/ubx/0236/Lb with id: 15241
D [(26348) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|375|: Successfully subscribed to topic: /pp/Lb/eu with id: 30356
D [(26739) xplrMqttWifi|xplrMqttWifiEventHandlerCb|640|: MQTT event subscribed!
D [(26884) xplrMqttWifi|xplrMqttWifiEventHandlerCb|640|: MQTT event subscribed!
...
I [(254200) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [267] bytes.
I [(255481) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [5349] bytes.
I [(255521) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [607] bytes.
I [(257571) xplrGnss|xplrGnssPrintLocation|771|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FLOAT
Location latitude: 37.980349 (raw: 379803489)
Location longitude: 23.657008 (raw: 236570083)
Location altitude: 66.248000 (m) | 66248 (mm)
Location radius: 0.212000 (m) | 212 (mm)
Speed: 0.057600 (km/h) | 0.016000 (m/s) | 16 (mm/s)
Estimated horizontal accuracy: 0.2121 (m) | 212.10 (mm)
Estimated vertical accuracy: 0.2589 (m) | 258.90 (mm)
Satellite number: 27
Time UTC: 1671709848
===============================
I [(258891) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|358|: Sent UBX formatted command [149] bytes.
I [(262566) xplrGnss|xplrGnssPrintLocation|771|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FLOAT
Location latitude: 37.980349 (raw: 379803489)
Location longitude: 23.657007 (raw: 236570074)
Location altitude: 66.373000 (m) | 66373 (mm)
Location radius: 0.207000 (m) | 207 (mm)
Speed: 0.046800 (km/h) | 0.013000 (m/s) | 13 (mm/s)
Estimated horizontal accuracy: 0.2066 (m) | 206.60 (mm)
Estimated vertical accuracy: 0.2558 (m) | 255.80 (mm)
Satellite number: 27
Time UTC: 1671709853
===============================
```
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
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Download **Root Certificate** from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**
   - **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then **Copy and Paste** the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
8.  Zero Touch provisioning [**Token** and **URL**](./../../../docs/README_thingstream_ztp.md):
    - Get your **device token** from **Thingstream**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream token` and configure it as needed.
    - Get the **URL** found in the **Endpoint**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream credentials URL` and configure it as needed.\
   The default value is already pointing to the required **URL** and this step can be skipped.
9. Go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP device name` and input a `device name`. This `device name` does not have to be something specific (i.e. taken from **Thingstream**), you can chose its value freely, but it cannot be empty.\
    The default value can be left as is and this step can be skipped.
10. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure your **Access Point's SSID** as needed.
11. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure your **Access Point's Password** as needed.
12. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.

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
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for .|
**`CONFIG_XPLR_TS_PP_ZTP_CREDENTIALS_URL`** | "https://api.thingstream.io/ztp/pointperfect/credentials" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | A URL to make a Zero Touch Provisioning POST request. Taken from **Thingstream**. | There's no need to replace this value unless the **URL** changes in the future. You can replace this value freely in the app.
**`CONFIG_XPLR_TS_PP_ZTP_TOKEN`** | "ztp-token" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | A device token taken from **Thingstream** devices. | You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_TS_PP_DEVICE_NAME`** | "device-name" | **[hpg_wifi_mqtt_correction_ztp](./main/hpg_wifi_mqtt_correction_ztp.c)** | A device name. Input a string to make your device more distinguishable. Cannot be empty! | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
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
**`APP_SERIAL_DEBUG_ENABLED 1U`** | Switches debug printing messages ON or OFF
**`KIB 1024U`** | Helper definition to denote a size of 1 KByte
**`APP_ZTP_PAYLOAD_BUF_SIZE ((10U) * (KIB))`** | A 10 KByte buffer size to store the POST response body from Zero Touch Provisioning.
**`APP_KEYCERT_PARSE_BUF_SIZE  ((2U) * (KIB))`** | A 2 KByte buffer size used for both key and cert/pem key parsed data from ZTP.
**`APP_MQTT_PAYLOAD_BUF_SIZE ((10U) * (KIB))`** | Definition of MQTT buffer size of 10 KBytes.
**`APP_MQTT_CLIENT_ID_BUF_SIZE (128U)`** | A 128 Bytes buffer size to store the parsed MQTT Client ID from ZTP Json Parser
**`APP_MQTT_HOST_BUF_SIZE (128U)`** | A 128 Bytes buffer size to store the parsed MQTT host URL/URI from ZTP Json Parser.
**`APP_REGION`** | Selected region/location for device.
**`APP_LOCATION_PRINT_PERIOD 5`** | Period in seconds on how often we want our print location function [**`appPrintLocation(uint8_t periodSecs)`**] to execute. Can be changed as desired.
**`APP_MAX_TOPIC_CNT 2`** | Maximum number of MQTT topics to subscribe to.
**`XPLR_GNSS_I2C_ADDR 0x42`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module

<br>
<br>


## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager
**[xplr_ztp](./../../../components/xplr_ztp)** | Performs Zero Touch Provisioning POST and gets necessary data for MQTT
**[xplr_ztp_json_parser](./../../../components/xplr_ztp_json_parser/)** | A parser able to extract data from the POST body
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**