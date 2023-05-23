message(STATUS "-----------Project Info---------")
message(STATUS " Building Component: xplr_ztp_json_parser")
idf_component_register(SRCS "xplr_ztp_json_parser.c"
                       INCLUDE_DIRS "include"
                       REQUIRES json log)
message(STATUS " xplr_ztp_json_parser: build finished")
message(STATUS "-----------Project Info End-----")