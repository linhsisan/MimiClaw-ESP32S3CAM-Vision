#include "tools/tool_servo.h" // 請確認這裡的標頭檔名稱符合你的專案
#include "driver/ledc.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <stdbool.h>

// 伺服馬達 PWM 參數 (適用於 SG90 / MG996R)
#define SERVO_MIN_DUTY 205
#define SERVO_MAX_DUTY 1024

// 記錄目前角度
static int current_angles[49] = {0};
// 記錄該腳位是否已經從 NVS 讀取過記憶
static bool is_initialized[49] = {false}; 

esp_err_t tool_servo_control_execute(const char *input_json, char *output, size_t output_size) {
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "JSON 解析失敗");
        return 0;
    }

    // 1. 解析 JSON (預設：GPIO 40, 角度 90, 延遲 3ms)
    int gpio = cJSON_GetObjectItem(root, "gpio") ? cJSON_GetObjectItem(root, "gpio")->valueint : 40;
    int target_angle = cJSON_GetObjectItem(root, "angle") ? cJSON_GetObjectItem(root, "angle")->valueint : 90;
    int speed = cJSON_GetObjectItem(root, "speed") ? cJSON_GetObjectItem(root, "speed")->valueint : 3;
    cJSON_Delete(root);

    // 2. 防呆機制：限制數值範圍
    if (gpio < 0 || gpio > 48) gpio = 40;
    if (target_angle < 0) target_angle = 0;
    if (target_angle > 180) target_angle = 180;
    if (speed < 0) speed = 0;

    // 3. ✨ [關鍵] 開機第一次執行時，從 NVS (永久記憶體) 讀取上次的角度
    if (!is_initialized[gpio]) {
        nvs_handle_t my_handle;
        // 嘗試打開名為 servo_memory 的儲存空間
        esp_err_t err = nvs_open("servo_memory", NVS_READONLY, &my_handle);
        if (err == ESP_OK) {
            char key[16];
            snprintf(key, sizeof(key), "pin_%d", gpio); // 針對不同腳位獨立記憶
            int32_t saved_angle = 0;
            
            // 嘗試讀取這個腳位上次儲存的角度
            if (nvs_get_i32(my_handle, key, &saved_angle) == ESP_OK) {
                current_angles[gpio] = saved_angle; // 恢復記憶！
                printf("Servo: GPIO %d 成功恢復上次記憶角度 %ld 度\n", gpio, saved_angle);
            } else {
                // 如果找不到記憶（這輩子第一次執行），為了防暴衝，先假設它就在目標位置
                current_angles[gpio] = target_angle; 
            }
            nvs_close(my_handle);
        } else {
             // 如果 NVS 打不開，當作沒記憶處理
             current_angles[gpio] = target_angle;
        }
        is_initialized[gpio] = true; // 標記為已初始化，下次就不再讀取硬碟了
    }

    // 4. 初始化 PWM (LEDC)
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_3, 
        .freq_hz = 50, // 伺服馬達標準頻率 50Hz
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_3,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch_cfg);

    // 5. 執行平滑轉動邏輯
    int start_angle = current_angles[gpio];
    int direction = (start_angle < target_angle) ? 1 : -1;
    // 計算目標的 PWM Duty
    int target_duty = SERVO_MIN_DUTY + (target_angle * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180);

    if (start_angle != target_angle) {
        int total_steps = (target_angle > start_angle) ? (target_angle - start_angle) : (start_angle - target_angle);

        // FOR 迴圈：嚴格控制每次 1 度，達到平滑效果
        for (int i = 1; i <= total_steps; i++) {
            int current = start_angle + (i * direction);
            int duty = SERVO_MIN_DUTY + (current * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180);
            
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            // 控制轉動速度
            if (speed > 0) {
                vTaskDelay(pdMS_TO_TICKS(speed));
            }
        }
        // 動作完成，更新 RAM 中的記憶
        current_angles[gpio] = target_angle;
        
    } else {
        // 軟體記憶與目標一致，強制輸出一次目標訊號，確保實體馬達有到位
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target_duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(300)); // 給馬達 0.3 秒的時間確實走到位置
    }

    // 6. ✨ [關鍵] 將最新的角度存入 NVS (永久記憶體)
    nvs_handle_t my_handle;
    if (nvs_open("servo_memory", NVS_READWRITE, &my_handle) == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "pin_%d", gpio);
        nvs_set_i32(my_handle, key, target_angle); // 將角度寫入緩衝區
        nvs_commit(my_handle);                     // 確認寫入 Flash 硬碟
        nvs_close(my_handle);
    }

    // 回報給 LLM 大腦
    snprintf(output, output_size, "成功：馬達 (GPIO %d) 已轉動並記憶至 %d 度", gpio, target_angle);
    return 0;
}