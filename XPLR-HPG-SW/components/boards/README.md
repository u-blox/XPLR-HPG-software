![u-blox](./../../media/shared/logos/ublox_logo.jpg)
<br>
<br>

# Boards

## Description
Collection of hardware specific board files that are used for initializing / de-initializing the hardware components available on the **[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)**/**[2](https://www.u-blox.com/en/product/xplr-hpg-2)** kits.

## Kconfig/Build Definitions-Macros
This is a description of definitions and macros configured by **[Kconfig](./../../docs/README_kconfig.md)**.<br>
In most cases, these values can be directly overwritten in the source code or just configured by running **[Kconfig](./../../docs//README_kconfig.md)** before building.\
**[Kconfig](./../../docs/README_kconfig.md)** is used for easy/fast parameter configuration and building, without the need of modifying the source code.

Name | Default value | Belongs to | Description
--- | --- | --- | ---
**`CONFIG_BOARD_XPLR_HPG2_C214`** | `true` | **[board manager](./)** | Configure workspace and pin mapping for **[XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2)**.
**`CONFIG_BOARD_XPLR_HPG1_C213`** | `false` | **[board manager](./)** | Configure workspace and pin mapping for **[XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)**.
**`CONFIG_BOARD_MAZGCH_HPG_SOLUTION`** | `false` | **[board manager](./)** | Configure workspace and pin mapping for **[MAZGCH HPG SOLUTION](https://github.com/mazgch/hpg)**.
<br>

**NOTE:** Board file can be selected using KConfig (`menuconfig -> Board Options`).

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRCOM_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`XPLR_BOARD_SELECTED_IS_C213`** | **`0`** | Defines [XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1) as selected board. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`XPLR_BOARD_SELECTED_IS_C214`** | **`1`** | Defines [XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-1) as selected board. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
**`XPLR_BOARD_SELECTED_IS_MAZGCH`** | **`0`** | Defines [MAZGCH HPG Solution](https://github.com/mazgch/hpg) as selected board. Present in [xplr_hpglib_cfg](./../hpglib/xplr_hpglib_cfg.h).
<br>

**NOTE:** Only **one** board can be selected at a time.