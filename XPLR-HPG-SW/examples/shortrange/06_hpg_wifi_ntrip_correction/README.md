![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# NTRIP Correction Data over WiFi

## Description

The example provided demonstrates how to connect to an NTRIP caster in order to get correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module. The application depends on provided libraries for communicating with the NTRIP caster of your choice, accessing the correct mountpoint and retrieving RTCM correction data. To test this example you must have access to RTK correction service, the necessary credentials to connect to the NTRIP caster (username & password) and a mountpoint  located within a ~50km radius.

**NOTE**: If you currently don't have access to a NTRIP caster, you can use a free one like **[RTK2GO](http://rtk2go.com)**.

<br>

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

Depending on the interval on which the NTRIP caster sends correction data and your connection you may need to change `XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE` and `XPLRWIFI_NTRIP_MSG_RECEIVE_DATA_SIZE` in **[xplr_ntrip_client.c](../../../components/hpglib/src/ntripWifiClient_service/xplr_wifi_ntrip_client.c)**. 

- `XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE` sets the total size of correction to receive on each call of the `xplrWifiNtripTask` function (can be multiple RTCM messages).
- `XPLRWIFI_NTRIP_MSG_RECEIVE_DATA_SIZE` sets the max size of a single RTCM message.

Also depending on the requirements of your NTRIP caster, you can change the interval on which the GGA message is sent by changing `XPLRWIFI_NTRIP_GGA_INTERVAL_S` in **[xplr_ntrip_client.c](../../../components/hpglib/src/ntripWifiClient_service/xplr_wifi_ntrip_client.c)**

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon successful connection the NTRIP client and adequate GNSS signal, diagnostic messages are printed similar to the ones below:

**Dead Reckoning disabled**
```
======== Location Info ========
Location type: 1
Location fix type: 3D\DGNSS\RTK-FLOAT
Location latitude: 38.048181 (raw: 380481806)
Location longitude: 23.809044 (raw: 238090444)
Location altitude: 227.760000 (m) | 227760 (mm)
Location radius: 0.242000 (m) | 242 (mm)
Speed: 0.082800 (km/h) | 0.023000 (m/s) | 23 (mm/s)
Estimated horizontal accuracy: 0.2417 (m) | 241.70 (mm)
Estimated vertical accuracy: 0.2298 (m) | 229.80 (mm)
Satellite number: 28
Time UTC: 09:57:35
Date UTC: 06.06.2023
Calendar Time UTC: Tue 06.06.2023 09:57:35
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
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure cellular and NTRIP settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_ntrip_correction` project, making sure that all other projects are commented out:
   ```
   ...
    #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
    #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
    #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
    #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
    set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_ntrip_correction")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED   (1U)
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRWIFI_NTRIP_DEBUG_ACTIVE        (1U)
   ...
   ```
4. Open the [app](./main/hpg_wifi_ntrip_correction.c) and select if you need logging to the SD card for your application.
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
   ...
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRWIFI_NTRIP_DEBUG_ACTIVE        (1U)
   ...

   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_wifi_ntrip_correction.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected.
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Wi-Fi Settings` and input the **SSID** and **password** of your access-point.
11. Go to `XPLR HPG Options -> NTRIP Client settings` and configure as needed.
12. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_ntrip_correction` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 10 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
6. Run `idf.py build` to compile the project.
7. Run `idf.py -p COMX flash` to flash the binary to the board, where **COMX** is the `COM Port` that the XPLR-HPGx has enumerated on.
8. Run `idf.py monitor -p COMX` to monitor the debug UART output.

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most case these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building the code.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used to make building easier and change certain variables to make app configuration easier, without the need of doing any modifications to the code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | WiFi access-point SSID
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | WiFi access-point password
**`CONFIG_XPLR_NTRIP_HOST`** | "host" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | NTRIP caster URL/IP address
**`CONFIG_XPLR_NTRIP_PORT`** | "2101" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | NTRIP caster port
**`CONFIG_XPLR_NTRIP_MOUNTPOINT`** | "mountpoint" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | NTRIP caster mountpoint
**`CONFIG_XPLR_NTRIP_USERAGENT`** | "NTRIP_XPLR_HPG" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | User-Agent
**`CONFIG_XPLR_NTRIP_USE_AUTH`** | "True" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | Use authentication
**`CONFIG_XPLR_NTRIP_USERNAME`** | "username" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | Username
**`CONFIG_XPLR_NTRIP_PASSWORD`** | "password" | **[hpg_wifi_ntrip_correction](./main/hpg_wifi_ntrip_correction.c)** | Password
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
**`APP_LOCATION_PRINT_PERIOD`** | Interval, in seconds, of how often we want the location to be printed.
**`APP_DEAD_RECKONING_PRINT_PERIOD`** | Interval, in seconds, of how often we want the location to be printed.
**`APP_TIMEOUT`** | Duration, in seconds, for which we want the application to run.
**`APP_GNSS_I2C_ADDR`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_RESTART_ON_ERROR`** | Trigger soft reset if device in error state.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
<br>

## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/ntripWifiClient_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR NTRIP WiFi client.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>