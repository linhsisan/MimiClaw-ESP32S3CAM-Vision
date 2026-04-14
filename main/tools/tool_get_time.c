#include <stdio.h>
#include <string.h>
#include <stdlib.h> // 提供 getenv
#include <time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"  // 解析 LLM 傳來的 JSON
#include "tools/tool_get_time.h"
#include "mimi_time_service.h" // 呼叫底層的時區儲存服務

static const char *TAG = "tool_time";

// ---------------------------------------------------------
// 1. 取得當前時間工具 (AI 會常常呼叫這個)
// ---------------------------------------------------------
esp_err_t tool_get_time_execute(const char *input_json, char *output, size_t output_size) {
    ESP_LOGI(TAG, "LLM 請求取得當前時間 (讀取內部 RTC)...");

    time_t now;
    struct tm timeinfo;
    
    // 取得系統當前時間 (會自動套用系統當前的時區設定)
    time(&now);
    localtime_r(&now, &timeinfo);

    // 防呆機制：檢查是否已經同步過網路時間 (ESP32 剛開機若沒對時，年份會是 1970)
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "時間尚未與網路同步，等待 WiFi 獲取中！");
        snprintf(output, output_size, "{\"error\": \"系統剛開機，時間尚未與網路同步，請等候幾秒鐘再試。\"}");
        return ESP_FAIL;
    }

    // 格式化輸出 (例如：2026-04-11 14:30:00)
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // 取得星期幾的文字，讓 AI 講話更自然
    const char* weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    const char* weekday_str = weekdays[timeinfo.tm_wday];

    // 🚀 動態取得當前的時區設定 (例如 CST-8 或 JST-9)
    char current_tz[32] = "Unknown";
    char* env_tz = getenv("TZ");
    if (env_tz) {
        strncpy(current_tz, env_tz, sizeof(current_tz) - 1);
    }

    ESP_LOGI(TAG, "目前內部時間: [%s] %s %s", current_tz, strftime_buf, weekday_str);

    // 回傳給 LLM 大腦 (動態回傳當前時區，不再寫死台北！)
    snprintf(output, output_size, 
             "{\"status\":\"success\", \"current_timezone\":\"%s\", \"time\":\"%s\", \"weekday\":\"%s\"}", 
             current_tz, strftime_buf, weekday_str);

    return ESP_OK;
}

// ---------------------------------------------------------
// 2. 設定與記憶時區工具 (當使用者移動國家時呼叫)
// ---------------------------------------------------------
esp_err_t tool_set_timezone_execute(const char *input_json, char *output, size_t output_size) {
    ESP_LOGI(TAG, "LLM 請求更改系統時區...");

    cJSON *root = cJSON_Parse(input_json);
    if (root == NULL) {
        snprintf(output, output_size, "{\"error\": \"JSON 格式錯誤\"}");
        return ESP_ERR_INVALID_ARG;
    }

    // 抓取大腦傳來的 tz_string (例如 "JST-9")
    cJSON *tz_item = cJSON_GetObjectItem(root, "tz_string");
    if (!cJSON_IsString(tz_item) || tz_item->valuestring == NULL) {
        snprintf(output, output_size, "{\"error\": \"缺少 tz_string 參數\"}");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    // 呼叫底層服務，改時區並存入 SPIFFS 永久記憶
    esp_err_t ret = mimi_time_service_set_tz(tz_item->valuestring);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AI 成功將系統時區改為: %s", tz_item->valuestring);
        snprintf(output, output_size, "{\"status\": \"success\", \"new_timezone\": \"%s\"}", tz_item->valuestring);
    } else {
        snprintf(output, output_size, "{\"error\": \"硬體儲存失敗，時區檔案無法寫入\"}");
    }

    cJSON_Delete(root); // 釋放記憶體
    return ret;
}