#ifndef TOOL_DHT_H
#define TOOL_DHT_H

#include "esp_err.h"

// 宣告執行函數
esp_err_t tool_dht_read_execute(const char *input_json, char *output, size_t output_size);

#endif