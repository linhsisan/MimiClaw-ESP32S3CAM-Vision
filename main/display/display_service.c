// 🚀 強制開啟 ILI9341 螢幕與中文字型支援
#define CONFIG_MIMI_DISPLAY_ILI9341 1
#define CONFIG_MIMI_DISPLAY_FONT_CJK 1

#include "display_service.h"
#include "lvgl.h"
#include "source-han-sans16.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#ifdef CONFIG_USE_ST7789
    #include "ST7789.h"
    #define DISP_INIT()          st7789_init()
    #define DISP_SET_WINDOW      st7789_set_window
    #define DISP_WRITE_PIXELS    st7789_write_pixels
    #define DISP_SET_BACKLIGHT   st7789_set_backlight  
    #define DISP_WIDTH           240
    #define DISP_HEIGHT          320
#else
    #include "ili9341.h"
    #define DISP_INIT()          ili9341_init()
    #define DISP_SET_WINDOW      ili9341_set_window
    #define DISP_WRITE_PIXELS    ili9341_write_pixels
    #define DISP_SET_BACKLIGHT   ili9341_set_backlight 
    #define DISP_WIDTH           240
    #define DISP_HEIGHT          320
#endif

static SemaphoreHandle_t     s_mutex;

// 🎨 UI 佈局參數
#define STATUS_HEIGHT   48   
#define DIVIDER_HEIGHT  8    
#define CHAT_Y          (STATUS_HEIGHT + DIVIDER_HEIGHT)  
#define CHAT_HEIGHT     (DISP_HEIGHT - CHAT_Y) 
#define MAX_BUBBLES     12   
#define BUBBLE_MAX_W    (DISP_WIDTH * 92 / 100)    
#define BUBBLE_PAD      6                             
#define BUBBLE_MARGIN   4                             

#define LVGL_BUF_LINES 40
static lv_disp_draw_buf_t  s_draw_buf;
static lv_color_t          s_lvgl_buf[DISP_WIDTH * LVGL_BUF_LINES];
static lv_disp_drv_t       s_disp_drv;
static SemaphoreHandle_t   s_lvgl_mutex;

static lv_obj_t *s_lbl_status, *s_lbl_model; 
static lv_obj_t *s_chat_cont;
static const lv_font_t *s_font_cn;
static const lv_font_t *s_font_en;
static lv_font_t s_cjk_font;

// 🎬 [純粹電子書翻頁引擎] 徹底消滅撕裂感
static lv_timer_t *s_scroll_timer = NULL;
static int s_scroll_target_y = 0;
static int s_page_wait_time = 0;

static void custom_scroll_timer_cb(lv_timer_t *t) {
    if (!s_chat_cont) return;

    // 靜止閱讀時間計數器
    if (s_page_wait_time > 0) {
        s_page_wait_time--;
        return;
    }

    lv_coord_t current_y = lv_obj_get_scroll_y(s_chat_cont);
    
    // 如果還沒到達文章最底部
    if (current_y < s_scroll_target_y) {
        lv_coord_t view_h = lv_obj_get_height(s_chat_cont);
        
        // 每次翻動的距離：一整頁的高度，扣掉 40 像素 (保留上一頁最後當提示)
        lv_coord_t step = view_h - 40; 
        lv_coord_t next_y = current_y + step;
        
        if (next_y > s_scroll_target_y) {
            next_y = s_scroll_target_y; // 確保不會捲過頭
        }
        
        // 🚀 【終極防海浪魔法】：強制使用 LV_ANIM_OFF (關閉動畫)
        // 這會讓畫面在一瞬間完成切換，SPI 匯流排只需要送一次畫面，絕不撕裂！
        lv_obj_scroll_to_y(s_chat_cont, next_y, LV_ANIM_OFF);
        
        // 翻頁完成後，強制定格靜止 4 秒 (計時器 100ms * 40次)
        s_page_wait_time = 40; 
    } else {
        lv_timer_pause(s_scroll_timer); // 到底自動停止
    }
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t pixels = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    DISP_SET_WINDOW(area->x1, area->y1, area->x2, area->y2);
    uint16_t *buf = (uint16_t *)color_p;
    
    for (uint32_t i = 0; i < pixels; i++) {
        buf[i] = (uint16_t)((buf[i] >> 8) | (buf[i] << 8));
    }
    DISP_WRITE_PIXELS((const uint16_t *)color_p, pixels);
    lv_disp_flush_ready(drv);
}

static void lvgl_task(void *arg) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        TickType_t now = xTaskGetTickCount();
        xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
        lv_tick_inc((now - last) * portTICK_PERIOD_MS);
        last = now;
        lv_timer_handler();
        xSemaphoreGive(s_lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static bool lvgl_lock(uint32_t timeout_ms) {
    TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    return xSemaphoreTake(s_lvgl_mutex, ticks) == pdTRUE;
}
static void lvgl_unlock(void) { xSemaphoreGive(s_lvgl_mutex); }

static void create_ui(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(0x1A, 0x1A, 0x2E), 0);

    s_font_en = LV_FONT_DEFAULT;
    s_cjk_font = source_han_sans16;
    s_cjk_font.fallback = s_font_en;
    s_font_cn = &s_cjk_font;

    lv_obj_t *status_bg = lv_obj_create(scr);
    lv_obj_set_size(status_bg, DISP_WIDTH, STATUS_HEIGHT);
    lv_obj_set_pos(status_bg, 0, 0); 
    lv_obj_set_style_bg_color(status_bg, lv_color_make(0x1A, 0x1A, 0x2E), 0);
    lv_obj_set_style_border_width(status_bg, 0, 0);
    lv_obj_clear_flag(status_bg, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_status, s_font_cn, 0);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_status, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_status, 5, 4); 
    lv_label_set_text(s_lbl_status, "系統啟動中...");

    s_lbl_model = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_model, lv_color_make(0x00, 0xFF, 0xFF), 0); 
    lv_obj_set_style_text_font(s_lbl_model, s_font_cn, 0);
    lv_label_set_long_mode(s_lbl_model, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_model, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_model, 5, 24);
    lv_label_set_text(s_lbl_model, "");

    s_chat_cont = lv_obj_create(scr);
    lv_obj_set_size(s_chat_cont, DISP_WIDTH, CHAT_HEIGHT);
    lv_obj_set_pos(s_chat_cont, 0, CHAT_Y);
    lv_obj_set_style_bg_color(s_chat_cont, lv_color_make(0x0F, 0x14, 0x21), 0); 
    lv_obj_set_style_border_width(s_chat_cont, 0, 0);
    
    lv_obj_set_style_pad_all(s_chat_cont, BUBBLE_MARGIN, 0);
    lv_obj_set_style_pad_row(s_chat_cont, 8, 0); 
    
    lv_obj_set_flex_flow(s_chat_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_chat_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_chat_cont, LV_SCROLLBAR_MODE_OFF);
}

esp_err_t display_service_init(void) {
    esp_err_t err = DISP_INIT();
    if (err != ESP_OK) return err;

    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf, NULL, DISP_WIDTH * LVGL_BUF_LINES);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.hor_res  = DISP_WIDTH;
    s_disp_drv.ver_res  = DISP_HEIGHT;
    lv_disp_drv_register(&s_disp_drv);

    s_mutex      = xSemaphoreCreateMutex();
    s_lvgl_mutex = xSemaphoreCreateMutex();

    if (lvgl_lock(0)) {
        create_ui();
        lvgl_unlock();
    }

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 1);
    DISP_SET_BACKLIGHT(true);
    return ESP_OK;
}

void display_service_show_wifi_connecting(const char *ssid) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0xE8, 0xE8, 0xE8), 0); 
        lv_label_set_text(s_lbl_status, "MiniClaw 連線中...");
        lv_label_set_text_fmt(s_lbl_model, "AP: %s", ssid ? ssid : "?");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_ok(const char *ip) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0x55, 0xFF, 0x55), 0); 
        lv_label_set_text(s_lbl_status, "MiniClaw Online");
        lv_label_set_text(s_lbl_model, "正在載入模型...");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_fail(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0xFF, 0x55, 0x55), 0); 
        lv_label_set_text(s_lbl_status, "WiFi 連線失敗");
        lv_label_set_text(s_lbl_model, "請檢查網路設定");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_telegram_fail(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0xFF, 0x55, 0x55), 0); 
        lv_label_set_text(s_lbl_status, "Telegram 連線失敗");
        lv_label_set_text(s_lbl_model, "請檢查 Bot Token");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_error(const char *error_msg) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0xFF, 0x55, 0x55), 0); 
        if (error_msg) {
            lv_label_set_text_fmt(s_lbl_status, "%s 連線失敗", error_msg);
        } else {
            lv_label_set_text(s_lbl_status, "系統連線失敗");
        }
        lv_label_set_text(s_lbl_model, "等待重試...");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_ready(const char *ip, const char *provider, const char *model) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lv_obj_set_style_text_color(s_lbl_status, lv_color_make(0x55, 0xFF, 0x55), 0); 
        lv_label_set_text(s_lbl_status, "MiniClaw Online");
        
        if (model) {
            lv_label_set_text_fmt(s_lbl_model, "Model: %s", model);
        } else {
            lv_label_set_text(s_lbl_model, "Model: Ready");
        }
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_thinking(const char *channel) {}
void display_service_show_message(const char *channel, const char *preview) {}

void display_service_push_chat(const char *role, const char *content) {
    if (!role || !content || content[0] == '\0') return;

    bool is_user      = (strcmp(role, "user")      == 0);
    bool is_assistant = (strcmp(role, "assistant") == 0);
    bool is_system    = (strcmp(role, "system")    == 0);

    if (!is_user && !is_assistant && !is_system) return;
    if (!lvgl_lock(200)) return;

    if (s_scroll_timer) {
        lv_timer_pause(s_scroll_timer);
    }

    // 完美座標補償
    while (lv_obj_get_child_cnt(s_chat_cont) >= MAX_BUBBLES) {
        lv_obj_update_layout(s_chat_cont);
        lv_obj_t *first = lv_obj_get_child(s_chat_cont, 0);
        lv_obj_t *second = lv_obj_get_child(s_chat_cont, 1);

        if (first && second) {
            lv_coord_t shift_y = lv_obj_get_y(second) - lv_obj_get_y(first);
            lv_coord_t current_scroll = lv_obj_get_scroll_y(s_chat_cont);
            lv_coord_t new_scroll = current_scroll - shift_y;
            if (new_scroll < 0) new_scroll = 0;

            lv_obj_del(first);
            lv_obj_scroll_to_y(s_chat_cont, new_scroll, LV_ANIM_OFF); 
        } else if (first) {
            lv_obj_del(first);
        }
    }

    lv_obj_t *row = lv_obj_create(s_chat_cont);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, BUBBLE_PAD, 0);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(bubble);
    lv_obj_set_style_text_font(lbl, s_font_cn, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    
    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_label_set_text(lbl, content);
    lv_obj_update_layout(lbl); 

    lv_coord_t max_lbl_w = BUBBLE_MAX_W - (BUBBLE_PAD * 2);
    if (lv_obj_get_width(lbl) > max_lbl_w) {
        lv_obj_set_width(lbl, max_lbl_w); 
    }

    lv_obj_set_size(bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_update_layout(lbl);
    lv_obj_update_layout(bubble);
    
    lv_coord_t final_bubble_h = lv_obj_get_height(bubble);
    lv_obj_set_height(row, final_bubble_h);

    if (is_user) {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x5B, 0x6B, 0xE8), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), 0);
        lv_obj_align(bubble, LV_ALIGN_TOP_RIGHT, 0, 0);
    } else if (is_assistant) {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x1E, 0x2C, 0x3A), 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xE8, 0xE8, 0xE8), 0);
        lv_obj_align(bubble, LV_ALIGN_TOP_LEFT, 0, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x2A, 0x3A, 0x4A), 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xAA, 0xBB, 0xCC), 0);
        lv_obj_align(bubble, LV_ALIGN_TOP_MID, 0, 0);
    }

    lv_obj_update_layout(row); 
    lv_obj_update_layout(s_chat_cont); 
    
    lv_coord_t max_scroll_y = lv_obj_get_scroll_y(s_chat_cont) + lv_obj_get_scroll_bottom(s_chat_cont);
    s_scroll_target_y = max_scroll_y;

    if (is_user) {
        lv_obj_scroll_to_y(s_chat_cont, s_scroll_target_y, LV_ANIM_OFF);
    } else {
        if (s_scroll_timer == NULL) {
            // 每 100ms 檢查一次狀態
            s_scroll_timer = lv_timer_create(custom_scroll_timer_cb, 100, NULL);
        }
        // 第一頁先等你 4 秒 (40 次 * 100ms) 再往下跳
        s_page_wait_time = 40; 
        lv_timer_resume(s_scroll_timer);
    }

    lvgl_unlock();
}