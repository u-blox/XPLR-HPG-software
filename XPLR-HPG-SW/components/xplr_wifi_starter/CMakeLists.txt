message(STATUS "-----------Project Info---------")
message(STATUS " Building Component: xplr_wifi_starter")
idf_component_register(SRCS "xplr_wifi_webserver.c" "xplr_wifi_starter.c" "xplr_wifi_dns.c" "xplr_wifi_webserver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server vfs newlib json fatfs heap
                       EMBED_FILES "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/templates/index.html"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/templates/settings.html"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/templates/tracker.html"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/templates/error.html"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/css/xplrHpg.css"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/js/xplrHpg.js"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/img/favicon.ico"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/img/ublox_logo.svg"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/css/bootstrap.min.css"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/css/bootstrap.min.css.map"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/js/bootstrap.bundle.min.js"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/js/bootstrap.bundle.min.js.map"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/js/fontawesome.min.js"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/css/fontawesome.min.css"
                       "../../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/portal/static/js/jquery.min.js")
message(STATUS " xplr_wifi_starter: build finished")
message(STATUS "-----------Project Info End-----")
