![u-blox](./../../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR GNSS lib

## Description
This is a library to help implement functionality for a F9 family GNSS device using I2C as a communication protocol since both board variants **[xplr-hpg1-c213](./../../../../boards/xplr-hpg1-c213/)** and **[xplr-hpg2-c214](./../../../../boards/xplr-hpg2-c214/)** only support that method.\
The library is also based/wrapped around **[ubxlib](https://github.com/u-blox/ubxlib)** and you can also have a look at it if you so wish.\
The main scope of this library is to make device initialization and data parsing as fast and easy as possible.

<br>
<br>

## Reference examples
Please see one of the following examples:
- [Correction Data over MQTT from Thingstream Point Perfect Service (manual)](../../../../../examples/cellular/02_hpg_cell_mqtt_correction_certs/)
- [Correction Data over MQTT from Thingstream Point Perfect Service (ZTP)](../../../../../examples/cellular/03_hpg_cell_mqtt_correction_ztp/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using certificates](../../../../../examples/shortrange/03_hpg_wifi_mqtt_correction_certs/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](../../../../../examples/shortrange/04_hpg_wifi_mqtt_correction_ztp/)
- [ZED-F9R GNSS & NEO-D9S LBAND module configuration](../../../../../examples/positioning/01_hpg_gnss_config/)
- [Correction data via NEO-D9S LBAND module to ZED-F9R with the help of MQTT (using certificates)](../../../../../examples/positioning/02_hpg_gnss_lband_correction/)

<br>
<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRGNSS_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](../../../xplr_hpglib_cfg.h).
**```XPLR_GNSS_FUNCTIONS_TIMEOUTS_MS```** | **```XPLR_HLPRLOCSRVC_FUNCTIONS_TIMEOUTS_MS```** | Timeout for blocking functions. Found in **[xplr_location_helpers.h](./../location_service_helpers/xplr_location_helpers.h)** You can replace this value freely.

<br>
<br>

## Modules-Components used
XPLR GNSS uses the following modules-components:

Name | Description 
--- | --- 
**[boards](./../../../../boards/)** | Board variant selection
**[hpglib/common](./../../common/)** | Common functions.
**[hpglib/location_service_helpers/](./../location_service_helpers/)** | Private helper functions for location devices.