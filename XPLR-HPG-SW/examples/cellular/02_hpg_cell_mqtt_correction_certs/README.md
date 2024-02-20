![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Correction Data over MQTT from Thingstream PointPerfect Service (manual)

## Description

The example provided demonstrates how to connect to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service in order to get correction data for the **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module. In this implementation all certificates required are entered manually by the user. The advantage of this implementation is that it uses the least amount of memory to subscribe to Thingstream's PointPerfect correction data topic, and then forward that data to the GNSS module. Zero touch provisioning alternative is presented in [ztp PointPerfect](./../03_hpg_cell_mqtt_correction_ztp/main/hpg_cell_mqtt_correction_ztp.c) example but the service is not supported using the Thingstream redemption code that is provided with the XPLR-HPG2 kit.

IP correction data can be used when no L-Band signal is available in order to get high precision geolocation data. Communication with Thingstream services is provided by [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular module. Please make sure that you have a data plan activated on the inserted SIM card.

**[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service is provided by **[Thingstream](https://developer.thingstream.io/home)** through an MQTT broker that we can connect to and subscribe to location related service topics.<br>
As mentioned before this example requires the related certificates to be given by the user hence it can be used with the redemption code found in the kit.<br>
We recommend using some of the libraries provided as a starting point to Evaluate PointPerfect
   ```
   [+] small footprint
   [-] certificate files provided by the user
   [-] MQTT subscription plan provided by the user
   ```

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and adequate GNSS signal, diagnostic messages are printed similar to the ones below:

**Dead Reckoning disabled**
```
D [(486) xplrBoard|xplrBoardInit|116|: Board init Done
I (490) gpio: GPIO[0]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
D [(499) app|appInitBoard|1190|: Boot0 pin configured as button OK
D [(505) app|appInitBoard|1191|: Board Initialized
D [(560) hpgLocHelpers|xplrHelpersUbxlibInit|108|: ubxlib init ok!
W [(560) app|gnssInit|1043|: Waiting for GNSS device to come online!
D [(562) hpgGnss|xplrGnssStartDevice|623|: GNSS module configured successfully.
D [(569) app|gnssInit|1053|: Location service initialized ok
D [(575) hpgCom|xplrUbxlibInit|132|: ubxlib init ok
D [(580) hpgCom|xplrUbxlibInit|136|: ubxlib dvc init ok
D [(585) hpgCom|dvcGetFirstFreeSlot|587|: ret index: 0
D [(591) hpgCom|cellSetConfig|640|: ok: 0
D [(595) hpgCom|xplrComCellInit|180|: ok, module settings configured
I (612) uart: queue free spaces: 20
I (633) gpio: GPIO[26]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (634) gpio: GPIO[37]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
D [(2201) hpgCom|cellDvcOpen|739|: ok
D [(2201) hpgCom|xplrComCellFsmConnect|282|: open ok, configuring MNO
D [(2211) hpgCom|cellDvcSetMno|755|: MNO in config: 100
W [(2341) hpgCom|cellDvcSetMno|762|: Module's MNO: 90 differs from config: 100
W [(2431) hpgCom|cellDvcSetMno|769|: MNO cannot be set in this module
D [(2431) hpgCom|cellDvcRebootNeeded|685|: no need to reset
D [(2432) hpgCom|xplrComCellFsmConnect|294|: MNO ok or cannot be changed, checking device
D [(2571) hpgCom|cellDvcCheckReady|660|: dvc ready
D [(2571) hpgCom|xplrComCellFsmConnect|342|: dvc rdy, setting RAT(s)...
D [(2641) hpgCom|cellDvcSetRat|799|: RAT[0] is LTE.

D [(2711) hpgCom|cellDvcSetRat|799|: RAT[1] is unknown or not used.

D [(2781) hpgCom|cellDvcSetRat|799|: RAT[2] is unknown or not used.

D [(2781) hpgCom|xplrComCellFsmConnect|305|: RAT ok, checking device
D [(2842) hpgCom|cellDvcCheckReady|660|: dvc ready
D [(2842) hpgCom|xplrComCellFsmConnect|346|: dvc rdy, setting Band(s)...
D [(2852) hpgCom|xplrComCellFsmConnect|316|: bands ok, checking device
D [(2902) hpgCom|cellDvcCheckReady|660|: dvc ready
D [(2902) hpgCom|xplrComCellFsmConnect|350|: dvc rdy, scanning networks...
D [(2912) hpgCom|cellDvcRegister|897|: Bringing up the network...

I [(4625) hpgCom|cellDvcRegister|900|: Network is up!

D [(4625) hpgCom|xplrComCellFsmConnect|327|: dvc interface up, switching to connected
D [(5038) hpgCom|cellDvcGetNetworkInfo|927|: cell network settings:
D [(5038) hpgCom|cellDvcGetNetworkInfo|928|: network_operator: vodafone GR
D [(5040) hpgCom|cellDvcGetNetworkInfo|929|: ip: ***.***.***.***
D [(5046) hpgCom|cellDvcGetNetworkInfo|930|: registered: 1
D [(5051) hpgCom|cellDvcGetNetworkInfo|931|: RAT: LTE
D [(5056) hpgCom|cellDvcGetNetworkInfo|932|: status: registered home
D [(5063) hpgCom|cellDvcGetNetworkInfo|933|: Mcc: 202
D [(5068) hpgCom|cellDvcGetNetworkInfo|934|: Mnc: 5
I [(5073) hpgCom|xplrComCellFsmConnect|369|: dvc connected!
I [(5079) app|cellNetworkRegister|858|: Cell module is Online.
D [(6456) hpgCom|xplrComSetGreetingMessage|223|: Set up greeting message and callback successfully!
I [(6456) app|cellSetGreeting|782|: Greeting message Set to <LARA JUST WOKE UP>

...
D [(37704) hpgCellMqtt|mqttClientCheckRoot|897|: MD5 hash of rootCa (user) is <0x3ffc2481>
D [(37753) hpgCellMqtt|mqttClientCheckRoot|914|: User and NVS Root certificate OK.
I [(37753) hpgCellMqtt|mqttClientCheckRoot|915|: Root Certificate verified OK.
D [(37758) hpgCellMqtt|mqttClientCheckCert|950|: MD5 hash of client cert (user) is <0x3ffc2481>
D [(37816) hpgCellMqtt|mqttClientCheckCert|967|: User and NVS Client certificate OK.
I [(37816) hpgCellMqtt|mqttClientCheckCert|968|: Client Certificate verified OK.
D [(37821) hpgCellMqtt|mqttClientCheckKey|1003|: MD5 hash of client key (user) is <0x3ffc2481>
D [(37881) hpgCellMqtt|mqttClientCheckKey|1020|: User and NVS Client key OK.
I [(37881) hpgCellMqtt|mqttClientCheckKey|1021|: Client key verified OK.
D [(37884) hpgCellMqtt|xplrCellMqttFsmRun|495|: Credentials chain is OK.
D [(37902) hpgCellMqtt|xplrCellMqttFsmRun|535|: Load module 0, client 0 credentials.
D [(38355) hpgCellMqtt|mqttClientConnectTLS|1245|: Client config OK.
D [(40815) hpgCellMqtt|mqttClientConnectTLS|1258|: Client connection established.
D [(41885) hpgCellMqtt|mqttClientSubscribeToTopicList|1366|: Client 0 subscribed to /pp/ubx/0236/Lb.
D [(42926) hpgCellMqtt|mqttClientSubscribeToTopicList|1366|: Client 0 subscribed to /pp/Lb/eu.
I [(42927) hpgCellMqtt|xplrCellMqttFsmRun|543|: MQTT client is connected.
...

D [(84440) hpgCellMqtt|xplrCellMqttGetNumofMsgAvailable|321|: There are 1 messages unread for Client 0.
D [(84917) hpgCellMqtt|xplrCellMqttUpdateTopicList|378|: Client 0 read 201 bytes from topic /pp/Lb/eu.
D [(84917) hpgCellMqtt|xplrCellMqttUpdateTopicList|403|: Client 0, topic /pp/Lb/eu updated.
Segment size 201 bytes
Msg size 201 bytes
D [(84928) app|cellMqttClientMsgUpdate|968|: Topic name </pp/Lb/eu> identified as <correction data topic>.
D [(84943) hpgLocHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|456|: Sent UBX data [201] bytes.
D [(84946) app|gnssFwdPpData|1088|: Correction data forwarded to GNSS module.
...

D [(102930) app|cellMqttClientStatisticsPrint|988|: Messages Received: 17.
D [(102933) app|cellMqttClientStatisticsPrint|989|: Bytes Received: 20173.
D [(102940) app|cellMqttClientStatisticsPrint|990|: Uptime: 60 seconds.
I [(102947) hpgGnss|gnssLocationPrinter|4258|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: RTK-FIXED
Location latitude: 38.048038 (raw: 380480380)
Location longitude: 23.809122 (raw: 238091223)
Location altitude: 233.138000 (m) | 233138 (mm)
Location radius: 0.017000 (m) | 17 (mm)
Speed: 0.007200 (km/h) | 0.002000 (m/s) | 2 (mm/s)
Estimated horizontal accuracy: 0.0170 (m) | 17.00 (mm)
Estimated vertical accuracy: 0.0276 (m) | 27.60 (mm)
Satellite number: 29
Time UTC: 13:24:29
Date UTC: 09.01.2024
Calendar Time UTC: Tue 09.01.2024 13:24:29
===============================
I [(103004) hpgGnss|xplrGnssPrintGmapsLocation|1806|: Printing GMapsLocation!
I [(103011) hpgGnss|xplrGnssPrintGmapsLocation|1807|: Gmaps Location: https://maps.google.com/?q=38.048038,23.809122
D [(104073) hpgCellMqtt|mqttClientUnsubscribeFromTopicList|1438|: Client 0 unsubscribed from /pp/ubx/0236/Lb.
D [(105114) hpgCellMqtt|mqttClientUnsubscribeFromTopicList|1438|: Client 0 unsubscribed from /pp/Lb/eu.
D [(106155) hpgCellMqtt|xplrCellMqttDisconnect|221|: Client 0 disconnected ok.
W [(106155) hpgGnss|xplrGnssStopDevice|1050|: Requested Stop
D [(106167) hpgGnss|xplrGnssFsm|968|: Trying to stop GNSS device.
D [(106167) hpgGnss|xplrGnssUbxMessagesAsyncStop|1386|: Trying to stop Gnss UBX Messages async.
D [(106172) hpgGnss|gnssAsyncStopper|2869|: Successfully stoped async function.
D [(106180) hpgGnss|xplrGnssNmeaMessagesAsyncStop|1354|: Trying to stop Gnss Get Fix Type async.
D [(106282) hpgGnss|gnssAsyncStopper|2869|: Successfully stoped async function.
D [(106282) hpgGnss|gnssAsyncStopper|2869|: Successfully stoped async function.
D [(106285) hpgGnss|xplrGnssStopAllAsyncs|1414|: Stopped all async getters.
D [(106293) hpgLocHelpers|xplrHlprLocSrvcDeviceClose|227|: ubxlib device closed!
D [(106300) hpgGnss|gnssDeviceStop|2419|: Sucessfully stoped GNSS device.
D [(106307) hpgGnss|xplrGnssFsm|971|: Device stopped.
I [(106322) app|appTerminate|1249|: App MQTT Statistics.
D [(106322) app|appTerminate|1250|: Messages Received: 17.
D [(106323) app|appTerminate|1251|: Bytes Received: 20173.
D [(106329) app|appTerminate|1252|: Uptime: 60 seconds.
W [(106334) app|appTerminate|1253|: App disconnected the MQTT client.
heap: min 210196 cur 218708 size 279312 maxBlock 110592 tasks: 11
Task List:
main            X       1       3620    4
IDLE            R       0       1012    6
IDLE            R       0       1020    5
deviceOffTask   B       10      3632    8
esp_timer       S       22      3660    3
Tmr Svc         B       1       1596    7
timerEvent      B       24      1452    9
ipc1            B       24      1068    2
atCallbacks     B       5       1676    10
eventTask       B       20      1384    11
ipc0            B       24      1076    1
```
**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
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

**NOTE:**
- Vehicle dynamics will only be printed if the module has been calibrated.
- Dead reckoning is only available in ZED-F9R GNSS module.


<br>

## Build instructions

### Building using Visual Studio Code
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
   #define XPLRTHINGSTREAM_DEBUG_ACTIVE       (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRCELL_MQTT_DEBUG_ACTIVE         (1U)
   ...
   ```
4. Open the [app](./main/hpg_cell_mqtt_correction_certs.c) and select if you need logging to the SD card for your application.
   ```
   #define APP_SD_LOGGING_ENABLED      0U
   ```
   This is a general option that enables or disables the SD logging functionality for all the modules. <br> 
   To enable/disable the individual module debug message SD logging:

   - Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED                         1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                            (1U)
   ...
   #define XPLRCOM_LOG_ACTIVE                             (1U)
   #define XPLRCELL_HTTP_LOG_ACTIVE                       (1U)
   #define XPLRCELL_MQTT_LOG_ACTIVE                       (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE                        (1U)
   ...
   #define XPLR_THINGSTREAM_LOG_ACTIVE                    (1U)
   ...

   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_cell_mqtt_correction_certs.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Cellular Settings -> APN value of cellular provider` and input the **APN** of your cellular provider.
11. Go to `XPLR HPG Options -> Correction Data Source -> Choose correction data source for your GNSS module` and select the **Correction data source** you need for your GNSS module.
12. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed.<br>
    You can also skip this step since the correct **hostname** is already configured.
13. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
14. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
15. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem** into the variable **keyPP**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
16. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt.csv** into the variable **certPP**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
17. **Copy and Paste** the contents of **Root Certificate** into the variable **rootCa**, declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), keeping the format of the provided template certificate.
18. Change **APP_THINGSTREAM_REGION** macro , declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), according to your **[Thingstream](https://developer.thingstream.io/home)** service location. Possible values are:
    - **XPLR_THINGSTREAM_PP_REGION_EU**
    - **XPLR_THINGSTREAM_PP_REGION_US**
    - **XPLR_THINGSTREAM_PP_REGION_KR**
    - **XPLR_THINGSTREAM_PP_REGION_AU**
    - **XPLR_THINGSTREAM_PP_REGION_JP**

19. Change **APP_THINGSTREAM_PLAN** macro , declared in [app mqtt pp](./../02_hpg_cell_mqtt_correction_certs/main/hpg_cell_mqtt_correction_certs.c), according to your **[Thingstream](https://developer.thingstream.io/home)** subscription plan. Possible values are IP, IPLBAND and LBAND.\
Check your subscription plan in the Location Thing Details tab in the **[Thingstream](https://developer.thingstream.io/home)** platform. PointPerfect Developer Plan is an IP plan, as is the included promo card.
20. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.

<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_cell_mqtt_correction_certs` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 19 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
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
**`CONFIG_GNSS_MODULE`** | "ZED-F9R" | **[boards](./../../../components/boards)** | Selects the GNSS module equipped. |
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_CELL_APN`** | "internet" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | APN value of cellular provider to register. | You will have to replace this value to with your specific APN of your cellular provider, either by directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_CORRECTION_DATA_SOURCE`** | "Correction via IP" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | Selects the source of the correction data to be forwarded to the GNSS module. |
**`CONFIG_XPLR_MQTTCELL_CLIENT_ID`** | "MQTT Client ID" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using Kconfig.
**`CONFIG_XPLR_MQTTCELL_THINGSTREAM_HOSTNAME`** | "pp.services.u-blox.com:8883" | **[hpg_cell_mqtt_correction_certs](./main/hpg_cell_mqtt_correction_certs.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL changes in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description
--- | ---
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED`** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED`** | Switches logging of the application messages to the SD card ON or OFF.
**`APP_STATISTICS_INTERVAL`** | Frequency, in seconds, of how often we want our network statistics data to be printed in the console. Can be changed as desired.
**`APP_GNSS_LOC_INTERVAL`** | Frequency, in seconds, of how often we want our print location function \[**`appPrintLocation(void)`**\] to execute. Can be changed as desired.
**`APP_GNSS_DR_INTERVAL`** | Frequency, in seconds, of how often we want our print dead reckoning data function \[**`gnssDeadReckoningPrint(void)`**\] to execute. Can be changed as desired.
**`APP_RUN_TIME`** | Period, in seconds, of how how long the application will run before exiting. Can be changed as desired.
**`APP_MQTT_BUFFER_SIZE`** | Size of each MQTT buffer used. The buffer must be at least this big (for this sample code) to fit the whole correction data which may be up to 6 KBytes long.
**`APP_GNSS_I2C_ADDR`** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_THINGSTREAM_REGION`** | Thingstream service location.
**`APP_THINGSTREAM_PLAN`** | Thingstream subscription plan.
**`APP_RESTART_ON_ERROR`** | Option to select if the board should perform a reboot in case of error.
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
**`APP_ENABLE_CORR_MSG_WDG`** | Option to enable the correction message watchdog mechanism.
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
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND service manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
<br>