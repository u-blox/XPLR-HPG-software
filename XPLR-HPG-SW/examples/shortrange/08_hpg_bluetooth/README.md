![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Bluetooth echo server

## Description

Thi example is used to demonstrate how to use the HPGLib Bluetooth classic/BLE virtual COM port client.
The module provides `xplrBluetoothRead` and `xplrBluetoothWrite` functions that allow you to read from and write to a connected device using its handle.
This example implements a simple echo server, whatever message is received by any connected device is send back to the same device.
This example was tested using the **[Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal&pcampaignid=web_share)** app on Android.  No special setup is needed, just pair your XPLR-HPG board to your mobile device and connect through the Serial Bluetooth Terminal mobile app to exchange data through the virtual COM port between your XPLR-HPG and your mobile device. Provided your mobile devices supports both BT Classic and BLE, you can use the Serial Bluetooth Terminal app to test both modes of operation. Instructions on how to change modes are provided in the HPGLib Bluetooth client **[README](./../../../components/hpglib/src/bluetooth_service/README.md)**

**NOTE**: BT Classic is only supported in HPG-2 boards (NINA-W1). In HPG-1 (NORA-W1) only BLE is available. 
<br>

## Build instructions

### Building using Visual Studio Code
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure cellular and NTRIP settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_bluetooth` project, making sure that all other projects are commented out:
   ```
   ...
    #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
    .
    .
    .
    set(ENV{XPLR_HPG_PROJECT} "hpg_bluetooth")
    #set(ENV{XPLR_HPG_PROJECT} "hpg_kit_info")
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
   #define XPLRBLUETOOTH_DEBUG_ACTIVE          (1U)
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
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRBLUETOOTH_LOG_ACTIVE           (1U)
   ...

   ```
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. By default the Bluetooth functionality is diasbled to conserve memory, refer to the **[Bluetooth module README](../../../components/hpglib/src/bluetooth_service/README.md)** for instructions on how to enable BLE / Bluetooth Classic.
7. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
8. Navigate to the `Board Options` section and select the board you wish to build the example for.
9. Go to `XPLR HPG Options -> Bluetooth Settings` and input the **Device name** (this is the name your XPLR-HPG kit will have as a Bluetooth device).

**NOTE**: The default settings allow 2 connected devices for Bluetooth Classic and 3 connected devices for BLE.

10. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_bluetooth` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 9 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
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
**`CONFIG_XPLR_BLUETOOTH_DEVICE_NAME`** | "CONFIG_XPLR_BLUETOOTH_DEVICE_NAME" | **[hpg_wifi_mqtt_correction_certs_sw_maps](./main/hpg_wifi_mqtt_correction_certs_sw_maps.c)** | Bluetooth device name|

XPLR_BLUETOOTH_DEVICE_NAME

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`APP_TIMEOUT`** | Duration, in seconds, for which we want the application to run.
**`DEVICES_PRINT_INTERVAL`** | Interval, in seconds, of how often we want the connected device diagnostics to be printed.
**`APP_BT_BUFFER_SIZE`** | The size of the allocated Bluetooth buffer in bytes.
**`APP_BT_MAX_MESSAGE_SIZE`** | The max size of any Bluetooth message in bytes.
<br>

## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/bluetooth_service](./../../../components/hpglib/src/bluetooth_service/)** | XPLR Bluetooth client.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>