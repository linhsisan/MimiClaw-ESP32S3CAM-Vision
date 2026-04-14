#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>


#define ILI9341_WIDTH   240
#define ILI9341_HEIGHT  320

/* Initialise SPI bus, GPIO pins, hardware-reset the panel, and send the
 * ILI9341 init sequence.  Must be called once before any other ili9341_* fn. */
esp_err_t ili9341_init(void);

/* Set the pixel write window (inclusive) and issue RAMWR.
 * Subsequent ili9341_write_pixels() calls fill this rectangle. */
void ili9341_set_window(int x0, int y0, int x1, int y1);

/* Write `count` RGB565 pixels (already byte-swapped for MSB-first SPI)
 * into the current window.  Caller must call ili9341_set_window() first. */
void ili9341_write_pixels(const uint16_t *data, uint32_t count);

/* Control backlight GPIO.  No-op if MIMI_ILI_BL_PIN == -1. */
void ili9341_set_backlight(bool on);
