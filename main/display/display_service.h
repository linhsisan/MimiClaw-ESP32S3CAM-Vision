#pragma once

#include "esp_err.h"

/* Initialise the SSD1306 and show the boot screen.
 * Returns ESP_ERR_NOT_FOUND if no display is connected (non-fatal). */
esp_err_t display_service_init(void);

/* ── Status screens ─────────────────────────────────────────────── */

/* "WiFi Connecting…" with SSID */
void display_service_show_wifi_connecting(const char *ssid);

/* "Connected" with IP address */
void display_service_show_wifi_ok(const char *ip);

/* "WiFi Failed" */
void display_service_show_wifi_fail(void);

/* All services up — idle home screen showing provider, model, and IP */
void display_service_show_ready(const char *ip, const char *provider, const char *model);

/* ── Activity screens ───────────────────────────────────────────── */

/* Animated "Thinking…" dots; call display_service_show_ready() to stop.
 * channel: e.g. "telegram", "feishu", "ws" */
void display_service_show_thinking(const char *channel);

/* Show first line of an incoming or outgoing message. */
void display_service_show_message(const char *channel, const char *preview);

/* ── Chat bubbles (RM68140 only) ────────────────────────────────── */

/* Push a chat message bubble into the scrollable chat area.
 * role: "user" | "assistant" | "system"
 * content: UTF-8 text (supports Chinese via Source Han Sans 16) */
void display_service_push_chat(const char *role, const char *content);
