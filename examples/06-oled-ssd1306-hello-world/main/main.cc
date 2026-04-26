/* ESP32-S3 SSD1306 OLED Hello World

    使用 I2C 接口驱动 0.91 寸蓝色 OLED 显示屏（SSD1306 芯片），
    在屏幕上显示 "Hello World" 文字。

    硬件连接：
    - SDA: GPIO41
    - SCL: GPIO42
    - VCC: 3V3
    - GND: GND

    屏幕规格：128x32 像素，I2C 地址 0x3C
*/

#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#include "font8x16.h"

static const char *TAG = "oled_hello";

// ========== 硬件引脚定义 ==========
static constexpr int kI2cPort    = 0;
static constexpr gpio_num_t kSdaGpio = GPIO_NUM_41;   // I2C 数据线
static constexpr gpio_num_t kSclGpio = GPIO_NUM_42;   // I2C 时钟线
static constexpr uint8_t kI2cAddr  = 0x3C;             // SSD1306 I2C 地址
static constexpr int kI2cClkHz     = 400 * 1000;       // 400 kHz

// ========== 屏幕参数 ==========
// 0.91 寸蓝色 OLED 标准分辨率：128x32
static constexpr int kHorRes = 128;
static constexpr int kVerRes = 32;

// 显存缓冲区：1bpp（每像素 1 bit），SSD1306 垂直页寻址格式
static uint8_t s_framebuffer[kHorRes * kVerRes / 8];

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== SSD1306 OLED Hello World ===");
    ESP_LOGI(TAG, "Screen: %dx%d, I2C addr: 0x%02X", kHorRes, kVerRes, kI2cAddr);

    // ---- Step 1: 创建 I2C 主机总线 ----
    ESP_LOGI(TAG, "Initializing I2C bus: SDA=%d, SCL=%d", kSdaGpio, kSclGpio);
    i2c_master_bus_handle_t i2c_bus = nullptr;
    i2c_master_bus_config_t bus_config = {
        .i2c_port        = static_cast<i2c_port_t>(kI2cPort),
        .sda_io_num      = kSdaGpio,
        .scl_io_num      = kSclGpio,
        .clk_source      = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags           = { .enable_internal_pullup = true },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    // ---- Step 2: 创建 LCD Panel IO 设备 ----
    ESP_LOGI(TAG, "Installing panel IO (addr=0x%02X)", kI2cAddr);
    esp_lcd_panel_io_handle_t io_handle = nullptr;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr            = kI2cAddr,
        .control_phase_bytes = 1,
        .dc_bit_offset       = 6,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .scl_speed_hz        = kI2cClkHz,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    // ---- Step 3: 安装 SSD1306 面板驱动 ----
    ESP_LOGI(TAG, "Installing SSD1306 panel driver (%dx%d)", kHorRes, kVerRes);
    esp_lcd_panel_handle_t panel_handle = nullptr;
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.bits_per_pixel = 1;
    panel_config.reset_gpio_num = GPIO_NUM_NC;  // 无硬件复位引脚

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {};
    ssd1306_config.height = kVerRes;
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    // ---- Step 4: 复位、初始化、开启显示 ----
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // ---- Step 4.5: 翻转显示（修正屏幕倒置问题）----
    // mirror_x + mirror_y 组合等效于 180 度旋转
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

    // ---- Step 5: 颜色反转 ----
    // 部分蓝色 OLED 模块需要反转显示才能正常呈现（取决于模块 PCB 设计）
    // invert_color(true): 显存 bit=0 发光, bit=1 熄灭 → 亮底暗字
    // invert_color(false): 显存 bit=1 发光, bit=0 熄灭 → 暗底亮字
    // 如果显示效果不理想，切换 true/false 即可
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    // ---- Step 6: 绘制 "Hello World" 并推送到屏幕 ----
    memset(s_framebuffer, 0, sizeof(s_framebuffer));

    const char *text = "Hello World";
    // 居中显示：X 起始位置 = (屏幕宽度 - 字符数*字符宽度) / 2
    // Y 位置 = (屏幕高度 - 字体高度) / 2 + 字体高度（基线）
    int text_len = strlen(text);
    int x = (kHorRes - text_len * 8) / 2;
    int y = (kVerRes - 16) / 2 + 16;

    font8x16_draw_string(s_framebuffer, kHorRes, kVerRes, text, x, y);

    ESP_LOGI(TAG, "Drawing '%s' at (%d, %d)", text, x, y);
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, kHorRes, kVerRes, s_framebuffer)
    );

    ESP_LOGI(TAG, "Done! Display should show 'Hello World'");
}
