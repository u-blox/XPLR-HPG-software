![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# ZED-F9R GNSS & NEO-D9S LBAND module configuration

## Description

The example provided demonstrates how to configure a GNSS or LBAND module.\
All needed functions to configure and initialize location modules are shown.\
It is recommended to the user to read the manuals of the modules which provide useful information for their setup and use.\
The manuals, along with other resources, can be found here:
- **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module?legacy=Current#Documentation-&-resources)**
- **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user.\
After successful device initialization a set of diagnostics are printed similar to the ones below:

```
...
D [(587) xplrCommonHelpers|xplrHelpersUbxlibInit|131|: ubxlib init ok!
...
I [(2802) app|app_main|194|: All inits OK!
I [(3052) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
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
I [(3082) app|app_main|200|: All infos OK!
D [(3086) xplrGnss|xplrGnssFsm|625|: Logging is enabled. Trying to initialize.
I [(3120) xplrGnss|xplrGnssAsyncLogInit|2016|: Initializing async logging
D [(3146) xplrGnss|xplrGnssFsm|628|: Sucessfully initialized GNSS logging.
...
I [(5472) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
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
...
D [(10254) xplrGnss|xplrGnssFsm|665|: Restart Completed.
...
D [(10810) xplrGnss|gnssNvsInit|3285|: NVS namespace <gnssDvc_0> for GNSS, init ok
D [(10822) xplrGnss|xplrGnssFsm|782|: Initialized NVS.
D [(10833) xplrLband|xplrLbandSetDestGnssHandler|260|: Successfully set GNSS device handler.
D [(10861) xplrLband|xplrLbandSetDestGnssHandler|261|: Stored GNSS device handler in config.
D [(10989) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
D [(11001) xplrLband|xplrLbandSetFrequency|362|: Stored frequency into LBAND config!
I [(11152) app|app_main|255|: Stored frequency: 1545260000 Hz
D [(11209) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValSet|319|: Set multiple configuration values.
I [(11356) app|app_main|302|: Read one value -- U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val 1
D [(11475) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
I [(11675) app|app_main|329|: Read one value -- Default U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L: val 0
I [(11792) app|app_main|347|: Read one value -- U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2: val 0
D [(11917) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValGet|376|: Got multiple configuration values.
I [(11944) app|app_main|371|: KEY: U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L | val: 0
I [(11944) app|app_main|374|: KEY: U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1 | val: 1
I [(11950) xplrLband|xplrLbandSendCorrectionDataAsyncStop|508|: Trying to stop LBAND Send Correction Data async.
I [(12010) xplrLband|xplrLbandSendCorrectionDataAsyncStop|510|: Looks like Correction data async sender is not running. Nothing to do.
D [(12038) xplrCommonHelpers|xplrHlprLocSrvcDeviceClose|249|: ubxlib device closed!
D [(12050) xplrLband|xplrLbandStopDevice|218|: Successfully stoped LBAND module!
D [(12165) xplrCommonHelpers|xplrHlprLocSrvcUbxlibDeinit|274|: ubxlib deinit ok!
I [(12190) app|app_main|386|: All devices stopped!
I [(12190) app|app_main|388|: ALL DONE
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
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED             1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   ...
   
   ```
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
9. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../../docs/README_kconfig.md)** before building.\
**[Kconfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_gnss_lband_correction](./main/hpg_gnss_lband_correction.c)** | Enables or Disables Dead Reckoning functionality. |

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.<br>
All definitions/macros below are meant to make variables more identifiable.<br>
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED`** | Switches debug printing messages ON or OFF
**`APP_SD_LOGGING_ENABLED `** | Switches logging of the application messages to the SD card ON or OFF.
**`APP_LBAND_FREQUENCY_EU `** | Sample frequency for EU region
**`APP_LBAND_FREQUENCY_US `** | Sample frequency for US region
**`APP_GNSS_I2C_ADDR  `** | I2C address for ZED-F9R GNSS module.
**`APP_LBAND_I2C_ADDR `** | I2C address for NEO-D9S module.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>
