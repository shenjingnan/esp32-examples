#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

httpd_handle_t start_webserver(led_strip_handle_t led_strip);

#ifdef __cplusplus
}
#endif

#endif
