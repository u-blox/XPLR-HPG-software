![u-blox](./../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# HPGLib
<br>

## Description
This is collection of modules/libraries that were implemented for supporting [xplr-hpg-1](https://www.u-blox.com/en/product/xplr-hpg-1) and [xplr-hpg-2](https://www.u-blox.com/en/product/xplr-hpg-2) kits.<br>
Included modules are meant to provide a kickstart for developers to interface with the ublox modules available on the kits and evaluate some of the **[Thingstream](https://developer.thingstream.io/home)** features which are related to point perfect and MQTT services.
<br>

## HPGLib modules

Name | Description
--- | ---
**[common](./src/common/)** | Library containing some common functions which are used across other libraries.
**[com_service](./src/com_service/)** | Library implementing common functions for using ublox cellular modules.
**[httpClient_service](./src/httpClient_service/)** | Library implementing typical http(s) client for cellular modules.
**[mqttClient_service](./src/mqttClient_service/)** | Library implementing typical mqtt(s) client for cellular modules.
**[thingstream_service](./src/thingstream_service/)** | Library implementing basic ztp functionality for acquiring certificates and topics used in Thingstream's location services.
**[gnss_service](./src/location_service/gnss_service/)** | Library implementing common functions for using ublox GNSS modules.
**[lband_service](./src/location_service/lband_service/)** | Library implementing common functions for using ublox LBand modules.
**[location_service_helpers](./src/location_service/location_service_helpers/)** | Library containing some common functions used across GNSS and LBand modules.
<br>