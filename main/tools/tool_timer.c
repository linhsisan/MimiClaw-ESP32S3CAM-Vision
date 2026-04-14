#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "tools/tool_timer.h" // 記得建立對應的 .h

static const char *TAG = "mimi_timer_tool";

// ---------------------------------------------------------
// AI 專用計時工具：讓 AI 能夠主動要求暫停一段時間
// 🚀 新增安全防護：禁止超過 5 秒的阻塞性等待，防止系統假死
// ---------------------------------------------------------
esp_err_t tool_timer_wait_execute(const char *input_json, char *output, size_t output_size) {
    
    // 1. 解析 AI 傳入的 JSON
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "{\"error\": \"JSON 格式錯誤\"}");
        return ESP_ERR_INVALID_ARG;
    }

    // 2. 獲取等待時間 (單位：毫秒)
    cJSON *ms_item = cJSON_GetObjectItem(root, "milliseconds");
    if (!cJSON_IsNumber(ms_item)) {
        snprintf(output, output_size, "{\"error\": \"缺少 milliseconds 參數\"}");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int delay_ms = ms_item->valueint;
    cJSON_Delete(root);

    // ---------------------------------------------------------
    // 🚀 3. 關鍵防禦機制：攔截過長的等待請求
    // ---------------------------------------------------------
    if (delay_ms > 5000) {
        ESP_LOGW(TAG, "⚠️ 阻擋過長等待！AI 試圖卡死系統 %d 毫秒", delay_ms);
        
        // 將明確的錯誤訊息傳回給 LLM，強迫它改用 cron_add
        snprintf(output, output_size, 
                 "{\"error\": \"Wait time too long (%d ms). Max allowed is 5000 ms. For anything longer, you MUST use the 'cron_add' tool to schedule a background task instead.\"}", 
                 delay_ms);
                 
        // 回傳 ESP_FAIL 讓 Agent Loop 知道工具執行失敗
        return ESP_FAIL; 
    }

    // 4. 執行物理延時 (僅限 5 秒內的合法請求)
    ESP_LOGI(TAG, "AI 獲准等待 %d 毫秒...", delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    // 5. 回報給 AI 執行結果
    snprintf(output, output_size, "{\"status\": \"success\", \"waited\": %d}", delay_ms);
    
    return ESP_OK;
}