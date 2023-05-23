![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction Data over MQTT from Thingstream Point Perfect Service (manual)

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module. In this implementation all certificates required are entered manually by the user. The advantage of this implementation is that it uses the least amount of memory to subscribe to Thingstream's point perfect correction data topic, and then forward that data to the GNSS module. Zero touch provisioning alternative is presented in [ztp point perfect](./../03_hpg_cell_mqtt_correction_ztp/main/hpg_cell_mqtt_correction_ztp.c) example but the service is not supported using the Thingstream redemption code that is provided with the XPLR-HPG2 kit.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. Communication with Thingstream services is provided by [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular module. Please make sure that you have a data plan activated on the inserted SIM card.

**[Point Perfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to location related service topics.<br>
As mentioned before this example requires the related certificates to be given by the user hence it can be used with the redemption code found in the kit.<br>
We recommend using some of the libraries provided as a starting point to Evaluate Point Perfect
   ```
   [+] small footprint
   [-] certificate files provided by the user
   [-] MQTT subscription topics provided by the user
   ```

**NOTE**: In the current implementation GNSS module does **NOT** support **Dead Reckoning** since the internal accelerometer needs to be calibrated before being able to use that feature. Calibration demo process is schedule for the next software release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and adequate GNSS signal, diagnostic messages are printed similar to the ones below:

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
<br>

## Build instructions
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure cellular and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_mqtt_correction_certs` project, making sure that all other projects are commented out:
   ```
   ...
   # Cellular examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_register")
   set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_ztp")
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
   #define XPLRTHINGSTREAM_DEBUG_ACTIVE      (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRCELL_MQTT_DEBUG_ACTIVE         (1U)
   ...
   ```
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
8. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem** into the variable **keyPP**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
9. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt.csv** into the variable **certPP**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
10. **Copy and Paste** the contents of **Root Certificate** into the variable **rootCa**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
11. Go to `XPLR HPG Options -> Cellular Settings -> APN value of cellular provider` and input the **APN** of your cellular provider.
12. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed.<br>
    You can also skip this step since the correct **hostname** is already configured.
13. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
14. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.

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
**`CONFIG_XPLR_CELL_APN`** | "internet" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | APN value of cellular provider to register. | You will have to replace this value to with your specific APN of your cellular provider, either by directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_MQTTCELL_CLIENT_ID`** | "MQTT Client ID" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using Kconfig.
**`CONFIG_XPLR_MQTTCELL_THINGSTREAM_HOSTNAME`** | "pp.services.u-blox.com:8883" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL changes in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**`APP_STATISTICS_INTERVAL`** | Frequency, in seconds, of how often we want our network statistics data to be printed in the console. Can be changed as desired.
**`APP_GNSS_INTERVAL`** | Frequency, in seconds, of how often we want our print location function [**`appPrintLocation(uint8_t periodSecs)`**] to execute. Can be changed as desired.
**`APP_RUN_TIME`** | Period, in seconds, of how how long the application will run before exiting. Can be changed as desired.
**`APP_MQTT_BUFFER_SIZE`** | Size of each MQTT buffer used. The buffer must be at least this big (for this sample code) to fit the whole correction data which may be up to 6 KBytes long.
**`APP_SERIAL_DEBUG_ENABLED`** | Switches application related debug messages ON or OFF
**`APP_GNSS_I2C_ADDR 0x42`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module
<br>

## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/com_service](./../../../components/hpglib/src/com_service/)** | XPLR communication service for cellular module.
**[hpglib/mqttClient_service](./../../../components/hpglib/src/mqttClient_service/)** | XPLR MQTT client for cellular module.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**
<br>