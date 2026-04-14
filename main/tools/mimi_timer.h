#ifndef MIMI_TIMER_H
#define MIMI_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h" // ESP32 的 64 位元高精度硬體計時器

// ==========================================
// 1. 定義通用計時器物件結構
// ==========================================
typedef struct {
    int64_t start_time_us; // 紀錄開始的微秒時間戳記
    int64_t interval_us;   // 目標等待的微秒數
    bool is_running;       // 計時器是否正在運行
} mimi_timer_t;

// ==========================================
// 2. 通用方法：啟動計時器 (單位：毫秒 ms)
// ==========================================
// 使用 static inline，讓所有 include 這個檔案的 .c 檔都能直接使用而不會報錯
static inline void mimi_timer_start(mimi_timer_t *timer, uint32_t delay_ms) {
    timer->start_time_us = esp_timer_get_time();
    timer->interval_us = (int64_t)delay_ms * 1000ULL; // 毫秒轉微秒
    timer->is_running = true;
}

// ==========================================
// 3. 通用方法：檢查時間是否到了
// ==========================================
// 回傳 true 代表時間到了，並會自動停止該計時器
static inline bool mimi_timer_is_timeout(mimi_timer_t *timer) {
    if (!timer->is_running) {
        return false;
    }
    
    int64_t now_us = esp_timer_get_time();
    if ((now_us - timer->start_time_us) >= timer->interval_us) {
        timer->is_running = false; // 時間到了就自動歸零停止
        return true;
    }
    
    return false;
}

// ==========================================
// 4. 通用方法：手動停止計時器 (如果中途想取消)
// ==========================================
static inline void mimi_timer_stop(mimi_timer_t *timer) {
    timer->is_running = false;
}

#endif // MIMI_TIMER_H