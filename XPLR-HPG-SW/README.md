![u-blox](./media/shared/logos/ublox_logo.jpg)
<br>
<br>

# XPLR-HPG

High precision positioning solution power by u-blox.

## Intro
**[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)** and **[XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2)** are u-blox projects made from developers for developers. Their purpose is to facilitate any developer to write his own application based on u-blox modules by providing tested and well documented code snippets and examples.


## Development Environment
Examples provided are tested and verified to run under Windows using the tools recommended by Espressif.

### Dependencies
* [Microsoft VS Code](https://code.visualstudio.com/).
* [ESP-IDF extension for VS Code](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md).
* [ESP-IDF core](https://github.com/espressif/esp-idf/tree/v4.4.2) version 4.4.2
* [ubxlib for HPG](https://github.com/u-blox/ubxlib/tree/hpg)
* [CP210x USB-to-Serial driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads)
* **LARA** (cellular module) firmware should be: **00.13,A00.01**
* **ZED-F9R** (GNSS module) firmware should be: **1.30**
* **NEO-D9S** (LBAND module) firmware should be: **1.04**
* In case of Wi-Fi dependent examples please make sure that you are using the **2.4GHz** band.


## Hardware Used
The project is meant to be tested and run on **[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)**/**[2](https://www.u-blox.com/en/product/xplr-hpg-2)** kits.<br>
Below is a list of the available hardware on the evaluation boards.
| u-blox Module                                                           | Function                                                     |
|:------------------------------------------------------------------------|:-------------------------------------------------------------|
| [NINA-W106](https://www.u-blox.com/en/product/nina-w10-series-open-cpu) | An ESP32 based WIFI and BT module with Open CPU that will host the application (available on **[XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2)**).     |
| [NORA-W106](https://www.u-blox.com/en/product/nora-w10-series) | An ESP32S3 based WIFI and BT module with Open CPU that will host the application (available on **[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)**).     |
| [LARA-R6001D](https://www.u-blox.com/en/product/lara-r6-series)         | A LTE Cat 1 cellular modem with 2G / 3G fallback.            |
| [ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)             | A GNSS Module with RTK, DR and L1/L2 multiband capability.   |
| [NEO-D9S](https://www.u-blox.com/en/product/neo-d9s-series)             | A LBAND receiver to get PointPerfect correction service.     |

For more hardware related info please consult the [XPLR-HPG-1 user guide](http://www.u-blox.com/docs/UBX-23000692) or the [XPLR-HPG-2 user guide](http://www.u-blox.com/docs/UBX-22039292), depending on the kit you have purchased.

## Project Structure
The project is structured similar to other ESP-IDF components and examples.

| Folder         | Content                                                                                 |
|:---------------|:----------------------------------------------------------------------------------------|
| [bin](./bin/)                 | Contains third party tools and vscode templates for configuring your workspace. |
| [components](./components/)   | Contains libraries (esp-idf components) which can be used by any example. |
| [docs](./docs/)               | Documents that are related to the hardware and software examples.        |
| [examples](./examples/)       | A set of examples showing u-blox modules functionality.                  |
| [media](./media/)             | Photos and/or videos used in project's documentation.                    |
| [misc](./media/)              | vscode settings and other helper files.                                  |

Examples and components are using relative paths meaning that they are portable across different machines.

## ESP-IDF Framework
[ESP-IDF](https://www.espressif.com/en/products/sdks/esp-idf) is a software development framework that allows ESP32 users
to start writing code without having to deal with low level drivers in order to access and configure their MCU.
Furthermore it provides higher level functions and wrappers that enable developers to build a variety of network-connected products.

The framework sits on top of a RTOS namely [FreeRTOS](https://www.freertos.org/) thus making easy to create multithreaded applications without
dealing with complex timing issues.<br>
In order to pack everything together and build a project, the ESP-IDF relies on its own tools and compiler.<br>
The tools provided by Espressif are making use of CMake and KConfig files which when combined together (along with the source code) they create what is known
as an "idf-component".

### IDF-Components
According to Espressif each project contains one or more components which can be seen as libraries.<br>
A component is any directory in the ```COMPONENT_DIRS``` which contains a ```CMAKELists.txt``` file.<br>
XPLR-HPG-SW repository is structured as such all the components required to build any of the examples are placed under a common folder located in the root folder ([components](./components/)).<br>
<!-- At the root of the repository you will find 2 files, namely [project.cmake](./project.cmake) and [project.mk](./project.mk). Both of these files register the [components](./components) folder to the ESP-IDF toolchain.<br>
The difference is that ```.cmake``` file is used for registering the ```components``` folder in the CMake-based ESP-IDF [build system](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/build-system.html) whereas the ``.mk`` file was used by the older, GNU Make based system.<br>
For more info regarding the ESP-IDF components, it is strongly recommended to go through the "Component" sections of the [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/build-system.html)<br> -->

At the root of the repository you will find the [project.cmake](./project.cmake) file. This  file registers the [components](./components) folder to the ESP-IDF toolchain.<br>
It is used for registering the ```components``` folder in the CMake-based ESP-IDF [build system](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/build-system.html).<br>
For more info regarding the ESP-IDF components, it is strongly recommended to go through the "Component" sections of the [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/build-system.html)<br>

**Note:** In case you need to add more components in the XPLR-HPG-SW repo, remember to update the include  paths into these files.

### MenuConfig
ESP-IDF framework provides a graphical environment, known as ```MenuConfig```, in order to configure many of its components and the freeRTOS.<br>
Before building any of the examples provided in this repo it is required to run the ```MenuConfig``` once, from the root of the [XPLR-HPG-SW](./) workspace.<br>
Doing so will require to:
1. Clone the [XPLR-HPG](https://github.com/u-blox/XPLR-HPG-software) repository. Remember to update included submodules as well.
2. Navigate to it, right-click on the ```XPLR-HPG-SW``` folder and ```Open with Code``` (or open the ```VS code``` app and open the folder from the app menu).
3. Open the [CMakeLists.txt](./CMakeLists.txt) located in the root of the project.
4. Select the example you wish to build by activating the corresponding ```set(ENV{XPLR_HPG_PROJECT} <example_name>)``` command.
5. Run ```MenuConfig``` using either the "cog" symbol present in the [Status bar](https://code.visualstudio.com/docs/getstarted/userinterface) or by opening the command palette (```Ctrl+Shift+P```) and typing
```ESP-IDF:SDK Configuration editor (menuconfig)```.
6. Check / Edit the current configuration according to your needs (see related README file inside example folder).
7. Click on ```Save``` button, present at the top of the menu.
8. Done, you are ready to build and flash your example on the XPLR-HPG-1/2 board by clicking on the "thunder" or the "flame" symbols, available in the status bar.

**NOTE:** **Only one (1)** ```set(ENV{XPLR_HPG_PROJECT} <example_name>)``` command can be active at a time!

Running the ```menuconfig``` will generate a new file inside the example's root folder called sdkconfig.<br>
For convenience we have added a file called sdkconfig.defaults which overrides some settings of ```menuconfig```. Feel free to modify it according to your needs.

More info on ```menuconfig``` can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)

**Note:** Do **not** edit the sdkconfig file manually. **Either** use the ```menuconfig``` and save your changes **or** delete the sdkconfig file (if present), add your settings in the sdkconfig.defaults file and run the ```menuconfig```.

### KConfig files
Going through the [examples](./examples/) folders you will notice that there are some ```Kconfig.projbuild``` files.<br>
These files are used to add extra configuration menus which become available through ```menuconfig```.<br>
* ```Kconfig.projbuild``` files are used to create new menu entries in ```menuconfig```.

Again, for more info regarding ```menuconfig``` it is strongly recommended to go through the [Project Configuration sections](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#) of ESP-IDF Programming Guide.

## Flashing Binaries
It is possible to flash pre-compiled binaries, without using VS Code or having to download and install the ESP-IDF framework.<br>
As an example we have provided, in binary format, the [Captive Portal Demo](./examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/) application under the [releases](./bin/releases/) folder along with [flashing instructions](./docs/README_flashing_guide.md).<br>
If need be to create binaries from another example, it is required to build the project and grab the generated binaries from the `./build` folder. Minor adjustments to match the `project_name`, `selected target` and `com port` in the build command will be needed but the process should be straight forward.
