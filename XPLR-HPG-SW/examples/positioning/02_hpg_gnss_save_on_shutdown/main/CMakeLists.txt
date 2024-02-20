idf_component_register(SRCS "hpg_gnss_save_on_shutdown.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES root.crt client.crt client.key)
message(STATUS "-----------Project Info---------")
message(STATUS " Building Example: HPG GNSS SAVE ON SHUTDOWN")
message(STATUS "-----------Project Info End-----")