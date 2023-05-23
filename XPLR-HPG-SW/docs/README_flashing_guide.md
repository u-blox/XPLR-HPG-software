![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Flashing procedure for Windows systems

To flash your **XPLR-HPG** kit you need to use the `esptool` script.

From a command line window run the Windows executable `esptool.exe` using the arguments described below (No setup needed, go to [Flash command](#flash-command) section).

(`esptool.exe` is located in misc directory)


Each example software has 3 binary files:

- **bootloader.bin**
- **[SW_NAME].bin**
- **partition-table.bin**


<br>
<br>

## Flash command arguments


```
misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip [MCU_TYPE] write_flash --flash_mode dio --flash_freq [FREQ] --flash_size detect [BOOTLOADER_ADDR] bootloader/bootloader.bin  0x10000 [SW_NAME].bin 0x8000 partition_table/partition-table.bin
```

 `COM_PORT` and `SW_NAME` vary based on your installation and folder structure as well as the name of the software you want to flash.

|Variables / Boards  |HPG-1 (C213)                   |HPG-2 (C214)                 |
|--------------------|-------------------------------|-----------------------------|
|MCU_TYPE            |`esp32s3`                      |`esp32`                      |
|FREQ                |`80m`                          |`40m`                        |
|BOOTLOADER_ADDR     |`0x0`                          |`0x1000`                     |

**For example:**



To flash you C214 board with the hpg_wifi_mqtt_correction_captive_portal example from our repository you would need the command:

```
misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x1000 bin/releases/C214/hpg_wifi_mqtt_correction_captive_portal/bootloader/bootloader.bin 0x10000 bin/releases/C214/hpg_wifi_mqtt_correction_captive_portal/hpg_wifi_mqtt_correction_captive_portal.bin 0x8000 bin/releases/C214/hpg_wifi_mqtt_correction_captive_portal/partition_table/partition-table.bin
```
<br>

To flash you C213 board with the hpg_wifi_mqtt_correction_certs_captive_portal example from our repository you would need the command:

```
misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 bin/releases/C213/hpg_wifi_mqtt_correction_captive_portal/bootloader/bootloader.bin 0x10000 bin/releases/C213/hpg_wifi_mqtt_correction_captive_portal/hpg_wifi_mqtt_correction_captive_portal.bin 0x8000 bin/releases/C213/hpg_wifi_mqtt_correction_captive_portal/partition_table/partition-table.bin
```
<br>

To flash you MAZGCH board with the hpg_wifi_mqtt_correction_captive_portal example from our repository you would need the command:

```
misc\esptool.exe -p [COM_PORT] erase_flash && misc\esptool.exe -p [COM_PORT] -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x1000 bin/releases/MAZGCH/hpg_wifi_mqtt_correction_captive_portal/bootloader/bootloader.bin 0x10000 bin/releases/MAZGCH/hpg_wifi_mqtt_correction_captive_portal/hpg_wifi_mqtt_correction_captive_portal.bin 0x8000 bin/releases/MAZGCH/hpg_wifi_mqtt_correction_captive_portal/partition_table/partition-table.bin
```
<br>
<br>


# Flashing procedure for MacOS/Linux systems

The same functionality as the `esptool.exe` can be achieved using the `esptool.py` python script.

You will need Python 3.7 or newer installed on your system to use the latest version of `esptool.py`. 

(`esptool.py` and `requirements.txt` are located in misc directory)

<br>
<br>

## Notes

- To avoid any configuration conflicts, it is **highly recommended** to erase the chip's memory before flashing a new binary.

Use the command 
```
misc\esptool.exe -p [COM_PORT] erase_flash
``` 

- If you experience serial port related communication problems, you may need to install the [CP210x USB Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=overview)


- Example commands to be run from `XPLR-HPG-SW` directory




