#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// 解析度定義 (橫向模式)
#define ST7789_WIDTH  320
#define ST7789_HEIGHT 240

// 介面函式
esp_err_t st7789_init(void);
void st7789_set_window(int x0, int y0, int x1, int y1);
void st7789_write_pixels(const uint16_t *data, uint32_t count);
void st7789_set_backlight(bool on);

#endif