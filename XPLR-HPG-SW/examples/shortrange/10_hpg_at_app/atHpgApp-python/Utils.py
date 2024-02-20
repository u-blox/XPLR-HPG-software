#!/usr/bin/env python
#-*- coding: latin-1 -*-

import socket
from urllib import request
import re


class Utils(object):

    @staticmethod
    def checkipformat(ip):

        try:

            regex = "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
            mskformat = re.match(regex, ip)

            if mskformat:
                #ip = mskformat.group()
                return True
            else:
                return False
        except:
            raise

    @staticmethod
    def getlocalIp():

        """"Get local IP address.
        """
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
        except:
            ip = None
            raise

        finally:
            s.close()
            return ip

    @staticmethod
    def getPublicIp():

        """Get external IP using dyndns"""
        try:
            data = str(request('http://checkip.dyndns.com/').read())
            #data = '<html><head><title>Current IP Check</title></head><body>Current IP Address: 65.96.168.198</body></html>\r\n'
            externalip = re.compile(r'Address: (\d+\.\d+\.\d+\.\d+)').search(data).group(1)

        except:
            externalip = None
            raise
        finally:
            return externalip

    @staticmethod
    def remove_special_car(data):

        """Get external IP using dyndns"""
        try:
            re.sub('[^A-Za-z0-9]+', ' ', data)
        except:
            data = None
            raise
        return data

    @staticmethod
    def datafile(name, mode):

        try:
            data = open(name, mode)
        except:
            data = None

        return data

    @staticmethod
    def writedatafile(name, mode, message):

        try:
            with open(name, mode) as file:
                file.write(message)
        except:
            raise