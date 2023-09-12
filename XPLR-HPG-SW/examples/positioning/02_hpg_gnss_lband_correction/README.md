![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via NEO-D9S LBAND module to ZED-F9R with the help of MQTT (using certificates)

## Description

This example demonstrates how to get correction data from a **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)** LBAND module and feed it to **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** GNSS module.\
Correction data are sent as SPARTN messages and for decryption we use keys taken from **Thingstream** using **MQTT** and **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)**. This way if the keys change we will get the new ones.\
There's of course the possibility to integrate **Zero Touch Provisioning** and not use certificates.

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user.\
Upon MQTT connection and valid LBAND & geolocation data a set of diagnostics are printed similar to the ones below:

**Dead Reckoning disabled**
```
...
D [(1324) xplrBoard|xplrBoardInit|114|: Board init Done
...
D [(1473) xplrCommonHelpers|xplrHelpersUbxlibInit|131|: ubxlib init ok!
...
I [(3654) app|appInitLocationDevices|596|: Successfully initialized all GNSS related devices/functions!
I [(3884) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
========= Device Info =========
Module variant: NEO-D9S
Module version: EXT CORE 1.00 (cd6322)
Hardware version: 00190000
Rom: 0x118B2060
Firmware: PMP 1.04
Protocol: 24.00
ID: 3840bac000
-------------------------------
I2C Port: 0
I2C Address: 0x43
I2C SDA pin: 21
I2C SCL pin: 22
===============================
...
D [(7369) xplrGnss|xplrGnssFsm|894|: Restart succeeded.
...
D [(11629) xplrGnss|xplrGnssFsm|652|: Trying to open device.
I [(11899) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
========= Device Info =========
Module variant: ZED-F9R
Module version: EXT CORE 1.00 (a59682)
Hardware version: 00190000
Rom: 0x118B2060
Firmware: HPS 1.30
Protocol: 33.30
ID: 5340dae800
-------------------------------
I2C Port: 0
I2C Address: 0x42
I2C SDA pin: 21
I2C SCL pin: 22
===============================
D [(11930) xplrGnss|xplrGnssFsm|665|: Restart Completed.
D [(11951) xplrMqttWifi|xplrMqttWifiFsm|210|: MQTT config successful!
...
D [(14391) xplrMqttWifi|xplrMqttWifiEventHandlerCb|748|: MQTT event connected!
D [(14413) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|409|: Successfully subscribed to topic: /pp/ubx/0236/Lb with id: 49106
D [(14421) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|409|: Successfully subscribed to topic: /pp/frequencies/Lb with id: 4615
..
D [(14800) xplrLband|xplrLbandSetFrequencyFromMqtt|403|: Set LBAND location: eu frequency: 1545260000 Hz successfully!
I [(15036) app|app_main|396|: Frequency 1545260000 Hz read from device successfully!
...
======== Location Info ========
Location type: 1
Location fix type: RTK-FLOAT
Location latitude: 38.048035 (raw: 380480351)
Location longitude: 23.809126 (raw: 238091255)
Location altitude: 236.823000 (m) | 236823 (mm)
Location radius: 0.227000 (m) | 227 (mm)
Speed: 0.010800 (km/h) | 0.003000 (m/s) | 3 (mm/s)
Estimated horizontal accuracy: 0.2272 (m) | 227.20 (mm)
Estimated vertical accuracy: 0.3979 (m) | 397.90 (mm)
Satellite number: 23
Time UTC: 12:03:58
Date UTC: 17.08.2023
Calendar Time UTC: Thu 17.08.2023 12:03:58
===============================
...
```
**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
```
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
```

**NOTE:** Vehicle dynamics will only be printed if the module has been calibrated.

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
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED             1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                (1U)
   #define XPLRLBAND_LOG_ACTIVE               (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   ...
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRMQTTWIFI_LOG_ACTIVE            (1U)
   ...

   ```
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
9. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
10. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed. Remember to put **`"mqtts://"`** in front.\
    You can also skip this step since the correct **hostname** is already configured.
11. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
12. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
13. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
14. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem**, into **client.key** located inside the main folder of the project. Replace everything inside the file.
15. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt.csv**, into **client.crt** located inside the main folder of the project. Replace everything inside the file.
16. **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then copy and Paste the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
17. Select your region frequency inside main application source according to where you are located **EU** or **Continental US**, by giving the right value to **`APP_REGION_FREQUENCY`**:
    - **`XPLR_LBAND_FREQUENCY_EU`**
    - **`XPLR_LBAND_FREQUENCY_US`**
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
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.v
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_MQTTWIFI_CLIENT_ID`** | "MQTT Client ID" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using Kconfig.
**`CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME`** | "mqtts://pp.services.u-blox.com" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL changes in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.

<br>
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF
**`APP_SERIAL_DEBUG_ENABLED`** | Switches debug printing messages ON or OFF
**`APP_SD_LOGGING_ENABLED`** | Switches logging of the application messages to the SD card ON or OFF.
**`APP_MQTT_PAYLOAD_BUF_SIZE`** | Definition of MQTT buffer size of 512Bytes. We are only parsing decryption keys and the buffer is more than enough to hold the MQTT payload.
**`APP_LOCATION_PRINT_PERIOD`** | Period in seconds on how often we want our location print function \[**`appPrintLocation(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_DEAD_RECKONING_PRINT_PERIOD`** | Period in seconds on how often we want our dead reckoning print function \[**`appPrintDeadReckoning(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_GNSS_I2C_ADDR`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_LBAND_I2C_ADDR`** | I2C address for **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**  module.
**`APP_KEYS_TOPIC`** | Decryption keys distribution topic. No need to change unless otherwise noted on Thingstream.
**`APP_FREQ_TOPIC`** | Correction data distribution topic. No need to change unless otherwise noted on Thingstream.
**`APP_REGION_FREQUENCY `** | Region frequency.

<br>
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>