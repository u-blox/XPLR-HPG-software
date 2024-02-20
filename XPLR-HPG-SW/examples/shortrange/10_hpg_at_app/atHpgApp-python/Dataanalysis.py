#!/usr/bin/env python
# -*- coding: latin-1 -*-

import sys
import os
import datetime
import json


debug = False

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
script_path = os.path.dirname(os.path.abspath(__file__))

import Utils


class DataAnalysis(object):
    def __init__(self):
        try:
            print("Data Console Init")
            self.previous_data = None

        except Exception as error:
            raise error

    def writeLog(self, description, data):
        try:
            timestamp = str(datetime.datetime.now())
            logPath = None

            if "C213" in description:
                logPath = "./logs/C213/"
            elif "C214" in description:
                logPath = "./logs/C214/"
            else:
                logPath = "./logs/"

            try:
                dataToLog = "{0};{1}\r".format(data, timestamp)
                Utils.Utils.writedatafile(logPath + "log.csv", "a", dataToLog)
                print(
                    "["
                    + dataToLog
                    + "]"
                    + " : "
                    + "["
                    + timestamp
                    + "]"
                    )
            except Exception as error:
                dataToLog = "{0};{1}\r".format(error, data)
                Utils.Utils.writedatafile(logPath + "errors.log", "a", dataToLog)
                raise error
        
        except Exception as error:
                print(error)
                raise error


    def processdata(self, data, description):
        try:
            if (self.previous_data != data):
                self.writeLog(description, data)
                self.previous_data = data

        except Exception as error:
            raise error
