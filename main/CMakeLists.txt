idf_component_register(
    SRCS
        "main.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "channels/telegram/telegram_bot.c"
        "channels/feishu/feishu_bot.c"
        "llm/llm_proxy.c"
        "agent/agent_loop.c"
        "agent/context_builder.c"
        "memory/memory_store.c"
        "memory/session_mgr.c"
        "gateway/ws_server.c"
        "cli/serial_cli.c"
        "proxy/http_proxy.c"
        "cron/cron_service.c"
        "heartbeat/heartbeat.c"
        "tools/tool_registry.c"
        "tools/tool_cron.c"
        "tools/tool_web_search.c"
        "tools/tool_get_time.c"
        "tools/tool_files.c"
        "tools/tool_gpio.c"
        "tools/gpio_policy.c"
        "skills/skill_loader.c"
        "onboard/wifi_onboard.c"
        "tools/tool_adc.c"
        "tools/tool_dht.c"
        "tools/tool_servo.c"
        "tools/tool_ws2812.c"
        "tools/tool_timer.c"
        "display/display_service.c"  # 👈 加上 display/
        "display/ili9341.c"          # 👈 加上 display/
        "display/source-han-sans16.c" # 👈 中文字型也要確保在這裡
        "mimi_time_service.c"  # 👈 把這行加進去！
        "display/ST7789.c"   # ✨ 新增這一行！告訴系統編譯這個檔案
        "camera/camera_service.c"
        "camera/tool_camera.c"  # ✨ 直接把 tool_camera.c 加進來，確保它被編譯
    INCLUDE_DIRS 
        "." 
        "tools" 
        "agent" 
        "bus"
        "llm"
        "wifi"
        "channels/telegram"  # 👈 增加這行，讓系統找得到 Telegram 的 .h
        "display"  # 👈 補上這行，確保 C 語言能找到 source-han-sans16.h
        "camera"               # ✨ 關鍵：告訴編譯器來這裡找 .h 檔
REQUIRES
# 僅保留最核心的、確定存在的組件
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_event json spiffs console vfs app_update
        esp_timer esp_websocket_client esp_driver_gpio
        esp_adc driver esp_lcd lvgl 
        esp_lvgl_port esp_lcd_st7789 cjson led_strip
        # ✨ 關鍵新增：告訴編譯器我們要用到這兩個官方模組
      
)

# 🚀 修正：使用 ../ 回退到根目錄，然後進入 spiffs_data
spiffs_create_partition_image(spiffs ../spiffs_data FLASH_IN_PROJECT)
