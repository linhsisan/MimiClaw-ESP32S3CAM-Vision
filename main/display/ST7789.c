#include "ST7789.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 腳位綁定
#define PIN_MOSI 45
#define PIN_SCLK 3
#define PIN_CS   14
#define PIN_DC   47
#define PIN_RST  21
#define PIN_BL   0
#define SPI_HZ   40000000 

static const char *TAG = "st7789";
static spi_device_handle_t s_spi_st7789;

static void st7789_write_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_data = {cmd}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(s_spi_st7789, &t);
    gpio_set_level(PIN_DC, 1);
}

static void st7789_write_data(uint8_t d) {
    spi_transaction_t t = { .length = 8, .tx_data = {d}, .flags = SPI_TRANS_USE_TXDATA };
    spi_device_polling_transmit(s_spi_st7789, &t);
}

esp_err_t st7789_init(void) {
    // 1. GPIO 初始化
    uint64_t gpio_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_BL);
    gpio_config_t gc = { .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = gpio_mask };
    gpio_config(&gc);

    // 2. SPI 初始化 (直屏 240x320)
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI, .miso_io_num = -1, .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 240 * 20 * 2 + 8,
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ, .mode = 0, .spics_io_num = PIN_CS, .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev, &s_spi_st7789);

    // 3. 硬體復位
    gpio_set_level(PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(150));

    // 4. ST7789 初始化指令
    st7789_write_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150)); // Reset
    st7789_write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); // Sleep Out

    // ✨ 回歸直向 (0x00)
    st7789_write_cmd(0x36); st7789_write_data(0x00); 
    st7789_write_cmd(0x3A); st7789_write_data(0x05); // 16-bit
    st7789_write_cmd(0x21); // 開啟顏色反轉

    st7789_write_cmd(0xB2); st7789_write_data(0x0C); st7789_write_data(0x0C);
    st7789_write_cmd(0xB7); st7789_write_data(0x35);
    st7789_write_cmd(0xBB); st7789_write_data(0x19);
    st7789_write_cmd(0xC0); st7789_write_data(0x2C);

    st7789_write_cmd(0x29); // Display ON
    
    ESP_LOGI(TAG, "\033[0;32m[OK] ST7789 DRIVER LOADED: 240x320 PORTRAIT\033[0m");
    return ESP_OK;
}

void st7789_set_window(int x0, int y0, int x1, int y1) {
    st7789_write_cmd(0x2A); 
    st7789_write_data(x0 >> 8); st7789_write_data(x0 & 0xFF);
    st7789_write_data(x1 >> 8); st7789_write_data(x1 & 0xFF);
    
    st7789_write_cmd(0x2B); 
    st7789_write_data(y0 >> 8); st7789_write_data(y0 & 0xFF);
    st7789_write_data(y1 >> 8); st7789_write_data(y1 & 0xFF);
    
    st7789_write_cmd(0x2C);
}

void st7789_write_pixels(const uint16_t *data, uint32_t count) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t rem = count * 2;
    while (rem > 0) {
        uint32_t chunk = (rem > 4092) ? 4092 : rem;
        spi_transaction_t t = { .length = chunk * 8, .tx_buffer = p };
        spi_device_polling_transmit(s_spi_st7789, &t);
        p += chunk; rem -= chunk;
    }
}

void st7789_set_backlight(bool on) {
    gpio_set_level(PIN_BL, on ? 1 : 0);
}