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