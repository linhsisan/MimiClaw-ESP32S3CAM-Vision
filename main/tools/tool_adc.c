#include "esp_adc/adc_oneshot.h"
#include <stdio.h>

esp_err_t tool_adc_read_execute(const char *input_json, char *output, size_t output_size) {
    adc_oneshot_unit_handle_t handle;
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &handle);

    // S3 的 GPIO 1 對應 ADC1_CHANNEL_0
    // atten = ADC_ATTEN_DB_12 是關鍵，這讓它能量測 0 到 3.3V 的範圍
    adc_oneshot_chan_cfg_t config = { .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12 };
    adc_oneshot_config_channel(handle, ADC_CHANNEL_0, &config);

    int val = 0;
    adc_oneshot_read(handle, ADC_CHANNEL_0, &val); // 這裡讀到的是 0 到 4095
    adc_oneshot_del_unit(handle);

    // 我們把輸出格式化成「RAW_VALUE: 數字」，讓 AI 知道這不是 0/1
    snprintf(output, output_size, "GPIO 1 類比數值: %d", val);
    return 0;
}