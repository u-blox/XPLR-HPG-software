idf_component_register(SRCS "hpg_wifi_mqtt_correction_certs_sw_maps.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES root.crt client.crt client.key)
message(STATUS "-----------Project Info---------")
message(STATUS " Building Example: HPG_WIFI_IP_MQTT_CORRECTION_CERTS_SW_MAPS")
message(STATUS "-----------Project Info End-----")