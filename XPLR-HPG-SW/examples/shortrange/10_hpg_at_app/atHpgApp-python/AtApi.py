#!/usr/bin/env python
# -*- coding: latin-1 -*-

import json
import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
script_path = os.path.dirname(os.path.abspath(__file__))

class at_api:
    def __init__(self):
        try:
            print("Loading AT API")
            # Load API template from JSON
            with open(script_path + "/hpgApi.json") as api_file:
                api = json.load(api_file)

            self.api = api

        except Exception as error:
            raise error
        
    def api_cmd_wifi_set(self, ssid: str, password: str):
        """
        Set wifi credentials.
        password param should not contain "," (comma) character
        """
        if (password.find(',') != -1):
            print("Password param contains <comma> character")
            pass
        else:
            cmd = self.api['wifi'][0]['set']
            cmd += ssid
            cmd += ','
            cmd += password
            return cmd
    
    def api_cmd_wifi_get(self):
        """
        Get wifi credentials.
        """
        cmd = self.api['wifi'][0]['get']
        return cmd
    
    def api_cmd_wifi_delete(self):
        """
        Delete wifi credentials.
        """
        cmd = self.api['wifi'][0]['delete']
        return cmd
    
    def api_cmd_apn_set(self, apn: str):
        """
        Set APN for cellular interface.
        """
        cmd = self.api['apn'][0]['set']
        cmd += apn
        return cmd
    
    def api_cmd_apn_get(self):
        """
        Get APN of cellular interface.
        """
        cmd = self.api['apn'][0]['get']
        return cmd
    
    def api_cmd_apn_delete(self):
        """
        Delete APN for cellular interface.
        """
        cmd = self.api['apn'][0]['delete']
        return cmd
    
    def api_cmd_thingstream_set_broker(self, broker: str, port: str):
        """
        Set Thingstream broker address.
        """
        cmd = self.api['thingstream'][0]['brokerSet']
        cmd += broker
        cmd += ','
        cmd += port
        return cmd
    
    def api_cmd_thingstream_get_broker(self):
        """
        Get Thingstream broker address.
        """
        cmd = self.api['thingstream'][0]['brokerGet']
        return cmd
    
    def api_cmd_thingstream_delete_broker(self):
        """
        Delete Thingstream broker address.
        """
        cmd = self.api['thingstream'][0]['brokerDelete']
        return cmd

    def api_cmd_thingstream_set_client_id(self, client_id: str):
        """
        Set Thingstream client ID.
        """
        cmd = self.api['thingstream'][0]['clientIdSet']
        cmd += client_id
        return cmd
    
    def api_cmd_thingstream_get_client_id(self):
        """
        Get Thingstream client ID.
        """
        cmd = self.api['thingstream'][0]['clientIdGet']
        return cmd
    
    def api_cmd_thingstream_delete_client_id(self):
        """
        Delete Thingstream client ID.
        """
        cmd = self.api['thingstream'][0]['clientIdDelete']
        return cmd
    
    def api_cmd_thingstream_set_certificate(self, cert: str):
        """
        Set Thingstream certificate.
        Should not contain any \r\n characters
        """
        cmd = self.api['thingstream'][0]['certSet']
        cmd += cert
        return cmd
    
    def api_cmd_thingstream_get_certificate(self):
        """
        Get Thingstream certificate.
        """
        cmd = self.api['thingstream'][0]['certGet']
        return cmd
    
    def api_cmd_thingstream_delete_certificate(self):
        """
        Delete Thingstream certificate.
        """
        cmd = self.api['thingstream'][0]['certDelete']
        return cmd
    
    def api_cmd_thingstream_set_key(self, key: str):
        """
        Set Thingstream key.
        Should not contain any \r\n characters
        """
        cmd = self.api['thingstream'][0]['keySet']
        cmd += key
        return cmd
    
    def api_cmd_thingstream_get_key(self):
        """
        Get Thingstream key.
        """
        cmd = self.api['thingstream'][0]['keyGet']
        return cmd
    
    def api_cmd_thingstream_delete_key(self):
        """
        Delete Thingstream key.
        """
        cmd = self.api['thingstream'][0]['keyDelete']
        return cmd
    
    def api_cmd_thingstream_set_root_ca(self, root_ca: str):
        """
        Set Thingstream root CA.
        Should not contain any \r\n characters
        """
        cmd = self.api['thingstream'][0]['rootCaSet']
        cmd += root_ca
        return cmd
    
    def api_cmd_thingstream_get_root_ca(self):
        """
        Get Thingstream root ca.
        """
        cmd = self.api['thingstream'][0]['rootCaGet']
        return cmd
    
    def api_cmd_thingstream_delete_root_ca(self):
        """
        Delete Thingstream root ca.
        """
        cmd = self.api['thingstream'][0]['rootCaDelete']
        return cmd
    
    def api_cmd_thingstream_set_region(self, region: str):
        """
        Set Thingstream region.
        region param: "eu", "us", "kr", "jp" or "au"
        """
        cmd = self.api['thingstream'][0]['regionSet']
        cmd += region
        return cmd
    
    def api_cmd_thingstream_get_region(self):
        """
        Get Thingstream region.
        """
        cmd = self.api['thingstream'][0]['regionGet']
        return cmd
    
    def api_cmd_thingstream_delete_region(self):
        """
        Delete Thingstream region.
        """
        cmd = self.api['thingstream'][0]['regionDelete']
        return cmd
    
    def api_cmd_thingstream_set_plan(self, plan: str):
        """
        Set Thingstream plan.
        plan param: "ip", "ip+lband" or "lband"
        """
        cmd = self.api['thingstream'][0]['planSet']
        cmd += plan
        return cmd
    
    def api_cmd_thingstream_get_plan(self):
        """
        Get Thingstream plan.
        """
        cmd = self.api['thingstream'][0]['planGet']
        return cmd
    
    def api_cmd_thingstream_delete_plan(self):
        """
        Delete Thingstream plan.
        """
        cmd = self.api['thingstream'][0]['planDelete']
        return cmd
    
    def api_cmd_ntrip_set_server(self, server: str, port: str):
        """
        Set NTRIP server address.
        """
        cmd = self.api['ntrip'][0]['serverSet']
        cmd += server
        cmd += ','
        cmd += port
        return cmd
    
    def api_cmd_ntrip_get_server(self):
        """
        Get NTRIP server address.
        """
        cmd = self.api['ntrip'][0]['serverGet']
        return cmd
    
    def api_cmd_ntrip_delete_server(self):
        """
        Delete NTRIP server address.
        """
        cmd = self.api['ntrip'][0]['serverDelete']
        return cmd
    
    def api_cmd_ntrip_set_user_agent(self, user_agent: str):
        """
        Set NTRIP server user agent.
        """
        cmd = self.api['ntrip'][0]['userAgentSet']
        cmd += user_agent
        return cmd
    
    def api_cmd_ntrip_get_user_agent(self):
        """
        Get NTRIP server user agent.
        """
        cmd = self.api['ntrip'][0]['userAgentGet']
        return cmd
    
    def api_cmd_ntrip_delete_user_agent(self):
        """
        Delete NTRIP server user agent.
        """
        cmd = self.api['ntrip'][0]['userAgentDelete']
        return cmd
    
    def api_cmd_ntrip_set_mount_point(self, mount_point: str):
        """
        Set NTRIP server mount point.
        """
        cmd = self.api['ntrip'][0]['mountPointSet']
        cmd += mount_point
        return cmd
    
    def api_cmd_ntrip_get_mount_point(self):
        """
        Get NTRIP server mount point.
        """
        cmd = self.api['ntrip'][0]['mountPointGet']
        return cmd
    
    def api_cmd_ntrip_delete_mount_point(self):
        """
        Delete NTRIP server mount point.
        """
        cmd = self.api['ntrip'][0]['mountPointDelete']
        return cmd
    
    def api_cmd_ntrip_set_credentials(self, username: str, password: str):
        """
        Set NTRIP server credentials.
        """
        cmd = self.api['ntrip'][0]['credsSet']
        cmd += username
        cmd += ','
        cmd += password
        return cmd
    
    def api_cmd_ntrip_get_credentials(self):
        """
        Get NTRIP server credentials.
        """
        cmd = self.api['ntrip'][0]['credsGet']
        return cmd
    
    def api_cmd_ntrip_delete_credentials(self):
        """
        Delete NTRIP server credentials.
        """
        cmd = self.api['ntrip'][0]['credsDelete']
        return cmd
    
    def api_cmd_ntrip_set_gga_relay(self, gga_relay: str):
        """
        Set NTRIP server GGA Relay option.
        """
        cmd = self.api['ntrip'][0]['ggaRelaySet']
        cmd += gga_relay
        return cmd
    
    def api_cmd_ntrip_get_gga_relay(self):
        """
        Get NTRIP server GGA Relay option.
        """
        cmd = self.api['ntrip'][0]['ggaRelayGet']
        return cmd
    
    def api_cmd_status_get_wifi(self):
        """
        Get Wi-FI status.
        """
        cmd = self.api['status'][0]['wifi']
        return cmd
    
    def api_cmd_status_get_cell(self):
        """
        Get Cell status.
        """
        cmd = self.api['status'][0]['cell']
        return cmd
    
    def api_cmd_status_get_thingstream(self):
        """
        Get Thingstream status.
        """
        cmd = self.api['status'][0]['thingstream']
        return cmd
    
    def api_cmd_status_get_ntrip(self):
        """
        Get NTRIP status.
        """
        cmd = self.api['status'][0]['ntrip']
        return cmd
    
    def api_cmd_status_get_gnss(self):
        """
        Get GNSS status.
        """
        cmd = self.api['status'][0]['gnss']
        return cmd
    
    def api_cmd_misc_set_dead_reckoning(self, enable: str):
        """
        Set GNSS dead reckoning option.
        """
        cmd = self.api['misc'][0]['gnssDrSet']
        cmd += enable
        
        return cmd
    
    def api_cmd_misc_get_dead_reckoning(self):
        """
        Get GNSS dead reckoning option.
        """
        cmd = self.api['misc'][0]['gnssDrGet']
        return cmd
    
    def api_cmd_misc_set_sd_log(self, enable: str):
        """
        Set SD log option.
        """
        cmd = self.api['misc'][0]['sdLogSet']
        cmd += enable
        
        return cmd
    
    def api_cmd_misc_get_sd_log(self):
        """
        Get SD log option.
        """
        cmd = self.api['misc'][0]['sdLogGet']
        return cmd
    
    def api_cmd_misc_set_interface(self, interface: str):
        """
        Set communication interface. 
        interface param: "wi-fi" or "cell".
        """
        cmd = self.api['misc'][0]['interfaceSet']
        if (interface == 'wi-fi' or interface == 'cell'):
            cmd += interface
            return cmd
        else:
            print('Error in interface param. Accepted values are "wi-fi" or "cell"')
            pass
    
    def api_cmd_misc_get_interface(self):
        """
        Get communication interface.
        """
        cmd = self.api['misc'][0]['interfaceGet']
        return cmd
    
    def api_cmd_misc_set_correction_data_source(self, correction_data_source: str):
        """
        Set correction data source. 
        correction_data_source param: "ts" or "ntrip".
        """
        cmd = self.api['misc'][0]['correctionSourceSet']
        if (correction_data_source == 'ts' or correction_data_source == 'ntrip'):
            cmd += correction_data_source
            return cmd
        else:
            print('Error in correction_data_source param. Accepted values are "ts" or "ntrip"')
            pass
    
    def api_cmd_misc_get_correction_data_source(self):
        """
        Get correction data source.
        """
        cmd = self.api['misc'][0]['correctionSourceGet']
        return cmd
    
    def api_cmd_misc_set_correction_data_module(self, correction_data_module: str):
        """
        Set correction data module. 
        correction_data_module param: "ip" or "lband".
        """
        cmd = self.api['misc'][0]['correctionModuleSet']
        if (correction_data_module == 'ip' or correction_data_module == 'lband'):
            cmd += correction_data_module
            return cmd
        else:
            print('Error in correction_data_module param. Accepted values are "ip" or "lband"')
            pass
    
    def api_cmd_misc_get_correction_data_module(self):
        """
        Get correction data module.
        """
        cmd = self.api['misc'][0]['correctionModuleGet']
        return cmd
    
    def api_cmd_misc_set_auto_start(self, auto_start: str):
        """
        Set auto start functionality. 
        auto_start param: "0" or "1".
        """
        cmd = self.api['misc'][0]['autoStartSet']
        cmd += auto_start
        
        return cmd
    
    def api_cmd_misc_get_auto_start(self):
        """
        Get auto start functionality.
        """
        cmd = self.api['misc'][0]['autoStartGet']
        return cmd
    
    def api_cmd_misc_set_nvs_config(self, mode: str):
        """
        Set device mode.
        mode param: "MANUAL", "AUTO", "SAVE"
        """
        cmd = self.api['misc'][0]['nvsConfigSet']
        if (mode == 'MANUAL' or mode == 'AUTO' or mode == 'SAVE'):
            cmd += mode
            return cmd
        else:
            print('Error in mode param. Accepted values are "MANUAL", "AUTO" or "SAVE')
            pass
    
    def api_cmd_misc_get_nvs_config(self):
        """
        Get device mode.
        """
        cmd = self.api['misc'][0]['nvsConfigGet']
        return cmd
    
    def api_cmd_misc_set_baudrate(self, baudrate: str):
        """
        Set uart baudrate.
        """
        cmd = self.api['misc'][0]['baudrateSet']
        cmd += baudrate
        
        return cmd
    
    def api_cmd_misc_get_baudrate(self):
        """
        Get uart baudrate.
        """
        cmd = self.api['misc'][0]['baudrateGet']
        return cmd
    
    def api_cmd_misc_set_device_mode(self, mode: str):
        """
        Set device mode.
        mode param: "config", "start", "stop"
        """
        cmd = self.api['misc'][0]['dvcModeSet']
        if (mode == 'config' or mode == 'start' or mode == 'stop'):
            cmd += mode
            return cmd
        else:
            print('Error in mode param. Accepted values are "config", "start" or "stop')
            pass
    
    def api_cmd_misc_get_device_mode(self):
        """
        Get device mode.
        """
        cmd = self.api['misc'][0]['dvcModeGet']
        return cmd
    
    def api_cmd_misc_get_device_info(self):
        """
        Get device info.
        """
        cmd = self.api['misc'][0]['boardInfo']
        return cmd
    
    def api_cmd_misc_restart_device(self):
        """
        Restart device.
        """
        cmd = self.api['misc'][0]['dvcRestart']
        return cmd
    
    def api_cmd_misc_check_device(self):
        """
        Check device.
        """
        cmd = self.api['misc'][0]['dvcCheck']
        return cmd
    
    def api_cmd_misc_reset_device(self):
        """
        Reset device to factory settings.
        """
        cmd = self.api['misc'][0]['factoryReset']
        return cmd
    
    def api_cmd_misc_get_location(self):
        """
        Get location data.
        """
        cmd = self.api['misc'][0]['location']
        return cmd
