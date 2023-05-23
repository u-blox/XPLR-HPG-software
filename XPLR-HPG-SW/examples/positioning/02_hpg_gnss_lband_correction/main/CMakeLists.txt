idf_component_register(SRCS "hpg_gnss_lband_correction.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES root.crt client.crt client.key)
message(STATUS "-----------Project Info---------")
message(STATUS " Building Example: HPG GNSS-LBAND correction")
message(STATUS "-----------Project Info End-----")