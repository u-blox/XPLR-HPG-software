# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# If XPLR_HPG_SW_PATH is not defined, use relative path as default value
if(NOT DEFINED $ENV{XPLR_HPG_SW_PATH})
    set(ENV{XPLR_HPG_SW_PATH} ${CMAKE_CURRENT_LIST_DIR})
endif()

message(STATUS "-----------Package Info---------")
message(STATUS " XPLR_HPG_SW_PATH is $ENV{XPLR_HPG_SW_PATH}")

# ============= Select project to build =============
# NOTE: Only one project must be active at a time!
#----------------------------------------------------

# shortrange examples
#set(ENV{XPLR_HPG_PROJECT} "hpg_base")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_http_ztp")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_ztp")
set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_captive_portal")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_ntrip_correction")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_sd_config_file")
#set(ENV{XPLR_HPG_PROJECT} "hpg_bluetooth")
#set(ENV{XPLR_HPG_PROJECT} "hpg_wifi_mqtt_correction_certs_sw_maps")
#set(ENV{XPLR_HPG_PROJECT} "hpg_at_app")
#set(ENV{XPLR_HPG_PROJECT} "hpg_kit_info")

# cellular examples
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_register")
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs")
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_ztp")
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_ntrip_correction")
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs_sw_maps")
#set(ENV{XPLR_HPG_PROJECT} "hpg_cell_mqtt_correction_certs_sd_autonomous")

# positioning examples
#set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_config")
#set(ENV{XPLR_HPG_PROJECT} "hpg_gnss_save_on_shutdown")

if(NOT DEFINED ENV{XPLR_HPG_PROJECT})
message(FATAL_ERROR " Only one (1) project must be selected.")
endif()

message(STATUS " XPLR_HPG_EXAMPLE_SELECTED: $ENV{XPLR_HPG_PROJECT}")

include($ENV{XPLR_HPG_SW_PATH}/project.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project($ENV{XPLR_HPG_PROJECT})
# ===================================================

message(STATUS "Building project: ${CMAKE_PROJECT_NAME}")
message(STATUS "-----------Package Info End---------")

