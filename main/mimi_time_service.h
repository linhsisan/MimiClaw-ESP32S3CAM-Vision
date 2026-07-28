#ifndef MIMI_TIME_SERVICE_H
#define MIMI_TIME_SERVICE_H

#include "esp_err.h"

// 啟動時間服務 (會自動讀取上次儲存的時區)
esp_err_t mimi_time_service_init(void);

// 供 AI 呼叫的工具：動態修改時區並永久存檔
esp_err_t mimi_time_service_set_tz(const char *tz_str);

#endif // MIMI_TIME_SERVICE_H