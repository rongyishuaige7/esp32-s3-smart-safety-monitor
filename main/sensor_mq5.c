#include "sensor_mq5.h"
#include "sensor_adc.h"
#include "esp_log.h"

static const char *TAG = "mq5";

esp_err_t sensor_mq5_init(void)
{
    (void)TAG;
    return ESP_OK;
}

int sensor_mq5_read_raw(void)
{
    adc_oneshot_unit_handle_t adc = sensor_adc_get_handle();
    if (!adc) {
        return -1;
    }
    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc, ADC_CHANNEL_1, &raw);
    if (err != ESP_OK) {
        return -1;
    }
    return raw;
}
