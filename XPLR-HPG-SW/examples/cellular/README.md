# Cellular Examples

## Description
This directory contains examples that are using the LARA-R6 cellular module for communicating with the outer world. <br>
They demonstrate basic functionality of common scenarios such as:
- network configuration and registration
- MQTT client functionality based on module's MQTT client for connecting to Thingstream's Location service and acquire correction data.
- HTTP client functionality implementing typical POST and GET request to acquire certificates and configuration parameters for Thingstream's Location Service using the Zero Touch Provisioning feature.

## Examples list

| Name | Description |
| --- | --- |
| **[01_hpg_cell_register](./01_hpg_cell_register/)** | Configure cellular module and register to a desired network. |
| **[02_hpg_cell_mqtt_correction_certs](./02_hpg_cell_mqtt_correction_certs/)** | Create an MQTT connection and get correction data from Thingstream using certificates. |
| **[03_hpg_cell_mqtt_correction_ztp](./03_hpg_cell_mqtt_correction_ztp/)** | Create an MQTT connection and get correction data from Thingstream using ZTP. |
| **[04_hpg_cell_ntrip_correction](./04_hpg_cell_ntrip_correction/)** | Demonstration of NTRIP client over cellular network, connect to NTRIP caster, get correction data and forward the to the GNSS module. |


## Notes
In most examples the `Boot0` button is used to power down the peripheral modules that are available on the xplr-hpg boards. To switch off the modules just press and hold the `Boot0` button for 3 seconds.<br>
**It is highly recommended to do this before unplugging the board from the USB to prevent any memory corruption.**