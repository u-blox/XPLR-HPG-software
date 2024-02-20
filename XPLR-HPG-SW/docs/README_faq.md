![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# FAQ
This section serves as the go to place for frequently asked questions when you may not find the answer you are looking from the documentation that is available through the repository. This will be a work in progress, expanding the questions list based on the issues submitted to our Github repository and from our customers input in general.

## Generic questions:<br>
- **Q:** How can I program / flash my XPLR-HPG kit?<br>
  **A:** There are two ways to flash your device:
    - By compiling the example you are interested for from sources, using VS Code and ESP-IDF extension.
    - Using ESP-IDF tools through a command line window.

    Both options are covered through the example READMEs that are available in the repository.
- **Q:** What version of esp-idf and esp tools should I use?<br>
  **A:** Current release (V1.1) is based on `ESP-IDF Tools v1.7.0` and `ESP-IDF SDK v4.4.4`.
- **Q:** I 'm having issues installing recommended tools. What should I do?<br>
  **A:** It is recommended to start clean by uninstalling your current ESP-IDF tools and SDK and follow [this](https://content.u-blox.com/sites/default/files/documents/XPLR-HPG_Required_Software_App_Note_UBX-23008217.pdf) guide.
- **Q:** I 'm connecting my XPLR-HPG kit to my computer using a USB cable but I do not see it enumerated. What should I do?<br>
  **A:** Make sure that you have downloaded and install the [CP210x USB-to-Serial](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads) driver and that your USB cable is not a power only but a power and data cable.
- **Q:** I 'm trying to program / flash my C213 board but I m getting an error message. What should I do?<br>
  **A:** The auto-reset function sometimes may fail, prohibiting any flashing action. Put the device in boot mode manually by holding the reset and boot buttons then releasing the reset and 1-2 seconds after the boot.
- **Q:** Should I have a battery attached to my XPLR-HPG kit?<br>
  **A:** It is highly recommended to use a battery when evaluating / testing a cellular based example.
- **Q:** What type of battery should I use for my C213 board?<br>
  **A:** C213 boards accept lithium batteries of 18650 type that do not include a protection circuit.
- **Q:** Where can I find more info for my XPLR-HPG kit?<br>
  **A:** Datasheets, userguides, application notes and other related material can be found in u-blox's webpage through the [XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1) and [XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2) links.
- **Q:** I 'm facing issues with my XPLR-HPG kit, who should I contact?<br>
  **A:** You can either create an issue in our Github repository or create a case in our [support portal](https://portal.u-blox.com/s/).
- **Q:** I have a software request for my XPLR-HPG kit, who should I contact?<br>
  **A:** Github issues are always welcomed.

## Framework related:<br>
- **Q:** I want to add more components to my project, what should I do?<br>
  **A:** To add a new component just place your component folder (along with its `CMakeLists.txt`) file under the [components](./../components/) folder of the repository and edit the [project.cmake](./../project.cmake) file accordingly.
- **Q:** Erase-flash command does not delete NVS data. How can I erase NVS memory?<br>
  **A:** NVS region can be explicitly deleted by running the `esptool.py erase_region 0x9000 0x6000` command from ESP-IDF terminal.

## Software related:<br>
- **Q:** I want to log data to the SD card. What should I do?<br>
  **A:** To activate the SD card logging functionality in any example of the repo, make sure that:
  - `XPLR_HPGLIB_LOG_ENABLED` macro definition is active in the [xplr_hpglib_config.h](./../components/hpglib/xplr_hpglib_cfg.h) file and you have activated the logging of the module you want to keep logs for.
  - `APP_SD_LOGGING_ENABLED` macro definition is active the "main" file of the example your are running.

  For more information have a look at the [logging](./../components/hpglib/src/log_service/README.md) module documentation.
- **Q:** I want to activate the dead reckoning feature of my ZED-F9R module. What should I do?<br>
  **A:** Dead reckoning functionality of the ZED-F9R module is controlled by the "menuconfig" options of each example.<br>
  When activated it is also possible to print IMU related data to the debug console by activating the `APP_PRINT_IMU_DATA` macro definition under the "main" file of the example you are running.
- **Q:** How can I calibrate my ZED-F9R module for dead reckoning functionality?<br>
  **A:** Dead reckoning functionality requires a calibration session before using it. It is advised that you place the module in a fixed place in your car / bike / moped and perform a couple of "8 figures" pattern. For more information have a look at the [dead reckoning](./README_dead_reckoning.md) readme file.
- **Q:** I 'm getting multiple messages to my serial terminal. How can I limit what is printed out?<br>
  **A:** You can control which modules print information messages to the debug console through the `<MODULE NAME>_DEBUG_ACTIVE` macro definitions list in the [xplr_hpglib_config.h](./../components/hpglib/xplr_hpglib_cfg.h) file.
- **Q:** I want to evaluate one of the cellular based examples. Is there anything I should pay attention to?<br>
  **A:** To run any of the cellular based examples make sure that you have an active data plan for the SIM card you are using and that you have properly configured the APN of your provided through the menuconfig.
- **Q:** I 'm trying one of the cellular based examples but I cannot connect to a network provider. What should I do?<br>
  **A:** Make sure that you have an active data plan for your SIM card and you have also configured your carrier's APN via the menuconfig. Further more the cellular examples configure the LARA-R6 module to work with LTE networks. You can choose other network technologies by modifying the `ratList` inside the `configCellSettings` function which is available in all cellular based examples.
- **Q:** I 'm trying one of the cellular based examples but I cannot connect to Thingstream. What should I do?<br>
  **A:** Make sure that you have an active data plan for your SIM card and you have also configured your carrier's APN via the menuconfig. Check also that you have provided the correct Thingstream certificates and that you have not included any `\r\n` characters.
- **Q:** I 'm trying one of the cellular based examples but I 'm getting multiple `BROWNOUT RESET`s. What should I do?<br>
  **A:** Cellular based examples might need more current than your USB host can provide. Make sure you have a battery connected to your kit to overcome such issues.
- **Q:** I 'm trying one of the cellular based examples but I see numerous disconnects from the network. What should I do?<br>
  **A:** Cellular based examples might need more current than your USB host can provide. Make sure you have a battery connected to your kit to overcome such issues.
- **Q:** I 'm trying one of the Wi-Fi based examples but my XPLR-HPG kit cannot connect to my router / access point. What should I do?<br>
  **A:** Double check that you have provided the correct SSID and Password through menuconfig. Also make sure that you are using a 2.4GHz and not a 5GHz router or access point. 
- **Q:** I 'm trying one of the Wi-Fi based examples but I cannot connect to Thingstream. What should I do?<br>
  **A:** Make sure that you have copied and paste your Thingstream credentials into the `client.crt`, `client.key` and `root.crt` files under the main folder of the example you are running.
- **Q:** I 'm trying the Captive Portal but I do not see my XPLR-HPG kit in my Wi-Fi list while scanning for it. What should I do?<br>
  **A:** Make sure that you are trying to connect to a 2.4GHz router / access point and not a 5GHz one.
- **Q:** I 'm trying the Captive Portal example but cannot access it using the http://xplr-hpg.local url. What should I do?<br>
  **A:** There might be network settings and restrictions preventing the XPLR-HPG device to advertise itself to the network. Try accessing the device using its local IP address at `192.168.4.1 `
- **Q:** When initializing the GNSS module I see `hpgLocHelpers|xplrHlprLocSrvcDeviceOpenNonBlocking|212|: uDeviceOpen failed with code <-8>` error message. What is wrong?<br>
  **A:** This is because of a deprecated message that is used to identify that a GNSS device is active. It will be fixed in our next release.
- **Q:** ZED module takes too long to print location info. What is wrong?<br>
  **A:** Apart from weather and environmental conditions that might affect how fast the GNSS module can get location data it is also important if your device is starting from a "hot" or "cold" start.<br>
  Hot start is the case where the GNSS module has not lost power (eg. the device has not disconnected from the USB host) while re-flashing or restarting it. In such case it is expected that the GNSS module will get location data almost instantly after such a reboot.
- **Q:** There are errors while reading the device configuration from NVS. What should I do?<br>
  **A:** Make sure that you erase the NVS by running the `esptool.py erase_region 0x9000 0x6000` command from an ESP-IDF terminal and try again.