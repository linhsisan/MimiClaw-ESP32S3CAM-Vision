#ifndef TOOL_ADC_H
#define TOOL_ADC_H

#include "esp_err.h"
#include <stddef.h>

// 宣告給 AI 用的類比讀取執行函數
esp_err_t tool_adc_read_execute(const char *input_json, char *output, size_t output_size);

#endif