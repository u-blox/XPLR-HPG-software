![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# NTRIP correction

## Description

NTRIP (Networked Transport of RTCM via Internet Protocol) is a protocol used for streaming corrections over the Internet from a base station (the NTRIP caster mountpoint) to a rover (your XPLR-HPG board) to achieve cm-level accuracy.
NTRIP is a Real-Time Kinematic (RTK) positioning correction transmission protocol. To perform an accurate RTK survey, a base station at a known geospatial location transmits corrections in real-time to the connected rover. The base station continuously observes the satellites and calculates position corrections that are sent to the rover once every second in a data stream. 
This library allows your XPLR-HPG board to take advantage of RTCM Correction Data via NTRIP, by connecting to the NTRIP caster of your choice, getting correction data and sending NMEA GGA messages back to the caster if needed.

Provided library implements a NTRIP client using the `LWIP socket` library (WiFi).

The intended use case for the HPGLib NTRIP module is:
- `xplrWifiNtripSetConfig` to configure the host, port etc...
- `xplrWifiNtripSetCredentials` to set the credentials (if) needed to access the NTRIP caster.
- `xplrWifiNtripInit` to initiate the connection to the caster and start the NTRIP task which runs asynchronously in the background
- You have to periodically check the state of the client with `xplrWifiNtripGetClientState` and according to the state you have to either provide3 the client with a GGA NMEA message or read the correction data from the provided buffer and inject it to the GNSS module with the xplr_gnss module.
- If `xplrWifiNtripGetClientState` returns error state you can get more info about the error using the `xplrWifiNtripGetDetailedError` function.
- `xplrWifiNtripDeInit` to de-init the client.

See **[06_hpg_wifi_ntrip_correction](./../../../../examples/shortrange/06_hpg_wifi_ntrip_correction)** example.

Tested NTRIP casters:
- http://igs-ip.net/home
- http://rtk2go.com
- http://www.civilpos.gr

<br>

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE`** | **`2 kB`** | Default single NTRIP message size.
**`XPLRWIFI_NTRIP_GGA_INTERVAL_S`** | **`20 S`** | Default GGA message interval (send GGA to caster).

**Note:** You have to change `XPLRWIFI_NTRIP_RECEIVE_DATA_SIZE` according to the RTCM messages that are being broadcasted by your NTRIP caster of choice.
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Common Functions](./../common/)** | Collection of common functions used across multiple HPGLib components.
<br>