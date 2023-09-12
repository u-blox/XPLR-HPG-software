![u-blox](./../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR MQTT Wi-Fi

## Description
This is a module that offers functions to manage MQTT client connection to a broker.
<br>
<br>

## Reference examples
Please see one of the following examples:
- [Correction data via Wi-Fi MQTT to ZED-F9R using certificates](./../../examples/shortrange/03_hpg_wifi_mqtt_correction_certs/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](./../../examples/shortrange/04_hpg_wifi_mqtt_correction_ztp/)
- [Correction data via NEO-D9S LBAND module to ZED-F9R with the help of MQTT (using certificates)](./../../examples/positioning/02_hpg_gnss_lband_correction/)

<br>
<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRMQTTWIFI_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`XPLR_MQTTWIFI_PAYLOAD_TOPIC_LEN`** | **`128U`** | Buffer size for topic. You can change this value according to your needs.
**`XPLR_MQTTWIFI_PAYLOAD_DATA_LEN`** | **`1024U`** | Buffer size for data. You can change this value according to your needs.

<br>
<br>

## Modules-Components used
XPLR MQTT Wi-Fi uses the following modules-components:

Name | Description 
--- | --- 
**[boards](./../../components/boards/)** | Board variant selection
**[hpglib/common](./../../components/hpglib/src/common)** | Common functions.
**[hpglib/thingstream_service/](./../../components/hpglib/src/thingstream_service)** | Parser for Zero Touch Provisioning data. Used here in order to be able to pass ZTP style topics.