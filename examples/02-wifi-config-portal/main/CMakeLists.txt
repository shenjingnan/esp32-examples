idf_component_register(SRCS main.cc
                       PRIV_REQUIRES esp_event nvs_flash esp_wifi esp_http_server dns_server ntp_time lwip
                       EMBED_FILES index.html)