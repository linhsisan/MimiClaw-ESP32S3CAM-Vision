#ifndef CAMERA_SERVICE_H
#define CAMERA_SERVICE_H

#include "esp_err.h"

esp_err_t camera_service_init(void);
esp_err_t camera_service_take_and_send_telegram(const char *chat_id);

#endif
