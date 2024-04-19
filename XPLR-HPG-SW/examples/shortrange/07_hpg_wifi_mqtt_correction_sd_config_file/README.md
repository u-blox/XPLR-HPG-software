![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via Wi-Fi MQTT to ZED-F9R using configuration file from the SD card

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for our **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. You will have to provide a Wi-Fi connection with internet access. 

**[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to the required topics.\
This example uses a single configuration file to be able to achieve connection to the MQTT broker, in contrast to **[Correction data via Wi-Fi MQTT to ZED-F9R using certificates](../03_hpg_wifi_mqtt_correction_certs/)**\
that needs the certificates to be passed manually. The example has more similarities with **[Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](../04_hpg_wifi_mqtt_correction_ztp/)**  without\
the need of ZTP post and setup. Some caveats to consider:
1. A SD card is required for this specific example.
2. The user needs to download the **[required configuration file](./../../../docs/README_thingstream_ucenter_config.md)** from **[Thingstream](https://developer.thingstream.io/home)** platform and copy it to the SD card.


**NOTE 1**: Make sure your subscription plan to **[Thingstream](https://developer.thingstream.io/home)** includes correction data via IP (is either PointPerfect IP or PointPerfect IP + LBAND), otherwise the client will not be able to subscribe to the required topics resulting to error in execution.

**NOTE 2**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and valid geolocation data a set of diagnostics are printed similar to the ones below:

```
...
D [(1394) xplrBoard|xplrBoardInit|114|: Board init Done
D [(1399) app|appInitBoard|369|: Board Initialized
I [(1406) app|appInitWiFi|379|: Starting WiFi in station mode.
D [(1413) app|appInitWiFi|385|: Wifi station mode Initialized
D [(1460) xplrCommonHelpers|xplrHelpersUbxlibInit|122|: ubxlib init ok!
D [(1463) app|appInitGnssDevice|401|: Waiting for GNSS device to come online!
D [(1473) xplrCommonHelpers|xplrHlprLocSrvcDeviceConfigDefault|265|: Device config set.
D [(1476) xplrCommonHelpers|xplrHlprLocSrvcNetworkConfigDefault|276|: Network config set.
D [(1481) xplrCommonHelpers|xplrHlprLocSrvcConfigAllDefault|245|: All default configs set.
D [(1494) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpen|140|: Trying to open device.
I [(3533) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpen|169|: ubxlib device opened!
D [(3538) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpen|188|: ubxlib device opened!
D [(3662) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValSet|334|: Set multiple configuration values.
I [(3685) xplrGnss|xplrGnssGetAllMsgsAsyncStart|916|: Started Gnss Get All messages async.
I [(3692) xplrGnss|xplrGnssStartAllAsyncGets|686|: Started all async getters.
D [(3870) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|318|: Set configuration value.
I [(3924) app|appInitGnssDevice|412|: Successfully initialized all GNSS related devices/functions!
D [(4168) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|390|: Got device info.
========= Device Info =========
Lband version: EXT CORE 1.00 (a59682)
Hardware: 00190000
Rom: 0x118B2060
Firmware: HPS 1.30
Protocol: 33.30
Model: ZED-F9R
ID: 5340dae800
-------------------------------
I2C Port: 0
I2C Address: 0x42
I2C SDA pin: 21
I2C SCL pin: 22
===============================
D [(4195) xplrWifiStarter|xplrWifiStarterFsm|182|: Config WiFi OK.
...
D [(4471) xplrWifiStarter|xplrWifiStarterFsm|191|: Init flash successful!
D [(4500) xplrWifiStarter|xplrWifiStarterFsm|218|: Init netif successful!
D [(4526) xplrWifiStarter|xplrWifiStarterFsm|230|: Init event loop successful!
D [(4556) xplrWifiStarter|xplrWifiStarterFsm|250|: Create station successful!
...
D [(4686) xplrWifiStarter|xplrWifiStarterFsm|282|: WiFi init successful!
D [(4721) xplrWifiStarter|xplrWifiStarterFsm|293|: Register handlers successful!
D [(4748) xplrWifiStarter|xplrWifiStarterFsm|312|: Wifi credentials configured:1 
D [(4780) xplrWifiStarter|xplrWifiStarterFsm|318|: WiFi mode selected: STATION
D [(4782) xplrWifiStarter|xplrWifiStarterFsm|326|: WiFi set mode successful!
D [(4834) xplrWifiStarter|xplrWifiStarterFsm|367|: WiFi set config successful!
I (4865) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07
I (4970) wifi:mode : sta (e8:31:cd:da:96:dc)
I (4971) wifi:enable tsf
D [(4973) xplrWifiStarter|xplrWifiStarterFsm|395|: WiFi started successful!
D [(5014) xplrWifiStarter|xplrWifiStarterFsm|444|: Call esp_wifi_connect success!
...
D [(6090) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6104) xplrThingstream|tsPpConfigFileGetBroker|1814|: Parsed Server URI from configuration payload
D [(6110) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6118) xplrThingstream|tsPpConfigFileGetDeviceId|1870|: Parsed Client ID from configuration payload
D [(6128) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6137) xplrThingstream|tsPpConfigFileGetClientKey|1923|: Parsed Client Key from configuration payload
D [(6162) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6168) xplrThingstream|tsPpConfigFileGetClientCert|1977|: Parsed Client Cert from configuration payload
D [(6173) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6186) xplrThingstream|tsPpConfigFileGetRootCa|2030|: Parsed Root CA from configuration payload
D [(6194) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6200) xplrThingstream|tsPpConfigFileGetDynamicKeys|2116|:
Dynamic keys parsed:
Current key:
         start (UTC):*******
         duration (UTC):********
         value:******************
Next key:
         start (UTC):*******
         duration (UTC):********
         value:******************

D [(6255) xplrThingstream|tsApiMsgParsePpZtpCheckTag|1572|: Tag <MQTT> found.
D [(6263) xplrThingstream|tsPpSetDescFilter|2395|: IP + L-Band plan does support correction data via MQTT!
I [(6269) app|appGetSdCredentials|434|: Successfully parsed configuration file
D [(6275) xplrMqttWifi|xplrMqttWifiFsm|206|: MQTT config successful!
D [(6391) xplrMqttWifi|xplrMqttWifiFsm|216|: MQTT event register successful!
D [(6418) xplrMqttWifi|xplrMqttWifiFsm|231|: MQTT client start successful!
D [(8558) xplrMqttWifi|xplrMqttWifiEventHandlerCb|711|: MQTT event connected!
D [(8575) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/ubx/0236/Lb with id: 29825
D [(8582) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/ubx/mga with id: 48009
D [(8595) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/Lb/eu with id: 25825
D [(8601) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/Lb/eu/gad with id: 3617
D [(8616) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/Lb/eu/hpac with id: 38988
D [(8623) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/Lb/eu/ocb with id: 13219
D [(8637) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|407|: Successfully subscribed to topic: /pp/Lb/eu/clk with id: 5498
D [(8746) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8851) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8876) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8887) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8893) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8903) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(8921) xplrMqttWifi|xplrMqttWifiEventHandlerCb|724|: MQTT event subscribed!
D [(10115) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [188] bytes.

D [(14751) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [196] bytes.
I [(16272) xplrGnss|xplrGnssPrintLocation|1005|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS
Location latitude: 38.048134 (raw: 380481342)
Location longitude: 23.808953 (raw: 238089533)
Location altitude: 238.641000 (m) | 238641 (mm)
Location radius: 1.921000 (m) | 1921 (mm)
Speed: 0.018000 (km/h) | 0.005000 (m/s) | 5 (mm/s)
Estimated horizontal accuracy: 1.9210 (m) | 1921.00 (mm)
Estimated vertical accuracy: 2.0275 (m) | 2027.50 (mm)
Satellite number: 19
Time UTC: 07:40:01
Date UTC: 07.07.2023
Calendar Time UTC: Fri 07.07.2023 07:40:01
===============================
I [(16319) xplrGnss|xplrGnssPrintGmapsLocation|792|: Printing GMapsLocation!
I [(16335) xplrGnss|xplrGnssPrintGmapsLocation|799|: Gmaps Location: https://maps.google.com/?q=38.048134,23.808953
D [(20350) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [188] bytes.
D [(25460) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [188] bytes.
D [(29867) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [267] bytes.
D [(30656) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [2248] bytes.
D [(30750) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [1578] bytes.
D [(30853) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [1797] bytes.
D [(30952) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [1804] bytes.
D [(31040) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|440|: Sent UBX formatted command [819] bytes.
...
I [(51267) xplrGnss|xplrGnssPrintLocation|1005|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FLOAT
Location latitude: 38.048150 (raw: 380481504)
Location longitude: 23.808956 (raw: 238089557)
Location altitude: 236.334000 (m) | 236334 (mm)
Location radius: 0.497000 (m) | 497 (mm)
Speed: 0.046800 (km/h) | 0.013000 (m/s) | 13 (mm/s)
Estimated horizontal accuracy: 0.4972 (m) | 497.20 (mm)
Estimated vertical accuracy: 0.5359 (m) | 535.90 (mm)
Satellite number: 19
Time UTC: 07:40:36
Date UTC: 07.07.2023
Calendar Time UTC: Fri 07.07.2023 07:40:36
===============================
```

**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
```
...
...
...
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
...
...
...
```

**NOTE:**
- Vehicle dynamics will only be printed if the module has been calibrated.
- Dead reckoning is only available in ZED-F9R GNSS module.

<br>

## Build instructions

### Building using Visual Studio Code
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and MQTT settings using Kconfig.
Please follow the steps described below:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_mqtt_correction_sd_config_file` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_ntrip_correction")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_captive_portal")
   set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_sd_config_file")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    1U
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   ...
   #define XPLRTHINGSTREAM_DEBUG_ACTIVE       (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   #define XPLRMQTTWIFI_DEBUG_ACTIVE          (1U)
   ...
   #define XPLRLOG_DEBUG_ACTIVE               (1U)
   #define XPLRSD_DEBUG_ACTIVE                (1U)
   ```
4. Open the [app](./main/hpg_wifi_mqtt_correction_sd_config_file.c) and select if you need logging to the SD card for your application.
   ```
   #define APP_SD_LOGGING_ENABLED      0U
   ```
   This is a general option that enables or disables the SD logging functionality for all the modules. <br> 
   To enable/disable the individual module debug message SD logging:
   - Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED             1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   #define XPLRNVS_LOG_ACTIVE                 (1U)
   ...
   #define XPLR_THINGSTREAM_LOG_ACTIVE        (1U)
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRMQTTWIFI_LOG_ACTIVE            (1U)
   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_wifi_mqtt_correction_sd_config_file.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected.
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Correction Data Source -> Choose correction data source for your GNSS module` and select the **Correction data source** you need for your GNSS module.
11. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
12. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
13. Download the required configuration file from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_ucenter_config.md)**, then copy the downloaded file in your SD card and insert it in the SD card slot of your board.
14. Go to `XPLR HPG Options -> u-center Config -> u-center Config Filename` and paste the name of your configuration file in your SD card.
15. Change **APP_THINGSTREAM_REGION** macro ,according to your **[Thingstream](https://developer.thingstream.io/home)** service location.
16. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_mqtt_correction_sd_config_file` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 15 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
6. Run `idf.py build` to compile the project.
7. Run `idf.py -p COMX flash` to flash the binary to the board, where **COMX** is the `COM Port` that the XPLR-HPGx has enumerated on.
8. Run `idf.py monitor -p COMX` to monitor the debug UART output.

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_GNSS_MODULE`** | "ZED-F9R" | **[boards](./../../../components/boards)** | Selects the GNSS module equipped. |
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_sd_config_file.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_CORRECTION_DATA_SOURCE`** | "Correction via IP" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_sd_config_file.c)** | Selects the source of the correction data to be forwarded to the GNSS module. |
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_sd_config_file.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.v
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_sd_config_file.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB `** | Helper definition to denote a size of 1 KByte
**`APP_JSON_PAYLOAD_BUF_SIZE   `** | Definition of the configuration file size of 5 KBytes.
**`APP_LOCATION_PRINT_PERIOD `** | Period in seconds on how often we want our print location function \[**`appPrintLocation(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_DEAD_RECKONING_PRINT_PERIOD`** | Interval, in seconds, of how often we want the location to be printed.
**`APP_GNSS_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_LBAND_I2C_ADDR`** | I2C address for **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**  module.
**`APP_THINGSTREAM_REGION`** | Thingstream service region.
**`APP_ENABLE_CORR_MSG_WDG`** | Option to enable the correction message watchdog mechanism.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_RESTART_ON_ERROR`** | Trigger soft reset if device in error state.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND service manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | XPLR SD card driver.