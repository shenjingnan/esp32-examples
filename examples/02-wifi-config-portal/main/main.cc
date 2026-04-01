/* ESP32 WiFi 配置示例 - 强制门户配网

    用户通过手机连接 ESP32 热点后，弹出表单填写 WiFi 名称和密码，
    ESP32 尝试连接目标 WiFi，成功则倒计时 3 秒，失败则显示失败原因。

    此示例代码属于公共领域（或根据您的选择采用 CC0 许可。）
*/

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "dns_server.h"
#include "ntp_time.h"

#include "wifi_state.h"
#include "http_server.h"

static const char *TAG = "esp32_wifi_config";

// 全局连接状态（在 wifi_state.h 中 extern 声明）
volatile wifi_connect_status_t s_connect_status = CONNECT_STATUS_IDLE;
char s_connect_fail_reason[64] = {0};
char s_sta_ip_addr[16] = {0};
EventGroupHandle_t s_wifi_event_group;

// ==================== 工具函数 ====================

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' (open, no password)", ssid);
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
