#include "mimi_time_service.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h> // ✨ 確保 bool 型態能正常編譯
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
#include "cron/cron_service.h"   // ✨ 加上這行，讓系統認識 Cron 引擎

// 大腦與工具
#include "llm/llm_proxy.h"
#include "tools/tool_registry.h"

// ✨ 新增：相機服務模組
#include "camera_service.h"

#include "esp_netif.h"

static const char *TAG = "mimi_main";

extern esp_err_t agent_loop_feed(mimi_msg_t *msg);

/* 🚀 備註：此處不再定義 CAMERA_TELEGRAM_BOT_TOKEN 
   系統將自動引用 mimi_config.h 連結的資安金鑰 */

/* ══════════════ 獨立的「定時觀察」任務 ══════════════ */

void camera_capture_task(void *pvParameters) {
    ESP_LOGI(TAG, "定時拍照任務啟動 (10分鐘間隔)...");
    while (1) {
        // ✨ 修改點：呼叫無參數版服務，內部會自動處理 Token
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
            
            /* =========================================================
             * 🧠 終極修改：隱藏警告注射器 (Context Injection)
             * 在把訊息餵給大腦前，強行加上不可見的系統級鐵律
             * ========================================================= */
            if (msg.content != NULL) {
                // 建立一個夠大的新字串緩衝區 (+512 bytes 給警告文字)
                size_t new_len = strlen(msg.content) + 512;
                char *injected_content = malloc(new_len);
                
                if (injected_content) {
                    // 將「主人的原話」與「恐嚇警告」完美融合
                    snprintf(injected_content, new_len, 
                        "%s\n\n[SYSTEM WARNING: If the master requested a physical action (motor, fan, led, photo), you MUST output a tool call IMMEDIATELY as your first response. DO NOT output conversational text first. Bypassing tools will cause a fatal system crash.]", 
                        msg.content);
                    
                    // 釋放原本沒有警告的舊字串，換成加料版
                    free(msg.content);
                    msg.content = injected_content;
                }
            }
            /* ========================================================= */

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
            
            // 透過 Telegram 傳回手機
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
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    // 3. 啟動螢幕
    display_service_init();

    // ✨ 4. 初始化相機硬體 (在 Wi-Fi 啟動前優先初始化硬體)
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
        
        /* ---------------------------------------------------------
         * ✨ 關鍵修復：啟動 Cron 持久化引擎與背景計時器
         * --------------------------------------------------------- */
        cron_service_init();   // 去 SPIFFS 讀取以前存檔的排程
        cron_service_start();  // 啟動背景任務，時間到了自動觸發

        message_bus_init();

        llm_proxy_init();      // 載入 API Key
        tool_registry_init();  // ✨ 註冊工具 (含拍照工具)

        agent_loop_init();
        agent_loop_start();

        // 7. 啟動訊息調度神經任務
        xTaskCreate(inbound_dispatch_task,  "inbound",  8192, NULL, 5, NULL);
        xTaskCreate(outbound_dispatch_task, "outbound", 8192, NULL, 5, NULL);
        
        // 8. 啟動 Telegram 文字 Bot
        telegram_bot_init();
        telegram_bot_start();
        
        // ✨ 9. 若相機硬體成功，啟動背景任務 (選填)
        if (cam_err == ESP_OK) {
            // 如果你希望背景定時拍照，請取消下一行的註解
            // xTaskCreate(camera_capture_task, "camera_task", 8192, NULL, 5, NULL);
        }

        display_service_show_ready("Online", "Telegram", "GPT-4o-Mini");
        ESP_LOGI(TAG, "MimiClaw 系統完全恢復，相機功能已掛載！");

        /* ---------------------------------------------------------
         * 🕵️ 靈魂檔案自檢：檢查 SOUL.md 是否真的包含核心鐵律
         * --------------------------------------------------------- */
        FILE *f_soul = fopen("/spiffs/config/SOUL.md", "r");
        if (f_soul != NULL) {
            fseek(f_soul, 0, SEEK_END);
            long fsize = ftell(f_soul);
            fseek(f_soul, 0, SEEK_SET);

            if (fsize > 0 && fsize < 32768) { // 避免檔案過大導致記憶體不足
                char *soul_content = malloc(fsize + 1);
                if (soul_content) {
                    fread(soul_content, 1, fsize, f_soul);
                    soul_content[fsize] = '\0';

                    // 🔍 嚴格比對三個核心工具是否出現在 SOUL.md 中
                    bool has_memory = strstr(soul_content, "append_file") != NULL;
                    bool has_camera = strstr(soul_content, "take_photo") != NULL;
                    bool has_timer  = strstr(soul_content, "cron_add") != NULL;

                    free(soul_content);

                    if (has_memory && has_camera && has_timer) {
                        // 🟢 驗證成功：印出綠色確認
                        printf("\n\033[1;32m"); 
                        printf("========================================================\n");
                        printf(" 🧠 [SOUL 自檢通過] 檔案驗證無誤！MimiClaw 將絕對遵守鐵律：\n");
                        printf("--------------------------------------------------------\n");
                        printf(" 1. 【記憶系統】永遠使用 append_file 記錄 MEMORY.md，絕不覆蓋。\n");
                        printf(" 2. 【視覺系統】主動使用 take_photo 拍攝最新環境照片。\n");
                        printf(" 3. 【時間管理】超過 5 秒的任務必須使用 cron_add，嚴禁死等。\n");
                        printf(" 4. 【網頁管理】192.168.XX.XX:18789\n");
                        printf("========================================================\n");
                        printf("\033[0m\n");
                    } else {
                        // 🟡 驗證失敗：缺少部分指令，印出黃色警告
                        printf("\n\033[1;33m"); 
                        printf("⚠️ [SOUL 警告] SOUL.md 內容驗證失敗！\n");
                        printf("   大腦可能缺少以下認知，請檢查 /spiffs/config/SOUL.md：\n");
                        if (!has_memory) printf("   - ❌ 缺少 'append_file' (記憶功能可能異常)\n");
                        if (!has_camera) printf("   - ❌ 缺少 'take_photo' (視覺功能可能異常)\n");
                        if (!has_timer)  printf("   - ❌ 缺少 'cron_add' (時間管理可能異常)\n");
                        printf("\033[0m\n");
                    }
                }
            }
            fclose(f_soul);
        } else {
            // 🔴 嚴重錯誤：找不到檔案，印出紅色警告
            printf("\n\033[1;31m"); 
            printf("❌ [SOUL 錯誤] 找不到 /spiffs/config/SOUL.md！MimiClaw 目前處於無靈魂狀態！\n");
            printf("\033[0m\n");
        }

    } else {
        display_service_show_wifi_fail();
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}