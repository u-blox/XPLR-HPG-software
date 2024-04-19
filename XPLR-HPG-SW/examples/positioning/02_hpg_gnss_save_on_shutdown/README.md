![u-blox](./../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# ZED-F9R Save on Shutdown functionality for ZED-F9R and graceful board shutdown

## Description

The example provided showcases the **Save on Shutdown** and **Clear Backup** functionality of ZED-F9R.\
**Save on Shutdown** is a routine in which the receiver can be instructed to dump its current state to flash memory as part of the shutdown procedure.\
This data is then automatically retrieved when the receiver is restarted.\
This allows the u-blox receiver to preserve some of the features available only with a battery backup (preserving configuration, IMU calibration and satellite orbit knowledge) without having a battery supply present.\
It does **not**, however, preserve any kind of time knowledge.\
The restoring of data on startup is automatically done if the corresponding data is present in the flash. Data expiration is not checked.\
Once ZED has started up it is recommended to delete the stored data. This procedure is performed with the **Clear Backup** functionality.

The example is structured similarly to the **[hpg_wifi_mqtt_correction_certs](./../../shortrange/03_hpg_wifi_mqtt_correction_certs/)** example.\
After successful connection to Wi-Fi and to the **[Thingstream](https://developer.thingstream.io/home)** platform and access the **[PointPerfect](https://developer.thingstream.io/guides/location-services/pointperfect-getting-started)** service, we achieve High Precision GNSS after feeding the correction data we receive to our **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.

During the runtime of the example, if the **BOOT0** button is pressed for more than 2 seconds and the is released, the procedure of **graceful shutdown** will start.\
Specifically the board:
   - Unsubscribes from MQTT topics
   - Disconnects from MQTT 
   - Disconnects from WiFi
   - Halts ZED module's logging (*if enabled*)
   - Clears ZED module's previously saved Backup Configuration (*if existent*)
   - Starts Save on Shutdown routine for ZED module, to store the current configuration as backup.
   - De-initializes application's logging to the SD (APPLOG and ERRORLOG) (*if enabled*)
   - Enters a halted state

**NOTE**: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

When running the code, depending on the debug settings configured, messages are printed to the debug UART providing useful information to the user. Upon MQTT connection and valid geolocation data a set of diagnostics are printed similar to the ones below:

**Dead Reckoning disabled**
```
...
D [(1293) xplrBoard|xplrBoardInit|114|: Board init Done
I (1298) gpio: GPIO[0]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
D [(1308) app|appInitBoard|473|: Boot0 pin configured as button OK
D [(1314) app|appInitBoard|474|: Board Initialized
...
D [(4184) xplrWifiStarter|xplrWifiStarterFsm|282|: WiFi init successful!
D [(4215) xplrWifiStarter|xplrWifiStarterFsm|293|: Register handlers successful!
D [(4240) xplrWifiStarter|xplrWifiStarterFsm|312|: Wifi credentials configured:1 
D [(4480) xplrGnss|xplrGnssFsm|692|: Trying to open device.
D [(4481) xplrWifiStarter|xplrWifiStarterFsm|318|: WiFi mode selected: STATION
D [(4482) xplrWifiStarter|xplrWifiStarterFsm|326|: WiFi set mode successful!
D [(4516) xplrWifiStarter|xplrWifiStarterFsm|367|: WiFi set config successful!
...
D [(5434) xplrMqttWifi|xplrMqttWifiFsm|209|: MQTT config successful!
D [(5464) xplrMqttWifi|xplrMqttWifiFsm|219|: MQTT event register successful!
D [(5489) xplrMqttWifi|xplrMqttWifiFsm|234|: MQTT client start successful!
D [(7599) xplrMqttWifi|xplrMqttWifiEventHandlerCb|757|: MQTT event connected!
I [(8453) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpenNonBlocking|226|: ubxlib device opened!
I [(8453) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpenNonBlocking|229|: Network interface opened!
D [(8461) xplrGnss|xplrGnssFsm|692|: Trying to open device.
I [(8676) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
========= Device Info =========
Module variant: ZED-F9R
Module version: EXT CORE 1.00 (a59682)
Hardware version: 00190000
Rom: 0x118B2060
Firmware: HPS 1.30
Protocol: 33.30
ID: 5340b2ee00
-------------------------------
I2C Port: 0
I2C Address: 0x42
I2C SDA pin: 21
I2C SCL pin: 22
===============================
D [(8701) xplrGnss|xplrGnssFsm|705|: Restart Completed.
D [(8732) xplrGnss|xplrGnssFsm|729|: xSemWatchdog created successfully
D [(8732) xplrGnss|xplrGnssFsm|731|: Successfully released xSemWatchdog!
D [(8759) xplrGnss|xplrGnssFsm|753|: Trying to set GNSS generic location settings.
D [(8885) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValSet|319|: Set multiple configuration values.
D [(8885) xplrGnss|xplrGnssFsm|757|: Set generic location settings on GNSS module.
D [(8916) xplrGnss|xplrGnssFsm|765|: Trying to set correction data decryption keys.
D [(8916) xplrGnss|xplrGnssFsm|776|: No keys stored. Skipping.
D [(8944) xplrGnss|xplrGnssFsm|783|: Trying to set correction data source.
D [(9058) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
D [(9058) xplrGnss|xplrGnssFsm|786|: Set configured correction data source.
D [(9087) xplrGnss|xplrGnssFsm|796|: Trying to start async getters.
D [(9107) xplrGnss|xplrGnssNmeaMessagesAsyncStart|1280|: Started Gnss NMEA async.
D [(9107) xplrGnss|xplrGnssUbxMessagesAsyncStart|1315|: Started Gnss UBX Messages async.
D [(9111) xplrGnss|xplrGnssStartAllAsyncs|1337|: Started all async getters.
D [(9119) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [8] bytes.
D [(9127) xplrGnss|gnssPollBackupRestore|2580|: Poll UPD-SOS message command issued successfully
I [(9140) xplrGnss|gnssUpdSoSParser|3188|: ZED is restored from Backup Memory
...
D [(9897) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|408|: Successfully subscribed to topic: /pp/ubx/0236/ip with id: 20513
D [(9903) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|408|: Successfully subscribed to topic: /pp/ip/eu with id: 49433
D [(10019) xplrMqttWifi|xplrMqttWifiEventHandlerCb|770|: MQTT event subscribed!
D [(10061) xplrGnss|xplrGnssSendDecryptionKeys|1584|: Saved keys into config struct.
D [(10063) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [60] bytes.
D [(10119) xplrMqttWifi|xplrMqttWifiEventHandlerCb|770|: MQTT event subscribed!
...
I [(95224) xplrGnss|gnssLocationPrinter|4229|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: RTK-FLOAT
Location latitude: 38.048191 (raw: 380481907)
Location longitude: 23.809118 (raw: 238091176)
Location altitude: 227.681000 (m) | 227681 (mm)
Location radius: 0.535000 (m) | 535 (mm)
Speed: 0.007200 (km/h) | 0.002000 (m/s) | 2 (mm/s)
Estimated horizontal accuracy: 0.5349 (m) | 534.90 (mm)
Estimated vertical accuracy: 0.4553 (m) | 455.30 (mm)
Satellite number: 17
Time UTC: 14:16:53
Date UTC: 13.09.2023
Calendar Time UTC: Wed 13.09.2023 14:16:53
===============================
I [(95270) xplrGnss|xplrGnssPrintGmapsLocation|1800|: Printing GMapsLocation!
I [(95277) xplrGnss|xplrGnssPrintGmapsLocation|1801|: Gmaps Location: https://maps.google.com/?q=38.048191,23.809118
W [(34789) app|appDeviceOffTask|702|: Device OFF triggered
D [(34789) app|appDeviceOffTask|704|: Disconnecting from MQTT
D [(34794) xplrMqttWifi|xplrMqttWifiUnsubscribeFromTopic|575|: Successfully unsubscribed from topic: /pp/ubx/0236/ip with id: 42936
D [(34804) xplrMqttWifi|xplrMqttWifiUnsubscribeFromTopic|575|: Successfully unsubscribed from topic: /pp/ip/eu with id: 4833
D [(34908) app|appDeviceOffTask|720|: Disconnected from MQTT
D [(34908) app|appDeviceOffTask|721|: Disconnecting from WiFi
I (34910) wifi:state: run -> init (0)
I (34913) wifi:pm stop, total sleep time: 24794394 us / 30196776 us

W (34919) wifi:<ba-del>idx
I (34921) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
W (34931) wifi:hmac tx: ifx0 stop, discard
W (34932) wifi:hmac tx: ifx0 stop, discard
E [(34936) xplrWifiStarter|xplrWifiStarterFsm|506|: WiFi disconnect failed!
I (34970) wifi:flush txq
I (34970) wifi:stop sw txq
I (34971) wifi:lmac stop hw txq
D [(34972) xplrWifiStarter|xplrWifiStarterFsm|503|: WiFi disconnected successful!
D [(34974) app|appDeviceOffTask|743|: Disconnected from WiFi
D [(34980) app|appDeviceOffTask|745|: Clearing GNSS Backup Memory Configuration
W [(34992) xplrGnss|xplrGnssFsm|950|: Triggered backup config erase.
D [(35019) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(35019) xplrGnss|gnssClearBackup|2620|: Clear Backup command issued successfully
D [(35400) xplrGnss|gnssClearBackup|2637|: Stored backup configuration erased from memory
D [(35400) xplrGnss|xplrGnssFsm|905|: Backup Configuration erased from memory
D [(35487) app|appDeviceOffTask|752|: Cleared GNSS Backup Memory Configuration
D [(35487) app|appDeviceOffTask|753|: Starting Save on Shutdown routine
D [(35504) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(35505) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(35511) xplrGnss|gnssCreateBackup|2503|: Create Backup command issued successfully
D [(35519) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(35528) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(35561) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(35561) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(35591) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(35591) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(35621) xplrGnss|gnssBackupCreationAck|2550|: Backup Creation routine was successful!
D [(35621) xplrGnss|xplrGnssFsm|927|: Save on Shutdown complete. Turning off device
D [(35675) xplrGnss|xplrGnssFsm|987|: Trying to stop GNSS device.
D [(35675) xplrGnss|xplrGnssUbxMessagesAsyncStop|1396|: Trying to stop Gnss UBX Messages async.
D [(35678) xplrGnss|gnssAsyncStopper|2844|: Successfully stoped async function.
D [(35686) xplrGnss|xplrGnssNmeaMessagesAsyncStop|1365|: Trying to stop Gnss Get Fix Type async.
D [(35736) xplrGnss|gnssAsyncStopper|2844|: Successfully stoped async function.
D [(35736) xplrGnss|xplrGnssStopAllAsyncs|1424|: Stopped all async getters.
D [(35739) xplrCommonHelpers|xplrHlprLocSrvcDeviceClose|249|: ubxlib device closed!
D [(35746) xplrGnss|gnssDeviceStop|2411|: Sucessfully stoped GNSS device.
D [(35753) xplrGnss|xplrGnssFsm|990|: Device stopped.
D [(36489) app|appDeviceOffTask|760|: Save on Shutdown routine finished
I [(40589) app|appDeviceOffTask|769|: Device graceful shutdown performed!
W [(40589) app|appHaltExecution|672|: Halting execution
W [(40608) app|appHaltExecution|672|: Halting execution
```

**Dead Reckoning enabled and printing flag APP_PRINT_IMU_DATA is set**
```
...
D [(1402) xplrBoard|xplrBoardInit|114|: Board init Done
I (1407) gpio: GPIO[0]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
D [(1417) app|appInitBoard|473|: Boot0 pin configured as button OK
D [(1425) app|appInitBoard|474|: Board Initialized
I [(1429) app|appInitWiFi|490|: Starting WiFi in station mode.
D [(1490) xplrCommonHelpers|xplrHelpersUbxlibInit|131|: ubxlib init ok!
D [(1501) xplrGnss|xplrGnssStartDevice|634|: GNSS module configured successfully.
I [(1510) app|appInitGnssDevice|554|: Successfully initialized all GNSS related devices/functions!
D [(1520) xplrWifiStarter|xplrWifiStarterFsm|182|: Config WiFi OK.
D [(1551) xplrGnss|xplrGnssFsm|665|: Logging is enabled. Trying to initialize.
I [(1560) xplrGnss|xplrGnssAsyncLogInit|2160|: Initializing async logging
D [(1591) xplrGnss|xplrGnssFsm|668|: Sucessfully initialized GNSS logging.
...
D [(4024) xplrGnss|xplrGnssFsm|692|: Trying to open device.
I [(4283) xplrCommonHelpers|xplrHlprLocSrvcGetDeviceInfo|412|: Got device info.
========= Device Info =========
Module variant: ZED-F9R
Module version: EXT CORE 1.00 (a59682)
Hardware version: 00190000
Rom: 0x118B2060
Firmware: HPS 1.30
Protocol: 33.30
ID: 5340b2ee00
-------------------------------
I2C Port: 0
I2C Address: 0x42
I2C SDA pin: 21
I2C SCL pin: 22
===============================
D [(4311) xplrGnss|xplrGnssFsm|702|: Configuration Completed.
D [(4343) xplrWifiStarter|xplrWifiStarterFsm|218|: Init netif successful!
D [(4374) xplrGnss|xplrGnssFsm|973|: Trying to restart GNSS device.
D [(4384) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(4394) xplrGnss|gnssDeviceRestart|2375|: Reset command issued successfully
D [(4403) xplrGnss|xplrGnssUbxMessagesAsyncStop|1393|: Looks like Gnss UBX Messages async is not running. Nothing to do.
D [(4423) xplrGnss|xplrGnssNmeaMessagesAsyncStop|1362|: Looks like Gnss Get Fix Type async is not running. Nothing to do.
D [(4432) xplrGnss|xplrGnssStopAllAsyncs|1424|: Stopped all async getters.
D [(4454) xplrCommonHelpers|xplrHlprLocSrvcDeviceClose|249|: ubxlib device closed!
D [(4463) xplrGnss|gnssDeviceStop|2411|: Sucessfully stoped GNSS device.
D [(4472) xplrGnss|gnssDeviceRestart|2378|: Device stop command issued successfully
D [(4492) xplrGnss|gnssDeviceRestart|2379|: Restart routine executed successfully.
D [(4501) xplrGnss|xplrGnssFsm|976|: Restart succeeded.
...
D [(5025) xplrWifiStarter|xplrWifiStarterFsm|282|: WiFi init successful!
D [(5064) xplrWifiStarter|xplrWifiStarterFsm|293|: Register handlers successful!
D [(5095) xplrWifiStarter|xplrWifiStarterFsm|312|: Wifi credentials configured:1 
D [(5137) xplrWifiStarter|xplrWifiStarterFsm|318|: WiFi mode selected: STATION
D [(5144) xplrWifiStarter|xplrWifiStarterFsm|326|: WiFi set mode successful!
D [(5176) xplrWifiStarter|xplrWifiStarterFsm|367|: WiFi set config successful!
...
D [(5322) xplrWifiStarter|xplrWifiStarterFsm|403|: WiFi started successful!
D [(5357) xplrWifiStarter|xplrWifiStarterFsm|447|: Call esp_wifi_connect success!
...
D [(6386) xplrMqttWifi|xplrMqttWifiFsm|209|: MQTT config successful!
D [(6416) xplrMqttWifi|xplrMqttWifiFsm|219|: MQTT event register successful!
D [(6446) xplrMqttWifi|xplrMqttWifiFsm|234|: MQTT client start successful!
D [(8837) xplrMqttWifi|xplrMqttWifiEventHandlerCb|757|: MQTT event connected!
I [(8882) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpenNonBlocking|226|: ubxlib device opened!
I [(8914) xplrCommonHelpers|xplrHlprLocSrvcDeviceOpenNonBlocking|229|: Network interface opened!
...
D [(9680) xplrGnss|xplrGnssFsm|796|: Trying to start async getters.
D [(9710) xplrGnss|xplrGnssNmeaMessagesAsyncStart|1280|: Started Gnss NMEA async.
D [(9732) xplrGnss|xplrGnssUbxMessagesAsyncStart|1315|: Started Gnss UBX Messages async.
D [(9742) xplrGnss|xplrGnssStartAllAsyncs|1337|: Started all async getters.
D [(9752) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [8] bytes.
D [(9774) xplrGnss|gnssPollBackupRestore|2580|: Poll UPD-SOS message command issued successfully
I [(9794) xplrGnss|gnssUpdSoSParser|3186|: ZED is restored from Backup Memory
...
D [(9912) xplrGnss|xplrGnssFsm|818|: Detected Dead Reckoning enable option in config. Initializing Dead Reckoning.
D [(9922) xplrGnss|xplrGnssFsm|823|: Initialized NVS.
D [(9968) xplrGnss|xplrGnssFsm|832|: Trying to init Dead Reckoning.
D [(10003) xplrGnss|xplrGnssFsm|857|: Trying to execute IMU automatic calibration.
D [(10103) xplrCommonHelpers|xplrHlprLocSrvcOptionMultiValSet|319|: Set multiple configuration values.
D [(10306) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
...
D [(10689) xplrGnss|gnssNvsReadConfig|3919|: Read NVS id: <gnssDvc_0>
D [(10710) xplrGnss|gnssNvsReadConfig|3920|: Read NVS yaw: <40000>
D [(10720) xplrGnss|gnssNvsReadConfig|3921|: Read NVS pitch: <10000>
D [(10735) xplrGnss|gnssNvsReadConfig|3922|: Read NVS roll: <20000>
D [(10861) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
D [(10897) xplrGnss|xplrGnssFsm|865|: Executed Auto Calibration.
D [(10932) xplrGnss|xplrGnssFsm|873|: Trying to enable Dead Reckoning.
D [(11102) xplrCommonHelpers|xplrHlprLocSrvcOptionSingleValSet|295|: Set configuration value.
D [(11152) xplrGnss|xplrGnssFsm|878|: Started dead Reckoning.
I [(11435) xplrGnss|xplrGnssPrintImuAlignmentInfo|1940|: Printing Imu Alignment Info.
===== Imu Alignment Info ======
Calibration Mode: Auto
Calibration Status: IMU-mount roll/pitch angles alignment is ongoing
Aligned yaw: 0.00
Aligned pitch: 0.00
Aligned roll: 0.00
-------------------------------
I [(11456) xplrGnss|gnssImuAlignStatPrinter|3627|: Printing Imu Alignment Statuses.
===== Imu Alignment Status ====
Fusion mode: Suspended
Number of sensors: 7
-------------------------------
Sensor type: Gyroscope Z Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Wheel Tick Single Tick
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Gyroscope Y Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Gyroscope X Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Accelerometer X Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Accelerometer Y Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Accelerometer Z Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
D [(11577) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|408|: Successfully subscribed to topic: /pp/ubx/0236/ip with id: 47477
D [(11591) xplrMqttWifi|xplrMqttWifiSubscribeToTopic|408|: Successfully subscribed to topic: /pp/ip/eu with id: 47374
D [(11775) xplrMqttWifi|xplrMqttWifiEventHandlerCb|770|: MQTT event subscribed!
D [(11807) xplrGnss|xplrGnssSendDecryptionKeys|1584|: Saved keys into config struct.
D [(11833) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [60] bytes.
D [(11908) xplrMqttWifi|xplrMqttWifiEventHandlerCb|770|: MQTT event subscribed!
I [(16265) xplrGnss|xplrGnssPrintImuAlignmentInfo|1940|: Printing Imu Alignment Info.
===== Imu Alignment Info ======
Calibration Mode: Auto
Calibration Status: IMU-mount roll/pitch angles alignment is ongoing
Aligned yaw: 0.00
Aligned pitch: 0.00
Aligned roll: 0.00
-------------------------------
I [(16301) xplrGnss|gnssImuAlignStatPrinter|3627|: Printing Imu Alignment Statuses.
===== Imu Alignment Status ====
Fusion mode: Initializing
Number of sensors: 7
-------------------------------
Sensor type: Gyroscope Z Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Wheel Tick Single Tick
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Gyroscope Y Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Gyroscope X Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer X Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Y Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Z Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
D [(16596) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [258] bytes.
...
I [(55318) xplrGnss|gnssLocationPrinter|4227|: Printing location info.
======== Location Info ========
Location type: 1
Location fix type: RTK-FLOAT
Location latitude: 38.048200 (raw: 380482000)
Location longitude: 23.809121 (raw: 238091208) 
Location altitude: 229.679000 (m) | 229679 (mm)
Location radius: 2.442000 (m) | 2442 (mm)      
Speed: 0.039600 (km/h) | 0.011000 (m/s) | 11 (mm/s)     
Estimated horizontal accuracy: 2.4417 (m) | 2441.70 (mm)
Estimated vertical accuracy: 1.6655 (m) | 1665.50 (mm)  
Satellite number: 15
Time UTC: 14:20:49
Date UTC: 13.09.2023
Calendar Time UTC: Wed 13.09.2023 14:20:49
===============================
I [(55368) xplrGnss|xplrGnssPrintGmapsLocation|1800|: Printing GMapsLocation!
I [(55397) xplrGnss|xplrGnssPrintGmapsLocation|1801|: Gmaps Location: https://maps.google.com/?q=38.048200,23.809121
I [(56272) xplrGnss|xplrGnssPrintImuAlignmentInfo|1940|: Printing Imu Alignment Info.
===== Imu Alignment Info ======
Calibration Mode: Auto
Calibration Status: IMU-mount roll/pitch angles alignment is ongoing
Aligned yaw: 0.00  
Aligned pitch: 0.00
Aligned roll: 0.00 
-------------------------------
I [(56302) xplrGnss|gnssImuAlignStatPrinter|3627|: Printing Imu Alignment Statuses.
===== Imu Alignment Status ====
Fusion mode: Initializing
Number of sensors: 7
-------------------------------
Sensor type: Gyroscope Z Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Wheel Tick Single Tick
Used: 0 | Ready: 1
Sensor observation frequency: 0 Hz
Sensor faults: Missing Meas
-------------------------------
Sensor type: Gyroscope Y Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Gyroscope X Angular Rate
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer X Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Y Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
Sensor type: Accelerometer Z Specific Force
Used: 0 | Ready: 1
Sensor observation frequency: 49 Hz
Sensor faults: No Errors
-------------------------------
... 
W [(62301) app|appDeviceOffTask|702|: Device OFF triggered
D [(62312) app|appDeviceOffTask|704|: Disconnecting from MQTT
D [(62347) xplrMqttWifi|xplrMqttWifiUnsubscribeFromTopic|575|: Successfully unsubscribed from topic: /pp/ubx/0236/ip with id: 54410
D [(62371) xplrMqttWifi|xplrMqttWifiUnsubscribeFromTopic|575|: Successfully unsubscribed from topic: /pp/ip/eu with id: 56216
D [(62499) app|appDeviceOffTask|720|: Disconnected from MQTT
D [(62549) app|appDeviceOffTask|721|: Disconnecting from WiFi
I (62552) wifi:state: run -> init (0)
I (62553) wifi:pm stop, total sleep time: 46906946 us / 58200900 us

W (62554) wifi:<ba-del>idx
I (62556) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
W (62564) wifi:hmac tx: ifx0 stop, discard
D [(62588) xplrMqttWifi|xplrMqttWifiFsm|209|: MQTT config successful!
I (62603) wifi:flush txq
I (62604) wifi:stop sw txq
I (62605) wifi:lmac stop hw txq
D [(62605) xplrWifiStarter|xplrWifiStarterFsm|503|: WiFi disconnected successful!
W (62617) MQTT_CLIENT: Client asked to stop, but was not started
D [(62626) app|appDeviceOffTask|729|: Disconnected from WiFi
D [(62647) xplrMqttWifi|xplrMqttWifiFsm|219|: MQTT event register successful!
D [(62653) app|appDeviceOffTask|730|: Halting GNSS logging
D [(62670) app|appDeviceOffTask|732|: Successfully halted GNSS logging
D [(62676) app|appDeviceOffTask|745|: Clearing GNSS Backup Memory Configuration
W [(62705) xplrGnss|xplrGnssFsm|950|: Triggered backup config erase.
...
D [(62760) xplrMqttWifi|xplrMqttWifiEventHandlerCb|766|: MQTT event disconnected!
D [(62765) xplrGnss|gnssClearBackup|2618|: Clear Backup command issued successfully
D [(62940) xplrGnss|gnssClearBackup|2635|: Stored backup configuration erased from memory
D [(62940) xplrGnss|xplrGnssFsm|905|: Backup Configuration erased from memory
D [(62978) app|appDeviceOffTask|752|: Cleared GNSS Backup Memory Configuration
D [(62980) app|appDeviceOffTask|753|: Starting Save on Shutdown routine
D [(62994) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(62995) xplrCommonHelpers|xplrHlprLocSrvcSendUbxFormattedCommand|478|: Sent UBX data [12] bytes.
D [(63001) xplrGnss|gnssCreateBackup|2503|: Create Backup command issued successfully
D [(63009) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(63018) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(63051) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(63051) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(63081) xplrGnss|gnssBackupCreationAck|2542|: Backup Creation routine has not responded yet...
D [(63081) xplrGnss|xplrGnssFsm|932|: Waiting for Save on Shutdown routine to finish
D [(63111) xplrGnss|gnssBackupCreationAck|2550|: Backup Creation routine was successful!
D [(63111) xplrGnss|xplrGnssFsm|927|: Save on Shutdown complete. Turning off device
D [(63165) xplrGnss|xplrGnssFsm|987|: Trying to stop GNSS device.
D [(63165) xplrGnss|xplrGnssUbxMessagesAsyncStop|1396|: Trying to stop Gnss UBX Messages async.
D [(63168) xplrGnss|gnssAsyncStopper|2842|: Successfully stoped async function.
D [(63176) xplrGnss|xplrGnssNmeaMessagesAsyncStop|1365|: Trying to stop Gnss Get Fix Type async.
D [(63229) xplrGnss|gnssAsyncStopper|2842|: Successfully stoped async function.
D [(63229) xplrGnss|xplrGnssStopAllAsyncs|1424|: Stopped all async getters.
D [(63232) xplrCommonHelpers|xplrHlprLocSrvcDeviceClose|249|: ubxlib device closed!
D [(63240) xplrGnss|gnssDeviceStop|2411|: Sucessfully stoped GNSS device.
D [(63246) xplrGnss|xplrGnssFsm|990|: Device stopped.
D [(63982) app|appDeviceOffTask|760|: Save on Shutdown routine finished
D [(63998) app|appDeviceOffTask|762|: De-initializing application logging
D [(64008) app|appDeviceOffTask|764|: De-initialized application logging
I [(68108) app|appDeviceOffTask|769|: Device graceful shutdown performed!
W [(68108) app|appHaltExecution|672|: Halting execution
W [(68127) app|appHaltExecution|672|: Halting execution
```
**NOTE:**
- Vehicle dynamics will only be printed if the module has been calibrated.
- Dead reckoning is only available in ZED-F9R GNSS module.

<br>

## Build instructions

### Building using Visual Studio Code
Building this example requires to edit a minimum set of files in order to select the corresponding source files and configure Wi-Fi and MQTT settings using Kconfig.
Please follow the steps described bellow:

1. Open the `XPLR-HPG-SW` folder in VS code.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_gnss_save_on_shutdown` project, making sure that all other projects are commented out:
   ```
   ...
   # positioning examples
   #set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_config")
   #set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_lband_correction")
   set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_save_on_shutdown")
   ...
   ```
3. Open the [app](./main/hpg_gnss_save_on_shutdown.c) and select if you need logging to the SD card for your application.
   ```
   #define APP_SD_LOGGING_ENABLED      0U
   ```
   This is a general option that enables or disables the SD logging functionality for all the modules. <br> 
   To enable/disable the individual module debug message SD logging: 

   - Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the debug UART.
   ```
   ...
   #define XPLR_HPGLIB_SERIAL_DEBUG_ENABLED    1U
   ...
   #define XPLR_BOARD_DEBUG_ACTIVE            (1U)
   ...
   #define XPLRNVS_DEBUG_ACTIVE               (1U)
   ...
   #define XPLRHELPERS_DEBUG_ACTIVE           (1U)
   #define XPLRGNSS_DEBUG_ACTIVE              (1U)
   ...
   #define XPLRWIFISTARTER_DEBUG_ACTIVE       (1U)
   #define XPLRMQTTWIFI_DEBUG_ACTIVE          (1U)
   ```
   - Alternatively, you can enable or disable the individual module debug message logging to the SD card by modifying the value of the `appLog.logOptions.allLogOpts` bit map, located in the [app](./main/hpg_gnss_save_on_shutdown.c). This gives the user the ability to enable/disable each logging instance in runtime, while the macro options in the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) give the option to the user to fully disable logging and ,as a result, have a smaller memory footprint.
4. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to logged in the SD card.\
   For more information about the **logging service of hpglib** follow **[this guide](./../../../components/hpglib/src/log_service/README.md)**
   ```
   ...
   #define XPLR_HPGLIB_LOG_ENABLED             1U
   ...
   #define XPLRGNSS_LOG_ACTIVE                (1U)
   ...
   #define XPLRLOCATION_LOG_ACTIVE            (1U)
   #define XPLRNVS_LOG_ACTIVE                 (1U)
   ...
   #define XPLRWIFISTARTER_LOG_ACTIVE         (1U)
   ...
   #define XPLRMQTTWIFI_LOG_ACTIVE            (1U)
   ...

   ```
5. From the VS code status bar select the `COM Port` that the XPLR-HPGx has enumerated on and the corresponding MCU platform (`esp32` for **[XPLR-HPG2](https://www.u-blox.com/en/product/xplr-hpg-2)** and `esp32s3` for **[XPLR-HPG1](https://www.u-blox.com/en/product/xplr-hpg-1)**).
6. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `menu config` by clicking on the "cog" symbol present in the vs code status bar.
7. Navigate to the `Board Options` section and select the board you wish to build the example for.
8. Under the `Board Options` settings make sure to select the GNSS module that your kit is equipped with. By default ZED-F9R is selected.
9. Navigate to the [Dead Reckoning](./../../../docs/README_dead_reckoning.md) and Enable/Disable it according to your needs.
10. Go to `XPLR HPG Options -> Correction Data Source -> Choose correction data source for your GNSS module` and select the **Correction data source** you need for your GNSS module.
11. Copy the MQTT **Hostname** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Broker Hostname` and configure your MQTT broker host as needed. Remember to put **`"mqtts://"`** in front.\
    You can also skip this step since the correct **hostname** is already configured.
12. Copy the MQTT **Client ID** from **Thingstream**, go to `XPLR HPG Options -> MQTT Settings -> Client ID` and configure it as needed.
13. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point SSID` and configure you **Access Point's SSID** as needed.
14. Go to `XPLR HPG Options -> Wi-Fi Settings -> Access Point Password` and configure you **Access Point's Password** as needed.
15. Download the 3 required files from **Thingstream** by following this **[guide](./../../../docs/README_thingstream_certificates.md)**:
   - **Client Key**
   - **Client Certificate**
   - **Root Certificate**
16. **Copy and Paste** the contents of **Client Key**, named **device-\[client_id\]-pp-key.pem**, into **client.key** located inside the main folder of the project. Replace everything inside the file.
17. **Copy and Paste** the contents of **Client Certificate**, named **device-\[client_id\]-pp-cert.crt**, into **client.crt** located inside the main folder of the project. Replace everything inside the file.
18. **Root Certificate** is already provided as a file **root.crt**. Check if the contents have changed and if yes then copy and Paste the contents of **Root Certificate** into **root.crt** located inside the main folder of the project. Replace everything inside the file.
19. In **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** file change **APP_THINGSTREAM_REGION** macro according to your correction data region. Possible values are "EU", "US", "KR", "AU" and "JP".
20. In **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** file change **APP_THINGSTREAM_PLAN** macro according to your Thingstream subscription plan. Possible values are "IP" and "IPLBAND" as the plans that offer correction data via IP.
21. Click `Save` and then `Build, Flash and Monitor` the project to the MCU using the "flame" icon.
<br>

**NOTE**:
- In the described file names above **\[client_id\]** is equal to your specific **DeviceID**.
<br>

### Building using ESP-IDF from a command line
1. Navigate to the `XPLR-HPG-SW` root folder.
2. In [CMakeLists](./../../../CMakeLists.txt) select the `hpg_gnss_save_on_shutdown` project, making sure that all other projects are commented out.
3. Open the [xplr_hpglib_cfg.h](./../../../components/hpglib/xplr_hpglib_cfg.h) file and select debug options you wish to be logged in the SD card or the debug UART.
4. In case you have already compiled another project and the `sdKconfig` file is present under the `XPLR-HPG-SW` folder please delete it and run `idf.py menuconfig`.
5. Navigate to the fields mentioned above from step 7 through 20 and provide the appropriate configuration. When finished press `q` and answer `Y` to save the configuration.
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
**`CONFIG_XPLR_GNSS_DEADRECKONING_ENABLE`** | "Disabled" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | Enables or Disables Dead Reckoning functionality. |
**`CONFIG_XPLR_CORRECTION_DATA_SOURCE`** | "Correction via IP" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | Selects the source of the correction data to be forwarded to the GNSS module. |
**`CONFIG_XPLR_WIFI_SSID`** | "ssid" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | AP SSID name to try and connect to. | You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_WIFI_PASSWORD`** | "password" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | AP password to try and connect to.| You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
**`CONFIG_XPLR_MQTTWIFI_CLIENT_ID`** | "MQTT Client ID" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | A unique MQTT client ID as taken from **Thingstream**. | You will have to replace this value to with your specific MQTT client ID, either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**..
**`CONFIG_XPLR_MQTTWIFI_THINGSTREAM_HOSTNAME`** | "mqtts://pp.services.u-blox.com" | **[hpg_gnss_save_on_shutdown](./main/hpg_gnss_save_on_shutdown.c)** | URL for MQTT client to connect to, taken from **Thingstream**.| There's no need to replace this value unless the URL changes in the future. You can replace this value by either directly editing source code in the app or using **[KConfig](./../../../docs/README_kconfig.md)**.
<br>

## Local Definitions-Macros
This is a description of definitions and macros found in the sample which are only present in main files.\
All definitions/macros below are meant to make variables more identifiable.\
You can change local macros as you wish inside the app.

Name | Description 
--- | --- 
**`APP_PRINT_IMU_DATA`** | Switches dead reckoning printing messages ON or OFF.
**`APP_SERIAL_DEBUG_ENABLED `** | Switches debug printing messages ON or OFF.
**`APP_SD_LOGGING_ENABLED   `** | Switches logging of the application messages to the SD card ON or OFF.
**`KIB `** | Helper definition to denote a size of 1 KByte
**`APP_MQTT_PAYLOAD_BUF_SIZE `** | Definition of MQTT buffer size of 10 KBytes.
**`APP_LOCATION_PRINT_PERIOD `** | Period in seconds on how often we want our print location function \[**`appPrintLocation(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_DEAD_RECKONING_PRINT_PERIOD `** | Period in seconds on how often we want our print dead reckoning data function \[**`appPrintDeadReckoning(uint8_t periodSecs)`**\] to execute. Can be changed as desired.
**`APP_GNSS_I2C_ADDR `** | I2C address for **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** module.
**`APP_LBAND_I2C_ADDR`** | I2C address for **[NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)**  module.
**`APP_DEVICE_OFF_MODE_BTN`** | Device off button for triggering the graceful shutdown.
**`APP_DEVICE_OFF_MODE_TRIGGER`** | Device off button press duration in seconds in order to trigger graceful shutdown.
**`APP_THINGSTREAM_REGION`** | Correction data region.
**`APP_THINGSTREAM_PLAN`** | Thingstream subscription plan.
**`APP_MAX_TOPICLEN`** | Maximum topic buffer size.
**`APP_SD_HOT_PLUG_FUNCTIONALITY`** | Option to enable the hot plug functionality of the SD card driver (being able to insert and remove the card in runtime).
**`APP_INACTIVITY_TIMEOUT`** | Time in seconds to trigger an inactivity timeout and cause a restart.
**`APP_RESTART_ON_ERROR`** | Trigger soft reset if device in error state.
**`APP_ENABLE_CORR_MSG_WDG`** | Option to enable the correction message watchdog mechanism.
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../components/boards)** | Board variant selection.
**[hpglib/common](./../../../components/hpglib/src/common)** | Common functions.
**[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** | XPLR Wi-Fi connection manager.
**[xplr_mqtt](./../../../components/xplr_mqtt)** | XPLR MQTT manager.
**[hpglib/location_services/xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)** | XPLR GNSS location device manager.
**[hpglib/location_services/xplr_lband_service](./../../../components/hpglib/src/location_service/lband_service/)** | XPLR LBAND service manager.
**[hpglib/location_services/location_service_helpers](./../../../components/hpglib/src/location_service/location_service_helpers/)** | Internally used by **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/nvs_service](./../../../components/hpglib/src/nvs_service/)** | Internally used by **[xplr_wifi_starter](./../../../components/xplr_wifi_starter)** and **[xplr_gnss_service](./../../../components/hpglib/src/location_service/gnss_service/)**.
**[hpglib/log_service](./../../../components/hpglib/src/log_service/)** | XPLR logging service.
**[hpglib/sd_service](./../../../components/hpglib/src/sd_service/)** | Internally used by **[log_service](./../../../components/hpglib/src/log_service/)**.
