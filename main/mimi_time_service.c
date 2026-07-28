#include "mimi_time_service.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>

static const char *TAG = "mimi_time";
#define TZ_FILE_PATH "/spiffs/tz.txt"
#define DEFAULT_TZ "CST-8" // 預設台北時間

esp_err_t mimi_time_service_init(void) {
    ESP_LOGI(TAG, "初始化 SNTP 網路校時服務...");

    // 1. 嘗試從檔案系統讀取上次記住的時區
    char tz_buf[64] = DEFAULT_TZ;
    FILE *f = fopen(TZ_FILE_PATH, "r");
    if (f) {
        fgets(tz_buf, sizeof(tz_buf), f);
        fclose(f);
        // 去除換行符號
        tz_buf[strcspn(tz_buf, "\r\n")] = 0;
        ESP_LOGI(TAG, "從記憶體讀取到時區: %s", tz_buf);
    } else {
        ESP_LOGI(TAG, "找不到時區記憶，使用預設值: %s", tz_buf);
    }

    // 2. 設定硬體時區
    // 在 SNTP 對時成功後，務必加上這兩行！
    setenv("TZ", "CST-8", 1); // 設定時區為 UTC+8 (台灣時間)
    tzset();                  // 套用時區設定
    ESP_LOGI("TIME", "時區已設定為台灣時間 (CST-8)");

    // 3. 啟動 SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.stdtime.gov.tw");
    esp_sntp_init();

    return ESP_OK;
}

// 讓 LLM 可以呼叫這個函式來改時區
esp_err_t mimi_time_service_set_tz(const char *tz_str) {
    // 寫入檔案永久保存
    FILE *f = fopen(TZ_FILE_PATH, "w");
    if (f) {
        fprintf(f, "%s", tz_str);
        fclose(f);
        ESP_LOGI(TAG, "時區已永久儲存至: %s", tz_str);
    } else {
        ESP_LOGE(TAG, "無法寫入時區檔案！");
        return ESP_FAIL;
    }

    // 立即套用新時區
    setenv("TZ", tz_str, 1);
    tzset();
    return ESP_OK;
}