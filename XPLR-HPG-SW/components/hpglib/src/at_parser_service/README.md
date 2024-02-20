![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

# AT Command Set Parser

## Description
Provided library exposes features related to parsing AT commands by utilizing the AT server module:
- Initializing the AT Server module and setting up the UART interface.
- Assigning appropriate handler functions to the received commands.
- Storing received configuration data and credentials in NVS.

## Command Format

All commands start with the `AT` prefix. Commands for receiving data from the host (thus assigning data to the parser) contain the `=` character after which the command variables follow, bounded by a comma delimiter. By default the parser expects the CRLF lined ending after each AT command. Commands for querying information end with the `=?` prefix (followed by the CRLF line ending).

Responses to query commands have the following format with multiple variables delimitered by commas: `+COMMAND_NAME:<variable0>,<variable1>,...`. An `OK` message is relayed when no data are expected in the response and in both cases an `ERROR` message is relayed in case of an error when assigning or retriving the requested data.

No concurrent execution of AT commands is allowed, the host should wait for a response before sending another command otherwise an `+ERROR:BUSY` messaged is sent without further execution.

## AT Parser Operation

Upon receiving a command, an appropriate handler is executed which parses the command variables, if any, then an asynchronous callback is set that is responsible for storing/retrieving the data from the NVS memory, copying the updated value in volatile memory and sending an AT response to the UART interface.

The configuration and credentials stored in volatile memory can then be used by an application for configuring the board functions.

## Supported commands

Description | Command | Response | Notes
--- | --- | --- | ---
Check | AT | OK | 
Set Wi-Fi | AT+WIFI=\<SSID>,\<PSWD> | OK | 
Get Wi-Fi | AT+WIFI=? | +WIFI:\<SSID>,\<PSWD> | 
Del Wi-Fi | AT+ERASE=WIFI | OK | 
Set APN | AT+APN=\<APN> | OK | 
Get APN | AT+APN=? | +APN:\<APN> | 
Del APN | AT+ERASE=APN | OK | 
Set Root CA | AT+ROOT=\<ROOT CA> | OK | no \<cr>\<lf>
Get Root CA | AT+ROOT=? | +ROOT:\<ROOT CA> | no \<cr>\<lf>
Del Root CA | AT+ERASE=ROOT | OK | 
Set TS Broker | AT+TSBROKER=\<URL>,\<PORT> | OK | 
Get TS Broker | AT+TSBROKER=? | +TSBROKER:\<URL>,\<PORT> | 
DEL TS Broker | AT+ERASE=TSBROKER | OK | 
Set TS client ID | AT+TSID=\<CLIENT ID> | OK | no \<cr>\<lf>
Get TS client ID | AT+TSID=? | +TSID:\<CLIENT ID> | no \<cr>\<lf>
Del TS client ID | AT+ERASE=TSID | OK | 
Set TS cert | AT+TSCERT=\<CERT> | OK | no \<cr>\<lf>
Get TS cert | AT+TSCERT=? | +TSCERT:\<CERT> | no \<cr>\<lf>
Del TS cert | AT+ERASE=TSCERT | OK | 
Set TS key | AT+TSKEY=<KEY> | OK | no \<cr>\<lf>
Get TS key | AT+TSKEY=? | +TSKEY:\<KEY> | no \<cr>\<lf>
Del TS key | AT+ERASE=TSKEY | OK | 
Set TS Plan | AT+TSPLAN=\<PLAN> | OK | ip / ip+lband / lband
Get TS Plan | AT+TSPLAN=? | +TSPLAN:\<PLAN> | ip / ip+lband / lband
Del TS Plan | AT+ERASE=TSPLAN | OK | 
Set TS Region | AT+TSREGION=\<REGION> | OK | eu / us
Get TS Region | AT+TSREGION=? | +TSREGION:\<REGION> | eu / us
Del TS Region | AT+ERASE=TSREGION | OK | 
Set NTRIP server | AT+NTRIPSRV=\<SERVER>,\<PORT> | OK | 
Get NTRIP server | AT+NTRIPSRV=? | +NTRIPSRV:\<SERVER>,\<PORT> | 
Del NTRIP server | AT+ERASE=NTRIPSRV | OK | 
Set NTRIP GGA relaying | AT+NTRIPGGA=\<OPTION> | OK | 0 / 1 (ascii)
Get NTRIP GGA relaying | AT+NTRIPGGA=? | +NTRIPGGA:\<OPTION> | 0 / 1 (ascii)
Set NTRIP User Agent | AT+NTRIPUA=\<USER AGENT> | OK | 
Get NTRIP User Agent | AT+NTRIPUA=? | +NTRIPUA:\<USER AGENT> | 
DEL NTRIP User Agent | AT+ERASE=NTRIPUA | OK | 
Set NTRIP Mount Point | AT+NTRIPMP=\<MOUNT POINT> | OK | 
Get NTRIP Mount Point | AT+NTRIPMP=? | +NTRIPMP:\<MOUNT POINT> | 
Del NTRIP Mount Point | AT+ERASE=NTRIPMP | OK | 
Set NTRIP creds | AT+NTRIPCREDS=\<USER>,\<PSWD> | OK | 
Get NTRIP creds | AT+NTRIPCREDS=? | +NTRIPCREDS:\<USER>,\<PSWD> | 
Del NTRIP creds | AT+ERASE=NTRIPCREDS | OK | 
Set GNSS DR | AT+GNSSDR=\<OPTION> | OK | 0 / 1 (ascii)
Get GNSS DR | AT+GNSSDR=? | +GNSSDR:\<OPTION> | 0 / 1 (ascii)
Set SD Log | AT+SD=\<OPTION> | OK | 0 / 1 (ascii)
Get SD Log | AT+SD=? | +SD:\<OPTION> | 0 / 1 (ascii)
Get Wi-Fi Status | AT+STATWIFI=? | +STATWIFI:\<STATUS> | 
Get Cell Status | AT+STATCELL=? | +STATCELL:\<STATUS> | 
Get TS Status | AT+STATTS=? | +STATTS:\<STATUS> | 
Get NTRIP Status | AT+STATNTRIP=? | +STATNTRIP:\<STATUS> | 
Get Location | AT+LOC=? | +LOC:\<TIME>,\<FIX TYPE>,\<LAT>,\<LON>,\<ALT>,\<SPEED>,\<RADIUS>,\<ACCURACY HORIZONTAL>,\<ACCURACY VERTICAL>,\<NUM OF SATELLITES> | 
Get GNSS Status | AT+STATGNSS=? | +STATGNSS:\<STATUS> | baudrate in ascii
Set Baudrate | AT+BAUD=\<BAUD> | OK | baudrate in ascii
Get Baudrate | AT+BAUD=? | +BAUD:\<BAUDRATE> | 
Set interface | AT+IF=\<INTERFACE> | OK | wi-fi / cell
Get interface | AT+IF=? | +IF:\<INTERFACE> | wi-fi / cell
Set correction source | AT+CORSRC=\<SOURCE> | OK | ts / ntrip
Get correction source | AT+CORSRC=? | +CORSRC:\<SOURCE> | ts / ntrip
Set correction module | AT+CORMOD=<MODULE> | OK | ip / lband
Get correction module | AT+CORMOD=? | +CORMOD:\<MODULE | ip / lband
Set device mode | AT+HPGMODE=\<MODE> | OK | config / start / stop / error
Get device mode | AT+HPGMODE=? | +HPGMODE:\<MODE> | config / start / stop / error
Factory Reset | AT+ERASE=ALL | OK | 
Get automatic start on boot | AT+STARTONBOOT=? | +STARTONBOOT=\<OPTION> | 0 / 1 (ascii)
Set automatic start on boot | AT+STARTONBOOT=\<OPTION> | OK |  0 / 1 (ascii)
Board Info | AT+BRDNFO=? | +BRDNFO:\<BRD NAME>,\<HW Version>,\<MCU>,\<MAC>,\<Flash Size>,\<RAM Size>,\<GNSS Model>,\<GNSS Version>,\<L-BAND Model>,\<L-BAND Version>,\<CELL Model>,\<CELL Version>,\<CELL IMEI> | 
Board Restart | AT+BRD=RST | OK | 


## Reference examples
Please see the [HPG AT Command](./../../../../examples/shortrange/22_hpg_at_command/) example.

## Local Definitions-Macros
Macro/definitions section which are not inherited from other modules/components or are not part of any **[KConfig](./../../../../docs/README_kconfig.md)**

Name | Value | Description
--- | --- | ---
**`XPLRATSERVER_DEBUG_ACTIVE`** | **`1`** | Controls logging of debug info to console. Present in [xplr_hpglib_cfg](./../../xplr_hpglib_cfg.h).
<br>

## Modules-Components dependencies
Name | Description
--- | ---
**[Common Functions](./../common/)** | Collection of common functions used across multiple HPGLib components.
**[AT Server](./../common/)** | Module for receiving AT commands using ubxlib uAtClient module.
**[Thingstream Client Service](./../thingstream_service/)** | Library for exposing features of the Thingstream platform.
**[GNSS Service](./../location_service/gnss_service/)** | F9 family GNSS device library using I2C.
<br>