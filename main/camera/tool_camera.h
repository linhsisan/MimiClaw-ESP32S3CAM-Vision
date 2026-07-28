#pragma once
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 宣告執行函式，讓 tool_registry 認識它
esp_err_t tool_camera_take_photo_execute(const char *input_json, char *output, size_t output_max_len);

#ifdef __cplusplus
}
#endif