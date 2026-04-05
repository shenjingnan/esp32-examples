/* ESP32 WiFi 色盘控制 LED

    ESP32-S3 开启 WiFi AP 热点，手机连接后通过浏览器色盘选择颜色，
    通过 WebSocket 实时同步到板载 WS2812 LED。
*/

#include <driver/gpio.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "lwip/inet.h"

#include "dns_server.h"

static const char *TAG = "wifi_color_picker";

// GPIO 48 — ESP32-S3 开发板上 WS2812 LED 的数据引脚
static constexpr gpio_num_t kLedGpio = GPIO_NUM_48;

// 前向声明（定义在 http_server.cc）
extern httpd_handle_t start_webserver(led_strip_handle_t led_strip);

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
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    // 生成 SSID: ESP32_COLOR_XXXXXX（MAC 地址后3字节）
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32_COLOR_%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, nullptr));

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
    ESP_LOGI(TAG, "AP started: SSID='%s' IP=%s", ssid, ip_addr);
}

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

extern "C" void app_main(void)
{
    // 关闭 HTTP 服务器的警告日志（重定向流量会产生大量无效请求）
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_netif_create_default_wifi_ap();
    wifi_init_softap();

    led_strip_handle_t led_strip = init_led();
    ESP_LOGI(TAG, "WS2812 LED initialized on GPIO %d", kLedGpio);

    start_webserver(led_strip);

    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    start_dns_server(&dns_config);
}
