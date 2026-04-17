#include "agent_loop.h"
#include "agent/context_builder.h"
#include "mimi_config.h"
#include "bus/message_bus.h"
#include "llm/llm_proxy.h"
#include "memory/session_mgr.h"
#include "tools/tool_registry.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

static const char *TAG = "agent";
#define TOOL_OUTPUT_SIZE  (8 * 1024)
static QueueHandle_t agent_queue = NULL;

/* --- 內部輔助函數 --- */
static cJSON *build_assistant_content(const llm_response_t *resp) {
    cJSON *content = cJSON_CreateArray();
    if (resp->text && resp->text_len > 0) {
        cJSON *text_block = cJSON_CreateObject();
        cJSON_AddStringToObject(text_block, "type", "text");
        cJSON_AddStringToObject(text_block, "text", resp->text);
        cJSON_AddItemToArray(content, text_block);
    }
    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call = &resp->calls[i];
        cJSON *tool_block = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_block, "type", "tool_use");
        cJSON_AddStringToObject(tool_block, "id", call->id);
        cJSON_AddStringToObject(tool_block, "name", call->name);
        cJSON *input = cJSON_Parse(call->input ? call->input : "{}");
        cJSON_AddItemToObject(tool_block, "input", input ? input : cJSON_CreateObject());
        cJSON_AddItemToArray(content, tool_block);
    }
    return content;
}

static void append_turn_context_prompt(char *prompt, size_t size, const mimi_msg_t *msg) {
    size_t off = strnlen(prompt, size - 1);
    snprintf(prompt + off, size - off,
             "\n## Current Turn Context\n- source_channel: %s\n- source_chat_id: %s\n",
             msg->channel, msg->chat_id);
}

esp_err_t agent_loop_feed(mimi_msg_t *msg) {
    if (!agent_queue || !msg) return ESP_FAIL;
    mimi_msg_t *queued_msg = malloc(sizeof(mimi_msg_t));
    memcpy(queued_msg, msg, sizeof(mimi_msg_t));
    queued_msg->content = strdup(msg->content);
    if (xQueueSend(agent_queue, &queued_msg, pdMS_TO_TICKS(100)) != pdPASS) {
        free(queued_msg->content); free(queued_msg); return ESP_FAIL;
    }
    return ESP_OK;
}

/* --- AI 處理任務 --- */
static void agent_loop_task(void *arg) {
    ESP_LOGI(TAG, "Agent Task Running...");

    char *system_prompt = heap_caps_calloc(1, MIMI_CONTEXT_BUF_SIZE, MALLOC_CAP_SPIRAM);
    char *history_json = heap_caps_calloc(1, MIMI_LLM_STREAM_BUF_SIZE, MALLOC_CAP_SPIRAM);
    char *tool_output = heap_caps_calloc(1, TOOL_OUTPUT_SIZE, MALLOC_CAP_SPIRAM);
    const char *tools_json = tool_registry_get_tools_json();

    while (1) {
        mimi_msg_t *msg_ptr;
        if (xQueueReceive(agent_queue, &msg_ptr, portMAX_DELAY) != pdPASS) continue;

        context_build_system_prompt(system_prompt, MIMI_CONTEXT_BUF_SIZE);
        append_turn_context_prompt(system_prompt, MIMI_CONTEXT_BUF_SIZE, msg_ptr);
        session_get_history_json(msg_ptr->chat_id, history_json, MIMI_LLM_STREAM_BUF_SIZE, MIMI_AGENT_MAX_HISTORY);
        
        cJSON *messages = cJSON_Parse(history_json);
        if (!messages) messages = cJSON_CreateArray();

        cJSON *user_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", msg_ptr->content);
        cJSON_AddItemToArray(messages, user_msg);

        char *final_text = NULL;
        int iteration = 0;
        int read_count = 0; // 用於紀錄這是第幾次讀取

        while (iteration < MIMI_AGENT_MAX_TOOL_ITER) {
            llm_response_t resp;
            if (llm_chat_tools(system_prompt, messages, tools_json, &resp) != ESP_OK) break;

            if (!resp.tool_use) {
                if (resp.text) final_text = strdup(resp.text);
                llm_response_free(&resp); break;
            }

            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON_AddItemToObject(asst_msg, "content", build_assistant_content(&resp));
            cJSON_AddItemToArray(messages, asst_msg);

            cJSON *results_array = cJSON_CreateArray();
            for (int i = 0; i < resp.call_count; i++) {
                tool_output[0] = '\0';
                tool_registry_execute(resp.calls[i].name, resp.calls[i].input, tool_output, TOOL_OUTPUT_SIZE);

                // 🚀 關鍵優化：將 JSON 轉換為口語化的即時訊息
                if (strcmp(resp.calls[i].name, "read_dht") == 0) {
                    read_count++;
                    cJSON *json = cJSON_Parse(tool_output);
                    char instant_buf[256];
                    
                    if (json) {
                        double temp = cJSON_GetObjectItem(json, "temp")->valuedouble;
                        double humi = cJSON_GetObjectItem(json, "humi")->valuedouble;
                        snprintf(instant_buf, sizeof(instant_buf), 
                                 "🔔 [即時回報] 第 %d 次讀取：\n🌡️ 溫度: %.1f°C\n💧 濕度: %.1f%%", 
                                 read_count, temp, humi);
                        cJSON_Delete(json);
                    } else {
                        snprintf(instant_buf, sizeof(instant_buf), "⚠️ 第 %d 次讀取失敗，正在重試...", read_count);
                    }

                    mimi_msg_t out = {0};
                    strncpy(out.channel, msg_ptr->channel, sizeof(out.channel)-1); 
                    strncpy(out.chat_id, msg_ptr->chat_id, sizeof(out.chat_id)-1);
                    out.content = strdup(instant_buf);
                    message_bus_push_outbound(&out);
                }

                // 🚀 優化：將計時器通知也口語化
                if (strcmp(resp.calls[i].name, "timer_wait") == 0) {
                    mimi_msg_t out = {0};
                    strncpy(out.channel, msg_ptr->channel, sizeof(out.channel)-1); 
                    strncpy(out.chat_id, msg_ptr->chat_id, sizeof(out.chat_id)-1);
                    
                    // 從輸入 JSON 解析等待時間（若需要更精確可解析 input）
                    out.content = strdup("⏳ 正在按照計畫等待中，請稍候...");
                    message_bus_push_outbound(&out);
                }

                cJSON *res_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(res_obj, "type", "tool_result");
                cJSON_AddStringToObject(res_obj, "tool_use_id", resp.calls[i].id);
                cJSON_AddStringToObject(res_obj, "content", tool_output);
                cJSON_AddItemToArray(results_array, res_obj);
            }
            cJSON *res_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(res_msg, "role", "user");
            cJSON_AddItemToObject(res_msg, "content", results_array);
            cJSON_AddItemToArray(messages, res_msg);
            llm_response_free(&resp);
            iteration++;
        }

        if (final_text) {
            session_append(msg_ptr->chat_id, "user", msg_ptr->content);
            session_append(msg_ptr->chat_id, "assistant", final_text);
            mimi_msg_t out = {0};
            strncpy(out.channel, msg_ptr->channel, sizeof(out.channel)-1);
            strncpy(out.chat_id, msg_ptr->chat_id, sizeof(out.chat_id)-1);
            out.content = final_text; 
            message_bus_push_outbound(&out);
        }
        cJSON_Delete(messages); free(msg_ptr->content); free(msg_ptr);
    }
}

esp_err_t agent_loop_init(void) {
    if (!agent_queue) agent_queue = xQueueCreate(10, sizeof(mimi_msg_t *));
    return ESP_OK;
}

esp_err_t agent_loop_start(void) {
    xTaskCreatePinnedToCore(agent_loop_task, "agent_loop", MIMI_AGENT_STACK, NULL, MIMI_AGENT_PRIO, NULL, MIMI_AGENT_CORE);
    return ESP_OK;
}