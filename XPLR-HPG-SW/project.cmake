add_compile_options(-fdiagnostics-color=always)
# make U_CFG_ENABLE_LOGGING=1 to enable ubxlib logging to console
add_compile_definitions(U_CFG_ENABLE_LOGGING=0)

if(DEFINED ENV{XPLR_HPG_PROJECT})
    message(STATUS "Loading ${CMAKE_PROJECT_NAME} source files...")
    if($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_base")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/shortrange/01_hpg_base")
        message(STATUS "Loaded from: examples/shortrange/01_hpg_base")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_wifi_http_ztp")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_wifi_starter"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp_json_parser"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/shortrange/02_hpg_wifi_http_ztp")
        message(STATUS "Loaded from: examples/shortrange/02_hpg_wifi_http_ztp")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_wifi_mqtt_correction_certs")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_mqtt"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_wifi_starter"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/shortrange/03_hpg_wifi_mqtt_correction_certs")
        message(STATUS "Loaded from: examples/shortrange/03_hpg_wifi_mqtt_correction_certs")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_wifi_mqtt_correction_ztp")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_mqtt"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_wifi_starter"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp_json_parser"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/shortrange/04_hpg_wifi_mqtt_correction_ztp")
        message(STATUS "Loaded from: examples/shortrange/04_hpg_wifi_mqtt_correction_ztp")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_wifi_mqtt_correction_captive_portal")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_mqtt"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_wifi_starter"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_ztp_json_parser"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal")
        message(STATUS " Adding webserver DNS to project.")
        add_compile_definitions(XPLR_CFG_ENABLE_WEBSERVERDNS=1)
        message(STATUS "Loaded from: examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_gnss_config")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/positioning/01_hpg_gnss_config")
        message(STATUS "Loaded from: examples/positioning/01_hpg_gnss_config")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_gnss_lband_correction")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_mqtt"
            "$ENV{XPLR_HPG_SW_PATH}/components/xplr_wifi_starter"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/positioning/02_hpg_gnss_lband_correction")
        message(STATUS "Loaded from: examples/positioning/02_hpg_gnss_lband_correction")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_cell_register")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/cellular/01_hpg_cell_register")
        message(STATUS "Loaded from: examples/cellular/01_hpg_cell_register")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_cell_mqtt_correction_certs")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/cellular/02_hpg_cell_mqtt_correction_certs")
        message(STATUS "Loaded from: examples/cellular/02_hpg_cell_mqtt_correction_certs")
    elseif($ENV{XPLR_HPG_PROJECT} STREQUAL "hpg_cell_mqtt_correction_ztp")
        list(APPEND EXTRA_COMPONENT_DIRS
            "$ENV{XPLR_HPG_SW_PATH}/components/boards"
            "$ENV{XPLR_HPG_SW_PATH}/components/hpglib"
            "$ENV{XPLR_HPG_SW_PATH}/components/ubxlib/port/platform/esp-idf/mcu/esp32/components/ubxlib"
            )
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{XPLR_HPG_SW_PATH}/examples/cellular/03_hpg_cell_mqtt_correction_ztp")
        message(STATUS "Loaded from: examples/cellular/03_hpg_cell_mqtt_correction_ztp")
    else()
        message(FATAL_ERROR "Source files folder not found!")
    endif()
else()
    message(FATAL_ERROR "No project defined in CMakeLists.txt")
endif()