![u-blox](./../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR Zero Touch Provisioning (ZTP)

## Description
This is a library to help perform a **ZTP** to **Thingstream** and get all the required settings for your **Point Perfect** needs; mainly **MQTT** connection settings.

<br>
<br>

## Reference examples
Please see one of the following examples:
- [Zero Touch Provisioning (ZTP) with Wi-Fi](./../../examples/shortrange/02_hpg_wifi_http_ztp/)
- [Correction data via Wi-Fi MQTT to ZED-F9R using Zero Touch Provisioning](./../../examples/shortrange/04_hpg_wifi_mqtt_correction_ztp/)

<br>
<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../docs/README_kconfig.md)**.

You can change the following types if something changes in the future.

Name | Value | Description
--- | --- | ---
**`XPLRZTP_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`HEADER_DATA_TYPE`** | **`"application/json"`** | Header setting for HTTPS post. Setting application type.
**`HTTP_POST_HEADER_TYPE_CONTENT`** | **`"Content-Type"`** | Header setting for HTTPS post. Setting content header type.
**`HTTP_POST_HEADER_TYPE_DATA_CONTENT`** | **`HEADER_DATA_TYPE`** | Header setting for HTTPS post. Data content data type.
**`HTTP_POST_HEADER_TYPE_JSON`** | **`"Accept"`** | Header setting for HTTPS post. Setting JSON header type.
**`HTTP_POST_HEADER_TYPE_CONTENT`** | **`HEADER_DATA_TYPE`** | Header setting for HTTPS post. JSON content data type.

<br>
<br>

## Modules-Components used
XPLR Zero Touch Provisioning (ZTP) uses the following modules-components:

Name | Description 
--- | --- 
**[boards](./../../components/boards/)** | Board variant selection
**[hpglib/common](./../../components/hpglib/src/common)** | Common functions.