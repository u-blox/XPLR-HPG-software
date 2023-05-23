![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# HTTP(S) Client Service (cellular)

## Description
Provided library exposes the HTTP(S) client available in ublox cellular modules offering to the user high level functions for:
- configuring client parameters.
- managing connection process to a webserver.
- storing certificates to module's memory.
- performing typical POST and GET requests.
<br>

## Reference examples
Please see the [Correction data over MQTT using ZTP](./../../../../examples/cellular/03_hpg_cell_mqtt_correction_ztp/) example.<br>
ZTP transaction and acquisition of root certificate from AWS are based on HTTP library.

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRCELL_HTTP_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Common Functions](./../common/)** | Collection of common functions used across multiple HPGLib components.
**[Communication Service](./../com_service/)** | XPLR communication service for cellular module.
<br>