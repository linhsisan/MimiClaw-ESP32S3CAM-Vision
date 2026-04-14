/* 攝影機服務  這個版本他沒有刪除。緩衝的相片，但是可以成功運作。 */
#include "camera_service.h"
#include "esp_camera.h"
#include "mimi_config.h"   
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <string.h>

static const char *TAG = "camera_service";

esp_err_t camera_service_init(void) {
    ESP_LOGI(TAG, "🚀 正在啟動攝影機硬體初始化...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    
    // 果雲 S3-CAM 腳位定義 (專為 ESP32-S3 設計)
    config.pin_d0 = 11; config.pin_d1 = 9; config.pin_d2 = 8; config.pin_d3 = 10;
    config.pin_d4 = 12; config.pin_d5 = 18; config.pin_d6 = 17; config.pin_d7 = 16;
    config.pin_xclk = 15; config.pin_pclk = 13; config.pin_vsync = 6; config.pin_href = 7;
    config.pin_sccb_sda = 4; config.pin_sccb_scl = 5;
    config.pin_pwdn = -1; config.pin_reset = -1;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_UXGA;    // 解析度 1600x1200
    config.jpeg_quality = 12;               // 1-63, 越小越清晰
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t ret = esp_camera_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 攝影機初始化失敗！錯誤代碼: 0x%x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "✅ 攝影機驅動已成功掛載！");
    return ESP_OK;
}

esp_err_t camera_service_take_and_send_telegram(void) {
    ESP_LOGI(TAG, "📸 收到拍照請求，正在抓取幀緩衝區...");
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "❌ 抓取影像失敗 (Frame Buffer Null)");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "🖼️ 影像抓取成功，大小: %zu bytes", fb->len);

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", MIMI_SECRET_TG_TOKEN);

    const char *boundary = "MimiClawBoundary";
    const char *footer = "\r\n--MimiClawBoundary--\r\n";
    char header[512];
    
    /* 🚀 修正點：根據你的 Log，Chat ID 應該是 "8732234465"
     * 建議這裡直接填入你正在使用的真實 ID，或使用 mimi_config 裡的宏 */
    snprintf(header, sizeof(header),
             "--%s\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n%s\r\n"
             "--%s\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"mimiclaw.jpg\"\r\n"
             "Content-Type: image/jpeg\r\n\r\n",
             boundary, "8732234465", boundary); 

    esp_http_client_config_t http_conf = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000, // 增加超時，因為上傳圖片較慢
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_conf);
    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    int total_len = strlen(header) + fb->len + strlen(footer);
    
    ESP_LOGI(TAG, "🌐 正在上傳至 Telegram API...");
    esp_err_t err = esp_http_client_open(client, total_len);
    if (err == ESP_OK) {
        esp_http_client_write(client, header, strlen(header));
        esp_http_client_write(client, (const char *)fb->buf, fb->len);
        esp_http_client_write(client, footer, strlen(footer));
        esp_http_client_fetch_headers(client);
        
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            ESP_LOGI(TAG, "✅ 照片發送成功！Telegram 回傳 HTTP 200");
        } else {
            ESP_LOGE(TAG, "⚠️ 照片發送失敗，HTTP 狀態碼: %d", status);
        }
    } else {
        ESP_LOGE(TAG, "❌ 無法連線至 Telegram API (HTTP Open Fail)");
    }

    esp_http_client_cleanup(client);
    esp_camera_fb_return(fb);
    return err;
}