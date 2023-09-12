![u-blox](./../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR Wi-Fi starter

## Description

This is a Wi-Fi manager to help connect to an access point.\
For now only **Client** mode is supported (i.e. connecting to another Access Point). An update with **Access Point** mode is in the plans.

<br>
<br>

## Reference examples
Please see one of the following examples:
- [Zero Touch Provisioning (ZTP) with Wi-Fi](./../../examples/shortrange/02_hpg_wifi_http_ztp/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using certificates](./../../examples/shortrange/03_hpg_wifi_mqtt_correction_certs/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](./../../examples/shortrange/04_hpg_wifi_mqtt_correction_ztp/)
- [Correction data via NEO-D9S LBAND module to ZED-F9R with the help of MQTT (using certificates)](./../../examples/positioning/02_hpg_gnss_lband_correction/)

<br>
<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRWIFISTARTER_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
<br>

## Modules-Components used

Name | Description 
--- | --- 
**[boards](./../boards/)** | Board variant selection
**[hpglib/common](./../hpglib/src/common/)** | Common functions.