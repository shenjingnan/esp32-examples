/* ESP32 WiFi mDNS 色盘控制 LED

    ESP32-S3 开启 WiFi APSTA 模式，首次通过 Captive Portal 配置 WiFi 凭据，
    设备连接路由器后通过 mDNS 广播域名 esp32-color.local。
    同一局域网内的设备直接通过 http://esp32-color.local/ 访问色盘控制 LED。
*/

#include <driver/gpio.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "led_strip.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "esp_timer.h"

#include "dns_server.h"
#include "wifi_state.h"
#include "http_server.h"

static const char *TAG = "wifi_mdns_color";

// GPIO 48 — ESP32-S3 开发板上 WS2812 LED 的数据引脚
static constexpr gpio_num_t kLedGpio = GPIO_NUM_48;

// 全局连接状态（在 wifi_state.h 中 extern 声明）
volatile wifi_connect_status_t s_connect_status = CONNECT_STATUS_IDLE;
char s_connect_fail_reason[64] = {0};
char s_sta_ip_addr[16] = {0};
EventGroupHandle_t s_wifi_event_group;
static bool s_mdns_initialized = false;  // 防止 mDNS 重复初始化

// ==================== 工具函数 ====================

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

// ==================== mDNS 初始化 ====================

static void init_mdns(void)
{
    // 在 STA 获得 IP 后再初始化 mDNS（从 IP_EVENT_STA_GOT_IP 回调中调用），
    // 确保网络接口已就绪。使用 socket 网络模式（CONFIG_MDNS_NETWORKING_SOCKET=y）
    // 避免 lwIP PCB 模式在 APSTA 场景下的接口状态同步问题。
    ESP_ERROR_CHECK(mdns_init());

    ESP_ERROR_CHECK(mdns_hostname_set("esp32-color"));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Color Picker"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI(TAG, "mDNS initialized: http://esp32-color.local/");
}

// STA 获得 IP 后延迟触发 mDNS announce，确保 probe 过程完成
static void mdns_announce_sta(void *arg)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        ESP_ERROR_CHECK(mdns_netif_action(sta_netif, MDNS_EVENT_ANNOUNCE_IP4));
        ESP_LOGI(TAG, "mDNS announced on STA interface");
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
        ESP_LOGI(TAG, "STA started");
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

        // 在 STA 获得 IP 后再初始化 mDNS，确保接口已就绪
        if (!s_mdns_initialized) {
            s_mdns_initialized = true;
            init_mdns();
        }
    }
}

// ==================== WiFi 初始化 ====================

static void wifi_init_apsta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32_MCOLOR_%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, nullptr));

    wifi_config_t ap_config = {};
    strcpy(reinterpret_cast<char *>(ap_config.ap.ssid), ssid);
    ap_config.ap.ssid_len = strlen(ssid);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "AP started: SSID='%s' IP=%s", ssid, ip_addr);

    // 自动重连：检查 NVS 中是否有已保存的 STA 配置
    wifi_config_t existing_config = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &existing_config) == ESP_OK) {
        if (strlen(reinterpret_cast<char *>(existing_config.sta.ssid)) > 0) {
            s_connect_status = CONNECT_STATUS_CONNECTING;
            ESP_LOGI(TAG, "Found saved WiFi: '%s', auto-connecting...",
                     reinterpret_cast<char *>(existing_config.sta.ssid));
            esp_wifi_connect();
        }
    }
}

// ==================== LED 初始化 ====================

static led_strip_handle_t init_led(void)
{
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = kLedGpio;
    strip_config.max_leds = 1;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;  // 10 MHz

    led_strip_handle_t led_strip = nullptr;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    return led_strip;
}

// ==================== 主函数 ====================

extern "C" void app_main(void)
{
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_apsta();

    // mDNS 已移至 IP_EVENT_STA_GOT_IP 回调中初始化（STA 获得 IP 后再初始化，
    // 确保网络接口已就绪，避免 lwIP 模式下的接口状态同步问题）

    led_strip_handle_t led_strip = init_led();
    ESP_LOGI(TAG, "WS2812 LED initialized on GPIO %d", kLedGpio);

    start_webserver(led_strip);

    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    start_dns_server(&dns_config);
}
