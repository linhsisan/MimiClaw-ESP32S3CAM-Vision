#ifndef TOOL_GET_TIME_H
#define TOOL_GET_TIME_H

#include "esp_err.h"

esp_err_t tool_get_time_execute(const char *input_json, char *output, size_t output_size);

// 加上這行宣告
esp_err_t tool_set_timezone_execute(const char *input_json, char *output, size_t output_size);

#endif