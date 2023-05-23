![u-blox](./../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# XPLR Zero Touch Provisioning Parser (ZTP Parser)

## Description
This is a library containing parser functions to get data from **Zero touch Provisioning** (ZTP) payload. When executing a ZTP HTTPS POST request the reply will be in the form of a JSON. With this library you can get every data available from a successful reply.

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

Name | Value | Description
--- | --- | ---
**`XPLRZTPJSONPARSER_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`XPLR_ZTP_JP_DESCRIPTION_LENGTH`** | **`256U`** | Size in bytes for the description buffer. Topics to subscribe to have a string description in ZTP reply. You can change this value freely according to your needs, although the buffer is large enough.
**`XPLR_ZTP_JP_PATH_LENGTH`** | **`128U`** | Size in bytes for the topics buffer. Topics to subscribe to are named **Paths** in ZTP reply. You can change this value freely according to your needs.
**`XPLR_ZTP_REGION_EU`** | **`"eu"`** | European region. Used to declare for which region we want to parse data. Do not change.
**`XPLR_ZTP_REGION_US`** | **`"us"`** | Continental US region. Used to declare for which region we want to parse data. Do not change.
**`XPLR_ZTP_REGION_KR`** | **`"kr"`** | Korea region. Not in service at the moment. Do not change.
**`XPLR_ZTP_TOPIC_KEY_DISTRIBUTION_ID`** | **`"0236"`** | **Decryption keys** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_ASSIST_NOW_ID`** | **`"mga"`** | **Assist now** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_CORRECTION_DATA_ID`** | **`"corr_topic"`** | Korea region. Not in service at the moment. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_GEOGRAPHIC_AREA_ID`** | **`"gad"`** | **Geographical data** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_ATMOSPHERE_CORRECTION_ID`** | **`"hpac"`** | **Atmospheric correction data** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_ORBIT_CLOCK_BIAS_ID`** | **` "ocb"`** | **Clock Bias** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.
**`XPLR_ZTP_TOPIC_CLOCK_ID`** | **`"clk"`** | **Clock data** topic key. Only change this if there's also a change in **Thingstream - Point Perfect**.

**NOTE**: `XPLR_ZTP_TOPIC_CORRECTION_DATA_ID` uses key **"corr_topic"** which does not really appear on any of the JSON topics but is rather used (albeit internally) to get the topic for correction data.
<br>
<br>

## Modules-Components used
XPLR Zero Touch Provisioning Parser uses the following modules-components:

Name | Description 
--- | --- 
**[boards](./../boards/)** | Board variant selection
**[hpglib/common](./../hpglib/src/common/)** | Common functions.