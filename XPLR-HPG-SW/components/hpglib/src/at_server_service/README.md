![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# AT Server library

## Description
Module for implementing AT Server functionality by using the uAtClient module of the Ubxlib library. 
- Supports multiple instances by setting the **`XPLRATSERVER_NUMOF_SERVERS`** macro definition.
- Provides functions for setting handler functions to certain AT command filters.
- Support for setting an asynchronous callback function that is run in its own task context (through a ubxlib created event queue).
- Synchronous reading and writing to the UART interface.



## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRATSERVER_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
**`XPLRATSERVER_NUMOF_SERVERS`** | **`1`** | Configures the maximum number of AT servers allowed by the module. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Ubxlib](./../../ubxlib/)** | Collection of portable C libraries to interface with u-blox module.
<br>