# 03 - RGB LED 颜色循环

使用 ESP32-S3 开发板上 GPIO 48 连接的 WS2812 RGB LED，实现红、黄、蓝三色循环切换，每秒换一种颜色。

## 硬件要求

- ESP32-S3 开发板（板载 WS2812 LED，连接在 GPIO 48）

## 工作原理

1. 通过 RMT 外设驱动 WS2812 LED
2. 使用 `espressif/led_strip` 组件（v3 API）
3. 主循环依次设置红(255,0,0) → 黄(255,255,0) → 蓝(0,0,255)，每秒切换一次

## 编译与烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash
```

## 预期效果

LED 按以下顺序循环：
- 红色亮 1 秒
- 黄色亮 1 秒
- 蓝色亮 1 秒
- 回到红色，循环往复
