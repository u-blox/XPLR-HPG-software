#
# Component Makefile
#

ifdef CONFIG_BOARD_XPLR_HPG2_C214
    COMPONENT_SRCDIRS := xplr-hpg2-c214
endif

ifdef CONFIG_BOARD_XPLR_HPG1_C213
    COMPONENT_SRCDIRS := xplr-hpg1-c213
endif

ifdef CONFIG_BOARD_MAZGCH_HPG_SOLUTION
    COMPONENT_SRCDIRS := mazgch-hpg-solution
endif

COMPONENT_ADD_INCLUDEDIRS := $(COMPONENT_SRCDIRS)
