import datetime
import time
import json
import os
import sys

from SerialCom import com
from AtInterface import at

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
script_path = os.path.dirname(os.path.abspath(__file__))


def loadDeviceConfig():
    try:
        # Load configurations using file JSON
        with open(script_path + "/config.json") as json_data_file:
            data = json.load(json_data_file)

    except Exception as error:
        print("Fail in open JSON File")
        raise error

    return data


def init_data_instances(datajson):
    """
    Load file of configuration and start come interface and UI threads
    """

    try:
        # After JSON LOAD
        device = datajson["devices"]

        timestamp = str(datetime.datetime.now())
        print(f"Start Server Logging {timestamp}")

        # Create thread for HPG com interface
        hpg = com(device[0]["description"])
        hpg.run(
            device[0]["serialport"],
            device[0]["baudrate"],
            device[0]["timeout"],
            )
        
        # Create thread for UI
        ui = at(hpg)
        ui.run()

    except Exception as error:
        raise error


def main():
    print("###############  Serial Logger  ########################")
    dvc_cfg = loadDeviceConfig()

    init_data_instances(dvc_cfg)

    while True:
        try:
            #keepAliveTimestamp = str(datetime.datetime.now())
            #print(f"Keep Alive ATHpg Interface {keepAliveTimestamp} \n\r")
            time.sleep(10)

        except Exception as error:
            print("Error: ", error)


if __name__ == "__main__":
    main()
