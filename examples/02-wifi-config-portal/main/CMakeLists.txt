idf_component_register(SRCS main.cc ntp_time.c
                       PRIV_REQUIRES esp_event nvs_flash esp_wifi esp_http_server dns_server lwip
                       EMBED_FILES index.html)