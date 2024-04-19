![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction data via Wi-Fi MQTT to ZED-F9R using certificates

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for our **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. You will have to provide a Wi-Fi connection with internet access.

**[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to the required topics.\
This example uses certificates to achieve a connection to the MQTT broker in contrast to **[Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](../04_hpg_wifi_mqtt_correction_ztp/)** which uses **Zero Touch Provisioning**.\
This example is recommended as a starting point since it's quite easier (doesn't involve a ZTP post and setup). Some caveats to consider:
1. consumes more memory
2. the user has to download certificate files and copy paste them manually
3. needs to setup MQTT topics manually

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and valid geolocation data a set of diagnostics are printed similar to the ones below:

**Dead Reckoning disabled**
```
D [(88001) hpgLocHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|461|: Sent UBX data [202] bytes.
I [(91254) hpgGnss|gnssLocationPrinter|4258|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: RTK-FLOAT
Location latitude: 38.048038 (raw: 380480378)
Location longitude: 23.809130 (raw: 238091304)
Location altitude: 237.230000 (m) | 237230 (mm)
Location radius: 0.244000 (m) | 244 (mm)
Speed: 0.007200 (km/h) | 0.002000 (m/s) | 2 (mm/s)
Estimated horizontal accuracy: 0.2442 (m) | 244.20 (mm)
Estimated vertical accuracy: 0.3372 (m) | 337.20 (mm)
Satellite number: 30
Time UTC: 12:41:24
Date UTC: 12.01.2024
Calendar Time UTC: Fri 12.01.2024 12:41:24
===============================
I [(91300) hpgGnss|xplrGnssPrintGmapsLocation|1806|: Printing GMapsLocation!
I [(91307) hpgGnss|xplrGnssPrintGmapsLocation|1807|: Gmaps Location: https://maps.google.com/?q=38.048038,23.809130
D [(93426) hpgLocHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|461|: Sent UBX data [202] bytes.
I [(96256) hpgGnss|gnssLocationPrinter|4258|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: RTK-FLOAT
Location latitude: 38.048036 (raw: 380480365)
Location longitude: 23.809128 (raw: 238091281)
Location altitude: 235.639000 (m) | 235639 (mm)
Location radius: 0.159000 (m) | 159 (mm)
Speed: 0.007200 (km/h) | 0.002000 (m/s) | 2 (mm/s)
Estimated horizontal accuracy: 0.1588 (m) | 158.80 (mm)
Estimated vertical accuracy: 0.2201 (m) | 220.10 (mm)
Satellite number: 30
Time UTC: 12:41:29
Date UTC: 12.01.2024
Calendar Time UTC: Fri 12.01.2024 12:41:29
===============================
I [(96303) hpgGnss|xplrGnssPrintGmapsLocation|1806|: Printing GMapsLocation!
I [(96309) hpgGnss|xplrGnssPrintGmapsLocation|1807|: Gmaps Location: https://maps.google.com/?q=38.048036,23.809128
```
**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
```
...
...
...
I [(131302) xplrGnss|xplrGnssPrintImuAlignmentInfo|1621|: Printing Imu Alignment Info.
====== Imu Alignment Info =====
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
   #define XPLRLBAND_DEBUG_ACTIVE             (1U)
   ...
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   #define XPLRMQTTWIFI_DEBUG_ACTIVE          (1U)
   ```
4. Open the [app](./main/hpg_wifi_mqtt_correction_certs.c) and select if you need logging to the SD card for your application.
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
   #define XPLRLBAND_LOG_ACTIVE               (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   ...
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRMQTTWIFI_LOG_ACTIVE            (1U)
   ...

   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_wifi_mqtt_correction_certs.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected.
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Correction Data Source -> Choose correction data source for your GNSS module` and select the **Correction data source** you need for your GNSS module.
11. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed. Remember to put **`"mqtts://"`** in front.\
    You can also skip this step since the correct **hostname** is already configured.
12. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
13. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
14. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
15. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
16. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem**, into **client.key** located inside the main folder of the project. Replace everything inside the file.
17. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt**, into **client.crt** located inside the main folder of the project. Replace everything inside the file.
18. **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then copy and Paste the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
19. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_mqtt_correction_certs` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 18 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
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
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_CORRECTION_DATA_SOURCE`** | "Correction via IP" | **[hpg_hpg_wifi_mqtt_correction_certs](./main/hpg_wifi_mqtt_correction_certs.c)** | Selects the source of the correction data to be forwarded to the GNSS module. |
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
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB `** | Helper definition to denote a size of 1 KByte
**`APP_MQTT_PAYLOAD_BUF_SIZE `** | Definition of MQTT buffer size of 10 KBytes.
**`APP_LOCATION_PRINT_PERIOD `** | Period in seconds on how often we want our print location function \[**`appPrintLocation(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_DEAD_RECKONING_PRINT_PERIOD `** | Period in seconds on how often we want our print dead reckoning data function \[**`appPrintDeadReckoning(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_GNSS_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_LBAND_I2C_ADDR`** | I2C address for **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**  module.
**`APP_THINGSTREAM_REGION`** | Correction data region.
**`APP_THINGSTREAM_PLAN`** | Thingstream subscription plan.
**`APP_MAX_TOPICLEN`** | Maximum topic buffer size.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_RESTART_ON_ERROR`** | Trigger soft reset if device in error state.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
**`APP_ENABLE_CORR_MSG_WDG`** | Option to enable the correction message watchdog mechanism.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND service manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
