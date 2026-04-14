#ifndef TOOL_TIMER_H
#define TOOL_TIMER_H

#include "esp_err.h"
#include <stddef.h>

/**
 * @brief AI 全域延時工具 (Timer Wait)
 * * 讓 AI 能夠主動要求系統暫停指定的毫秒數。
 * 這是實現多步驟自動化（如：開燈 -> 等待 -> 關燈）的關鍵中介工具。
 */
esp_err_t tool_timer_wait_execute(const char *input_json, char *output, size_t output_size);

#endif // TOOL_TIMER_H