![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# MQTT(S) Client Service (cellular)

## Description
Provided library exposes the MQTT(S) client available in u-blox cellular modules offering to the user high level functions for:
- configuring client parameters.
- managing connection process to a MQTT broker.
- storing certificates to module's memory.
- subscribing to broker's topics.
- listening to new messages and forwarding them to the application.
<br>

## Reference examples
Please see the [Correction data over MQTT using certs](./../../../../examples/cellular/02_hpg_cell_mqtt_correction_certs/) example.

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRCELL_MQTT_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
**`XPLRCELL_MQTT_NUMOF_CLIENTS`** | **`1`** | Defines number of MQTT clients to be initialized by the library. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
**`XPLRCELL_MQTT_WATCHDOG_TIMEOUT_SEC`** | **`10U`** | Defines the time in seconds in which the internal watchdog gets triggered. The trigger will occur when no MQTT messages are received for this amount of seconds. It is relevant only when the MQTT broker selected is Thingstream. When connecting with third party MQTT brokers the check for watchdog event, regarding the reception of MQTT messages is disabled.<br> 
**NOTE:** Currently only **one** client can be initialized. In a future update of [ubxlib](https://www.u-blox.com/en/product/ubxlib) multiple clients will be supported.
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Common Functions](./../common/)** | Collection of common functions used across multiple HPGLib components.
**[Communication Service](./../com_service/)** | XPLR communication service for cellular module.
<br>