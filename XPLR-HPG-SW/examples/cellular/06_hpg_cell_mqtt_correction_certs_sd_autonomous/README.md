![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction Data over MQTT from Thingstream PointPerfect Service Using configuration and credentials from SD card

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module. In this implementation all certificates required are entered automatically from a configuration file stored in the SD card (for instructions regarding how to obtain this file follow **[this guide](./../../../docs/README_thingstream_ucenter_config.md)**). This example also showcases the functionality of the HPG kit to be configured via a json configuration file in the SD card, removing the need for different builds and compiles in order to change the board's configuration. Finally, another unique functionality of this example is the format of the logging filenames, based on the current timestamp and the update of the filenames in intervals (of size or time).

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. Communication with Thingstream services is provided by [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular module. Please make sure that you have a data plan activated on the inserted SIM card.

**[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to location related service topics.<br>

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and adequate GNSS signal, diagnostic messages are printed similar to the ones below:

**Dead Reckoning disabled**
```
======== Location Info ========
Location type: 1
Location fix type: RTK-FIXED
Location latitude: 38.048038 (raw: 380480381)
Location longitude: 23.809122 (raw: 238091217)
Location altitude: 233.218000 (m) | 233218 (mm)
Location radius: 0.017000 (m) | 17 (mm)
Speed: 0.003600 (km/h) | 0.001000 (m/s) | 1 (mm/s)
Estimated horizontal accuracy: 0.0172 (m) | 17.20 (mm)
Estimated vertical accuracy: 0.0239 (m) | 23.90 (mm)
Satellite number: 32
Time UTC: 10:24:47
Date UTC: 22.11.2023
Calendar Time UTC: Wed 22.11.2023 10:24:47
===============================
```
**Dead Reckoning enabled and printing flag PrintIMUData is set in the configuration file**
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

**NOTE:** Vehicle dynamics will only be printed if the module has been calibrated.
<br>

## Build instructions
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure cellular and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_mqtt_correction_certs_sd_autonomous` project, making sure that all other projects are commented out:
   ```
   ...
   # cellular examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_register")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_ntrip_correction")
   set(ENV{XPLR_HPG_PROJECT}  "hpg_cell_mqtt_correction_certs_sd_autonomous")
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
   #define XPLRTHINGSTREAM_DEBUG_ACTIVE       (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRCELL_MQTT_DEBUG_ACTIVE         (1U)
   ...
   ```
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and make sure all logging options are **set**. 
   This is required for this example to be able to compile successfully.
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED    1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                            (1U)
   #define XPLRLBAND_LOG_ACTIVE                           (1U)
   #define XPLRCOM_LOG_ACTIVE                             (1U)
   #define XPLRCELL_HTTP_LOG_ACTIVE                       (1U)
   #define XPLRCELL_MQTT_LOG_ACTIVE                       (1U)
   #define XPLRLOCATION_LOG_ACTIVE                        (1U)
   #define XPLRNVS_LOG_ACTIVE                             (1U)
   #define XPLR_THINGSTREAM_LOG_ACTIVE                    (1U)
   #define XPLRWIFISTARTER_LOG_ACTIVE                     (1U)
   #define XPLRWIFIWEBSERVER_LOG_ACTIVE                   (1U)
   #define XPLRMQTTWIFI_LOG_ACTIVE                        (1U)
   #define XPLRZTP_LOG_ACTIVE                             (1U)
   #define XPLRWIFI_NTRIP_LOG_ACTIVE                      (1U)
   #define XPLRCELL_NTRIP_LOG_ACTIVE                      (1U)
   #define XPLRBLUETOOTH_LOG_ACTIVE                       (1U)
   #define XPLRATSERVER_LOG_ACTIVE                        (1U)
   #define XPLRATPARSER_LOG_ACTIVE                        (1U)
   ...
   ```
   If you wish to disable any module's logging functionality, you need to select it in the relevant option of the `xplr_config.json` configuration file (see step 9).
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Download (following **[this guide](./../../../docs/README_thingstream_ucenter_config.md)**) your u-center configuration file and copy it into the SD card.
9. Create your `xplr_config.json` configuration file following **[this guide](./../../../docs/README_xplr_config_file.md)** and copy it into the SD card.
10. Insert the SD card on the board slot.
11. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.

<br>

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_GNSS_MODULE`** | "ZED-F9R" | **[boards](./../../../components/boards)** | Selects the GNSS module equipped. |
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**`APP_GNSS_I2C_ADDR`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_MQTT_BUFFER_SIZE`** | Size of each MQTT buffer used. The buffer must be at least this big (for this sample code) to fit the whole correction data.
**`APP_DEVICE_OFF_MODE_BTN`** | Button for shutting down the device. Changing this macro may interfere with the correct operation of the shutdown mechanism implemented in the app.
**`APP_DEVICE_OFF_MODE_TRIGGER`** | The press duration of `APP_DEVICE_OFF_MODE_BTN` that will trigger the shutdown mechanism.
**`APP_JSON_PAYLOAD_BUF_SIZE`** | Size of the buffer that stores the data (configuration/credentials) from the SD card. Should be at least this big (for this sample code) to fit the whole configuration file data.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
<br>


## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/com_service](./../../../components/hpglib/src/com_service/)** | XPLR communication service for cellular module.
**[hpglib/mqttClient_service](./../../../components/hpglib/src/mqttClient_service/)** | XPLR MQTT client for cellular module.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND module device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>