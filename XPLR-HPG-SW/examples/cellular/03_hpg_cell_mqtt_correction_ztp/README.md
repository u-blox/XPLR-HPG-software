![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction Data over MQTT from Thingstream PointPerfect Service (ZTP)

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module. In this implementation the [Zero Touch Provisioning](https://developer.thingstream.io/guides/location-services/pointperfect-zero-touch-provisioning) feature is demonstrated for accessing and registering to the PointPerfect services of the platform. The application depends on provided libraries for communicating with **[Thingstream](https://developer.thingstream.io/home)**, retrieving required certificates for PointPerfect service and subscribe to available PointPerfect topics.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. Communication with Thingstream services is provided by [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular module. Please make sure that you have a data plan activated on the inserted SIM card.

**[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to location related service topics.<br>
Using the ZTP feature of the platform the application requires minimum configuration by the user but requires more memory.
   ```
   [+] minimum user configuration
   [+] automated retrieval of required certificates
   [+] subscription to all available topics
   [-] memory footprint
   ```

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and adequate GNSS signal, diagnostic messages are printed similar to the ones below:

**Dead Reckoning disabled**
```
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
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure cellular and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_mqtt_correction_ztp` project, making sure that all other projects are commented out:
   ```
   ...
   # Cellular examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_register")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs")
   set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_ztp")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED   (1U)
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   #define XPLRCOM_DEBUG_ACTIVE               (1U)
   #define XPLRCELL_MQTT_DEBUG_ACTIVE         (1U)
   #define XPLRCELL_HTTP_DEBUG_ACTIVE         (1U)
   #define XPLRTHINGSTREAM_DEBUG_ACTIVE       (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRCELL_MQTT_DEBUG_ACTIVE         (1U)
   ...
   ```
4. Open the [app](./main/hpg_cell_mqtt_correction_ztp.c) and select if you need logging to the SD card for your application.
   ```
   #define APP_SD_LOGGING_ENABLED      0U
   ```
   This is a general option that enables or disables the SD logging functionality for all the modules. <br> 
   To enable/disable the individual module debug message SD logging:
   - Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**.
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED                         1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                            (1U)
   ...
   #define XPLRCOM_LOG_ACTIVE                             (1U)
   #define XPLRCELL_HTTP_LOG_ACTIVE                       (1U)
   #define XPLRCELL_MQTT_LOG_ACTIVE                       (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE                        (1U)
   ...
   #define XPLR_THINGSTREAM_LOG_ACTIVE                    (1U)
   ...

   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_cell_mqtt_correction_ztp.c.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Correction Data Source -> Choose correction data source for your GNSS module` and select the **Correction data source** you need for your GNSS module.
11. Go to `XPLR HPG Options -> Cellular Settings -> APN value of cellular provider` and input the **APN** of your cellular provider.
12. Using [this](./../../../docs/README_thingstream_ztp.md) guide, get your **device token** from **Thingstream**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream token` and configure it as needed.
13. Change variable `ppRegion` defined in [app cell ztp](./../03_hpg_cell_mqtt_correction_ztp/main/hpg_cell_mqtt_correction_ztp.c) according to your **[Thingstream](https://developer.thingstream.io/home)** service location. Possible values are:
    - **XPLR_THINGSTREAM_PP_REGION_EU**
    - **XPLR_THINGSTREAM_PP_REGION_US**
    - **XPLR_THINGSTREAM_PP_REGION_KR**
    - **XPLR_THINGSTREAM_PP_REGION_AU**
    - **XPLR_THINGSTREAM_PP_REGION_JP**

14. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_mqtt_correction_ztp` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 12 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
6. Run `idf.py build` to compile the project.
7. Run `idf.py -p COMX flash` to flash the binary to the board, where **COMX** is the `COM Port` that the XPLR-HPGx has enumerated on.
8. Run `idf.py monitor -p COMX` to monitor the debug UART output.

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_Kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_Kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_Kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_GNSS_MODULE`** | "ZED-F9R" | **[boards](./../../../components/boards)** | Selects the GNSS module equipped. |
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_CELL_APN`** | "internet" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | APN value of cellular provider to register. | You will have to replace this value with your specific APN of your cellular provider, either by directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_CORRECTION_DATA_SOURCE`** | "Correction via IP" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | Selects the source of the correction data to be forwarded to the GNSS module. |
**`CONFIG_XPLR_AWS_ROOTCA_URL`** | "www.amazontrust.com:443" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | The webserver address and port to connect to in order to retrieve the AWS root ca.
**`CONFIG_XPLR_AWS_ROOTCA_PATH`** | "/repository/AmazonRootCA1.pem" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | Actual path to the certificate.
**`CONFIG_XPLR_TS_PP_ZTP_TOKEN`** | "ztp-token" | **[hpg_cell_mqtt_correction_ztp](./main/hpg_cell_mqtt_correction_ztp.c)** | A device ztp token taken from **Thingstream** platform. | You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED`** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED`** | Switches logging of the application messages to the SD card ON or OFF.
**`APP_MAX_RETRIES_ON_ERROR`** | Frequency, in seconds, of how often we want our network statistics data to be printed in the console. Can be changed as desired.
**`APP_STATISTICS_INTERVAL`** | Frequency, in seconds, of how often we want our network statistics data to be printed in the console. Can be changed as desired.
**`APP_GNSS_LOC_INTERVAL`** | Frequency, in seconds, of how often we want our print location function \[**`appPrintLocation(void)`**\] to execute. Can be changed as desired.
**`APP_GNSS_DR_INTERVAL`** | Frequency, in seconds, of how often we want our print dead reckoning data function \[**`gnssDeadReckoningPrint(void)`**\] to execute. Can be changed as desired.
**`APP_RUN_TIME`** | Period, in seconds, of how how long the application will run before exiting. Can be changed as desired.
**`APP_MQTT_BUFFER_SIZE_LARGE`** | Size of large MQTT buffers used. These buffers must be at least 10 KBytes long.
**`APP_MQTT_BUFFER_SIZE_SMALL`** | Size of normal MQTT buffers used. These buffers must be at least 2 KBytes long.
**`APP_HTTP_BUFFER_SIZE`** | Size of HTTP buffer used. Buffer must be at least 6 KBytes long.
**`APP_CERTIFICATE_BUFFER_SIZE`** | Size of each certificate buffer used. These buffers must be at least 2 KBytes long.
**`APP_GNSS_I2C_ADDR`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_RESTART_ON_ERROR`** | Option to select if the board should perform a reboot in case of error.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
**`APP_ENABLE_CORR_MSG_WDG`** | Option to enable the correction message watchdog mechanism.
<br>

## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/com_service](./../../../components/hpglib/src/com_service/)** | XPLR communication service for cellular module.
**[hpglib/httpClient_service](./../../../components/hpglib/src/httpClient_service/)** | XPLR HTTP client for cellular module.
**[hpglib/mqttClient_service](./../../../components/hpglib/src/mqttClient_service/)** | XPLR MQTT client for cellular module.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND service manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/ztp_service](./../../../components/hpglib/src/ztp_service/)** | Performs Zero Touch Provisioning POST and gets necessary data for MQTT.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>