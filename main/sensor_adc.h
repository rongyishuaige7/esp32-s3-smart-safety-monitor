#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/** 共享 ADC1 oneshot，用于 MQ-2 / MQ-5 模拟引脚。 */
esp_err_t sensor_adc_init(void);
void sensor_adc_deinit(void);
adc_oneshot_unit_handle_t sensor_adc_get_handle(void);
