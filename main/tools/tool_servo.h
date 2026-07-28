#ifndef TOOL_SERVO_H
#define TOOL_SERVO_H

#include "esp_err.h"

// 必須在這裡宣告，tool_registry.c 才看得到它
void tool_servo_init(void);

// 執行工具的宣告
esp_err_t tool_servo_control_execute(const char *input_json, char *output, size_t output_size);

#endif