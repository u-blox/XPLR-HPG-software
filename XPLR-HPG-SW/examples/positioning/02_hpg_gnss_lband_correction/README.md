![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via NEO-D9S LBAND module to ZED-F9R with the help of MQTT (using certificates)

## Description

This example demonstrates how to get correction data from a **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)** LBAND module and feed it to **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** GNSS module.\
The correction data are sent as SPARTN messages and for decryption we use keys taken from **Thingstream** using **MQTT** and **[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)**. This way if the keys change we will get the new ones. As long as the GNSS module is not restarted it will retain the decryption keys.\
There's of course the possibility to integrate **Zero Touch Provisioning** and not use certificates.

**NOTE**: In the current implementation the GNSS module does **NOT** support **Dead Reckoning** since the internal accelerometer needs to go through a calibration procedure before being able to use that feature. This will be fixed in future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and valid LBAND & geolocation data a set of diagnostics are printed similar to the ones below:

```
D [(25125) xplrMqttWifi|xplrMqttWifiFsm|171|: MQTT config successful!
D [(25152) xplrMqttWifi|xplrMqttWifiFsm|180|: MQTT event register successful!
D [(25177) xplrMqttWifi|xplrMqttWifiFsm|195|: MQTT client start successful!
D [(27446) xplrMqttWifi|xplrMqttWifiEventHandlerCb|628|: MQTT event connected!
D [(10064) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|370|: Successfully subscribed to topic: /pp/ubx/0236/Lb with id: 55197
D [(10067) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|370|: Successfully subscribed to topic: /pp/frequencies/Lb with id: 16340
D [(10361) xplrMqttWifi|xplrMqttWifiEventHandlerCb|675|: MQTT event subscribed!
D [(10377) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|413|: Sent UBX formatted command [60] bytes.
I [(10378) app|app_main|253|: Decryption keys sent successfully!
D [(10562) xplrMqttWifi|xplrMqttWifiEventHandlerCb|675|: MQTT event subscribed!
I [(10701) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|291|: Set configuration value.
D [(10701) xplrLband|xplrLbandSetFrequencyFromMqtt|341|: Set LBAND location: eu frequency: 1545260000 Hz successfully!
I [(10710) app|app_main|264|: Frequency set successfully!

...

D [(107548) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|413|: Sent UBX formatted command [536] bytes.
I [(107548) xplrLband|xplrLbandMessageReceivedCB|581|: Sent LBAND correction data size [536]
D [(109251) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|413|: Sent UBX formatted command [536] bytes.
I [(109251) xplrLband|xplrLbandMessageReceivedCB|581|: Sent LBAND correction data size [536]
D [(110907) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|413|: Sent UBX formatted command [536] bytes.
I [(110907) xplrLband|xplrLbandMessageReceivedCB|581|: Sent LBAND correction data size [536]
I [(111216) xplrGnss|xplrGnssPrintLocation|760|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FIXED
Location latitude: 38.048039 (raw: 380480386)
Location longitude: 23.809122 (raw: 238091223)
Location altitude: 233.159000 (m) | 233159 (mm)
Location radius: 0.099000 (m) | 99 (mm)
Speed: 0.000000 (km/h) | 0.000000 (m/s) | 0 (mm/s)
Estimated horizontal accuracy: 0.0992 (m) | 99.20 (mm)
Estimated vertical accuracy: 0.1340 (m) | 134.00 (mm)
Satellite number: 29
Time UTC: 08:22:28
Date UTC: 26.05.2023
Calendar Time UTC: Fri 26.05.2023 08:22:28
...
```

<br>
<br>

## Build instructions
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_gnss_lband_correction` project, making sure that all other projects are commented out:
   ```
   ...
   # positioning examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_config")
   set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_lband_correction")
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
   #define XPLRLBAND_DEBUG_ACTIVE             (1U)
   ...
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   #define XPLRMQTTWIFI_DEBUG_ACTIVE          (1U)
   ```
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
8. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem**, into **client.key** located inside the main folder of the project. Replace everything inside the file.
9. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt.csv**, into **client.crt** located inside the main folder of the project. Replace everything inside the file.
10. **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then copy and Paste the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
11. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed. Remember to put **`"mqtts://"`** in front.\
    You can also skip this step since the correct **hostname** is already configured.
12. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
13. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
14. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
15. Select your region frequency inside main application source according to where you are located **EU** or **Continental US**, by giving the right value to **`APP_REGION_FREQUENCY`**:
    - **`XPLR_LBAND_FREQUENCY_EU`**
    - **`XPLR_LBAND_FREQUENCY_US`**
16. **`APP_KEYS_TOPIC "/pp/ubx/0236/Lb"`** Change topic, if needed, according to your correction data plan.
17. **`APP_CORRECTION_DATA_TOPIC "/pp/Lb/eu"`** Change topic and region, if needed, according to your data plan and geographic location respectively.
18. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.

<br>
<br>

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.v
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_MQTTWIFI_CLIENT_ID`** | "MQTT Client ID" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using Kconfig.
**`CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME`** | "mqtts://pp.services.u-blox.com" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL chanegs in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.

<br>
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED 1U`** | Switches debug printing messages ON or OFF
**`APP_MQTT_PAYLOAD_BUF_SIZE (512U)`** | Definition of MQTT buffer size of 512Bytes. We are only parsing decryption keys and the buffer is more than enough to hold the MQTT payload.
**`APP_LOCATION_PRINT_PERIOD 5`** | Period in seconds on how often we want our print location function [**`appPrintLocation(uint8_t periodSecs)`**] to execute. Can be changed as desired.
**`XPLR_GNSS_I2C_ADDR 0x42`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`XPLR_LBAND_I2C_ADDR 0x43`** | I2C address for **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**  module.
**`APP_KEYS_TOPIC  "/pp/ubx/0236/Lb"`** | Decryption keys distribution topic. No need to change unless otherwise noted on Thingstream.
**`APP_FREQ_TOPIC  "/pp/frequencies/Lb"`** | Correction data distribution topic. No need to change unless otherwise noted on Thingstream.

<br>
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND location device manager
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**


