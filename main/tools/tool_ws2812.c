#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"              // 必須引入 cJSON 函式庫
#include "led_strip.h"          // 官方 LED 組件
#include "tools/tool_ws2812.h"

static const char *TAG = "mimi_ws2812";

// 內部私有變數 (延遲初始化鎖)
static led_strip_handle_t led_strip = NULL;
static bool is_initialized = false; 

// ---------------------------------------------------------
// 1. 內部私有初始化函式 
// ---------------------------------------------------------
static esp_err_t mimi_ws2812_internal_init(void) {
    ESP_LOGI(TAG, "硬體喚醒：初始化 WS2812 (GPIO 48)...");

    led_strip_config_t strip_config = {
        .strip_gpio_num = 48,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    
    if (ret == ESP_OK) {
        // 成功後亮微弱藍光 (R:0, G:0, B:10)
        led_strip_set_pixel(led_strip, 0, 0, 0, 10);
        led_strip_refresh(led_strip);
    }
    return ret;
}

// ---------------------------------------------------------
// 2. AI 工具執行入口
// ---------------------------------------------------------
esp_err_t tool_set_led_color_execute(const char *input_json, char *output, size_t output_size) {
    
    // 【階段 A：延遲初始化】
    if (!is_initialized) {
        if (mimi_ws2812_internal_init() == ESP_OK) {
            is_initialized = true;
        } else {
            snprintf(output, output_size, "{\"error\": \"WS2812 硬體初始化失敗\"}");
            return ESP_FAIL;
        }
    }

    // 【階段 B：使用 cJSON 解析 input_json 字串】
    cJSON *root = cJSON_Parse(input_json);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON 解析失敗: %s", input_json);
        snprintf(output, output_size, "{\"error\": \"傳入的 JSON 格式錯誤\"}");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *r_item = cJSON_GetObjectItem(root, "r");
    cJSON *g_item = cJSON_GetObjectItem(root, "g");
    cJSON *b_item = cJSON_GetObjectItem(root, "b");
    // 💡 新增：解析 brightness 參數
    cJSON *bright_item = cJSON_GetObjectItem(root, "brightness");

    // 檢查 RGB 欄位是否存在且為數字
    if (!cJSON_IsNumber(r_item) || !cJSON_IsNumber(g_item) || !cJSON_IsNumber(b_item)) {
        snprintf(output, output_size, "{\"error\": \"缺少 r, g, b 參數或型態非數字\"}");
        cJSON_Delete(root); 
        return ESP_ERR_INVALID_ARG;
    }

    int r = r_item->valueint;
    int g = g_item->valueint;
    int b = b_item->valueint;
    
    // 💡 亮度邏輯：若 AI 有傳 brightness 則使用，否則預設為 100%
    int brightness = 100; 
    if (cJSON_IsNumber(bright_item)) {
        brightness = bright_item->valueint;
    }

    cJSON_Delete(root); // 釋放記憶體

    // 【階段 C：防呆限流與邊界檢查】
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    brightness = (brightness < 0) ? 0 : (brightness > 100) ? 100 : brightness;

    // 💡 【核心計算】：將 RGB 依照亮度百分比進行縮放
    int final_r = (r * brightness) / 100;
    int final_g = (g * brightness) / 100;
    int final_b = (b * brightness) / 100;

    // 【階段 D：物理執行】
    led_strip_set_pixel(led_strip, 0, final_r, final_g, final_b);
    led_strip_refresh(led_strip);
    ESP_LOGI(TAG, "原色 R:%d G:%d B:%d | 亮度 %d%% -> 實際輸出 R:%d G:%d B:%d", 
             r, g, b, brightness, final_r, final_g, final_b);

    // 【階段 E：安全寫入 Output 緩衝區】
    snprintf(output, output_size, 
             "{\"status\": \"success\", \"color\": \"R:%d G:%d B:%d\", \"brightness\": %d}", 
             r, g, b, brightness);

    return ESP_OK;
}