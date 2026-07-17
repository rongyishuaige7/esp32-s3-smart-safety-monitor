#pragma once

#include "esp_err.h"

esp_err_t sensor_ultrasonic_init(void);
/**
 * @brief 测距，单位 cm。超时或错误时返回负数。
 */
float sensor_ultrasonic_measure_cm(void);
