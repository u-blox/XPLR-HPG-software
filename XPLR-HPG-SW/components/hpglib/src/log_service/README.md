![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR Log lib

## Description
This is a library to help implement functionality for logging data to an SD card via SPI\
The main scope of this library is to make device initialization and data logging as fast and easy as possible.

Please note that every example and module of **hpglib** has built-in logging functionality. You are able to enable or disable logging for all modules (collectively or individually) via the macros in **[xplr_hpglib_cfg.h file](./../../xplr_hpglib_cfg.h)**. The **enabled** logging modules can be halted and restarted at runtime via a flag in the log struct or via specific functions (such as xplrGnssHaltLogModule and xplrGnssInitLogModule). Logging is initialized and enabled automatically in every **enabled** module (by the macros in **[xplr_hpglib_cfg.h file](./../../xplr_hpglib_cfg.h)**) initialization function. In a future release an option will be added for each module to be able to enable/disable the automatic initialization of the logging module.

**NOTE #1**: Beware of the available heap size of your code before enabling multiple log instances. You can use the xplrMemUsagePrint function located in **[xplr_common.h file](./../common/xplr_common.h)** to debug such potential issues.  
**NOTE #2**: In case the SD card is removed from the slot (even when logging is halted), a reboot is needed for the logging to be restarted, as there is **no hot-plug** functionality in the library.   
**NOTE #3**: **[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)** board's SD Detect Pin is connected with the CS pin of the SD card's SPI interface via a NOT gate, that ensures that when the card is in the SD card slot the CS Pin is left floating, but when it is not in the SD card slot the CS pin is driven low. However, there is an issue with the reading of the CS pin (cannot be configured in INPUT mode), thus the functionality of the xplrBoardDetectSd() function cannot be the same as the other two board variations (that have a dedicated Card Detect pin for the SD card). For more information about the GPIO restrictions of the esp32s3 please visit **[esp32s3 gpio restrictions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html)**.\
**NOTE #4**: **XPLRLOG_DEBUG_ACTIVE** and **XPLRSD_DEBUG_ACTIVE** macros in **[xplr_hpglib_cfg.h file](./../../xplr_hpglib_cfg.h)** enable or disable the debug messages for the corresponding libraries. They are **off** by default, since they may produce an overwhelming amount of message prints in the console (depending on the logging configuration and the amount of logs). Users are advised to *enable* these debug messages only in case of debugging possible SD errors, otherwise, there is possibility for timeout warnings throughout the execution of the **[examples](./../../../../examples/)**.

<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**```LOG_MAXIMUM_NAME_SIZE```** | **```20```** | Maximum size of name allowed for the logging file's name. You can replace this value freely.
**```LOG_BUFFER_MAX_SIZE```** | **```256```** | Maximum size internal logging buffer. You can replace this value freely. As you increase this value the write operations to the SD will be more sparse, but the write operation will take more time.

<br>
<br>

## Other Definitions-Macros used by logging service
Macro/definitions inherited by **[sd_service](./../sd_service/xplr_sd.c)** and are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**  

Name | Value | Description
--- | --- | ---
**```CONFIG_FORMAT_IF_FAILED```** | **```1U```** | If set to 1 the SD card will be automatically formatted in case of failure to mount to filesystem. If this is not desired set the value to 0.
**```CONFIG_MAX_FILES_OPEN```** | **```2```** | Maximum number of files allowed to be open at the same time. The modification of this value is **not** recommended as it greatly affects heap usage from the log service.
**```CONFIG_ALLOC_UNIT_SIZE```** | **```8```** | Minimum file size (in KBytes). You can replace this value freely. Will change only if card is formatted. 
**```CONFIG_MAXIMUM_FILES```** | **```10```** | Maximum files allowed in filesystem. You can replace this value freely.
**```MOUNT_POINT```** | **```"/sdcard"```** | Filesystem's mountpoint. You can replace this value freely. Follow the format "/*your_mountpoint*" or set it to empty string "" for correct operation.
**```DEL_EXCEPTION```** | **```MOUNT_POINT"/SYSTEM~1"```** | File name that is protected in full erase of sd's filesystem. You can replace this value freely. Keep **MOUNT_POINT"*/your_protected_file*"** format for correct operation.
**```MAX_TIMEOUT_SEC```** | **```1*1```** | Maximum timeout in seconds before an erase all operation is aborted. You can replace this value freely. The smallest the value the less probability for a bottleneck to the code, with the drawback of potential files to be unable to be deleted, due to big size.

<br>
<br>

## Usage
In order to use this library you need to follow these steps:

1. Include the "xplr_log.h" header file to your code 

2. Create an empty **xplr_log_t** struct (It can be more than one, or even an array of xplr_log_t structs).

3. Initialize the logging by calling the **xplrLogInit** function for each struct created in the previous step. \
   The  **xplrLogInit** function needs to have the following arguments:
   Parameter Name | Type | Description
   --- |--- |---
   **```*xplrLog ```** | **```xplrLog_t```** | Pointer to the log struct to be initialized
   **```tag ```** | **```xplrLog_dvcTag_t```** | Selects the type of logging to be made. If the type of data to be logged are console messages it is best to be logged as strings, but when the data to be logged is serial communication data it would be best to be logged in binary format. To pick the correct tag check the possible tags in the **xplrLog_dvcTag_t** enumeration in the **[xplr_log.h](./xplr_log.h)** file.
   **```*logFilename```** | **```char```** | String pointer containing the desired filename of the log file. More than one log structs can have the same log filename. Cannot be NULL. The format needs to be *"/your_desired_filename"* . **Please follow FAT32 compatible naming**.
   **```maxSize```** | **```uint16_t```** | Unsigned integer to show file max allowed size (e.g. if 10MB is to be the max size this value should be 10). Since the filesystem is in the format of FAT32, the maximum file size cannot be greater than 4GB.
   **```maxSizeType```** | **```xplrLog_size_t```** | Size type value to indicate the size (e.g. if 10MB is to be the max size this value should be XPLR_SIZE_MB). Check the possible values in the **xplrLog_size_t** enumeration in the **[xplr_log.h](./xplr_log.h)** file, or in the **xplrSd_size_t** enumeration in the **[xplr_sd.h](./../sd_service/xplr_sd.h)** file.

4. After successful initialization of the **xplrLog_t** structs the logging is performed by the macro command **XPLRLOG**.The arguments are:
    Parameter Name | Type | Description
    --- |--- |---
    **```*xplrLog ```** | **```xplrLog_t```** | Pointer to the log struct-instance in which the data is to be stored.
    **```*message ```** | **```char```** | Pointer to the data to be logged in the SD. If the data is not of type char, still the pointer needs to be a char pointer (can be done with casting to char) for the service to be executed successfully. In case of uint8_t data the casting will be undone in the function, assuming the xplrLog_t struct's tag is of type XPLR_LOG_DEVICE_ZED or XPLR_LOG_DEVICE_NEO.

<br>
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../../../boards/)** | Board variant selection
**[sd_service](./../sd_service/)** | SD card driver functions.