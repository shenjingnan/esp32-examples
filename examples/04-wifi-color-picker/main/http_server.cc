/* HTTP/WebSocket 服务器

    提供 HTTP 静态页面服务和 WebSocket 颜色控制接口。
    前端通过 WebSocket 发送 "r,g,b" 格式的颜色数据，
    服务端解析后设置 WS2812 LED 颜色。
*/

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_http_server.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "http_server";
static led_strip_handle_t s_led_strip = nullptr;

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");

// HTTP GET "/" — 返回色盘页面
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, index_end - index_start);
    return ESP_OK;
}

// WebSocket "/ws" — 接收颜色数据并设置 LED
static esp_err_t ws_handler(httpd_req_t *req)
{
    // WebSocket 握手阶段（HTTP GET），直接返回
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    uint8_t buf[16] = {0};
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.payload = buf;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket recv failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len > 0) {
        int r = 0, g = 0, b = 0;
        if (sscanf(reinterpret_cast<char *>(buf), "%d,%d,%d", &r, &g, &b) == 3) {
            r = std::max(0, std::min(255, r));
            g = std::max(0, std::min(255, g));
            b = std::max(0, std::min(255, b));
            led_strip_set_pixel(s_led_strip, 0, r, g, b);
            led_strip_refresh(s_led_strip);
        }
    }

    return ESP_OK;
}

// HTTP 404 — 重定向到根页面（支持 Captive Portal）
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t start_webserver(led_strip_handle_t led_strip)
{
    s_led_strip = led_strip;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册根路径 GET 处理函数
        httpd_uri_t root_uri = {};
        root_uri.uri = "/";
        root_uri.method = HTTP_GET;
        root_uri.handler = root_get_handler;
        httpd_register_uri_handler(server, &root_uri);

        // 注册 WebSocket 处理函数
        httpd_uri_t ws_uri = {};
        ws_uri.uri = "/ws";
        ws_uri.method = HTTP_GET;
        ws_uri.handler = ws_handler;
        ws_uri.is_websocket = true;
        httpd_register_uri_handler(server, &ws_uri);

        // 注册 404 重定向处理
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        ESP_LOGI(TAG, "HTTP server started with WebSocket support");
    }

    return server;
}
