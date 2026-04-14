// 🚀 強制開啟 ILI9341 螢幕與中文字型支援
#define CONFIG_MIMI_DISPLAY_ILI9341 1
#define CONFIG_MIMI_DISPLAY_FONT_CJK 1

#include "display_service.h"
#include "lvgl.h"
#include "source-han-sans16.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

// ✨ 正確的判斷區塊
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
static char                  s_ip[16];

#define BANNER_HEIGHT   24   
#define STATUS_HEIGHT   84   
#define DIVIDER_HEIGHT  12                            
#define CHAT_Y          (STATUS_HEIGHT + DIVIDER_HEIGHT)  
#define CHAT_HEIGHT     (DISP_HEIGHT - CHAT_Y)
#define MAX_BUBBLES     12                            
#define BUBBLE_MAX_W    (DISP_WIDTH * 92 / 100)    
#define BUBBLE_PAD      6                             
#define BUBBLE_MARGIN   4                             

#define LVGL_BUF_LINES 20
static lv_disp_draw_buf_t  s_draw_buf;
static lv_color_t          s_lvgl_buf[DISP_WIDTH * LVGL_BUF_LINES];
static lv_disp_drv_t       s_disp_drv;
static SemaphoreHandle_t   s_lvgl_mutex;

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
        vTaskDelay(2);
    }
}

static bool lvgl_lock(uint32_t timeout_ms) {
    TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    return xSemaphoreTake(s_lvgl_mutex, ticks) == pdTRUE;
}
static void lvgl_unlock(void) { xSemaphoreGive(s_lvgl_mutex); }

static lv_obj_t *s_lbl_a, *s_lbl_b, *s_lbl_c, *s_lbl_ip;
static lv_obj_t *s_chat_cont;
static const lv_font_t *s_font_cn;
static const lv_font_t *s_font_en;
static lv_font_t s_cjk_font;

static void create_ui(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(0x1A, 0x1A, 0x2E), 0);

    s_font_en = LV_FONT_DEFAULT;
    s_cjk_font = source_han_sans16;
    s_cjk_font.fallback = s_font_en;
    s_font_cn = &s_cjk_font;

    lv_obj_t *banner_bg = lv_obj_create(scr);
    lv_obj_set_size(banner_bg, DISP_WIDTH, BANNER_HEIGHT);
    lv_obj_set_pos(banner_bg, 0, 0);
    lv_obj_set_style_bg_color(banner_bg, lv_color_make(0x0D, 0x1B, 0x2E), 0);
    lv_obj_set_style_border_width(banner_bg, 0, 0);
    lv_obj_clear_flag(banner_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *banner_lbl = lv_label_create(banner_bg);
    lv_obj_set_style_text_color(banner_lbl, lv_color_make(0xFF, 0xFF, 0xFF), 0);
    lv_obj_set_style_text_font(banner_lbl, s_font_cn, 0);
    lv_label_set_text(banner_lbl, "MimiClaw");
    lv_obj_align(banner_lbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *status_bg = lv_obj_create(scr);
    lv_obj_set_size(status_bg, DISP_WIDTH, STATUS_HEIGHT - BANNER_HEIGHT);
    lv_obj_set_pos(status_bg, 0, BANNER_HEIGHT);
    lv_obj_set_style_bg_color(status_bg, lv_color_make(0x1A, 0x1A, 0x2E), 0);
    lv_obj_set_style_border_width(status_bg, 0, 0);
    lv_obj_clear_flag(status_bg, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_a = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_a, lv_color_make(0xE8, 0xE8, 0xE8), 0);
    lv_obj_set_style_text_font(s_lbl_a, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_a, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_a, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_a, 5, BANNER_HEIGHT + 4);
    lv_label_set_text(s_lbl_a, "");

    s_lbl_b = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_b, lv_color_make(0xE8, 0xE8, 0xE8), 0);
    lv_obj_set_style_text_font(s_lbl_b, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_b, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_b, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_b, 5, BANNER_HEIGHT + 22);
    lv_label_set_text(s_lbl_b, "");

    s_lbl_ip = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_ip, lv_color_make(0x7F, 0x8C, 0x9A), 0);
    lv_obj_set_style_text_font(s_lbl_ip, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_ip, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lbl_ip, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_ip, 5, BANNER_HEIGHT + 40);
    lv_label_set_text(s_lbl_ip, "");

    s_lbl_c = lv_label_create(scr);
    lv_obj_add_flag(s_lbl_c, LV_OBJ_FLAG_HIDDEN);

    s_chat_cont = lv_obj_create(scr);
    lv_obj_set_size(s_chat_cont, DISP_WIDTH, CHAT_HEIGHT);
    lv_obj_set_pos(s_chat_cont, 0, CHAT_Y);
    lv_obj_set_style_bg_color(s_chat_cont, lv_color_make(0x0F, 0x14, 0x21), 0); 
    lv_obj_set_style_border_width(s_chat_cont, 0, 0);
    lv_obj_set_style_pad_all(s_chat_cont, BUBBLE_MARGIN, 0);
    lv_obj_set_flex_flow(s_chat_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_chat_cont, LV_SCROLLBAR_MODE_OFF);
}

static void lvgl_clear_status(void) {
    lv_label_set_text(s_lbl_a, "");
    lv_label_set_text(s_lbl_b, "");
    lv_label_set_text(s_lbl_c, "");
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
        lv_label_set_text(s_lbl_a, "Booting...");
        lvgl_unlock();
    }

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 1);
    DISP_SET_BACKLIGHT(true);
    return ESP_OK;
}

void display_service_show_wifi_connecting(const char *ssid) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi connecting...");
        lv_label_set_text(s_lbl_b, ssid ? ssid : "?");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_ok(const char *ip) {
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi connected!");
        lv_label_set_text(s_lbl_ip, s_ip);
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_fail(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi failed!");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_ready(const char *ip, const char *provider, const char *model) {
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, provider ? provider : "");
        lv_label_set_text(s_lbl_b, model ? model : "");
        lv_label_set_text(s_lbl_ip, s_ip);
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_thinking(const char *channel) {}
void display_service_show_message(const char *channel, const char *preview) {}

void display_service_push_chat(const char *role, const char *content) {
    if (!role || !content || content[0] == '\0') return;

    bool is_user      = (strcmp(role, "user")      == 0);
    bool is_assistant = (strcmp(role, "assistant")  == 0);
    bool is_system    = (strcmp(role, "system")     == 0);

    if (!is_user && !is_assistant && !is_system) return;
    if (!lvgl_lock(200)) return;

    uint32_t cnt = lv_obj_get_child_cnt(s_chat_cont);
    if (cnt >= MAX_BUBBLES) {
        lv_obj_t *oldest = lv_obj_get_child(s_chat_cont, 0);
        if (oldest) lv_obj_del(oldest);
    }

    lv_obj_t *row = lv_obj_create(s_chat_cont);
    lv_obj_set_width(row, DISP_WIDTH - BUBBLE_MARGIN * 2);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, BUBBLE_PAD, 0);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    lv_obj_t *lbl = lv_label_create(bubble);
    lv_obj_set_style_text_font(lbl, s_font_cn, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl, content);

    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_obj_update_layout(lbl);
    lv_coord_t tw = lv_obj_get_width(lbl);
    lv_coord_t bw = (tw < BUBBLE_MAX_W) ? tw : BUBBLE_MAX_W;
    lv_obj_set_width(lbl, bw);
    lv_obj_set_width(bubble, bw + BUBBLE_PAD * 2);

    if (is_user) {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x5B, 0x6B, 0xE8), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, -4, 0);
    } else if (is_assistant) {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x1E, 0x2C, 0x3A), 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xE8, 0xE8, 0xE8), 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 4, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x2A, 0x3A, 0x4A), 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xAA, 0xBB, 0xCC), 0);
        lv_obj_align(bubble, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_scroll_to_view_recursive(row, LV_ANIM_ON);
    lvgl_unlock();
}