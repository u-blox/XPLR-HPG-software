#!/usr/bin/env python
#-*- coding: latin-1 -*-

import serial
import time
import datetime
import threading
import re


debug = False

from Dataanalysis import DataAnalysis


class com(threading.Thread):

    def __init__(self, description):

        """
        :type description: Name logger in test
        :type database: database
        """

        try:
            threading.Thread.__init__(self)

            print("New instance logging"), description
            self.s = None
            self.baudrate = None
            self.port = None
            self.timeout = None
            self.connState = False
            self.description = description

        except:
            raise

    def run(self, port, baudrate, timeout):

        """
        Start thread for data processpath
        :type port: serial port conection ex. /dev/ttyUSB
        :type baudrate: 115200
        :type timeout: time for response
        """
        #print "Starting " + self.name
        self.baudrate = baudrate
        self.port = port
        self.timeout = timeout

        self._process = p = threading.Thread(target=self._process)
        p.daemon = True
        p.start()

    def _process(self):

        while True:
            try:
                if self.serialstate() is False:
                    self.connect(self.port, self.baudrate, self.timeout)
                    self.connState = True
                else:
                    self.receivedata()
            except:
                self.connState = False
                self.disconnect()
                #raise
            time.sleep(1)

    def connect(self, port, baudrate, timeout, rtscts=False, dsrdtr=False):

        try:
            self.baudrate = baudrate
            self.port = port
            self.timeout = timeout

            self.s = serial.Serial(port, baudrate=baudrate, timeout=timeout, rtscts=rtscts, dsrdtr=dsrdtr)
            self.connState = self.s.isOpen()

            if debug is True:
                print("Baudrate: ", self.baudrate)
                print("Port: ", self.port)
                print("Timeout: {0} ms".format(self.timeout))

        except Exception as error:
            self.connState = False
            print(error)
            raise

    def disconnect(self):

        try:
            if self.connState is True:
                self.s = None
                self.connState = False
        except:
            self.connState = False
            raise

    def serialstate(self):
        return self.connState

    def receivedata(self):

        header = "[" + self.description + " ] "

        dataanalysis = DataAnalysis()

        while True:

            try:

                data = self.s.readline().decode()
                if len(data):
                    log = data

                    if debug is True:
                        print(log + '[' + str(datetime.datetime.now()) + ']')

                    # Data processing class
                    dataanalysis.processdata(log, self.description)

            except Exception as error:

                self.connState = False
                print("exception data: ", error)
                raise

    def transmitdata(self, data):
        try:
            if self.connect:
                data += '\r\n'
                print('sending: ' + data)
                enc_data = data.encode(encoding = 'ascii')
                self.s.write(enc_data)
        
        except Exception as error:
            self.connState = False
            print("exception data: ", error)
            raise