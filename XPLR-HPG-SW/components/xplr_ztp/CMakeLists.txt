message(STATUS "-----------Project Info---------")
message(STATUS " Building Component: xplr_ztp")
        idf_component_register(SRCS "xplr_ztp.c"
            INCLUDE_DIRS "include"
            REQUIRES esp_http_client mbedtls log esp_ringbuf)
message(STATUS " xplr_ztp: build finished")
message(STATUS "-----------Project Info End-----")