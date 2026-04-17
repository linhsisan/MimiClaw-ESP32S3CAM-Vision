#include "mimi_time_service.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_spiffs.h"

// 基礎組件
#include "wifi/wifi_manager.h"
#include "display_service.h"
#include "bus/message_bus.h"
#include "agent/agent_loop.h" 
#include "channels/telegram/telegram_bot.h"
#include "cron/cron_service.h"  

// 大腦與工具
#include "llm/llm_proxy.h"
#include "tools/tool_registry.h"

// 相機服務模組
#include "camera_service.h"

#include "esp_netif.h"

static const char *TAG = "mimi_main";

extern esp_err_t agent_loop_feed(mimi_msg_t *msg);

/* ══════════════ 獨立的「定時觀察」任務 ══════════════ */
void camera_capture_task(void *pvParameters) {
    ESP_LOGI(TAG, "定時拍照任務啟動 (10分鐘間隔)...");
    while (1) {
        camera_service_take_and_send_telegram();
        vTaskDelay(pdMS_TO_TICKS(600000)); 
    }
}

/* ══════════════ 訊息調度任務 ══════════════ */
void inbound_dispatch_task(void *arg) {
    ESP_LOGI(TAG, "Inbound dispatcher 啟動成功");
    while (1) {
        mimi_msg_t msg = {0};
        if (message_bus_pop_inbound(&msg, portMAX_DELAY) == ESP_OK) {
            display_service_push_chat("user", msg.content);
            
            if (msg.content != NULL) {
                size_t new_len = strlen(msg.content) + 512;
                char *injected_content = malloc(new_len);
                
                if (injected_content) {
                    snprintf(injected_content, new_len, 
                        "%s\n\n[SYSTEM WARNING: If the master requested a physical action (motor, fan, led, photo), you MUST output a tool call IMMEDIATELY as your first response. DO NOT output conversational text first. Bypassing tools will cause a fatal system crash.]", 
                        msg.content);
                    
                    free(msg.content);
                    msg.content = injected_content;
                }
            }

            agent_loop_feed(&msg); 
            if (msg.content) free(msg.content);
        }
    }
}

void outbound_dispatch_task(void *arg) {
    ESP_LOGI(TAG, "Outbound dispatcher 啟動成功");
    while (1) {
        mimi_msg_t msg = {0};
        if (message_bus_pop_outbound(&msg, portMAX_DELAY) == ESP_OK) {
            display_service_push_chat("assistant", msg.content);
            
            if (strcmp(msg.channel, "telegram") == 0) {
                telegram_send_message(msg.chat_id, msg.content);
            }
            if (msg.content) free(msg.content);
        }
    }
}

/* ══════════════ 主程式入口 ══════════════ */
void app_main(void) {
    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. 掛載 SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,  // 增加最大開啟檔案數以利 SOUL.md 操作
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    // 3. 啟動螢幕
    display_service_init();

    // 4. 初始化相機硬體
    esp_err_t cam_err = camera_service_init();
    if (cam_err != ESP_OK) {
        ESP_LOGE(TAG, "相機硬體啟動失敗");
    }

    // 5. 連接 WiFi
    wifi_manager_init();
    wifi_manager_start();

    if (wifi_manager_wait_connected(15000) == ESP_OK) {
        display_service_show_wifi_ok("Connected");

        // 6. 系統服務初始化
        mimi_time_service_init();
        
        // 啟動 Cron 持久化引擎與背景計時器
        cron_service_init();   
        cron_service_start();  

        message_bus_init();
        llm_proxy_init();      
        tool_registry_init();  

        agent_loop_init();
        agent_loop_start();

        // 7. 啟動訊息調度神經任務
        xTaskCreate(inbound_dispatch_task,  "inbound",  8192, NULL, 5, NULL);
        xTaskCreate(outbound_dispatch_task, "outbound", 8192, NULL, 5, NULL);
        
        // 8. 啟動 Telegram 文字 Bot
        telegram_bot_init();
        telegram_bot_start();
        
        // 9. 背景定時照片 (選填)
        // xTaskCreate(camera_capture_task, "camera_task", 8192, NULL, 5, NULL);

        display_service_show_ready("Online", "Telegram", "GPT-4o-Mini");
        ESP_LOGI(TAG, "MimiClaw 系統完全啟動！");
        
        // --- 🕵️ 靈魂檔案自檢已移除 ---
        // 系統現在將直接信任自動寫入的 SOUL.md 配置

    } else {
        display_service_show_wifi_fail();
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}