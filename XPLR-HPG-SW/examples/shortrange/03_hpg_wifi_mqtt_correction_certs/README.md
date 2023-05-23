![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via Wi-Fi MQTT to ZED-F9R using certificates

## Description

The example provided demonstrates how to connect to the **Thingstream** platform and access the **[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for our **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. You will have to provide a Wi-Fi connection with internet access.

**[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **Thingstream** through an MQTT broker that we can connect to and subscribe to the required topics.\
This example uses certificates to achieve a connection to the MQTT broker in contrast to **[Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](../04_hpg_wifi_mqtt_correction_ztp/)** which uses **Zero Touch Provisioning**.\
This example is recommended as a starting point since it's quite easier (doesn't involve a ZTP post and setup). Some caveats to consider:
1. consumes more memory
2. the user has to download certificate files and copy paste them manually
3. needs to setup MQTT topics manually

**NOTE**: In the current implementation the GNSS module does **NOT** support **Dead Reckoning** since the internal accelerometer needs to go through a calibration procedure before being able to use that feature. This will be fixed in future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and valid geolocation data a set of diagnostics are printed similar to the ones below:

```
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
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_mqtt_correction_certs` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
   set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
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
9. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt**, into **client.crt** located inside the main folder of the project. Replace everything inside the file.
10. **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then copy and Paste the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
11. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed. Remember to put **`"mqtts://"`** in front.\
    You can also skip this step since the correct **hostname** is already configured.
12. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
13. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
14. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
15. **`APP_KEYS_TOPIC "/pp/ubx/0236/Lb"`** Change topic, if needed, according to your correction data plan.
16. **`APP_CORRECTION_DATA_TOPIC "/pp/Lb/eu"`** Change topic and region, if needed, according to your data plan and geographic location respectively.
17. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.v
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_MQTTWIFI_CLIENT_ID`** | "MQTT Client ID" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using Kconfig.
**`CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME`** | "mqtts://pp.services.u-blox.com" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL changes in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED 1U`** | Switches debug printing messages ON or OFF
**`KIB 1024U`** | Helper definition to denote a size of 1 KByte
**`APP_MQTT_PAYLOAD_BUF_SIZE ((10U) * (KIB))`** | Definition of MQTT buffer size of 10 KBytes.
**`APP_LOCATION_PRINT_PERIOD 5`** | Period in seconds on how often we want our print location function [**`appPrintLocation(uint8_t periodSecs)`**] to execute. Can be changed as desired.
**`XPLR_GNSS_I2C_ADDR 0x42`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_KEYS_TOPIC "/pp/ubx/0236/Lb"`** | Decryption keys distribution topic. Change this according to your needs if needed.
**`APP_CORRECTION_DATA_TOPIC "/pp/Lb/eu"`** | Correction data distribution topic. Change this according to your needs if needed.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
