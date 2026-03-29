/* ESP32 WiFi 配置示例 - 强制门户配网

    用户通过手机连接 ESP32 热点后，弹出表单填写 WiFi 名称和密码，
    ESP32 尝试连接目标 WiFi，成功则倒计时 3 秒，失败则显示失败原因。

    此示例代码属于公共领域（或根据您的选择采用 CC0 许可。）
*/

#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"
#include "ntp_time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");

static const char *TAG = "esp32_wifi_config";

// WiFi 连接状态枚举
typedef enum {
    CONNECT_STATUS_IDLE = 0,
    CONNECT_STATUS_CONNECTING,
    CONNECT_STATUS_CONNECTED,
    CONNECT_STATUS_FAILED
} wifi_connect_status_t;

// 全局连接状态
static volatile wifi_connect_status_t s_connect_status = CONNECT_STATUS_IDLE;
static char s_connect_fail_reason[64] = {0};
static char s_sta_ip_addr[16] = {0};

// 连接成功/失败的事件组标志
static EventGroupHandle_t s_wifi_event_group;
static const int CONNECTED_BIT = BIT0;
static const int FAILED_BIT = BIT1;

// ==================== 工具函数 ====================

// URL 解码：将 %XX 转为字符，+ 转为空格
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

// 从 application/x-www-form-urlencoded 数据中提取指定 key 的值
static bool get_form_value(const char *body, const char *key, char *value, size_t value_size)
{
    size_t key_len = strlen(key);
    const char *p = body;

    while ((p = strstr(p, key)) != nullptr) {
        // 确保 key 前面是 & 或位于开头
        if (p != body && *(p - 1) != '&') {
            p++;
            continue;
        }
        // 检查 key 后面是否紧跟 '='
        if (p[key_len] != '=') {
            p++;
            continue;
        }
        const char *val_start = p + key_len + 1;
        const char *val_end = strchr(val_start, '&');
        size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

        // 限制长度
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

// 将 WiFi 断开原因码转为中文提示
static const char *get_disconnect_reason(uint8_t reason)
{
    switch (reason) {
    case 2:  return "认证无效";
    case 15: return "密码错误或认证失败";
    case 201: return "找不到指定的 WiFi";
    case 202: return "连接失败，认证不匹配";
    case 204: return "握手超时";
    default: return "连接失败";
    }
}

// ==================== 事件处理 ====================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto *event = static_cast<wifi_event_ap_staconnected_t *>(event_data);
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto *event = static_cast<wifi_event_ap_stadisconnected_t *>(event_data);
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } else if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA started, connecting...");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
        ESP_LOGI(TAG, "STA disconnected, reason=%d", event->reason);
        s_connect_status = CONNECT_STATUS_FAILED;
        snprintf(s_connect_fail_reason, sizeof(s_connect_fail_reason),
                 "%s (reason=%d)", get_disconnect_reason(event->reason), event->reason);
        xEventGroupSetBits(s_wifi_event_group, FAILED_BIT);
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        inet_ntoa_r(event->ip_info.ip.addr, s_sta_ip_addr, sizeof(s_sta_ip_addr));
        ESP_LOGI(TAG, "STA got ip: %s", s_sta_ip_addr);
        s_connect_status = CONNECT_STATUS_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        // WiFi 连接成功，启动 NTP 时间同步
        ntp_time_init();
    }
}

// ==================== WiFi 初始化 ====================

static void wifi_init_softap(void)
{
    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();

    // 在 WiFi 初始化前获取 MAC 地址
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    // 生成 SSID: ESP32_WIFI_XXXXXX（MAC 地址后3字节）
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32_WIFI_%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, nullptr));

    wifi_config_t wifi_config = {};
    strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), ssid);
    wifi_config.ap.ssid_len = strlen(ssid);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' (open, no password)", ssid);
}

// ==================== HTTP 处理函数 ====================

// GET / - 返回配置页面
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = index_end - index_start;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, root_len);

    return ESP_OK;
}

// POST /connect - 接收 WiFi 凭据并发起连接
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

    // 重置状态
    s_connect_status = CONNECT_STATUS_CONNECTING;
    s_connect_fail_reason[0] = '\0';
    s_sta_ip_addr[0] = '\0';
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT | FAILED_BIT);

    // 切换到 APSTA 模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

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

// GET /status - 返回当前连接状态 JSON
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

// HTTP 错误（404）处理函数 - 将所有请求重定向到根页面
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

// ==================== 路由注册 ====================

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

static const httpd_uri_t connect_uri = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = nullptr;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &connect_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

// ==================== 主函数 ====================

extern "C" void app_main(void)
{
    // 关闭 HTTP 服务器的警告日志
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // 初始化网络协议栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建主应用所需的默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化 WiFi 所需的 NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 创建默认 AP 和 STA netif
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    // 将 ESP32 初始化为 SoftAP 模式
    wifi_init_softap();

    // 启动 Web 服务器
    start_webserver();

    // 启动 DNS 服务器，将所有查询重定向到 softAP IP
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&config);
}
