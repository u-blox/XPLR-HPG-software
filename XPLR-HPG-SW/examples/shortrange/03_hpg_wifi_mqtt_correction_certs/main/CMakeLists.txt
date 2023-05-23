idf_component_register(SRCS "hpg_wifi_mqtt_correction_certs.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES root.crt client.crt client.key)
message(STATUS "-----------Project Info---------")
message(STATUS " Building Example: HPG_WIFI_IP_MQTT_CORRECTION_CERTS")
message(STATUS "-----------Project Info End-----")