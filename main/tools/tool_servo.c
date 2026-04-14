#include "tools/tool_servo.h"
#include "driver/ledc.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h" // 必須加回這個來使用延遲功能
#include "freertos/task.h"
#include <stdio.h>

#define SERVO_MIN_DUTY 205
#define SERVO_MAX_DUTY 1024

// 記憶每根腳位(0~48)目前的角度，開機預設為 0 度
static int current_angles[49] = {0};

esp_err_t tool_servo_control_execute(const char *input_json, char *output, size_t output_size) {
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "JSON 解析失敗");
        return 0;
    }

    // 1. 解析 JSON：gpio(預設40), angle(指定角度), speed(延遲毫秒數, 預設3)
    int gpio = cJSON_GetObjectItem(root, "gpio") ? cJSON_GetObjectItem(root, "gpio")->valueint : 40;
    int target_angle = cJSON_GetObjectItem(root, "angle") ? cJSON_GetObjectItem(root, "angle")->valueint : 90;
    int speed = cJSON_GetObjectItem(root, "speed") ? cJSON_GetObjectItem(root, "speed")->valueint : 3;

    cJSON_Delete(root);

    // 2. 防呆機制
    if (gpio < 0 || gpio > 48) {
        gpio = 40;
    }
    if (target_angle < 0) {
        target_angle = 0;
    }
    if (target_angle > 180) {
        target_angle = 180;
    }
    if (speed < 0) {
        speed = 0;
    }

    // 3. 懶加載初始化 PWM
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_3, 
        .freq_hz = 50,
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

    // 4. FOR 迴圈平滑轉動邏輯 (每次嚴格轉動 1 度)
    int start_angle = current_angles[gpio];
    int direction = (start_angle < target_angle) ? 1 : -1; // 判斷往前(+1)還是往後(-1)

    if (start_angle != target_angle) {
        // 計算總共需要走幾步 (因為每次 1 度，所以總步數等於角度差)
        int total_steps = (target_angle > start_angle) ? (target_angle - start_angle) : (start_angle - target_angle);

        // FOR 迴圈：嚴格控制每次 1 度
        for (int i = 1; i <= total_steps; i++) {
            // 當前角度 = 起始角度 + (目前步數 * 方向)
            int current = start_angle + (i * direction);

            // 計算 PWM 並更新
            int duty = SERVO_MIN_DUTY + (current * (SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            // 速度 = 每次轉動延遲時間 (使用 vTaskDelay 讓出 CPU，1 = 1毫秒)
            if (speed > 0) {
                vTaskDelay(pdMS_TO_TICKS(speed));
            }
        }
        // 動作完成後，記憶最新角度
        current_angles[gpio] = target_angle;
    }

    snprintf(output, output_size, "成功：馬達 (GPIO %d) 已平滑轉動至 %d 度 (每度延遲 %d 毫秒)", gpio, target_angle, speed);
    return 0;
}