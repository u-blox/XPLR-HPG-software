![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Using ZED-F9R Dead Reckoning

## Description
**Dead Reckoning** is the ability of the GNSS module to estimate/calculate location based on its internal sensor (IMU) when there's no satellite coverage e.g. tunnels or underground parking lots.\
Function is based on the calculation of vehicle dynamics (accelerations) with the last known location from satellites coverage.\
You can find detailed information regarding **Dead Reckoning** and **High precision sensor fusion (HPS)** on **[ZED-F9R](https://www.u-blox.com/en/product/zed-f9r-module)** product page under **[Documentation & Resources](https://www.u-blox.com/en/product/zed-f9r-module?legacy=Current#Documentation-&-resources)**:
- [Integration manual](https://www.u-blox.com/sites/default/files/ZED-F9R_Integrationmanual_UBX-20039643.pdf)
- [Interface description manual HPS 1.21](https://www.u-blox.com/sites/default/files/F9-HPS-1.21_InterfaceDescription_UBX-21019746.pdf)
- [Interface description manual HPS 1.30](https://www.u-blox.com/sites/default/files/documents/u-blox-F9-HPS-1.30_InterfaceDescription_UBX-22010984.pdf)
<br>
<br>
The documentation has detailed information regrading supported vehicles, mounting guide, sensor data and others.
This quick guide is not meant to cover the official documentation but focuses more on a very quick start-up guide and mostly on the available software/API options.

NOTE: In the current version **Dead Reckoning** does not support **Wheel Tick**. This will be added in a future release.

## How to use
1. **Enable** or **Disable** Dead Reckoning by navigating to the Dead Reckoning [KConfig](./README_kconfig.md) section in each example (where supported)
2. In KConfig chose Enable for Dead Reckoning (example dependent)
3. Inside the app configure the Dead Reckoning options as desired:
    - Calibration mode
    - Vehicle mode
    - Calibration angles (Not needed/Ignored in **Manual** calibration mode)\
Please consider having a look at the **[API](./../components/hpglib/src/location_service/gnss_service/)** and **[examples](./../examples/)** for available options and functions.
4. Mount the HPG board firmly into your vehicle. This is an important step as the board should not be able to move in order to get good results.
5. Wait for calibration procedure to be done
    - If you are using Automatic calibration wait for the procedure to be done
    - If in Manual mode you will have to provide your own calibration/alignment values during app configuration (step 3)
6. When the calibration is done you will have Dead Reckoning function
7. You can print information regarding **Calibration status** and **Sensors statuses** as desired with the provided API functions
8. Dead reckoning will show up in the location status print only if there's no GNSS coverage

## Calibration modes
There are 2 ways of calibrating the **IMU** to make **Dead Reckoning** operational:
- Manual:\
The user provides known alignment values:
    - Yaw angle
    - Pitch angle
    - Roll Angle\
When all 3 angles are populated then the IMU will be ready.
- Automatic:\
The user selects automatic calibration and start driving the car normally. When the IMU collect sufficient data then it will be ready to offer Dead Reckoning data.

## Further functionality
Since Automatic calibration might take some time (depending on the way of driving) to calibrate, the HPG library implements a save to NVS function which automatically stores calibration angles to NVS. This way the automatically generated angles (yaw, pitch and roll) will be saved and will automatically be used the next time the board reboots.
During the next reboot even if calibration mode is set to Automatic it will fall back to manual so it can provide the IMU with the correct data.\
If the calibration angles are not valid anymore (due to different vehicle, mounting position is changed) you can use the provided `xplrGnssNvsDeleteCalibrations(uint8_t dvcProfile)` function to clear them.

## Supported examples
this is a list of examples that support **Dead Reckoning**
|Category                                   |Example name                                                                                                            |
|-------------------------------------------|------------------------------------------------------------------------------------------------------------------------|
|[Cellular](./../examples/cellular/)        |[Cell MQTT Correction data using Certs](./../examples/cellular/02_hpg_cell_mqtt_correction_certs/)                      |
|[Cellular](./../examples/cellular/)        |[Cell MQTT Correction data using ZTP](./../examples/cellular/03_hpg_cell_mqtt_correction_ztp/)                          |
|[Cellular](./../examples/cellular/)        |[Cell NTRIP Correction data](./../examples/cellular/04_hpg_cell_ntrip_correction/)                                      |
|[Positioning](./../examples/positioning/)  |[GNSS & LBAND config](./../examples/positioning/01_hpg_gnss_config/)                                                    |
|[Positioning](./../examples/positioning/)  |[LBAND Correction data using Certs](./../examples/positioning/02_hpg_gnss_lband_correction/)                            |
|[Short Range](./../examples/shortrange/)   |[Wi-Fi MQTT Correction data using Certs](./../examples/shortrange/03_hpg_wifi_mqtt_correction_certs/)                   |
|[Short Range](./../examples/shortrange/)   |[Wi-Fi MQTT Correction data using ZTP](./../examples/shortrange/04_hpg_wifi_mqtt_correction_ztp/)                       |
|[Short Range](./../examples/shortrange/)   |[Wi-Fi MQTT correction data with Captive Portal](./../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/)  |
|[Short Range](./../examples/shortrange/)   |[Wi-Fi NTRIP Correction data](./../examples/shortrange/06_hpg_wifi_ntrip_correction/)                                   |

## Limitations
Wheel tick has not been implemented in the current version of the library. It will be added on next releases.

## Caveats
Every time you change something using **KConfig** the compiler will re-compile all source files.