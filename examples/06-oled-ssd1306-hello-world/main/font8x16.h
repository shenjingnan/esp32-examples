#pragma once
#include <cstdint>

/**
 * 在 1bpp 显存缓冲区中绘制字符串（8x16 字体）
 *
 * @param fb       显存指针（SSD1306 垂直页寻址格式，大小 hor_res * ver_res / 8 字节）
 * @param hor_res  水平分辨率（像素）
 * @param ver_res  垂直分辨率（像素）
 * @param str      要绘制的 ASCII 字符串
 * @param x        起始 X 坐标（像素，左上角为原点）
 * @param y        起始 Y 坐标（像素，文本基线位置）
 */
void font8x16_draw_string(uint8_t *fb, int hor_res, int ver_res,
                          const char *str, int x, int y);
