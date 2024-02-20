![u-blox](./../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# u-blox RF calibration

This document provides instructions on how to configure the NINA-W1 / NORA-W1 modules radio to comply with regulations.

For more information please refer to [NINA-W1 System Integration Manual](https://www.u-blox.com/docs/UBX-17005730) and [NORA-W1 System Integration Manual](https://www.u-blox.com/docs/UBX-22005601)

## Applying the RF calibration


Before building any XPLR-HPG examples you will need to open a Git Bash terminal and apply the [patch](./../misc/ubx_phy_cal_esp_idf_4_4.patch) with this command: 

`patch -p2 -i ./misc/ubx_phy_cal_esp_idf_4_4.patch -d [ESP-IDF INSTALL DIRECTORY]/components/`

**Note:** The patch affects ESP-IDF sources and will have effect on any esp32 or esp32s3 variant. If the ESP-IDF is updated the patch will need to be re-applied.

## Verifying patches

After patching the ESP-IDF you can build any example run the application and look for one of these prints:

`ubx_phy_cal: NINA-W1 phy calibration applied`<br>
or<br>
`ubx_phy_cal: NORA-W1 phy calibration applied`

If it shows that means the RF calibration with updated values work correctly.
