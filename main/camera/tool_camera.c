#include "tool_camera.h"
#include "camera_service.h" // 👈 ✨ 讓 AI 工具能呼叫底層拍照服務
#include "esp_log.h"
#include <stdio.h>

esp_err_t tool_camera_take_photo_execute(const char *input_json, char *output, size_t output_max_len) {
    (void)input_json; 
    
    // 執行相機動作
    esp_err_t err = camera_service_take_and_send_telegram();

    if (err == ESP_OK) {
        if (output) snprintf(output, output_max_len, "{\"status\": \"ok\", \"message\": \"Photo captured and sent\"}");
        return ESP_OK;
    } else {
        if (output) snprintf(output, output_max_len, "{\"status\": \"error\", \"message\": \"Camera failed\"}");
        return ESP_FAIL;
    }
}