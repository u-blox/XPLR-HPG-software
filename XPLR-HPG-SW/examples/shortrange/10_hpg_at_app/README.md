![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

## Description
This example utilizes the AT command parser module in order to provide an interface for (re)configuring its services during runtime, without the need of rebooting or reflashing the device. By exposing an AT command set through its UART interface, the XPLR-HPG can be used as a peripheral by receiving configuration options and providing location information to the host device.

## Supported Functionality
Using appropriate configuration it is possible to receive correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module with one of the following options:

* Connection to Thingstream over Wi-fi or cellular network.
* Reception using the **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)** LBAND module with decryption keys and frequency taken from Thingstream over Wi-fi or cellular network.
* Connection to an NTRIP caster over Wi-fi or Cellular Network.

## Build instructions
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_at_app` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs_sw_maps")
   set(ENV{XPLR_HPG_PROJECT} "hpg_at_app")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART. When using an application expecting AT response messages, it is recommended to disable any other serial debug messages as they utilize the same UART interface.
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
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
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
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
7. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it.
8. Navigate to the `Board Options` section and select the board you wish to build the example for.

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_at_app` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig` by clicking on the "cog" symbol present in the vs code status bar.
5. Navigate to the `Board Options` section and select the board you wish to build the example for. When finished press `q` and answer `Y` to save the configuration.
6. Run `idf.py build` to compile the project.
7. Run `idf.py -p COMX flash` to flash the binary to the board, where **COMX** is the `COM Port` that the XPLR-HPGx has enumerated on.
8. Run `idf.py monitor -p COMX` to monitor the debug UART output.

### Flashing pre-built binaries
Firmware binaries with serial console messages disabled are provided in the [release folder](./../../../bin/releases/). You can flash the binary using the esptool.exe utility located in the [misc](./../../../misc/) folder, with the following commands, where **[COM_PORT]** is the `COM Port` that the XPLR-HPGx has enumerated on:

- Open a cmd line/terminal window.
- Navigate to `xplr-hpg-sw\XPLR-HPG-SW` folder.
- Run the following command, depending on your xplr-hpg-kit variant:

   - For XPLR-HPG-1:
   ```
   misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 bin\releases\C213-hpg_at_app.bin

   ```
   - For XPLR-HPG-2
   ```
   misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x0 bin\releases\C214-hpg_at_app.bin

   ```
   - For MAZGCH HPG Solution
   ```
   misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x0 bin\releases\MAZGCH-hpg_at_app.bin

   ```

## Usage instructions
Receiving correction data using one of the options above requires setting up their corresponding configuration using AT commands from the **[AT Parser API](./../../../components/hpglib/src/at_parser_service/README.md)**.

### Thingstream connection over Wi-fi
1. Configure your **Access Point's Credentials** as needed using the AT+WIFI command.
```
AT+WIFI=<ssid>,<password>
```
2. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)** and copy the MQTT broker hostname, port and client ID:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
3. Configure the thingstream broker and broker's port:
```
AT+TSBROKER=pp.services.u-blox.com,8883
```
4. Set the thingstream client ID, root certificate, client certificate and client key using the following commands (Remove the header, footer and new line characters from the key and certificates beforehand): 
```
AT+TSID=<Client ID>
AT+ROOT=<ROOT Certificate>
AT+TSCERT=<client certificate>
AT+TSKEY=<client key>
```
5. Set the thingstream plan and region to the values matching your thingstream's thing details:\
    For the thingstream plan:
   - PointPerfect L-band and IP: **ip+lband**
   - PointPerfect IP: **ip**
   - PointPerfect L-band: **lband**

    For the thingstream region:
   - Europe region: **eu**
   - USA region: **us**
   - Korea region: **kr**
   - Australia region: **au**
   - Japan region: **jp**
   - IP and LBAND topics of any region available: **all**
```
AT+TSPLAN=<TSPLAN>
AT+TSREGION=<REGION>
```
6. Set the correction source to thingstream and the network interface to Wi-fi.
```
AT+IF=wi-fi
AT+CORSRC=ts
```
7. Set the application mode to start.
```
AT+HPGMODE=start
```

### Thingstream connection over cellular network
1. Input the **APN** of your cellular provider using the AT+APN command.
```
AT+APN=<apn>
```
2. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)** and copy the MQTT broker hostname, port and client ID:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
3. Configure the thingstream broker and broker's port:
```
AT+TSBROKER=pp.services.u-blox.com,8883
```
4. Set the thingstream client ID, root certificate, client certificate and client key using the following commands (Remove the header, footer and new line characters from the key and certificates beforehand): 
```
AT+TSID=<Client ID>
AT+ROOT=<ROOT Certificate>
AT+TSCERT=<client certificate>
AT+TSKEY=<client key>
```
5. Set the thingstream plan and region to the values matching your thingstream's thing details:\
    For the thingstream plan:
   - PointPerfect L-band and IP: **ip+lband**
   - PointPerfect IP: **ip**
   - PointPerfect L-band: **lband**

    For the thingstream region:
   - Europe region: **eu**
   - USA region: **us**
   - Korea region: **kr**
   - Australia region: **au**
   - Japan region: **jp**
   - IP and LBAND topics of any region available: **all**
```
AT+TSPLAN=<TSPLAN>
AT+TSREGION=<REGION>
```
6. Set the correction source to thingstream and the network interface to cellular.
```
AT+IF=cell
AT+CORSRC=ts
```
7. Set the application mode to start.
```
AT+HPGMODE=start
```

### NTRIP connection over Wi-fi
1. Configure your **Access Point's Credentials** as needed using the AT+WIFI command:
```
AT+WIFI=<ssid>,<password>
```
2. Input the NTRIP configuration:
   * NTRIP Server URL:
   ```
   AT+NTRIPSRV=<server url>
   ```
   * NTRIP Server Port:
   ```
   AT+NTRIPPORT=<server port>
   ```
   * NTRIP User Agent string:
   ```
   AT+NTRIPUA=<User Agent>
   ```
   * NTRIP Mount Point:
   ```
   AT+NTRIPMP=<mount point>
   ```
   * NTRIP server username and password:
   ```
   AT+NTRIPCREDS=<username>,<password>
   ```
   * Input GGA relaying to server option to **0** or **1**:
   ```
   AT+NTRIPGGA=<option>
   ```
3. Set the correction source to ntrip and the network interface to wi-fi.
```
AT+IF=wi-fi
AT+CORSRC=ntrip
```
4. Set the application mode to start.
```
AT+HPGMODE=start
```

### NTRIP connection over cell
1. Input the **APN** of your cellular provider using the AT+APN command.
```
AT+APN=<apn>
```
2. Input the NTRIP configuration:
   * NTRIP Server URL:
   ```
   AT+NTRIPSRV=<server url>
   ```
   * NTRIP Server Port:
   ```
   AT+NTRIPPORT=<server port>
   ```
   * NTRIP User Agent string:
   ```
   AT+NTRIPUA=<User Agent>
   ```
   * NTRIP Mount Point:
   ```
   AT+NTRIPMP=<mount point>
   ```
   * NTRIP server username and password:
   ```
   AT+NTRIPCREDS=<username>,<password>
   ```
   * Input GGA relaying to server option to **0** or **1**:
   ```
   AT+NTRIPGGA=<option>
   ```
3. Set the correction source to ntrip and the network interface to cellular.
```
AT+IF=cellular
AT+CORSRC=ntrip
```
4. Set the application mode to start.
```
AT+HPGMODE=start
```
## Configuration using the atHPG python script
The device can also be configured using the **[console based UI](./atHpgApp-python/README.md)**.

## Notes

1. The comma `,` character isn't supported in wi-fi credentials, ntrip credentials and thingstream MQTT broker url, as it is used as a delimiter character on their corresponding AT configuration commands.
2. `AT+BRDNFO=?` AT command, used for obtaining information about the device's modules, won't return cellular device information unless the device was used after the last reset.
3. The `AT+STARTONBOOT=<OPTION>` AT command can be used to toggle weither the device will wait for user input before connecting to the MQTT/NTRIP server or attempt to connect with the already stored configuration without user intervention.
4. When automatic NVS saving is disabled with `AT+NVSCONFIG=MANUAL`, changes to the configuration should be saved manually using `AT+NVSCONFIG=SAVE` before starting the application.
5. When using `AT+NVSCONFIG=MANUAL`, the minimum interval between sending successive AT commands is 0.5 seconds. This interval can be configured in the atHPG python script using the variable `sleepInterval` in [AtInterface.py](./atHpgApp-python/AtApi.py) file. The `ERROR:BUSY` message is returned when the AT application can't keep up parsing and executing all received commands.
6. The `AT+LOC=?` command can be used to retrieve updated GNSS location information with a frequency of at least once a second.

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

| Name | Description |
| ---- | ----------- |
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB `** | Helper definition to denote a size of 1 KByte
**`APP_MQTT_PAYLOAD_BUF_SIZE `** | Definition of MQTT buffer size of 10 KBytes.
**`APP_GNSS_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_LBAND_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_MAX_TOPICLEN`** | Maximum topic buffer size.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[hpglib/at_parser_service](./../../../components/hpglib/src/at_parser_service/)** | Module for parsing AT commands.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[hpglib/com_service](./../../../components/hpglib/src/com_service/)** | XPLR communication service for cellular module.
**[hpglib/mqttClient_service](./../../../components/hpglib/src/mqttClient_service/)** | XPLR MQTT client for cellular module.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/ntripWifiClient_service](./../../../components/hpglib/src/ntripWifiClient_service/)** | XPLR NTRIP WiFi client.
**[hpglib/ntripCellClient_service](./../../../components/hpglib/src/ntripCellClient_service/)** | XPLR NTRIP Cell client.
**[hpglib/ntripClientCommon](./../../../components/hpglib/src/ntripClientCommon/)** | XPLR NTRIP common client definitions.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND location device manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.

