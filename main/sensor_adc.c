#include "sensor_adc.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "sensor_adc";

static adc_oneshot_unit_handle_t s_adc1 = NULL;

esp_err_t sensor_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };

    err = adc_oneshot_config_channel(s_adc1, ADC_CHANNEL_0, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config CH0: %s", esp_err_to_name(err));
        return err;
    }
    err = adc_oneshot_config_channel(s_adc1, ADC_CHANNEL_1, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config CH1: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ADC1 ready (MQ-2=CH0/GPIO1, MQ-5=CH1/GPIO2)");
    return ESP_OK;
}

void sensor_adc_deinit(void)
{
    if (s_adc1) {
        adc_oneshot_del_unit(s_adc1);
        s_adc1 = NULL;
    }
}

adc_oneshot_unit_handle_t sensor_adc_get_handle(void)
{
    return s_adc1;
}
