/* ESP32 WiFi 示例 - 强制门户

    此示例代码属于公共领域（或根据您的选择采用 CC0 许可。）

    除非适用法律要求或书面同意，否则本软件按"原样"分发，
    不附带任何明示或暗示的担保或条件。
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

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");

static const char *TAG = "esp32_wifi_demo";

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
    }
}

static void wifi_init_softap(void)
{
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

    wifi_config_t wifi_config = {};
    strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), ssid);
    wifi_config.ap.ssid_len = strlen(ssid);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;  // 开放 WiFi，无密码

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

// HTTP GET 处理函数
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = index_end - index_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, root_len);

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

// HTTP 错误（404）处理函数 - 将所有请求重定向到根页面
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // 设置状态码
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // 重定向到 "/" 根目录
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS 需要响应中包含内容才能检测强制门户，仅重定向是不够的。
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = nullptr;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // 启动 HTTP 服务器
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册 URI 处理函数
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

extern "C" void app_main(void)
{
    /*
        关闭 HTTP 服务器的警告日志，因为重定向流量会产生大量无效请求
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // 初始化网络协议栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建主应用所需的默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化 WiFi 所需的 NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 使用默认配置初始化 WiFi（包括 netif）
    esp_netif_create_default_wifi_ap();

    // 将 ESP32 初始化为 SoftAP 模式
    wifi_init_softap();

    // 首次启动 Web 服务器
    start_webserver();

    // 启动 DNS 服务器，将所有查询重定向到 softAP IP
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&config);
}