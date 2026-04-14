#include "ili9341.h"

// 🚀 強制綁定你的硬體腳位！(覆蓋掉原本依賴 mimi_config.h 的設定)
#undef MIMI_ILI_MOSI_PIN
#undef MIMI_ILI_SCLK_PIN
#undef MIMI_ILI_CS_PIN
#undef MIMI_ILI_DC_PIN
#undef MIMI_ILI_RST_PIN
#undef MIMI_ILI_BL_PIN
#define MIMI_ILI_MOSI_PIN 45
#define MIMI_ILI_SCLK_PIN 3
#define MIMI_ILI_CS_PIN   14
#define MIMI_ILI_DC_PIN   47
#define MIMI_ILI_RST_PIN  21
#define MIMI_ILI_BL_PIN   0
#define MIMI_ILI_SPI_HZ   40000000 

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ili9341";
static spi_device_handle_t s_spi;

static void write_cmd(uint8_t cmd) {
    gpio_set_level(MIMI_ILI_DC_PIN, 0);
    spi_transaction_t t = { .length = 8, .tx_data = {cmd}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(MIMI_ILI_DC_PIN, 1);
}

static void write_data8(uint8_t d) {
    spi_transaction_t t = { .length = 8, .tx_data = {d}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(s_spi, &t);
}

esp_err_t ili9341_init(void) {
    uint64_t gpio_mask = (1ULL << MIMI_ILI_DC_PIN) | (1ULL << MIMI_ILI_RST_PIN) | (1ULL << MIMI_ILI_BL_PIN);
    gpio_config_t gc = { .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = gpio_mask };
    gpio_config(&gc);

    gpio_set_level(MIMI_ILI_DC_PIN,  1);
    gpio_set_level(MIMI_ILI_RST_PIN, 1);
    gpio_set_level(MIMI_ILI_BL_PIN, 0);

    spi_bus_config_t bus = {
        .mosi_io_num = MIMI_ILI_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = MIMI_ILI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ILI9341_WIDTH * 20 * 2 + 8,
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = MIMI_ILI_SPI_HZ,
        .mode = 0,
        .spics_io_num = MIMI_ILI_CS_PIN,
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev, &s_spi);

    gpio_set_level(MIMI_ILI_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(MIMI_ILI_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    write_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150));
    write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(0x3A); write_data8(0x55);
    write_cmd(0x36); write_data8(0x48);
    write_cmd(0xC0); write_data8(0x23);
    write_cmd(0xC1); write_data8(0x10);
    write_cmd(0xC5); write_data8(0x3E); write_data8(0x28);
    write_cmd(0xC7); write_data8(0x86);
    write_cmd(0xB1); write_data8(0x00); write_data8(0x18);
    write_cmd(0xB6); write_data8(0x08); write_data8(0x82); write_data8(0x27);
    
    write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(0x29); vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

void ili9341_set_window(int x0, int y0, int x1, int y1) {
    write_cmd(0x2A); write_data8(x0 >> 8); write_data8(x0 & 0xFF); write_data8(x1 >> 8); write_data8(x1 & 0xFF);
    write_cmd(0x2B); write_data8(y0 >> 8); write_data8(y0 & 0xFF); write_data8(y1 >> 8); write_data8(y1 & 0xFF);
    write_cmd(0x2C);
}

void ili9341_write_pixels(const uint16_t *data, uint32_t count) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t rem = count * 2;
    while (rem > 0) {
        uint32_t chunk = (rem > 4092) ? 4092 : rem;
        spi_transaction_t t = { .length = chunk * 8, .tx_buffer = p };
        spi_device_polling_transmit(s_spi, &t);
        p += chunk;
        rem -= chunk;
    }
}

void ili9341_set_backlight(bool on) {
    gpio_set_level(MIMI_ILI_BL_PIN, on ? 1 : 0);
}