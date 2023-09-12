message(STATUS "-----------Project Info---------")
message(STATUS " Building Component: xplr_mqtt")
idf_component_register(SRCS "xplr_mqtt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mqtt fatfs)
message(STATUS " xplr_mqtt Component: build finished")
message(STATUS "-----------Project Info End-----")
