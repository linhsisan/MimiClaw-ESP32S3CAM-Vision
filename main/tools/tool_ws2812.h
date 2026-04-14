#ifndef TOOL_WS2812_H
#define TOOL_WS2812_H

#include "esp_err.h"
#include <stddef.h>

// AI 呼叫接口：嚴格遵守 MimiClaw Tool 介面協定
esp_err_t tool_set_led_color_execute(const char *input_json, char *output, size_t output_size);

#endif // TOOL_WS2812_H