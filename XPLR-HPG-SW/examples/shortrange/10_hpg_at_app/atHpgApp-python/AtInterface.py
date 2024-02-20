#!/usr/bin/env python
# -*- coding: latin-1 -*-

import time
import datetime
import threading
import re
import json
import os
import sys

from AtApi import at_api

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
script_path = os.path.dirname(os.path.abspath(__file__))


class at(threading.Thread):
    def __init__(self, interface):
        try:
            threading.Thread.__init__(self)

            print("New AT Interface instance")
            # Load settings from JSON
            with open(script_path + "/config.json") as settings_file:
                settings = json.load(settings_file)
            #with open(script_path + "/hpgApi.json") as api_file:
            #    api = json.load(api_file)

            self.settings = settings
            self.api = at_api()
            self.interface = interface
            self.sleepInterval = 2.0

        except Exception as error:
            raise error

    def run(self):
        """
        Start thread for ui
        """
        # print "Starting " + self.name

        self._process = p = threading.Thread(target=self._process)
        p.daemon = True
        p.start()

    def _process(self):
        menu = "default"
        choice = None
        choice_fields = []

        self.display_title_bar()

        while choice != "q":
            try:
                if self.interface.serialstate() is True:
                    choice = self.display_menu(menu)

                    # Respond to the user's choice.
                    self.display_title_bar()

                    if (menu == 'default'):
                        if (choice == '1'):
                            menu = 'dvc'
                        elif (choice == '2'):
                            menu = 'thingstream'
                        elif (choice == '3'):
                            menu = 'ntrip'
                        elif (choice == '4'):
                            menu = 'api'
                        elif (choice == 'q'):
                            self.clear_terminal()
                            sys.exit()
                        else:
                            pass
                    elif (menu == 'dvc'):
                        if (choice == 'b'):
                            menu = 'default'
                        elif (choice == 'q'):
                            self.clear_terminal()
                            sys.exit()
                        else:
                            choiceNum = -1 
                            try:
                                choice_filtered = choice.replace(',', '')
                                fields = choice_filtered.split(' ')
                                choiceNum = int(fields[0])
                            except Exception as error:
                                choice = None
                                print("Not a valid choice, format should be: <option>, <value> ...")

                            if (choiceNum == 1):
                                self.dvc_fconfig_set()
                            elif (choiceNum == 2):
                                self.dvc_fconfig_get()
                            elif (choiceNum == 3):
                                self.dvc_fconfig_interface_set()
                            elif (choiceNum == 4):
                                self.dvc_fconfig_interface_get()
                            elif (choiceNum == 5):
                                self.dvc_fconfig_wifi_set()
                            elif (choiceNum == 6):
                                self.dvc_fconfig_wifi_get()
                            elif (choiceNum == 7):
                                self.dvc_fconfig_apn_set()
                            elif (choiceNum == 8):
                                self.dvc_fconfig_apn_get()
                            elif (choiceNum == 9):
                                self.dvc_fconfig_correction_source_set()
                            elif (choiceNum == 10):
                                self.dvc_fconfig_correction_source_get()
                            elif (choiceNum == 11):
                                self.dvc_fconfig_correction_module_set()
                            elif (choiceNum == 12):
                                self.dvc_fconfig_correction_module_get()
                            elif (choiceNum == 13):
                                self.dvc_fconfig_dead_reckon_set()
                            elif (choiceNum == 14):
                                self.dvc_fconfig_dead_reckon_get()
                            elif (choiceNum == 15):
                                self.dvc_fconfig_sd_log_set()
                            elif (choiceNum == 16):
                                self.dvc_fconfig_sd_log_get()
                    elif (menu == 'thingstream'):
                        if (choice == 'b'):
                            menu = 'default'
                        elif (choice == 'q'):
                            self.clear_terminal()
                            sys.exit()
                        else:
                            choiceNum = -1 
                            try:
                                choice_filtered = choice.replace(',', '')
                                fields = choice_filtered.split(' ')
                                choiceNum = int(fields[0])
                            except Exception as error:
                                choice = None
                                print("Not a valid choice, format should be: <option>, <value> ...")
                            
                            if (choiceNum == 1):
                                self.thingstream_fconfig_set()
                            elif (choiceNum == 2):
                                self.thingstream_fconfig_get()
                            elif (choiceNum == 3):
                                self.thingstream_fconfig_broker_set()
                            elif (choiceNum == 4):
                                self.thingstream_fconfig_broker_get()
                            elif (choiceNum == 5):
                                self.thingstream_fconfig_client_id_set()
                            elif (choiceNum == 6):
                                self.thingstream_fconfig_client_id_get()
                            elif (choiceNum == 7):
                                self.thingstream_fconfig_key_set()
                            elif (choiceNum == 8):
                                self.thingstream_fconfig_key_get()
                            elif (choiceNum == 9):
                                self.thingstream_fconfig_certificate_set()
                            elif (choiceNum == 10):
                                self.thingstream_fconfig_certificate_get()
                            elif (choiceNum == 11):
                                self.thingstream_fconfig_root_ca_set()
                            elif (choiceNum == 12):
                                self.thingstream_fconfig_root_ca_get()
                            elif (choiceNum == 13):
                                self.thingstream_fconfig_plan_set()
                            elif (choiceNum == 14):
                                self.thingstream_fconfig_plan_get()
                            elif (choiceNum == 15):
                                self.thingstream_fconfig_region_set()
                            elif (choiceNum == 16):
                                self.thingstream_fconfig_region_get()
                    elif (menu == 'ntrip'):
                        if (choice == 'b'):
                            menu = 'default'
                        elif (choice == 'q'):
                            self.clear_terminal()
                            sys.exit()
                        else:
                            choiceNum = -1 
                            try:
                                choice_filtered = choice.replace(',', '')
                                fields = choice_filtered.split(' ')
                                choiceNum = int(fields[0])
                            except Exception as error:
                                choice = None
                                print("Not a valid choice, format should be: <option>, <value> ...")

                            if (choiceNum == 1):
                                self.ntrip_fconfig_set()
                            elif (choiceNum == 2):
                                self.ntrip_fconfig_get()
                            elif (choiceNum == 3):
                                self.ntrip_fconfig_server_set()
                            elif (choiceNum == 4):
                                self.ntrip_fconfig_server_get()
                            elif (choiceNum == 5):
                                self.ntrip_fconfig_user_agent_set()
                            elif (choiceNum == 6):
                                self.ntrip_fconfig_user_agent_get()
                            elif (choiceNum == 7):
                                self.ntrip_fconfig_mount_point_set()
                            elif (choiceNum == 8):
                                self.ntrip_fconfig_mount_point_get()
                            elif (choiceNum == 9):
                                self.ntrip_fconfig_credentials_set()
                            elif (choiceNum == 10):
                                self.ntrip_fconfig_credentials_get()
                            elif (choiceNum == 11):
                                self.ntrip_fconfig_gga_relay_set()
                            elif (choiceNum == 12):
                                self.ntrip_fconfig_gga_relay_get()
                    elif (menu == 'api'):
                        if (choice == 'b'):
                            menu = 'default'
                        elif (choice == 'q'):
                            self.clear_terminal()
                            sys.exit()
                        else:
                            choiceNum = -1 
                            try:
                                choice_filtered = choice.replace(',', '')
                                fields = choice_filtered.split(' ')
                                choiceNum = int(fields[0])
                            except Exception as error:
                                choice = None
                                print("Not a valid choice")

                            # WIFI options
                            if (choiceNum == 1): #set wifi
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 3:
                                    ssid = choice_fields[1]
                                    password = choice_fields[2]
                                    self.wifi_set(ssid, password)
                                else:
                                    print('Command should be of format: <option>, <ssid>, <password>')
                            elif (choiceNum == 2): #get wifi
                                self.wifi_get()
                            elif (choiceNum == 3): #delete wifi
                                self.wifi_delete()
                            
                            # APN options
                            elif (choiceNum == 4): #set apn
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    apn = choice_fields[1]
                                    self.apn_set(apn)
                                else:
                                    print('Command should be of format: <option>, <apn>')
                            elif (choiceNum == 5): #get apn
                                self.apn_get()
                            elif (choiceNum == 6): #delete apn
                                self.apn_delete()
                            
                            # Thingstream options
                            elif (choiceNum == 7): #set ts broker
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 3:
                                    broker = choice_fields[1]
                                    port = choice_fields[2]
                                    self.thingstream_broker_set(broker, port)
                                else:
                                    print('Command should be of format: <option>, <broker>, <port>')
                            elif (choiceNum == 8): #get ts broker
                                self.thingstream_broker_get()
                            elif (choiceNum == 9): #delete ts broker
                                self.thingstream_broker_delete()
                            
                            elif (choiceNum == 10): #set ts client ID
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    client_id = choice_fields[1]
                                    self.thingstream_client_id_set(client_id)
                                else:
                                    print('Command should be of format: <option>, <client id>')
                            elif (choiceNum == 11): #get ts client ID
                                self.thingstream_client_id_get()
                            elif (choiceNum == 12): #delete ts client ID
                                self.thingstream_client_id_delete()
    
                            elif (choiceNum == 13): #set ts certificate
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    cert = choice_fields[1]
                                    self.thingstream_certificate_set(cert)
                                else:
                                    print('Command should be of format: <option>, <cert>')
                            elif (choiceNum == 14): #get ts certificate
                                self.thingstream_certificate_get()
                            elif (choiceNum == 15): #delete ts certificate
                                self.thingstream_certificate_delete()
    
                            elif (choiceNum == 16): #set ts key
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    key = choice_fields[1]
                                    self.thingstream_key_set(key)
                                else:
                                    print('Command should be of format: <option>, <key>')
                            elif (choiceNum == 17): #get ts key
                                self.thingstream_key_get()
                            elif (choiceNum == 18): #delete ts key
                                self.thingstream_key_delete()
    
                            elif (choiceNum == 19): #set ts plan
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    plan = choice_fields[1]
                                    self.thingstream_plan_set(plan)
                                else:
                                    print('Command should be of format: <option>, <key>')
                            elif (choiceNum == 20): #get ts plan
                                self.thingstream_plan_get()
                            elif (choiceNum == 21): #delete ts plan
                                self.thingstream_plan_delete()
                            
                            elif (choiceNum == 22): #set ts region
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    region = choice_fields[1]
                                    self.thingstream_region_set(region)
                                else:
                                    print('Command should be of format: <option>, <region>')
                            elif (choiceNum == 23): #get ts region
                                self.thingstream_region_get()
                            elif (choiceNum == 24): #delete ts region
                                self.thingstream_region_delete()
    
                            elif (choiceNum == 25): #set ts root ca
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    root_ca = choice_fields[1]
                                    self.thingstream_root_ca_set(root_ca)
                                else:
                                    print('Command should be of format: <option>, <root_ca>')
                            elif (choiceNum == 26): #get ts root ca
                                self.thingstream_root_ca_get()
                            elif (choiceNum == 27): #delete ts root ca
                                self.thingstream_root_ca_delete()
    
                            #NTRIP options
                            elif (choiceNum == 28): #set ntrip server
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 3:
                                    server = choice_fields[1]
                                    port = choice_fields[2]
                                    self.ntrip_server_set(server, port)
                                else:
                                    print('Command should be of format: <option>, <server>')
                            elif (choiceNum == 29): #get ntrip server
                                self.ntrip_server_get()
                            elif (choiceNum == 30): #delete ntrip server
                                self.ntrip_server_delete()
    
                            elif (choiceNum == 31): #set ntrip user agent
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    ua = choice_fields[1]
                                    self.ntrip_user_agent_set(ua)
                                else:
                                    print('Command should be of format: <option>, <user_agent>')
                            elif (choiceNum == 32): #get ntrip user agent
                                self.ntrip_user_agent_get()
                            elif (choiceNum == 33): #delete ntrip user agent
                                self.ntrip_user_agent_delete()
    
                            elif (choiceNum == 34): #set ntrip mount point
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    mp = choice_fields[1]
                                    self.ntrip_mount_point_set(mp)
                                else:
                                    print('Command should be of format: <option>, <mount_point>')
                            elif (choiceNum == 35): #get ntrip mount point
                                self.ntrip_mount_point_get()
                            elif (choiceNum == 36): #delete ntrip mount point
                                self.ntrip_mount_point_delete()
    
                            elif (choiceNum == 37): #set credentials
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 3:
                                    user = choice_fields[1]
                                    password = choice_fields[2]
                                    self.ntrip_credentials_set(user, password)
                                else:
                                    print('Command should be of format: <option>, <username>, <password>')
                            elif (choiceNum == 38): #get credentials
                                self.ntrip_credentials_get()
                            elif (choiceNum == 39): #delete credentials
                                self.ntrip_credentials_delete()
    
                            elif (choiceNum == 40): #set gga relay
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    gga = choice_fields[1]
                                    self.ntrip_gga_relay_set(gga)
                                else:
                                    print('Command should be of format: <option>, <gga_relay>')
                            elif (choiceNum == 41): #get gga relay
                                self.ntrip_gga_relay_get()
    
                            elif (choiceNum == 42): #set gnss dead reckoning
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    dr = choice_fields[1]
                                    self.misc_dead_reckoning_set(dr)
                                else:
                                    print('Command should be of format: <option>, <dead_reckoning>')
                            elif (choiceNum == 43): #get gnss dead reckoning
                                self.misc_dead_reckoning_get()
    
                            elif (choiceNum == 44): #set sd log
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    sd = choice_fields[1]
                                    self.misc_sd_log_set(sd)
                                else:
                                    print('Command should be of format: <option>, <sd_log>')
                            elif (choiceNum == 45): #get sd log
                                self.misc_sd_log_get()
    
                            elif (choiceNum == 46): #set interface
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    interface = choice_fields[1]
                                    self.misc_interface_set(interface)
                                else:
                                    print('Command should be of format: <option>, <interface>')
                            elif (choiceNum == 47): #get interface
                                self.misc_interface_get()
    
                            elif (choiceNum == 48): #set correction data source
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    source = choice_fields[1]
                                    self.misc_correction_data_source_set(source)
                                else:
                                    print('Command should be of format: <option>, <source>')
                            elif (choiceNum == 49): #get correction data source
                                self.misc_correction_data_source_get()
    
                            elif (choiceNum == 50): #set correction data module
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    module = choice_fields[1]
                                    self.misc_correction_data_module_set(module)
                                else:
                                    print('Command should be of format: <option>, <source>')
                            elif (choiceNum == 51): #get correction data module
                                self.misc_correction_data_module_get()

                            elif (choiceNum == 52): #set auto start
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    auto_start = choice_fields[1]
                                    self.misc_auto_start_set(auto_start)
                                else:
                                    print('Command should be of format: <option>, <auto_start>')
                            elif (choiceNum == 53): #get auto start
                                self.misc_auto_start_get()
    
                            elif (choiceNum == 54): #set baudrate
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    baudrate = choice_fields[1]
                                    self.misc_baudrate_set(baudrate)
                                else:
                                    print('Command should be of format: <option>, <auto_start>')
                            elif (choiceNum == 55): #get baudrate
                                self.misc_baudrate_get()
    
                            elif (choiceNum == 56): #set mode
                                choice_fields = choice.split(', ')
                                if len(choice_fields) > 1 and len(choice_fields) == 2:
                                    mode = choice_fields[1]
                                    self.misc_device_mode_set(mode)
                                else:
                                    print('Command should be of format: <option>, <mode>')
                            elif (choiceNum == 57): #get mode
                                self.misc_device_mode_get()
    
                            elif (choiceNum == 58): #get board info
                                self.misc_device_info_get()
    
                            elif (choiceNum == 59): #get location
                                self.misc_location_get()
    
                            elif (choiceNum == 60): #check device
                                self.misc_check_device()
    
                            elif (choiceNum == 61): #restart device
                                self.misc_restart_device()
    
                            elif (choiceNum == 62): #factory reset
                                self.misc_reset_device()
    
                            elif (choiceNum == 63): #get wifi status
                                self.status_wifi_get()
    
                            elif (choiceNum == 64): #get cell status
                                self.status_cell_get()
    
                            elif (choiceNum == 65): #get thingstream status
                                self.status_thingstream_get()
    
                            elif (choiceNum == 66): #get ntrip status
                                self.status_ntrip_get()
    
                            elif (choiceNum == 67): #get gnss status
                                self.status_gnss_get()

                            choice = None
                            
            except Exception as error:
                self.interface.connState = False
                self.interface.disconnect()
                raise error
            
            time.sleep(0.5)

    #UI
            
    def display_menu(self, page):
        try:
            if page == "default":
                print("\n[1] Config Device Settings.")
                print("[2] Config Thingstream Settings.")
                print("[3] Config NTRIP Settings.")
                print("[4] HPG AT API.")
                print("\n[q] Quit.")
            elif page == "dvc":
                print("\n[1] Load Settings from file.")
                print("[2] Display Current Settings.")
                print("[3] Set Device Communication Interface.")
                print("[4] Read Device Communication Interface.")
                print("[5] Set Wi-Fi Credentials.")
                print("[6] Read Wi-Fi Credentials.")
                print("[7] Set APN.")
                print("[8] Read APN.")
                print("[9] Set Correction Source.")
                print("[10] Read Correction Source.")
                print("[11] Set Correction Module.")
                print("[12] Read Correction Module.")
                print("[13] Set GNSS DR Option.")
                print("[14] Read GNSS DR Option.")
                print("[15] Set SD Log Option.")
                print("[16] Read SD Log Option.")
                print("\n[b] Back.")
                print("[q] Quit.")
            elif page == "thingstream":
                print("\n[1] Load Settings from file.")
                print("[2] Display Current Settings.")
                print("[3] Set Broker Address.")
                print("[4] Read Broker Address.")
                print("[5] Set Client ID.")
                print("[6] Read Client ID.")
                print("[7] Set Key.")
                print("[8] Read Key.")
                print("[9] Set Cert.")
                print("[10] Read Cert.")
                print("[11] Set Root CA.")
                print("[12] Read Root CA.")
                print("[13] Set Plan.")
                print("[14] Read Plan.")
                print("[15] Set Region.")
                print("[16] Read Region.")
                print("\n[b] Back.")
                print("[q] Quit.")
            elif page == "ntrip":
                print("\n[1] Load Settings from file.")
                print("[2] Display Current Settings.")
                print("[3] Set NTRIP Server Address.")
                print("[4] Read NTRIP Server Address.")
                print("[5] Set NTRIP Server User Agent.")
                print("[6] Read NTRIP Server User Agent.")
                print("[7] Set NTRIP Server Mount Point.")
                print("[8] Read NTRIP Server Mount Point.")
                print("[9] Set NTRIP Server Credentials.")
                print("[10] Read NTRIP Server Credentials.")
                print("[11] Set GGA Relay Option.")
                print("[12] Read GGA Relay Option.")
                print("\n[b] Back.")
                print("[q] Quit.")
            elif page == "api":
                print("\n[1] Set Wi-Fi Credentials.")
                print("[2] Get Wi-Fi Credentials.")
                print("[3] Erase Wi-Fi Credentials.")
                print("[4] Set Cellular APN.")
                print("[5] Get Cellular APN.")
                print("[6] Erase Cellular APN.")
                print("[7] Set Thingstream Broker.")
                print("[8] Get Thingstream Broker.")
                print("[9] Erase Thingstream Broker.")
                print("[10] Set Thingstream Client ID.")
                print("[11] Get Thingstream Client ID.")
                print("[12] Erase Thingstream Client ID.")
                print("[13] Set Thingstream Certificate.")
                print("[14] Get Thingstream Certificate.")
                print("[15] Erase Thingstream Certificate.")
                print("[16] Set Thingstream Key.")
                print("[17] Get Thingstream Key.")
                print("[18] Erase Thingstream Key.")
                print("[19] Set Thingstream Plan.")
                print("[20] Get Thingstream Plan.")
                print("[21] Erase Thingstream Plan.")
                print("[22] Set Thingstream Region.")
                print("[23] Get Thingstream Region.")
                print("[24] Erase Thingstream Region.")
                print("[25] Set Root CA.")
                print("[26] Get Root CA.")
                print("[27] Erase Root CA.")
                print("[28] Set NTRIP Server.")
                print("[29] Get NTRIP Server.")
                print("[30] Erase NTRIP Server.")
                print("[31] Set NTRIP User Agent.")
                print("[32] Get NTRIP User Agent.")
                print("[33] Erase NTRIP User Agent.")
                print("[34] Set NTRIP Mount Point.")
                print("[35] Get NTRIP Mount Point.")
                print("[36] Erase NTRIP Mount Point.")
                print("[37] Set NTRIP Credentials.")
                print("[38] Get NTRIP Credentials.")
                print("[39] Erase NTRIP Credentials.")
                print("[40] Set NTRIP GGA Relay.")
                print("[41] Get NTRIP GGA Relay.")
                print("[42] Set GNSS Dead Reckoning.")
                print("[43] Get GNSS Dead Reckoning.")
                print("[44] Set SD Log.")
                print("[45] Get SD Log.")
                print("[46] Set Interface.")
                print("[47] Get Interface.")
                print("[48] Set Correction Source.")
                print("[49] Get Correction Source.")
                print("[50] Set Correction Module.")
                print("[51] Get Correction Module.")
                print("[52] Set Auto Start.")
                print("[53] Get Auto Start.")
                print("[54] Set Baudrate.")
                print("[55] Get Baudrate.")
                print("[56] Set Device Mode.")
                print("[57] Get Device Mode.")
                print("[58] Get Board Info.")
                print("[59] Get Location.")
                print("[60] Check Device.")
                print("[61] Restart Device.")
                print("[62] Erase Device (Factory Reset).")
                print("[63] Get Wi-Fi Status.")
                print("[64] Get Cell Status.")
                print("[65] Get Thingstream Status.")
                print("[66] Get NTRIP Status.")
                print("[67] Get GNSS Status.")
                print("\n[b] Back.")
                print("[q] Quit.")

            time.sleep(0.1)
            return input("\nEnter Option to continue...\n")

        except Exception as error:
            print(error)
            raise error

    def clear_terminal(self):
        if os.name == "nt":
            os.system("cls")  # windows
        else:
            os.system("clear")  # linux / mac

    def display_title_bar(self):
        # Clears the terminal screen, and displays a title bar.
        self.clear_terminal()

        print("\t**********************************************")
        print("\t***            HPG AT Interface            ***")
        print("\t**********************************************")

    #DVC Settings From File
    
    def dvc_fconfig_interface_set(self):
        """
        Set device communication interface from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['interface']
        self.misc_interface_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_interface_get(self):
        """
        Get communication interface option from device.
        """
        self.misc_interface_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_wifi_set(self):
        """
        Set device wi-fi credentials from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        ssid = self.settings['settings'][0]['ssid']
        password = self.settings['settings'][0]['password']
        self.wifi_set(ssid, password)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_wifi_get(self):
        """
        Get wi-fi credentials from device.
        """
        self.wifi_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_apn_set(self):
        """
        Set device APN from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['apn']
        self.apn_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_apn_get(self):
        """
        Get APN from device.
        """
        self.apn_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_correction_source_set(self):
        """
        Set device correction data source from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['correctionSource']
        self.misc_correction_data_source_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_correction_source_get(self):
        """
        Get correction data source from device.
        """
        self.misc_correction_data_source_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_correction_module_set(self):
        """
        Set device correction data module from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['correctionModule']
        self.misc_correction_data_module_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_correction_module_get(self):
        """
        Get correction data module from device.
        """
        self.misc_correction_data_module_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_dead_reckon_set(self):
        """
        Set device dead reckoning option from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['gnssDR']
        self.misc_dead_reckoning_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_dead_reckon_get(self):
        """
        Get dead reckoning option from device.
        """
        self.misc_dead_reckoning_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_sd_log_set(self):
        """
        Set device SD log option from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['settings'][0]['sdLog']
        self.misc_sd_log_set(param)
        time.sleep(self.sleepInterval)

    def dvc_fconfig_sd_log_get(self):
        """
        Get dead reckoning option from device.
        """
        self.misc_sd_log_get()
        time.sleep(self.sleepInterval)

    def dvc_fconfig_set(self):
        """
        Set device options from file.
        """
        self.dvc_fconfig_interface_set()
        self.dvc_fconfig_wifi_set()
        self.dvc_fconfig_apn_set()
        self.dvc_fconfig_correction_source_set()
        self.dvc_fconfig_correction_module_set()
        self.dvc_fconfig_dead_reckon_set()
        self.dvc_fconfig_sd_log_set()

    def dvc_fconfig_get(self):
        """
        Get device options from device.
        """
        self.dvc_fconfig_interface_get()
        self.dvc_fconfig_wifi_get()
        self.dvc_fconfig_apn_get()
        self.dvc_fconfig_correction_source_get()
        self.dvc_fconfig_correction_module_get()
        self.dvc_fconfig_dead_reckon_get()
        self.dvc_fconfig_sd_log_get()
    
    #Thingstream Settings From File
        
    def thingstream_fconfig_broker_set(self):
        """
        Set Thingstream broker from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        broker = self.settings['thingstream'][0]['broker']
        port = self.settings['thingstream'][0]['port']
        self.thingstream_broker_set(broker, port)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_broker_get(self):
        """
        Get Thingstream broker from device.
        """
        self.thingstream_broker_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_client_id_set(self):
        """
        Set Thingstream client ID from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['clientId']
        self.thingstream_client_id_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_client_id_get(self):
        """
        Get Thingstream client ID from device.
        """
        self.thingstream_client_id_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_certificate_set(self):
        """
        Set Thingstream certificate from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['cert']
        self.thingstream_certificate_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_certificate_get(self):
        """
        Get Thingstream certificate from device.
        """
        self.thingstream_certificate_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_key_set(self):
        """
        Set Thingstream key from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['key']
        self.thingstream_key_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_key_get(self):
        """
        Get Thingstream key from device.
        """
        self.thingstream_key_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_root_ca_set(self):
        """
        Set Thingstream root CA from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['rootCa']
        self.thingstream_root_ca_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_root_ca_get(self):
        """
        Get Thingstream root CA from device.
        """
        self.thingstream_root_ca_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_region_set(self):
        """
        Set Thingstream region from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['region']
        self.thingstream_region_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_region_get(self):
        """
        Get Thingstream region from device.
        """
        self.thingstream_region_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_plan_set(self):
        """
        Set Thingstream region from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['thingstream'][0]['plan']
        self.thingstream_plan_set(param)
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_plan_get(self):
        """
        Get Thingstream region from device.
        """
        self.thingstream_plan_get()
        time.sleep(self.sleepInterval)

    def thingstream_fconfig_set(self):
        """
        Set Thingstream options from file.
        """
        self.thingstream_fconfig_broker_set()
        self.thingstream_fconfig_client_id_set()
        self.thingstream_fconfig_root_ca_set()
        self.thingstream_fconfig_certificate_set()
        self.thingstream_fconfig_key_set()
        self.thingstream_fconfig_region_set()
        self.thingstream_fconfig_plan_set()

    def thingstream_fconfig_get(self):
        """
        Get Thingstream options from device.
        """
        self.thingstream_fconfig_broker_get()
        self.thingstream_fconfig_client_id_get()
        self.thingstream_fconfig_root_ca_get()
        self.thingstream_fconfig_certificate_get()
        self.thingstream_fconfig_key_get()
        self.thingstream_fconfig_region_get()
        self.thingstream_fconfig_plan_get()
    
    #NTRIP Settings From File
    
    def ntrip_fconfig_server_set(self):
        """
        Set NTRIP server from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        server = self.settings['ntrip'][0]['server']
        port = self.settings['ntrip'][0]['port']
        self.ntrip_server_set(server, port)
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_server_get(self):
        """
        Get NTRIP server from device.
        """
        self.ntrip_server_get()
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_mount_point_set(self):
        """
        Set NTRIP mount point from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['ntrip'][0]['mountPoint']
        self.ntrip_mount_point_set(param)
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_mount_point_get(self):
        """
        Get NTRIP mount point from device.
        """
        self.ntrip_mount_point_get()
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_user_agent_set(self):
        """
        Set NTRIP user agent from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['ntrip'][0]['userAgent']
        self.ntrip_user_agent_set(param)
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_user_agent_get(self):
        """
        Get NTRIP user agent from device.
        """
        self.ntrip_user_agent_get()
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_credentials_set(self):
        """
        Set NTRIP credentials from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        user = self.settings['ntrip'][0]['username']
        password = self.settings['ntrip'][0]['password']
        self.ntrip_credentials_set(user, password)
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_credentials_get(self):
        """
        Get NTRIP credentials from device.
        """
        self.ntrip_credentials_get()
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_gga_relay_set(self):
        """
        Set NTRIP GGA relay from config file.
        """
        self.misc_device_mode_set('config')
        time.sleep(self.sleepInterval)
        param = self.settings['ntrip'][0]['ggaRelay']
        self.ntrip_gga_relay_set(param)
        time.sleep(self.sleepInterval)

    def ntrip_fconfig_gga_relay_get(self):
        """
        Get NTRIP GGA relay from device.
        """
        self.ntrip_gga_relay_get()
        time.sleep(self.sleepInterval)
    
    def ntrip_fconfig_set(self):
        """
        Set NTRIP options from file.
        """
        self.ntrip_fconfig_server_set()
        self.ntrip_fconfig_mount_point_set()
        self.ntrip_fconfig_user_agent_set()
        self.ntrip_fconfig_credentials_set()
        self.ntrip_fconfig_gga_relay_set()

    def ntrip_fconfig_get(self):
        """
        Get NTRIP options from device.
        """
        self.ntrip_fconfig_server_get()
        self.ntrip_fconfig_mount_point_get()
        self.ntrip_fconfig_user_agent_get()
        self.ntrip_fconfig_credentials_get()
        self.ntrip_fconfig_gga_relay_get()

    #AT HPG API Command Set
        
    def wifi_set(self, ssid: str, password: str):
        """
        Set wifi credentials.
        password param should not contain "," (comma) character
        """
        cmd = self.api.api_cmd_wifi_set(ssid, password)
        self.interface.transmitdata(cmd)

    def wifi_get(self):
        """
        Get wifi credentials.
        """
        cmd = self.api.api_cmd_wifi_get()
        self.interface.transmitdata(cmd)

    def wifi_delete(self):
        """
        Delete wifi credentials.
        """
        cmd = self.api.api_cmd_wifi_delete()
        self.interface.transmitdata(cmd)

    def apn_set(self, apn: str):
        """
        Set APN for cellular interface.
        """
        cmd = self.api.api_cmd_apn_set(apn)
        self.interface.transmitdata(cmd)
    
    def apn_get(self):
        """
        Get APN of cellular interface.
        """
        cmd = self.api.api_cmd_apn_get()
        self.interface.transmitdata(cmd)
    
    def apn_delete(self):
        """
        Delete APN for cellular interface.
        """
        cmd = self.api.api_cmd_apn_delete()
        self.interface.transmitdata(cmd)

    def thingstream_broker_set(self, broker: str, port: str):
        """
        Set Thingstream broker address.
        """
        cmd = self.api.api_cmd_thingstream_set_broker(broker, port)
        self.interface.transmitdata(cmd)

    def thingstream_broker_get(self):
        """
        Get Thingstream broker address.
        """
        cmd = self.api.api_cmd_thingstream_get_broker()
        self.interface.transmitdata(cmd)

    def thingstream_broker_delete(self):
        """
        Delete Thingstream broker address.
        """
        cmd = self.api.api_cmd_thingstream_delete_broker()
        self.interface.transmitdata(cmd)

    def thingstream_client_id_set(self, client_id: str):
        """
        Set Thingstream client ID.
        """
        cmd = self.api.api_cmd_thingstream_set_client_id(client_id)
        self.interface.transmitdata(cmd)
    
    def thingstream_client_id_get(self):
        """
        Get Thingstream client ID.
        """
        cmd = self.api.api_cmd_thingstream_get_client_id()
        self.interface.transmitdata(cmd)
    
    def thingstream_client_id_delete(self):
        """
        Delete Thingstream client ID.
        """
        cmd = self.api.api_cmd_thingstream_delete_client_id()
        self.interface.transmitdata(cmd)

    def thingstream_certificate_set(self, cert: str):
        """
        Set Thingstream certificate.
        Should not contain any /r/n characters
        """
        cmd = self.api.api_cmd_thingstream_set_certificate(cert)
        self.interface.transmitdata(cmd)
    
    def thingstream_certificate_get(self):
        """
        Get Thingstream certificate.
        """
        cmd = self.api.api_cmd_thingstream_get_certificate()
        self.interface.transmitdata(cmd)
    
    def thingstream_certificate_delete(self):
        """
        Delete Thingstream certificate.
        """
        cmd = self.api.api_cmd_thingstream_delete_certificate()
        self.interface.transmitdata(cmd)

    def thingstream_key_set(self, key: str):
        """
        Set Thingstream key.
        Should not contain any /r/n characters
        """
        cmd = self.api.api_cmd_thingstream_set_key(key)
        self.interface.transmitdata(cmd)

    def thingstream_key_get(self):
        """
        Get Thingstream key.
        """
        cmd = self.api.api_cmd_thingstream_get_key()
        self.interface.transmitdata(cmd)
        
    def thingstream_key_delete(self):
        """
        Delete Thingstream key.
        """
        cmd = self.api.api_cmd_thingstream_delete_key()
        self.interface.transmitdata(cmd)

    def thingstream_root_ca_set(self, root_ca: str):
        """
        Set Thingstream root CA.
        Should not contain any /r/n characters
        """
        cmd = self.api.api_cmd_thingstream_set_root_ca(root_ca)
        self.interface.transmitdata(cmd)

    def thingstream_root_ca_get(self):
        """
        Get Thingstream root ca.
        """
        cmd = self.api.api_cmd_thingstream_get_root_ca()
        self.interface.transmitdata(cmd)

    def thingstream_root_ca_delete(self):
        """
        Delete Thingstream root ca.
        """
        cmd = self.api.api_cmd_thingstream_delete_root_ca()
        self.interface.transmitdata(cmd)

    def thingstream_region_set(self, region: str):
        """
        Set Thingstream region.
        region param: "eu", "us", "kr", "jp" or "au"
        """
        cmd = self.api.api_cmd_thingstream_set_region(region)
        self.interface.transmitdata(cmd)

    def thingstream_region_get(self):
        """
        Get Thingstream region.
        """
        cmd = self.api.api_cmd_thingstream_get_region()
        self.interface.transmitdata(cmd)
        
    def thingstream_region_delete(self):
        """
        Delete Thingstream region.
        """
        cmd = self.api.api_cmd_thingstream_delete_region()
        self.interface.transmitdata(cmd)
        
    def thingstream_plan_set(self, plan: str):
        """
        Set Thingstream plan.
        plan param: "ip", "ip+lband" or "lband"
        """
        cmd = self.api.api_cmd_thingstream_set_plan(plan)
        self.interface.transmitdata(cmd)

    def thingstream_plan_get(self):
        """
        Get Thingstream plan.
        """
        cmd = self.api.api_cmd_thingstream_get_plan()
        self.interface.transmitdata(cmd)
        
    def thingstream_plan_delete(self):
        """
        Delete Thingstream plan.
        """
        cmd = self.api.api_cmd_thingstream_delete_plan()
        self.interface.transmitdata(cmd)

    def ntrip_server_set(self, server: str, port: str):
        """
        Set NTRIP server address.
        """
        cmd = self.api.api_cmd_ntrip_set_server(server, port)
        self.interface.transmitdata(cmd)

    def ntrip_server_get(self):
        """
        Get NTRIP server address.
        """
        cmd = self.api.api_cmd_ntrip_get_server()
        self.interface.transmitdata(cmd)

    def ntrip_server_delete(self):
        """
        Delete NTRIP server address.
        """
        cmd = self.api.api_cmd_ntrip_delete_server()
        self.interface.transmitdata(cmd)

    def ntrip_user_agent_set(self, user_agent: str):
        """
        Set NTRIP server user agent.
        """
        cmd = self.api.api_cmd_ntrip_set_user_agent(user_agent)
        self.interface.transmitdata(cmd)

    def ntrip_user_agent_get(self):
        """
        Get NTRIP server user agent.
        """
        cmd = self.api.api_cmd_ntrip_get_user_agent()
        self.interface.transmitdata(cmd)

    def ntrip_user_agent_delete(self):
        """
        Delete NTRIP server user agent.
        """
        cmd = self.api.api_cmd_ntrip_delete_user_agent()
        self.interface.transmitdata(cmd)

    def ntrip_mount_point_set(self, mount_point: str):
        """
        Set NTRIP server mount point.
        """
        cmd = self.api.api_cmd_ntrip_set_mount_point(mount_point)
        self.interface.transmitdata(cmd)

    def ntrip_mount_point_get(self):
        """
        Get NTRIP server mount point.
        """
        cmd = self.api.api_cmd_ntrip_get_mount_point()
        self.interface.transmitdata(cmd)

    def ntrip_mount_point_delete(self):
        """
        Delete NTRIP server mount point.
        """
        cmd = self.api.api_cmd_ntrip_delete_mount_point()
        self.interface.transmitdata(cmd)

    def ntrip_credentials_set(self, username: str, password: str):
        """
        Set NTRIP server credentials.
        """
        cmd = self.api.api_cmd_ntrip_set_credentials(username, password)
        self.interface.transmitdata(cmd)

    def ntrip_credentials_get(self):
        """
        Get NTRIP server credentials.
        """
        cmd = self.api.api_cmd_ntrip_get_credentials()
        self.interface.transmitdata(cmd)

    def ntrip_credentials_delete(self):
        """
        Delete NTRIP server credentials.
        """
        cmd = self.api.api_cmd_ntrip_delete_credentials()
        self.interface.transmitdata(cmd)

    def ntrip_gga_relay_set(self, gga_relay: str):
        """
        Set NTRIP server GGA Relay option.
        """
        cmd = self.api.api_cmd_ntrip_set_gga_relay(gga_relay)
        self.interface.transmitdata(cmd)

    def ntrip_gga_relay_get(self):
        """
        Get NTRIP server GGA Relay option.
        """
        cmd = self.api.api_cmd_ntrip_get_gga_relay()
        self.interface.transmitdata(cmd)

    def status_wifi_get(self):
        """
        Get Wi-FI status.
        """
        cmd = self.api.api_cmd_status_get_wifi()
        self.interface.transmitdata(cmd)

    def status_cell_get(self):
        """
        Get Cell status.
        """
        cmd = self.api.api_cmd_status_get_cell()
        self.interface.transmitdata(cmd)

    def status_thingstream_get(self):
        """
        Get Thingstream status.
        """
        cmd = self.api.api_cmd_status_get_thingstream()
        self.interface.transmitdata(cmd)

    def status_ntrip_get(self):
        """
        Get NTRIP status.
        """
        cmd = self.api.api_cmd_status_get_ntrip()
        self.interface.transmitdata(cmd)

    def status_gnss_get(self):
        """
        Get GNSS status.
        """
        cmd = self.api.api_cmd_status_get_gnss()
        self.interface.transmitdata(cmd)

    def misc_dead_reckoning_set(self, enable: str):
        """
        Set GNSS dead reckoning option.
        """
        cmd = self.api.api_cmd_misc_set_dead_reckoning(enable)
        self.interface.transmitdata(cmd)

    def misc_dead_reckoning_get(self):
        """
        Get GNSS dead reckoning option.
        """
        cmd = self.api.api_cmd_misc_get_dead_reckoning()
        self.interface.transmitdata(cmd)

    def misc_sd_log_set(self, enable: str):
        """
        Set SD log option.
        """
        cmd = self.api.api_cmd_misc_set_sd_log(enable)
        self.interface.transmitdata(cmd)

    def misc_sd_log_get(self):
        """
        Get SD log option.
        """
        cmd = self.api.api_cmd_misc_get_sd_log()
        self.interface.transmitdata(cmd)

    def misc_interface_set(self, interface: str):
        """
        Set communication interface. 
        interface param: "wi-fi" or "cell".
        """
        cmd = self.api.api_cmd_misc_set_interface(interface)
        self.interface.transmitdata(cmd)

    def misc_interface_get(self):
        """
        Get communication interface.
        """
        cmd = self.api.api_cmd_misc_get_interface()
        self.interface.transmitdata(cmd)

    def misc_correction_data_source_set(self, correction_data_source: str):
        """
        Set correction data source. 
        correction_data_source param: "ts" or "ntrip".
        """
        cmd = self.api.api_cmd_misc_set_correction_data_source(correction_data_source)
        self.interface.transmitdata(cmd)

    def misc_correction_data_source_get(self):
        """
        Get correction data source.
        """
        cmd = self.api.api_cmd_misc_get_correction_data_source()
        self.interface.transmitdata(cmd)

    def misc_correction_data_module_set(self, correction_data_module: str):
        """
        Set correction data module. 
        correction_data_module param: "ip" or "lband".
        """
        cmd = self.api.api_cmd_misc_set_correction_data_module(correction_data_module)
        self.interface.transmitdata(cmd)

    def misc_correction_data_module_get(self):
        """
        Get correction data module.
        """
        cmd = self.api.api_cmd_misc_get_correction_data_module()
        self.interface.transmitdata(cmd)
    
    def misc_auto_start_set(self, auto_start: str):
        """
        Set auto start functionality. 
        auto_start param: "0" or "1".
        """
        cmd = self.api.api_cmd_misc_set_auto_start(auto_start)
        self.interface.transmitdata(cmd)

    def misc_auto_start_get(self):
        """
        Get auto start functionality.
        """
        cmd = self.api.api_cmd_misc_get_auto_start()
        self.interface.transmitdata(cmd)

    def misc_baudrate_set(self, baudrate: str):
        """
        Set uart baudrate.
        """
        cmd = self.api.api_cmd_misc_set_baudrate(baudrate)
        self.interface.transmitdata(cmd)

    def misc_baudrate_get(self):
        """
        Get uart baudrate.
        """
        cmd = self.api.api_cmd_misc_get_baudrate()
        self.interface.transmitdata(cmd)

    def misc_device_mode_set(self, mode: str):
        """
        Set device mode.
        mode param: "config", "start", "stop"
        """
        cmd = self.api.api_cmd_misc_set_device_mode(mode)
        self.interface.transmitdata(cmd)

    def misc_device_mode_get(self):
        """
        Get device mode.
        """
        cmd = self.api.api_cmd_misc_get_device_mode()
        self.interface.transmitdata(cmd)

    def misc_device_info_get(self):
        """
        Get device info.
        """
        cmd = self.api.api_cmd_misc_get_device_info()
        self.interface.transmitdata(cmd)

    def misc_restart_device(self):
        """
        Restart device.
        """
        cmd = self.api.api_cmd_misc_restart_device()
        self.interface.transmitdata(cmd)

    def misc_check_device(self):
        """
        Check device.
        """
        cmd = self.api.api_cmd_misc_check_device()
        self.interface.transmitdata(cmd)

    def misc_reset_device(self):
        """
        Reset device to factory settings.
        """
        cmd = self.api.api_cmd_misc_reset_device()
        self.interface.transmitdata(cmd)

    def misc_location_get(self):
        """
        Get location data.
        """
        cmd = self.api.api_cmd_misc_get_location()
        self.interface.transmitdata(cmd)
