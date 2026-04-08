/*
 * WS2812 bit-bang 翻车演示
 *
 * 故意不使用 RMT 硬件外设，用软件 bit-bang 方式驱动 WS2812，
 * 同时开启 WiFi AP 产生大量中断干扰，展示时序被破坏后的翻车效果。
 *
 * 正确做法请参考 03-rgb-led-color-cycle 示例（使用 RMT + led_strip 组件）。
 */

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_rom_gpio.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <lwip/inet.h>

static const char *TAG = "bitbang_demo";

// GPIO 48 — ESP32-S3 开发板上 WS2812 LED 的数据引脚
static constexpr gpio_num_t kLedGpio = GPIO_NUM_48;

// WS2812 时序参数（单位：微秒）
static constexpr uint32_t kT0h = 0;   // bit 0 高电平 ≈ 0.35μs
static constexpr uint32_t kT0l = 1;   // bit 0 低电平 ≈ 0.9μs
static constexpr uint32_t kT1h = 1;   // bit 1 高电平 ≈ 0.9μs
static constexpr uint32_t kT1l = 0;   // bit 1 低电平 ≈ 0.35μs

// 发送一个 bit 0：高电平 ~0.35μs + 低电平 ~0.9μs
static inline void IRAM_ATTR send_bit_0() {
  gpio_set_level(kLedGpio, 1);
  esp_rom_delay_us(kT0h);
  gpio_set_level(kLedGpio, 0);
  esp_rom_delay_us(kT0l);
}

// 发送一个 bit 1：高电平 ~0.9μs + 低电平 ~0.35μs
static inline void IRAM_ATTR send_bit_1() {
  gpio_set_level(kLedGpio, 1);
  esp_rom_delay_us(kT1h);
  gpio_set_level(kLedGpio, 0);
  esp_rom_delay_us(kT1l);
}

// 发送一个字节（高位先发）
static void IRAM_ATTR send_byte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    if (byte & (1 << i)) {
      send_bit_1();
    } else {
      send_bit_0();
    }
  }
}

// 发送 24-bit 颜色数据（GRB 顺序）
static void IRAM_ATTR send_color(uint8_t r, uint8_t g, uint8_t b) {
  send_byte(g);
  send_byte(r);
  send_byte(b);
}

// 发送 reset 信号（低电平 > 50μs）
static void send_reset() {
  gpio_set_level(kLedGpio, 0);
  esp_rom_delay_us(60);
}

// 初始化 WiFi AP（产生中断干扰）
static void wifi_init_softap() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

  char ssid[32];
  snprintf(ssid, sizeof(ssid), "ESP32_BITBANG_%02X%02X%02X",
           mac[3], mac[4], mac[5]);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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
  ESP_LOGI(TAG, "WiFi AP started — SSID: %s, IP: %s", ssid, ip_addr);
}

extern "C" void app_main() {
  ESP_LOGI(TAG, "=== WS2812 Bit-Bang Demo (DO NOT DO THIS) ===");
  ESP_LOGI(TAG, "This demo intentionally uses software bit-bang + WiFi");
  ESP_LOGI(TAG, "to show timing issues. Use RMT in production!");

  // 初始化网络协议栈和 WiFi
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(nvs_flash_init());
  esp_netif_create_default_wifi_ap();
  wifi_init_softap();

  // 配置 GPIO
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = 1ULL << kLedGpio;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  ESP_LOGI(TAG, "Sending RED (255,0,0) continuously via bit-bang...");
  ESP_LOGI(TAG, "Watch the LED — it should be solid red, but won't be!");

  // 持续发送红色（GRB: 0x00, 0xFF, 0x00）
  while (true) {
    send_color(255, 0, 0);  // Red
    send_reset();
    // 短暂延时让 WiFi 中断有机会介入
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
