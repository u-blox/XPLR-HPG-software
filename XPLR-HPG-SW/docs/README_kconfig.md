![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Using KConfig

## Description 
**KConfig** is a tool that allows for certain aspects of you **ESP-IDF** project to be configured easily before compiling.\
These settings are persistent during compilations unless the user changes something.\
Software components and modules often have different options exposed via **KConfig**.\
When running **KConfig**, depending on your development tools, a menu will open showing all related settings/options related to a certain component/module.

## Using on VScode
### Steps
Using **KConfig** in **VScode** is quite easy. You can do it by following the steps:
1. Locate the **Cog** icon in the **ESP-IDF** toolbar at the bottom of your **IDE**.
2. A Menu will appear.
3. Search for the settings you wish to edit.\
   Note: Inside the code **KConfig** settings use the **CONFIG_** prefix.\
   In order to get the exact match for your option you have to remove the prefix:\
   e.g. Lets assume we want to enter an **SSID** for our board to connect to.
   We know that the corresponding **KConfig** option is named **```CONFIG_XPLR_WIFI_SSID```**, that means that we have to search for **```XPLR_WIFI_SSID```**.
   By doing so we get exactly the setting we are looking for.
4. Enter your value and hit **Save**.
5. Compile your code!

### Video
Follow the video below for a more detailed explanation of the steps above:\
[![KConfig](./../media/shared/misc/vid_rsz.jpg)](https://youtu.be/NphIvv4sEqg)

<br>
<br>

## Caveats
Every time you change something using **KConfig** the compiler will re-compile all source files.