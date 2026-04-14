#include "mimi_config.h"
#include "tool_registry.h"

#include "tools/tool_web_search.h"
#include "tools/tool_get_time.h"
#include "tools/tool_files.h"
#include "tools/tool_cron.h"
#include "tools/tool_gpio.h"
#include "tools/tool_adc.h"
#include "tools/tool_dht.h"
#include "tools/tool_servo.h"
#include "tools/tool_ws2812.h"
#include "tools/tool_timer.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"

// 引入硬體服務
#include "display_service.h"
#include "tool_camera.h"
#include "esp_camera.h"

static const char *TAG = "tools";

#define MAX_TOOLS 32

static mimi_tool_t s_tools[MAX_TOOLS];
static int s_tool_count = 0;
static char *s_tools_json = NULL;

// 🚀 關鍵宣告
extern esp_err_t tool_camera_take_photo_execute(const char *input_json, char *output, size_t output_max_len);

static void register_tool(const mimi_tool_t *tool)
{
    if (s_tool_count >= MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return;
    }
    s_tools[s_tool_count++] = *tool;
    ESP_LOGI(TAG, "Registered tool: %s", tool->name);
}

static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);
        cJSON *schema = cJSON_Parse(s_tools[i].input_schema_json);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }
        cJSON_AddItemToArray(arr, tool);
    }
    free(s_tools_json);
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "Tools JSON built (%d tools)", s_tool_count);
}

// 🚀 螢幕對話顯示工具實作
esp_err_t tool_display_chat_execute(const char *input_json, char *output, size_t output_max_len) {
    cJSON *root = cJSON_Parse(input_json);
    if (!root) return ESP_FAIL;

    cJSON *msg_item = cJSON_GetObjectItem(root, "message");
    cJSON *is_ai_item = cJSON_GetObjectItem(root, "is_ai");

    if (cJSON_IsString(msg_item)) {
        const char *role = (cJSON_IsBool(is_ai_item) && !cJSON_IsTrue(is_ai_item)) ? "user" : "assistant";
        display_service_push_chat(role, msg_item->valuestring);
        if (output) snprintf(output, output_max_len, "Message displayed successfully");
    }
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t tool_registry_init(void)
{
    s_tool_count = 0;

    // === [1] 系統核心工具 (核心層) ===
    
    mimi_tool_t ws = {
        .name = "web_search",
        .description = "Search the web for real-time information.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}",
        .execute = tool_web_search_execute,
    };
    register_tool(&ws);

    mimi_tool_t gt = {
        .name = "get_current_time",
        .description = "Get current local time (CST-8). Use this to verify the clock before scheduling.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"dummy\":{\"type\":\"string\",\"description\":\"unused\"}}}",
        .execute = tool_get_time_execute,
    };
    register_tool(&gt);

    mimi_tool_t disp = {
        .name = "display_chat",
        .description = "Display text bubbles on the local TFT screen.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\"},\"is_ai\":{\"type\":\"boolean\"}},\"required\":[\"message\"]}",
        .execute = tool_display_chat_execute,
    };
    register_tool(&disp);

    // === [2] 儲存與記憶工具 (記憶層) ===

    mimi_tool_t append = {
        .name = "append_file",
        .description = "Add text to /spiffs/memory/MEMORY.md. NEVER overwrite MEMORY.md.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
        .execute = tool_append_file_execute,
    };
    register_tool(&append);

    mimi_tool_t rf = { .name = "read_file", .description = "Read a file from internal storage.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}", .execute = tool_read_file_execute };
    register_tool(&rf);

    mimi_tool_t wf = { .name = "write_file", .description = "Write/Overwrite a file.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}", .execute = tool_write_file_execute };
    register_tool(&wf);

    mimi_tool_t ld = { .name = "list_dir", .description = "List files in a directory.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"prefix\":{\"type\":\"string\"}}}", .execute = tool_list_dir_execute };
    register_tool(&ld);

    // === [3] 自動化與排程工具 (邏輯層) ===

    mimi_tool_t ca = {
        .name = "cron_add",
        .description = "Schedule a task. Use 'target_time' (format: YYYY-MM-DD HH:MM:SS) and 'offset_s'.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{"
                             "\"name\":{\"type\":\"string\"},"
                             "\"schedule_type\":{\"type\":\"string\",\"enum\":[\"at\",\"every\"]},"
                             "\"target_time\":{\"type\":\"string\",\"description\":\"Format: YYYY-MM-DD HH:MM:SS\"},"
                             "\"offset_s\":{\"type\":\"integer\",\"description\":\"Delay from target_time in seconds\"},"
                             "\"interval_s\":{\"type\":\"integer\"},"
                             "\"message\":{\"type\":\"string\",\"description\":\"COMMAND to self when fired\"}"
                             "},\"required\":[\"name\",\"schedule_type\",\"message\"]}",
        .execute = tool_cron_add_execute,
    };
    register_tool(&ca);

    mimi_tool_t cl = { 
        .name = "cron_list", 
        .description = "List all active cron jobs and their IDs.", 
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"dummy\":{\"type\":\"string\"}}}", 
        .execute = tool_cron_list_execute 
    };
    register_tool(&cl);

    mimi_tool_t cr = { .name = "cron_remove", .description = "Delete a cron job using its Job ID.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"job_id\":{\"type\":\"string\"}},\"required\":[\"job_id\"]}", .execute = tool_cron_remove_execute };
    register_tool(&cr);

    mimi_tool_t timer = { .name = "timer_wait", .description = "Wait for < 5s delay.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"milliseconds\":{\"type\":\"integer\"}},\"required\":[\"milliseconds\"]}", .execute = tool_timer_wait_execute };
    register_tool(&timer);

    // === [4] 硬體執行工具 (動作層) ===

    mimi_tool_t tp = {
        .name = "take_photo",
        .description = "Take a photo and send via Telegram.",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"dummy\":{\"type\":\"string\"}}}",
        .execute = tool_camera_take_photo_execute,
    };
    register_tool(&tp);

    mimi_tool_t led = {
        .name = "set_led_color",
        .description = "Set WS2812 RGB color (0-255) and brightness (0-100).",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"r\":{\"type\":\"integer\"},\"g\":{\"type\":\"integer\"},\"b\":{\"type\":\"integer\"},\"brightness\":{\"type\":\"integer\"}},\"required\":[\"r\",\"g\",\"b\"]}",
        .execute = tool_set_led_color_execute,
    };
    register_tool(&led);

    mimi_tool_t servo = {
        .name = "control_servo",
        .description = "Set servo angle (0-180).",
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"angle\":{\"type\":\"integer\"},\"speed\":{\"type\":\"integer\"}},\"required\":[\"angle\"]}",
        .execute = tool_servo_control_execute,
    };
    register_tool(&servo);

    // === [5] 低階與感測工具 (物理層) ===

    tool_gpio_init();
    mimi_tool_t gw = { .name = "gpio_write", .description = "Set GPIO pin state.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"pin\":{\"type\":\"integer\"},\"state\":{\"type\":\"integer\"}},\"required\":[\"pin\",\"state\"]}", .execute = tool_gpio_write_execute };
    register_tool(&gw);

    mimi_tool_t ra = { .name = "read_adc", .description = "Read analog voltage from a pin.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"pin\":{\"type\":\"integer\"}},\"required\":[\"pin\"]}", .execute = tool_adc_read_execute };
    register_tool(&ra);

    mimi_tool_t rd = { .name = "read_dht", .description = "Read DHT11 temperature/humidity.", .input_schema_json = "{\"type\":\"object\",\"properties\":{\"pin\":{\"type\":\"integer\"}},\"required\":[\"pin\"]}", .execute = tool_dht_read_execute };
    register_tool(&rd);

    mimi_tool_t ga = { 
        .name = "gpio_read_all", 
        .description = "Check all GPIO pin statuses.", 
        .input_schema_json = "{\"type\":\"object\",\"properties\":{\"dummy\":{\"type\":\"string\"}}}", 
        .execute = tool_gpio_read_all_execute 
    };
    register_tool(&ga);

    // === [6] 啟動清單生成 ===

    build_tools_json();
    ESP_LOGI(TAG, "Tool registry fully initialized and OpenAI compliant.");
    return ESP_OK;
}

const char *tool_registry_get_tools_json(void) { return s_tools_json; }

esp_err_t tool_registry_execute(const char *name, const char *input_json, char *output, size_t output_size)
{
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i].execute(input_json, output, output_size);
        }
    }
    return ESP_ERR_NOT_FOUND;
}