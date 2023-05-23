message(STATUS "-----------Project Info---------")
message(STATUS " Building Component: hpglib")

if(IDF_VERSION_MAJOR GREATER_EQUAL 4)
    idf_component_register(SRC_DIRS
                            "src/common"
                            "src/nvs_service"
                            "src/com_service"
                            "src/mqttClient_service"
                            "src/httpClient_service"
                            "src/thingstream_service"
                            "src/location_service/location_service_helpers"
                            "src/location_service/gnss_service"
                            "src/location_service/lband_service"
        REQUIRES ubxlib json nvs_flash
        INCLUDE_DIRS
                    "src/common"
                    "src/nvs_service"
                    "src/com_service"
                    "src/mqttClient_service"
                    "src/httpClient_service"
                    "src/thingstream_service"
                    "src/location_service/location_service_helpers"
                    "src/location_service/gnss_service"
                    "src/location_service/lband_service"
                    )
else()
    set(COMPONENT_SRCDIRS src/common)
    set(COMPONENT_ADD_INCLUDEDIRS src/common)
    set(COMPONENT_SRCDIRS src/nvs_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/nvs_service)
    set(COMPONENT_SRCDIRS src/com_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/com_service)
    set(COMPONENT_SRCDIRS src/mqttClient_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/mqttClient_service)
    set(COMPONENT_SRCDIRS src/httpClient_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/httpClient_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/thingstream_service)
    set(COMPONENT_SRCDIRS src/location_service/gnss_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/location_service/gnss_service)
    set(COMPONENT_SRCDIRS src/location_service/lband_service)
    set(COMPONENT_ADD_INCLUDEDIRS src/location_service/lband_service)
    set(COMPONENT_SRCDIRS src/location_service/location_service_helpers)
    set(COMPONENT_ADD_INCLUDEDIRS src/location_service/location_service_helpers)
    set(COMPONENT_REQUIRES ubxlib)
    set(COMPONENT_REQUIRES json)
    register_component()
endif()

message(STATUS "-----------Project Info End-----")