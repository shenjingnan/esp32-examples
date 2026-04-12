idf_component_register(SRCS main.cc http_server.cc
                       PRIV_REQUIRES esp_event nvs_flash esp_wifi esp_http_server dns_server esp_driver_gpio lwip mdns esp_timer
                       EMBED_FILES index.html setup.html)
