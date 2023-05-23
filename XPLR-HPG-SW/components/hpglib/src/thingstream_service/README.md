![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Thingstream Client Service (cellular)

## Description
Provided library exposes some features of the Thingstream platform which are related to Location and Communication (MQTT) services available, offering to the user high level functions for:
- initializing parser parameters.
- create requests according to Thingstream's API.
- parse Thingstream messages and forward them to the application.
- filter Thingstream messages by type/info.
<br>

## Reference examples
Please see the [Correction data over MQTT using certs](./../../../../examples/cellular/02_hpg_cell_mqtt_correction_certs/) example.

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRTHINGSTREAM_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Common Functions](./../common/)** | Collection of common functions used across multiple HPGLib components.
<br>