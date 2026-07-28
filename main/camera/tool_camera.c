#include "tool_camera.h"
#include "camera_service.h" // 👈 ✨ 讓 AI 工具能呼叫底層拍照服務
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>

esp_err_t tool_camera_take_photo_execute(const char *input_json, char *output, size_t output_max_len) {
    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    const cJSON *chat_id_item = root ? cJSON_GetObjectItemCaseSensitive(root, "chat_id") : NULL;
    const char *chat_id = cJSON_IsString(chat_id_item) ? chat_id_item->valuestring : NULL;

    if (!chat_id || chat_id[0] == '\0') {
        if (output) {
            snprintf(output, output_max_len,
                     "{\"status\":\"error\",\"message\":\"Missing Telegram chat_id\"}");
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 執行相機動作
    esp_err_t err = camera_service_take_and_send_telegram(chat_id);
    cJSON_Delete(root);

    if (err == ESP_OK) {
        if (output) snprintf(output, output_max_len, "{\"status\":\"ok\",\"message\":\"Photo captured and sent\"}");
        return ESP_OK;
    } else {
        if (output) snprintf(output, output_max_len,
                             "{\"status\":\"error\",\"message\":\"Photo captured but Telegram upload failed\"}");
        return err;
    }
}
