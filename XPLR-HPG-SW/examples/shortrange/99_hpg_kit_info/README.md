![u-blox](./../../../media/shared/logos/ublox_logo.jpg)
<br>
<br>

# HPG-Kit Info

## Description
This project provides some diagnostics info of the available modules on the kit.<br>
The example initializes the XPLR-HPG kit using the corresponding [board file](./../../../components/boards/) component and prints module specific info to the debug terminal like firmware version of kit's peripherals, MAC address of host MCU etc.<br>
In case there is a problem with the sw available in this repo, the user is advised to run this example and provide a copy of the console output of the project along with the raised issue.
<br>

## Build instructions

### Building using Visual Studio Code
To build the example please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_kit_info` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
   set(ENV{XPLR_HPG_PROJECT} "hpg_kit_info")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED   (1U)
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   #define XPLRLBAND_DEBUG_ACTIVE             (1U)
   ...
   ```
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_kit_info` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the `Board Options` section and select the board you wish to build the example for. When finished press `q` and answer `Y` to save the configuration.
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

## Modules-Components used

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/communication service](./../../../components/hpglib/src/com_service)** | cellular communication library
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**
<br>

## Flashing Binaries
A set of binaries files are provided to ease the programming of the kits without having to build the example from sources. To do so, just follow the steps bellow:
- Open a cmd line/terminal window.
- Navigate to `xplr-hpg-sw\XPLR-HPG-SW` folder.
- Run the following command, depending on your xplr-hpg-kit variant:
   - For C213:<br>
      ```
      misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 bin\releases\C213-hpg_kit_info.bin
      ```
   - For C214:<br>
      ```
      misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x0 bin\releases\C214-hpg_kit_info.bin
      ```
   - For MAZGCH:<br>
      ```
      misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x0 bin\releases\MAZGCH-hpg_kit_info.bin
      ```
### Notes
- To avoid any configuration conflicts, it is **highly recommended** to erase the chip's memory before flashing a new binary using the following command:<br>
   `misc\esptool.exe -p [COM_PORT] erase_flash`
- If you experience serial port related communication problems, you may need to install the [CP210x USB Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=overview)
- In `[COM_PORT]` field of flashing commands enter the serial com port that the xplr-hpg-kit is connected to eg. `COM3`
