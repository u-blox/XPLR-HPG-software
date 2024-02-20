set(boards_dir "")

if(CONFIG_BOARD_XPLR_HPG2_C214)
    set(boards_dir "xplr-hpg2-c214")
endif()

if(CONFIG_BOARD_XPLR_HPG1_C213)
    set(boards_dir "xplr-hpg1-c213")
endif()

if(CONFIG_BOARD_MAZGCH_HPG_SOLUTION)
    set(boards_dir "mazgch-hpg-solution")
endif()

message(STATUS " -----------Board Info---------")
message(STATUS " IDF_TARGET = ${IDF_TARGET}")
message(STATUS " Board DIR = ${boards_dir}")
if(CONFIG_GNSS_MODULE_ZED_F9R)
    message(STATUS " GNSS Module = ZED-F9R")
elseif(CONFIG_GNSS_MODULE_ZED_F9P)
    message(STATUS " GNSS Module = ZED-F9P")
else()
    message(STATUS " GNSS Module = None")
endif()
message(STATUS " ---------Board Info End---------")


idf_component_register( SRC_DIRS "${boards_dir}"
                        INCLUDE_DIRS "${boards_dir}"
                        )