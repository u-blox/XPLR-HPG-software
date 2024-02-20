![u-blox](./../../../../media/shared/logos/ublox_logo.jpg)

<br>
<br>

## Description
This python application is used to demonstrate the functionality of the [AT-HPG-example](./../main/hpg_at_app.c) providing a console based UI that can be used to easily configure the XPLR-HPG kit. Configuration of the kit can be done by either the [config-file](./config.json) or using the [api](./AtApi.py) commands directly.

## Installation and Dependencies
The application is developed using Python version `3.10.5` and the `pySserial` package version `3.5`.<br>
To install and run it recommended to create virtual python environment, install the required package using `pip` and then run the [At-Hpg](./AtHpg.py) like below:
- Using a terminal / powershell window navigate to the [AtHpgApp](./../atHpgApp-python/) folder.
- Create a virtual python environment `python3 -m venv .venv`
- Activate the virtual environment
    - `source .venv/bin/activate` (for Linux)
    - `.venv/Scripts/activate` (for Windows)
- Install python package requirements using pip `pip install -r requirements.txt`
- Run the app `python ./AtHpg.py`

## Application Settings and Configuration
As mentioned before the application can configure the XPLR-HPG kit either by using the [config-file](./config.json) or directly through the [api](./AtApi.py) commands.<br>

### Easy Config using the configuration file:
- Open the [file](./config.json) with a text editor and fill in the configuration variables. Make sure that keys and certificates do not contain any special characters (eg. `\r\n`). Certificates format is similar to the one present in `u-centerConfig` file from Thingstream.
- Save the file and run the application from the terminal `python ./AtHpg.py`.
- Using the UI options configure the device, Thingstream and / or NTRIP settings.
- When done with the configuration, navigate to the API interface and switch the device mode to `start`.
- Poll the device for location data from the API interface

```
**Note:**
- Easy config refers to menu options [1-3].
- API interface for switching the device mode and polling for location data is accessible through menu option [4].
```

### Manual config using the API menu:
- Open the [file](./config.json) with a text editor and fill in the `serialport` that the device is connected to.
- Save the file and run the application from the terminal `python ./AtHpg.py`.
- Using the UI navigate to the API interface and set the device mode to `config`.
- Start configuring the device settings. At a minimum you should config the following settings:
    - Device interface (wi-fi or cell).
    - Credentials for the selected device interface.
    - Correction data source (Thingstream or NTRIP).
    - Credentials / settings for the selected correction data source.
- When done with the configuration, switch the device mode to `start`.
- Poll the device for location data

The application will log any incoming message to the serial port in a log file, located under the [logs](./logs/) folder.