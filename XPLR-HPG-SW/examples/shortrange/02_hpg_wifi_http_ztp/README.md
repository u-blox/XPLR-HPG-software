![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Zero Touch Provisioning (ZTP) with Wi-Fi

## Description
This example shows how to execute a **Zero Touch Provisioning** (referred as ZTP from now on) request to the appropriate **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service of **[Thingstream](https://developer.thingstream.io/home)**.\
ZTP will perform an HTTPS POST request to the defined credentials URL and if successful will receive a JSON reply populated with settings such as keys, certificates, topics, etc (please refer to the **[ZTP PointPerfect documentation](./../../../docs/README_thingstream_ztp.md)**).\
ZTP will fetch all required settings for **[Thingstream's](https://developer.thingstream.io/home)** services.\
ZTP requires a minimal setup to perform.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon a successful POST request a set of diagnostics are printed similar to the ones below:

```
...
D [(1872) hpgWifiStarter|xplrWifiStarterFsm|278|: WiFi init successful!
D [(1929) hpgWifiStarter|xplrWifiStarterFsm|289|: Register handlers successful!
D [(1979) hpgWifiStarter|xplrWifiStarterFsm|308|: Wifi credentials configured:1
D [(2029) hpgWifiStarter|xplrWifiStarterFsm|314|: WiFi mode selected: STATION
D [(2030) hpgWifiStarter|xplrWifiStarterFsm|322|: WiFi set mode successful!
D [(2085) hpgWifiStarter|xplrWifiStarterFsm|363|: WiFi set config successful!
...
I [(3357) app|app_main|230|: Performing HTTPS POST request.
I (3777) esp-x509-crt-bundle: Certificate validated
...
I [(4815) app|appGetRootCa|496|: HTTPS GET request OK: code [200] - payload size [1188].
D [(4822) hpgZtp|xplrZtpGetPayloadWifi|123|: POST URL: https://api.thingstream.io/ztp/pointperfect/credentials
D [(4830) hpgZtp|ztpWifiSetHeaders|285|: Successfully set headers for HTTP POST
D [(4836) hpgThingstream|tsApiMsgCreatePpZtp|1152|: Thingstream API PP ZTP POST of 124 bytes created:
{
        "tags": ["ztp"],
        "token":        ***REMOVED***,
        "hardwareId":   "hpg-040148",
        "givenName":    "xplrHpg"
}
D [(4857) hpgZtp|ztpWifiSetPostData|321|: Successfully set POST field
D [(6045) hpgZtp|httpWifiCallback|510|: HTTP_EVENT_ON_CONNECTED!
D [(6455) hpgZtp|httpWifiCallback|538|: HTTP_EVENT_ON_FINISH
D [(6455) hpgZtp|ztpWifiPostMsg|357|: HTTPS POST request OK.
D [(6455) hpgZtp|ztpWifiPostMsg|364|: HTTPS POST: Return Code - 200
D [(6466) hpgZtp|ztpWifiHttpCleanup|379|: Client cleanup succeeded!
...
I [(6764) app|appApplyThingstreamCreds|530|: ZTP Payload parsed successfully
...
D [(6851) hpgWifiStarter|xplrWifiStarterFsm|499|: WiFi disconnected successful!
I [(6854) app|app_main|282|: ALL DONE!!!
...
```
<br>

## Build instructions

### Building using Visual Studio Code
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and ZTP settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_http_ztp` project, making sure that all other projects are commented out:
   ```
   ...
   # shortrange examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_base")
   set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
   ...
   ```
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    1U
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   ...
   #define XPLRZTP_DEBUG_ACTIVE               (1U)
   #define XPLRZTPJSONPARSER_DEBUG_ACTIVE     (1U)
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   ```
4. Open the [app](./main/hpg_wifi_http_ztp.c) and select if you need logging to the SD card for your application.
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
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRZTP_LOG_ACTIVE                 (1U)
   #define XPLRZTPJSONPARSER_LOG_ACTIVE       (1U)
   ...

   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_wifi_http_ztp.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable.
9.  Zero Touch provisioning [**Token** and **URL**](./../../../docs/README_thingstream_ztp.md):
    - Get the **URL** found in the **Endpoint**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream credentials URL` and configure it as needed.\
    The default value is already pointing to the required **URL** and this step can be skipped.
    - Get your **device token** from **Thingstream**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream token` and configure it as needed.
10. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure your **Access Point's SSID** as needed.
11. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure your **Access Point's Password** as needed.
12. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_wifi_http_ztp` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 11 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
6. Run `idf.py build` to compile the project.
7. Run `idf.py -p COMX flash` to flash the binary to the board, where **COMX** is the `COM Port` that the XPLR-HPGx has enumerated on.
8. Run `idf.py monitor -p COMX` to monitor the debug UART output.

## KConfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[KConfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[KConfig](./../../../docs/README_kconfig.md)** before building.\
**[KConfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for.|
**`CONFIG_XPLR_AWS_ROOTCA_URL`** | "https://www.amazontrust.com/repository/AmazonRootCA1.pem" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | Amazon Url in order to fetch the Root CA certificate. If not using a different Root CA certificate leave to default value.| You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_TS_PP_ZTP_TOKEN`** | "ztp-token" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | A device token taken from **Thingstream** devices. | You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED`** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED`** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB`** | Helper definition to denote a size of 1 KByte.
**`APP_ZTP_PAYLOAD_BUF_SIZE`** | A 10 KByte buffer size to store the POST response body from Zero Touch Provisioning.
**`APP_KEYCERT_PARSE_BUF_SIZE`** | A 2 KByte buffer size used for both key and cert/pem key parsed data from ZTP.
**`APP_TOPICS_ARRAY_MAX_SIZE`** | Max topics we can parse.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[hpglib/ztp_service](./../../../components/hpglib/src/ztp_service/)** | Performs Zero Touch Provisioning POST and gets necessary data for MQTT.
**[hpglib/thingstream_service](./../../../components/hpglib/src/thingstream_service/)** | XPLR thingstream parser.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
