# Shortrange Examples

## Description
Examples provided are based on the functionality of host MCU, expanding its capabilities with HPG u-blox modules.<br>
The directory contains examples that address some typical use cases when building a connected HPG application.<br>
The device communicates with the world using its Wi-Fi peripheral, connecting to a router/access point that will provide internet access to it.<br>
Being connected to the internet the xplr-hpg-kit can:
- connect to Thingstream platform.
- access correction data using Thingstream's location service.
- connect to NTRIP server/caster
- access RTCM correction data using the NTRIP protocol
<br>

## Examples list

| Name | Description |
| --- | --- |
| **[01_hpg_base](./01_hpg_base/)** | Configuration and initialization of the base board. |
| **[02_hpg_wifi_http_ztp](./02_hpg_wifi_http_ztp/)** | Demonstration of HTTP client over Wi-Fi, implementing the Zero Touch Provisioning of Thingstream's location service to get required parameters for accessing Thingstream's services. |
| **[03_hpg_wifi_mqtt_correction_certs](./03_hpg_wifi_mqtt_correction_certs/)** | Demonstration of a MQTT client over Wi-Fi, connecting to Thingstream's location service and accessing correction data that are forwarded to the GNSS module. <br>Connection with Thingstream is achieved by **manually** providing required parameters and certificates to the device. |
| **[04_hpg_wifi_mqtt_correction_ztp](./04_hpg_wifi_mqtt_correction_ztp/)** | Demonstration of MQTT and HTTP clients over Wi-Fi, connecting to Thingstream's location service and accessing correction data that are forwarded to the GNSS module.<br> Connection with Thingstream is achieved by implementing the Zero Touch Provisioning feature. |
| **[05_hpg_wifi_mqtt_correction_captive_portal](./05_hpg_wifi_mqtt_correction_captive_portal/)** | Demonstration of how to use a captive portal to configure U-blox XPLR-HPG-1/2 kits with Wi-Fi and Thingstream credentials to acquire correction data from Thingstream's PointPerfect location service over Wi-Fi. |
| **[06_hpg_wifi_ntrip_correction](./06_hpg_wifi_ntrip_correction/)** | Demonstration of NTRIP client over WiFi, connect to NTRIP caster, get correction data and forward the to the GNSS module. |
| **[07_hpg_wifi_mqtt_correction_sd_config_file](./07_hpg_wifi_mqtt_correction_sd_config_file/)** | Demonstration of how to use **Thingstream's** u-center config file via the SD card, connecting to Thingstream's location service and accessing correction data that are forwarded to the GNSS module, for easier provisioning. |
| **[99_hpg_kit_info](./99_hpg_kit_info/)** | Diagnostics info regarding modules available on kit. |
