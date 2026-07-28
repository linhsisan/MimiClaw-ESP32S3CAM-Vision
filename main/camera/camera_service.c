/* 攝影機服務  這個版本他刪除緩衝的相片，延長傳送的時間，避免http time out。 */
#include "camera_service.h"
#include "esp_camera.h"
#include "mimi_config.h"   
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <string.h>

static const char *TAG = "camera_service";

static esp_err_t http_write_all(esp_http_client_handle_t client, const void *data, size_t len)
{
    const char *cursor = (const char *)data;
    size_t written = 0;

    while (written < len) {
        int result = esp_http_client_write(client, cursor + written, len - written);
        if (result <= 0) {
            ESP_LOGE(TAG, "HTTP request body write failed after %zu/%zu bytes", written, len);
            return ESP_FAIL;
        }
        written += (size_t)result;
    }
    return ESP_OK;
}

esp_err_t camera_service_init(void) {
    camera_config_t config = {0};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = 11; config.pin_d1 = 9; config.pin_d2 = 8; config.pin_d3 = 10;
    config.pin_d4 = 12; config.pin_d5 = 18; config.pin_d6 = 17; config.pin_d7 = 16;
    config.pin_xclk = 15; config.pin_pclk = 13; config.pin_vsync = 6; config.pin_href = 7;
    config.pin_sccb_sda = 4; config.pin_sccb_scl = 5;
    config.pin_pwdn = -1; config.pin_reset = -1;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    // 💡 建議：先改為 QVGA 或 VGA 測試傳輸穩定性
    config.frame_size   = FRAMESIZE_VGA; 
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t ret = esp_camera_init(&config);
    return ret;
}

esp_err_t camera_service_take_and_send_telegram(const char *chat_id) {
    if (!chat_id || chat_id[0] == '\0') {
        ESP_LOGE(TAG, "無法傳送照片：缺少 Telegram chat_id");
        return ESP_ERR_INVALID_ARG;
    }

    // 1. 沖刷舊幀
    for (int i = 0; i < 2; i++) {
        camera_fb_t *temp_fb = esp_camera_fb_get();
        if (temp_fb) esp_camera_fb_return(temp_fb);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "影像抓取失敗");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "影像抓取成功 (%zu bytes)，準備上傳...", fb->len);

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", MIMI_SECRET_TG_TOKEN);

    const char *boundary = "----MimiClawBoundary7MA4YWxk";
    const char *footer = "\r\n------MimiClawBoundary7MA4YWxk--\r\n";
    char header[512];
    
    // 💡 使用你 Log 裡的真實 Chat ID
    snprintf(header, sizeof(header),
             "--%s\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n%s\r\n"
             "--%s\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"mimiclaw.jpg\"\r\n"
             "Content-Type: image/jpeg\r\n\r\n",
             boundary, chat_id, boundary);

    esp_http_client_config_t http_conf = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,          // 🚀 關鍵：增加到 30 秒，避免 Timeout
        .buffer_size = 4096,          // 增加緩衝區大小
        .keep_alive_enable = true,    // 保持連線
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_conf);
    if (!client) {
        ESP_LOGE(TAG, "❌ 無法建立 HTTP client");
        esp_camera_fb_return(fb);
        return ESP_ERR_NO_MEM;
    }

    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // 精確計算內容長度
    int total_len = strlen(header) + fb->len + strlen(footer);
    
    esp_err_t err = esp_http_client_open(client, total_len);
    if (err == ESP_OK) {
        if (http_write_all(client, header, strlen(header)) != ESP_OK ||
            http_write_all(client, fb->buf, fb->len) != ESP_OK ||
            http_write_all(client, footer, strlen(footer)) != ESP_OK) {
            err = ESP_FAIL;
        } else {
            int64_t content_length = esp_http_client_fetch_headers(client);
            if (content_length < 0) {
                ESP_LOGE(TAG, "❌ 讀取 Telegram HTTP headers 失敗");
                err = ESP_FAIL;
            } else {
                char response[512] = {0};
                int response_len = esp_http_client_read_response(client, response, sizeof(response) - 1);
                if (response_len < 0) {
                    response_len = 0;
                }
                response[response_len] = '\0';

                int status = esp_http_client_get_status_code(client);
                if (status == 200) {
                    ESP_LOGI(TAG, "✅ 照片已傳送到 Telegram chat_id=%s", chat_id);
                    err = ESP_OK;
                } else {
                    ESP_LOGE(TAG, "❌ Telegram sendPhoto 失敗，HTTP %d，回應: %.400s",
                             status, response_len > 0 ? response : "(empty)");
                    err = ESP_FAIL;
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "❌ 無法打開 HTTP 連線: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    esp_camera_fb_return(fb);
    return err;
}
