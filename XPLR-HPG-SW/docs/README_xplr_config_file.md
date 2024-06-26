![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Configuring the HPG board via the SD card

To configure your HPG board via the SD card you need to create a file named `xplr_config.json`, based on **[this template file](./../bin/xplr_config_template.json)**

Then, you need to populate the fields according to your desired configuration:

1. **AppSettings**: These are options regarding the application
    - `RunTimeUtc` : This field defines the application's total run time in seconds. After this time has elapsed the application will terminate. Default value is 604800 seconds (7 days).
    - `LocationPrintInterval` : This field defines the interval, in which the location print messages will be printed to the console and/or be logged in SD card, in seconds.
    - `StatisticsPrintInterval` : This field defines the interval, in which the application statistics (messages received, bytes received and uptime in seconds) will be printed to the console and/or be logged in SD card, in seconds.
    - `MQTTClientWatchdogEnable` : This field enables (when true) or disables (when false) the MQTT client watchdog mechanism. If enabled, the MQTT watchdog is fed from the application when correction data (either IP or LBAND) is forwarded to the GNSS module successfully. When this watchdog gets triggered (thus no correction data are fed to the GNSS and RTK accuracy is unachievable) a reset of the MQTT client is performed.
2. **CellSettings**: These are options regarding the configuration of the cellular module
    - `APN` : This field contains the APN value of cellular provider to register. Even if your application does not make use of the cellular module this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
3. **WifiSettings**: These are options regarding the WiFi configuration of the HPG board
    - `SSID` : This field contains the Access Point's SSID name to try and connect to. Even if your application does not make use of WiFi this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
    - `Password` : This field contains the Access Point's password to try and connect to. Even if your application does not make use of WiFi this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
4. **ThingstreamSettings**: These are options regarding the configuration of the thingstream module 
    - `Region` : This field contains the Thingstream region to fetch correction data for. Possible values are `"EU"` and `"US"`, as the only supported regions, as of now. Even if your application does not make use of the thingstream module this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
    - `ConfigFilename` : This field contains the filename of the ucenter-config file containing the credentials of the connection to Thingstream. Filename must not exceed 64 characters. Even if your application does not make use of the thingstream module, or does not use the file to fetch the credentials, this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
    - `ZTPToken` : This field contains the token for the Zero Touch Provisioning (ZTP) to Thingstream. Even if your application does not perform ZTP this field **must** exist, even empty. Otherwise, the configuration parse will result to error.
5. **NTRIPSettings**: These are options regarding the configuration of the NTRIP client service of hpglib. Even if your application does not include the NTRIP client service these fields **must** exist, even empty. Otherwise, the configuration parse will result to error. 
    - `Host` : This field contains the URL/IP address of your NTRIP caster (without the port).
    - `Port` : This field contains the port t use in order to access the NTRIP caster. Default value is 2101.
    - `MountPoint` : This field contains the NTRIP caster mountpoint to use.
    - `UserAgent` :  This field contains the User Agent to identify the device.
    - `SendGGA` : This field should be true (default value) if your NTRIP caster requires to send periodic GGA message. If not, set value to false.
    - `UseAuthentication` : This field should be true if your NTRIP caster requires authentication, If not, set value to false (default value).
    - `Username` : This field contains the client's username. Will be used only if `UseAuthentication` is set to true.
    - `Password` : This field contains the client's password. Will be used only if `UseAuthentication` is set to true.
5. **LogSettings**: These are options regarding the logging module of the application and the individual modules
    - `Instances` : This is an array containing the different logging instance options and configuration. The size of the array can be from 0 (empty array), up to `XPLR_LOG_MAX_INSTANCES` instances (macro defined in **[xplr_hpglib_cfg.h](./../components/hpglib/xplr_hpglib_cfg.h)**). The individual instance must have the following fields populated:
        - `Description` : A string describing the instance's relation to an individual module of the application (should be left to default values).
        - `Enable` : A boolean flag indicating the option to enable the particular logging instance or not. If true, a logging instance will be created with the defined configuration and the module will log its messages to the SD card. If false, logging for the selected module will be disabled and no message will be logged in the SD card.
        - `Filename` : A string containing the name of the logging file that will be created in the SD card to log the particular modules messages (if `Enable` flag is `true`). Can be chosen freely by the user, as long as the name is smaller than 64 characters, and it does not contain parenthesis characters `(` or `)`. 
        - `ErasePrev` : A boolean flag that selects if the user wants to erase a potential existing file in the SD card and replace it with the new file, or simply append to it. When the logging file is to be created during the particular logging instance initialization, a check will be made in the SD card for a file with the same `Filename`. If it exists and the `ErasePrev` flag is `true`, then the old file will be deleted and replaced with the new one, if `false` then the logs will be appended to the previous file.
        - `SizeIntervalKbytes` : Indicates the number of KBytes interval between the filename increment mechanism. When a log file reaches this size (in KBytes), then a new file will be created with an incremented name and the logging will continue there. The maximum size of a file in a FAT system is 4GB, so this is the maximum `SizeIntervalKbytes` value.
    - `FilenameUpdateInterval` : Value indicating the time interval between the logging filenames update. The filename is updated using the current time timestamp from the GNSS receiver with the format **"YYYY-HH-MM-DD-hh-mm-ss-*Filename*"**. The mechanism for the filename update is performed by the individual application. An example of usage of this mechanism can be found in the **[cell_mqtt_correction_certs_sd_autonomous example](./../examples/cellular/05_hpg_cell_mqtt_correction_certs_sd_autonomous/)**.
    - `HotPlugEnable` : Boolean flag that enables (when `true`) or disables (when `false`) the hot plug mechanism of the SD card. The hot plug mechanism gives the ability to remove the SD card during runtime and ,when inserted again, the operation will resume (logging of modules and application). **#NOTE**: Enabling this functionality results in a significant increase to the memory footprint of the application.
6. **DeadReckoningSettings**: These are options regarding the configuration of the dead reckoning functionality of the GNSS module. For more information on Dead Reckoning please read **[this guide](./README_dead_reckoning.md)**
    - `Enable` : Boolean flag indicating the option to enable (when `true`) or disable (when `false`) the Dead Reckoning functionality.
    - `PrintIMUData` : Boolean flag indicating the option to enable (when `true`) or disable (when `false`) the serial print of the IMU data. Even if your application does not enable Dead Reckoning (`Enable` flag is `false`), this field **must** exist. Otherwise, the configuration parse will result to error.
    - `PrintInterval` : A value indicating the time interval between IMU data serial prints. Relevant only when (`PrintIMUData` is true). Even if your application does not enable Dead Reckoning (`Enable` flag is `false`), or you do not need to print the IMU data (`PrintIMUData` is `false`) this field **must** exist. Otherwise, the configuration parse will result to error.
7. **GNSSModuleSettings**: These are options regarding the configuration and setup of the GNSS module.
    - `Module` : String that defines the GNSS module that is present to your kit. Valid options are `"F9R"` or `"F9P"`.
    - `CorrectionDataSource` : String that defines the selected correction data source for the GNSS module. Valid options are `"IP"`, indicating that the correction data will be fed via the internet (MQTT), or `"LBAND"`, indicating that the correction data will be fed by the NEO-D9S module (SPARTN). Please note that for the IP option a valid subscription plan to Thingstream is required (IP or IP + LBAND plans). Also note that for the LBAND option also a valid subscription is required (LBAND only or IP + LBAND plans), the only supported regions as of now are Europe and America and for the correct initialization of the modules, an active connection to the internet (either via WiFi or Cellular) is mandatory.