![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Cellular network registration

## Description

The example provided demonstrates how to configure the [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular module and register to a network provider.<br>
In order to use the code a SIM card with an activated data plan must be present in the SIM slot of C214 board or, in case you are using the C213 board, in the slot available under the [LARA-R6 clickboard](https://www.mikroe.com/4g-lte-e-click). Please make sure that a battery is also connected to the board to avoid any power issues.

Before compiling and flashing the code to the MCU the APN of your network provider has to be configured using the kConfig menu under the section `XPLR HPG Options -> Cellular Options -> APN value of cellular provider`.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon network registration a set of diagnostics are printed similar to the ones below:
```
D [(11358) hpgCom|xplrComCellFsmConnect|259|: dvc interface up, switching to connected
D [(12230) hpgCom|cellDvcGetNetworkInfo|778|: cell network settings:
D [(12230) hpgCom|cellDvcGetNetworkInfo|779|: operator: vodafone GR vodafone GR
D [(12232) hpgCom|cellDvcGetNetworkInfo|780|: ip: ************
D [(12238) hpgCom|cellDvcGetNetworkInfo|781|: registered: 1
D [(12244) hpgCom|cellDvcGetNetworkInfo|782|: RAT: LTE
D [(12249) hpgCom|cellDvcGetNetworkInfo|783|: status: registered home
D [(12256) hpgCom|cellDvcGetNetworkInfo|784|: Mcc: 202
D [(12261) hpgCom|cellDvcGetNetworkInfo|785|: Mnc: 5
```
<br>

## Build instructions
Building the cellular network registration example requires to edit a minimum set of files in order to select the corresponding source files and configure the APN value of your network provider using kConfig.<br>
Please follow the steps described bellow:<br>
1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_register` project, making sure that all other projects are commented out:
   ```
   ...
   # Cellular examples
   set(ENV{XPLR_HPG_PROJECT} "hpg_cell_register")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_ztp")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    1U
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE                        (1U)
   #define XPLRCOM_DEBUG_ACTIVE                           (1U)
   ...
   ```
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdkconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Go to the `XPLR HPG Options -> Cellular Settings -> APN value of cellular provider` section and type in the APN value of your network provider.
8. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>
<br>

## KConfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[KConfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[KConfig](./../../../docs/README_kconfig.md)** before building.\
**[KConfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" |  **[boards](./../../../components/boards)** | Board variant to build firmware for .|
**`CONFIG_XPLR_CELL_APN`** | "internet" |  **[hpg_cell_register](./../01_hpg_cell_register/main/hpg_cell_register.c)** | APN value of the cellular provider in use. | You can replace this value freely in the app.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the application.<br>
All definitions/macros below are meant to make variables more identifiable.<br>
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**```APP_SERIAL_DEBUG_ENABLED 1U```** | Switches app specific debug printing messages ON or OFF
<br>

## Modules-Components used
The cellular network registration example makes use of the following components:

Name | Description
--- | ---
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/communication service](./../../../components/hpglib/src/com_service)** | cellular communication library
<br>