#include "tool_dht.h"
#include "mimi_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "tool_dht";
static portMUX_TYPE dht_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * 使用硬體定時器精確測量電平持續時間
 */
static int64_t wait_for_level(int level, uint32_t timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(PIN_DHT) == level) {
        if ((esp_timer_get_time() - start) > timeout_us) return -1;
    }
    return esp_timer_get_time() - start;
}

static esp_err_t read_dht_raw(uint8_t data[5]) {
    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    // 1. 強力啟動訊號
    gpio_set_direction(PIN_DHT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_DHT, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // 拉低 20ms 喚醒感測器
    
    taskENTER_CRITICAL(&dht_mux);
    
    // 🚀 關鍵操作：拉高並立刻轉為輸入模式
    gpio_set_level(PIN_DHT, 1);
    esp_rom_delay_us(30); 
    gpio_set_direction(PIN_DHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_DHT, GPIO_PULLUP_ONLY); // 盡力利用內部上拉

    // 2. 捕獲感測器的響應訊號 (低80us -> 高80us)
    // 等待拉低
    if (wait_for_level(1, 100) == -1) { taskEXIT_CRITICAL(&dht_mux); return ESP_ERR_TIMEOUT; }
    // 等待拉高
    if (wait_for_level(0, 100) == -1) { taskEXIT_CRITICAL(&dht_mux); return ESP_ERR_TIMEOUT; }
    // 等待正式數據開始 (再次拉低)
    if (wait_for_level(1, 100) == -1) { taskEXIT_CRITICAL(&dht_mux); return ESP_ERR_TIMEOUT; }

    // 3. 讀取 40 Bits
    for (int i = 0; i < 40; i++) {
        // 等待低電平結束 (每個 Bit 前奏)
        if (wait_for_level(0, 100) == -1) { taskEXIT_CRITICAL(&dht_mux); return ESP_FAIL; }
        
        // 測量高電平持續時間
        int64_t high_time = wait_for_level(1, 100);
        if (high_time == -1) { taskEXIT_CRITICAL(&dht_mux); return ESP_FAIL; }

        data[i / 8] <<= 1;
        // 🚀 閾值修正：正常為 28us vs 70us，但在無電阻情況下，0 會被拉長
        // 我們將門檻設為 45us 以過濾雜訊
        if (high_time > 45) { 
            data[i / 8] |= 1;
        }
    }

    taskEXIT_CRITICAL(&dht_mux);

    // 4. 校驗和 Checksum
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        return ESP_OK;
    }
    return ESP_ERR_INVALID_CRC;
}

esp_err_t tool_dht_read_execute(const char *input_json, char *output, size_t output_size) {
    uint8_t data[5];
    (void)input_json; 

    // 🚀 既然硬體不穩，我們採取「七取三」策略：測 7 次，只要成功一次就立即回傳
    for (int retry = 0; retry < 7; retry++) {
        if (read_dht_raw(data) == ESP_OK) {
            float h, t;
            // 優先嘗試高精度解碼 (DHT22)
            h = ((data[0] << 8) | data[1]) * 0.1f;
            t = (((data[2] & 0x7F) << 8) | data[3]) * 0.1f;
            
            // 如果 DHT22 解碼出的數值太誇張，改用 DHT11 解法
            if (h > 100 || t > 80 || h < 1) {
                h = (float)data[0];
                t = (float)data[2];
            }

            // 合理性檢查：濕度不應超過 100%，溫度不應為 0 (除非你在冰箱)
            if (h > 5 && h <= 100 && t > 0) {
                snprintf(output, output_size, "{\"status\":\"ok\", \"temp\":%.1f, \"humi\":%.1f}", t, h);
                ESP_LOGI(TAG, "成功取得正確數值：T=%.1f H=%.1f", t, h);
                return ESP_OK;
            }
        }
        ESP_LOGW(TAG, "第 %d 次嘗試失敗，等待重試...", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }

    snprintf(output, output_size, "{\"status\":\"error\", \"message\":\"硬體雜訊過大，請確認線路或重啟\"}");
    return ESP_FAIL;
}