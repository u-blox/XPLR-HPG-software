![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# Communication Service (cellular)

## Description
Provided library makes interface with u-blox [LARA-R6](https://www.u-blox.com/en/product/lara-r6-series) cellular  module as easy as possible offering to the user high level functions for:
- configuring network parameters.
- registering to a cellular provider.
<br>

## Reference examples
Please see the [network registration](./../../../../examples/cellular/01_hpg_cell_register/) example.

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRCOM_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
**`XPLRCOM_NUMOF_DEVICES`** | **`1`** | Defines number of cellular modules to be initialized by the library. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
<br>

## Modules-Components dependencies
There are no dependencies to other HPGLib components.