/* HTTP/WebSocket 服务器

    融合 WiFi 配置 API 和色盘 WebSocket 控制接口。
    路由：
      GET  /        — 色盘页面
      GET  /setup   — WiFi 配置页面
      POST /connect — WiFi 连接
      GET  /scan    — WiFi 扫描
      GET  /status  — 连接状态 JSON
      WS   /ws      — WebSocket LED 控制
*/

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/param.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "led_strip.h"

#include "wifi_state.h"
#include "http_server.h"

static const char *TAG = "http_server";
static led_strip_handle_t s_led_strip = nullptr;

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[]   asm("_binary_index_html_end");
extern const char setup_start[] asm("_binary_setup_html_start");
extern const char setup_end[]   asm("_binary_setup_html_end");

// ==================== 工具函数 ====================

static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            dst[j++] = (char)strtol(hex, nullptr, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static bool get_form_value(const char *body, const char *key, char *value, size_t value_size)
{
    size_t key_len = strlen(key);
    const char *p = body;

    while ((p = strstr(p, key)) != nullptr) {
        if (p != body && *(p - 1) != '&') {
            p++;
            continue;
        }
        if (p[key_len] != '=') {
            p++;
            continue;
        }
        const char *val_start = p + key_len + 1;
        const char *val_end = strchr(val_start, '&');
        size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

        if (val_len >= value_size) {
            val_len = value_size - 1;
        }

        char raw[128] = {0};
        memcpy(raw, val_start, val_len);
        url_decode(value, raw, value_size);
        return true;
    }
    return false;
}

static int rssi_to_bars(int rssi)
{
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    if (rssi >= -80) return 1;
    return 0;
}

static bool is_encrypted(wifi_auth_mode_t auth_mode)
{
    return auth_mode != WIFI_AUTH_OPEN;
}

// ==================== HTTP 处理函数 ====================

// GET / — 返回色盘页面
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, index_end - index_start);
    return ESP_OK;
}

// GET /setup — 返回 WiFi 配置页面
static esp_err_t setup_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, setup_start, setup_end - setup_start);
    return ESP_OK;
}

// POST /connect — 接收 WiFi 凭据并发起连接
static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    char ssid[64] = {0};
    char password[64] = {0};

    bool has_ssid = get_form_value(buf, "ssid", ssid, sizeof(ssid));

    if (!has_ssid || strlen(ssid) == 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "SSID is required", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    get_form_value(buf, "password", password, sizeof(password));

    ESP_LOGI(TAG, "Connecting to WiFi SSID: '%s'", ssid);

    s_connect_status = CONNECT_STATUS_CONNECTING;
    s_connect_fail_reason[0] = '\0';
    s_sta_ip_addr[0] = '\0';
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT | FAILED_BIT);

    wifi_config_t sta_config = {};
    strncpy(reinterpret_cast<char *>(sta_config.sta.ssid), ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy(reinterpret_cast<char *>(sta_config.sta.password), password, sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = strlen(password) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    esp_wifi_connect();

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// GET /status — 返回当前连接状态 JSON
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json[256];
    const char *status_str;

    switch (s_connect_status) {
    case CONNECT_STATUS_IDLE:       status_str = "idle"; break;
    case CONNECT_STATUS_CONNECTING: status_str = "connecting"; break;
    case CONNECT_STATUS_CONNECTED:  status_str = "connected"; break;
    case CONNECT_STATUS_FAILED:     status_str = "failed"; break;
    default:                        status_str = "idle"; break;
    }

    int len = snprintf(json, sizeof(json),
        "{\"status\":\"%s\",\"ip\":\"%s\",\"reason\":\"%s\"}",
        status_str, s_sta_ip_addr, s_connect_fail_reason);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);

    return ESP_OK;
}

// GET /scan — 扫描周围 WiFi 并返回 JSON 列表
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {};
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.max = 120;

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(ret));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"scan_failed\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (ap_count > 20) {
        ap_count = 20;
    }

    wifi_ap_record_t ap_records[20];
    uint16_t actual_count = ap_count;
    esp_wifi_scan_get_ap_records(&actual_count, ap_records);

    char *json_buf = (char *)malloc(3072);
    if (!json_buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"no_memory\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    int pos = 0;
    json_buf[pos++] = '[';

    for (int i = 0; i < actual_count; i++) {
        if (i > 0) {
            json_buf[pos++] = ',';
        }

        char escaped_ssid[128];
        const char *src = (const char *)ap_records[i].ssid;
        int epos = 0;
        for (int j = 0; src[j] && epos < (int)sizeof(escaped_ssid) - 2; j++) {
            if (src[j] == '"' || src[j] == '\\') {
                if (epos < (int)sizeof(escaped_ssid) - 2) {
                    escaped_ssid[epos++] = '\\';
                }
            }
            if (src[j] >= 0x20) {
                escaped_ssid[epos++] = src[j];
            }
        }
        escaped_ssid[epos] = '\0';

        pos += snprintf(json_buf + pos, 3072 - pos,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"bars\":%d,\"encrypted\":%s,\"channel\":%d}",
            escaped_ssid,
            ap_records[i].rssi,
            rssi_to_bars(ap_records[i].rssi),
            is_encrypted(ap_records[i].authmode) ? "true" : "false",
            ap_records[i].primary);
    }

    json_buf[pos++] = ']';
    json_buf[pos] = '\0';

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, json_buf, pos);

    free(json_buf);
    return ESP_OK;
}

// WebSocket /ws — 接收颜色数据并设置 LED
static esp_err_t ws_handler(httpd_req_t *req)
{
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

// ==================== 路由注册 ====================

httpd_handle_t start_webserver(led_strip_handle_t led_strip)
{
    s_led_strip = led_strip;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // 色盘页面
        httpd_uri_t root_uri = {};
        root_uri.uri = "/";
        root_uri.method = HTTP_GET;
        root_uri.handler = root_get_handler;
        httpd_register_uri_handler(server, &root_uri);

        // WiFi 配置页面
        httpd_uri_t setup_uri = {};
        setup_uri.uri = "/setup";
        setup_uri.method = HTTP_GET;
        setup_uri.handler = setup_get_handler;
        httpd_register_uri_handler(server, &setup_uri);

        // WiFi 连接
        httpd_uri_t connect_uri = {};
        connect_uri.uri = "/connect";
        connect_uri.method = HTTP_POST;
        connect_uri.handler = connect_post_handler;
        httpd_register_uri_handler(server, &connect_uri);

        // WiFi 状态
        httpd_uri_t status_uri = {};
        status_uri.uri = "/status";
        status_uri.method = HTTP_GET;
        status_uri.handler = status_get_handler;
        httpd_register_uri_handler(server, &status_uri);

        // WiFi 扫描
        httpd_uri_t scan_uri = {};
        scan_uri.uri = "/scan";
        scan_uri.method = HTTP_GET;
        scan_uri.handler = scan_get_handler;
        httpd_register_uri_handler(server, &scan_uri);

        // WebSocket LED 控制
        httpd_uri_t ws_uri = {};
        ws_uri.uri = "/ws";
        ws_uri.method = HTTP_GET;
        ws_uri.handler = ws_handler;
        ws_uri.is_websocket = true;
        httpd_register_uri_handler(server, &ws_uri);

        // 404 重定向
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        ESP_LOGI(TAG, "HTTP server started with WebSocket support");
    }

    return server;
}
