![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Zero Touch Provisioning (ZTP) with Wi-Fi

## Description
This example shows how to execute a **Zero Touch Provisioning** (referred as ZTP from now on) request to the appropriate **Point Perfect** service of **Thingstream**.\
ZTP will perform an HTTPS POST request to the defined credentials URL and if successful will receive a JSON reply populated with settings such as keys, certificates, topics, etc (please refer to the **ZTP Point Perfect documentation**).\
ZTP will fetch all required settings for **Thingstream' s** services.\
ZTP requires a minimal setup to perform.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon a successful POST request a set of diagnostics are printed similar to the ones below:

```
...
D [(1200) xplrWifiStarter|xplrWifiStarterFsm|154|: WiFi init successful!
D [(1256) xplrWifiStarter|xplrWifiStarterFsm|165|: Register handlers successful!
D [(1307) xplrWifiStarter|xplrWifiStarterFsm|176|: WiFi set mode successful!
D [(1377) xplrWifiStarter|xplrWifiStarterFsm|187|: WiFi set config successful!
...
I [(3998) app|app_main|179|: Performing HTTPS POST request.
D [(3998) xplrZtp|xplrZtpGetPayload|107|: POST URL: https://api.thingstream.io/ztp/pointperfect/credentials
D [(4004) xplrZtp|xplrZtpGetDeviceID|190|: Device ID: **********
D [(4009) xplrZtp|xplrZtpPrivateSetPostData|334|: Post data: {"token":"**********","givenName":"device-name","hardwareId":"**********","tags": []}
D [(5793) xplrZtp|client_event_post_handler|207|: HTTP_EVENT_ON_CONNECTED!
D [(6218) xplrZtp|client_event_post_handler|242|: HTTP_EVENT_ON_FINISH
D [(6218) xplrZtp|xplrZtpGetPayload|152|: HTTPS POST request OK.
D [(6220) xplrZtp|xplrZtpGetPayload|157|: HTTPS POST: Return Code - 200
I [(15234) app|appZtpMqttCertificateParse|286|: Parsed Certificate:
-----BEGIN CERTIFICATE-----
**********
-----END CERTIFICATE-----

I [(15341) app|appZtpMqttClientIdParse|299|: Parsed MQTT client ID: **********
I [(15352) app|appZtpMqttSupportParse|324|: Is MQTT supported: true
I [(15358) app|appZtpDeallocateJSON|348|: Deallocating JSON object.
...
D [(7539) xplrWifiStarter|xplrWifiStarterFsm|252|: WiFi disconnected successful!
I [(7542) app|app_main|228|: ALL DONE!!!
...
```
<br>

## Build instructions
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
4. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
5. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
6. Navigate to the `Board Options` section and select the board you wish to build the example for.
7. Download **Root Certificate** from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**
   - **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then **Copy and Paste** the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
8.  Zero Touch provisioning [**Token** and **URL**](./../../../docs/README_thingstream_ztp.md):
    - Get the **URL** found in the **Endpoint**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream credentials URL` and configure it as needed.
    - Get your **device token** from **Thingstream**, go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP Thingstream token` and configure it as needed.\
   The default value is already pointing to the required **URL** and this step can be skipped.
9. Go to `XPLR HPG Options -> Thingstream Zero Touch Provisioning Settings -> ZTP device name` and input a `device name`. This `device name` does not have to be something specific (i.e. taken from **Thingstream**), you can chose its value freely, but it cannot be empty.\
    The default value can be left as is and this step can be skipped.
10. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure your **Access Point's SSID** as needed.
11. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure your **Access Point's Password** as needed.
12. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

## KConfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[KConfig](./../../../docs/README_kconfig.md)**.\
In most cases, these values can be directly overwritten in the source code or just configured by running **[KConfig](./../../../docs/README_kconfig.md)** before building.\
**[KConfig](./../../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description | Manual overwrite notes
--- | --- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPGx_C21x`** | "CONFIG_BOARD_XPLR_HPG2_C214" | **[boards](./../../../components/boards)** | Board variant to build firmware for .|
**`CONFIG_XPLR_TS_PP_ZTP_CREDENTIALS_URL`** | "https://api.thingstream.io/ztp/pointperfect/credentials" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | A URL to make a Zero Touch Provisioning POST request. Taken from **Thingstream**. | There's no need to replace this value unless the **URL** changes in the future. You can replace this value freely in the app.
**`CONFIG_XPLR_TS_PP_ZTP_TOKEN`** | "ztp-token" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | A device token taken from **Thingstream** devices. | You will have to replace this value with your specific token, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_TS_PP_DEVICE_NAME`** | "device-name" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | A device name. Input a string to make your device more distinguishable. Cannot be empty! | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_wifi_http_ztp](./main/hpg_wifi_http_ztp.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_SERIAL_DEBUG_ENABLED 1U`** | Switches debug printing messages ON or OFF
**`KIB 1024U`** | Helper definition to denote a size of 1 KByte
**`APP_ZTP_PAYLOAD_BUF_SIZE ((10U) * (KIB))`** | A 10 KByte buffer size to store the POST response body from Zero Touch Provisioning.
**`APP_KEYCERT_PARSE_BUF_SIZE  ((2U) * (KIB))`** | A 2 KByte buffer size used for both key and cert/pem key parsed data from ZTP.
**`APP_TOPICS_ARRAY_MAX_SIZE`** | Max topics we can parse.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager
**[xplr_ztp](./../../../components/xplr_ztp)** | Performs Zero Touch Provisioning POST and gets necessary data for MQTT
**[xplr_ztp_json_parser](./../../../components/xplr_ztp_json_parser/)** | A parser able to extract data from the POST body
