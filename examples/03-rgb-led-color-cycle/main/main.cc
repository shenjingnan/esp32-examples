#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>

static const char *TAG = "rgb_led";

// GPIO 48 — ESP32-S3 开发板上 WS2812 LED 的数据引脚
static constexpr gpio_num_t kLedGpio = GPIO_NUM_48;

struct Color {
  const char *name;
  uint8_t r, g, b;
};

// 红 → 黄 → 蓝 循环
static constexpr Color kColors[] = {
    {"Red", 255, 0, 0},
    {"Yellow", 255, 255, 0},
    {"Blue", 0, 0, 255},
};
static constexpr int kColorCount = sizeof(kColors) / sizeof(kColors[0]);

extern "C" void app_main() {
  ESP_LOGI(TAG, "Initializing WS2812 LED on GPIO %d", kLedGpio);

  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = kLedGpio;
  strip_config.max_leds = 1;
  strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
  strip_config.led_model = LED_MODEL_WS2812;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.resolution_hz = 10 * 1000 * 1000; // 10 MHz

  led_strip_handle_t led_strip = nullptr;
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  // 清屏（熄灭 LED）
  led_strip_clear(led_strip);

  ESP_LOGI(TAG, "Starting color cycle: red -> yellow -> blue (1s each)");

  int color_index = 0;
  while (true) {
    const Color &c = kColors[color_index];
    ESP_LOGI(TAG, "Color: %s (R=%d, G=%d, B=%d)", c.name, c.r, c.g, c.b);

    led_strip_set_pixel(led_strip, 0, c.r, c.g, c.b);
    led_strip_refresh(led_strip);

    vTaskDelay(pdMS_TO_TICKS(1000));
    color_index = (color_index + 1) % kColorCount;
  }
}
