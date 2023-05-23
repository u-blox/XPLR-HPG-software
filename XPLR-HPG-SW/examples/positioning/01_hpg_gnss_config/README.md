![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# ZED-F9R GNSS & NEO-D9S LBAND module configuration

## Description

The example provided demonstrates how to configure a GNSS or LBAND module.\
All needed functions are provided; a default setup considering current hardware setup function is available to easily initialize your modules.\
It is recommended to the user to read the manuals of the modules which provide useful information for their setup and use.\
The manuals, along with other resources, can be found here:
- **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module?legacy=Current#Documentation-&-resources)**
- **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**

**NOTE**: In the current implementation the GNSS module does **NOT** support **Dead Reckoning** since the internal accelerometer needs to go through a calibration procedure before being able to use that feature. This will be fixed in future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. After successful device initialization a set of diagnostics are printed similar to the ones below:

```
...
I [(456) xplrCommonHelpers|xplrHelpersUbxlibInit|90|: ubxlib init ok!
I [(456) app|appInitAll|300|: Waiting for devices to come online!
...
I [(19634) app|app_main|145|: All inits OK!
I [(19872) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|310|: Got device info.
========= Device Info =========
Lband version: EXT CORE 1.00 (a59682)
Hardware: 00190000
Rom: 0x118B2060
Firmware: HPS 1.30
Protocol: 33.30
Model: ZED-F9R
ID: 6342620b00
-------------------------------
I2C Port: 0
I2C Address: 0x42
I2C SDA pin: 21
I2C SCL pin: 22
===============================
I [(20038) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|310|: Got device info.
========= Device Info =========
Lband version: EXT CORE 1.00 (cd6322)
Hardware: 00190000
Rom: 0x118B2060
Firmware: PMP 1.04
Protocol: 24.00
Model: NEO-D9S
ID: f340b2f700
-------------------------------
I2C Port: 0
I2C Address: 0x43
I2C SDA pin: 21
I2C SCL pin: 22
===============================
I [(20061) app|app_main|151|: All infos OK!
I [(20183) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|238|: Set configuration value.
I [(20235) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValSet|254|: Set multiple configuration values.
I [(20360) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValGet|270|: Got configuration value.
I [(20360) app|app_main|203|: Read one value -- U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val 1
I [(20490) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|238|: Set configuration value.
I [(20614) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValGet|270|: Got configuration value.
I [(20614) app|app_main|220|: Read one value -- Default U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val 0
I [(20736) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValGet|270|: Got configuration value.
I [(20736) app|app_main|234|: Read one value -- U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2: val 0
I [(20955) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValGet|286|: Got multiple configuration values.
I [(20955) app|app_main|253|: KEY: U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L | val: 0
I [(20962) app|app_main|254|: KEY: U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1 | val: 1
...
I [(21108) xplrCommonHelpers|xplrHlprLocSrvcDeviceClose|120|: ubxlib device closed!
I [(21157) xplrCommonHelpers|xplrHlprLocSrvcUbxlibDeinit|149|: ubxlib deinit ok!
I [(21157) app|app_main|266|: All devices stopped!
I [(21159) app|app_main|268|: ALL DONE
...
```
<br>

## Build instructions
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_gnss_config` project, making sure that all other projects are commented out:
   ```
   ...
   # positioning examples
   set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_config")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_lband_correction")
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
   ```
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.<br>
All definitions/macros below are meant to make variables more identifiable.<br>
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED 1U`** | Switches debug printing messages ON or OFF
**`XPLR_GNSS_I2C_ADDR  0x42`** | I2C address for ZED-F9R GNSS module.
**`XPLR_LBAND_I2C_ADDR 0x43`** | I2C address for NEO-D9S module.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**
