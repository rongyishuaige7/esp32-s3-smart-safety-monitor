#pragma once

#include "esp_err.h"

esp_err_t sensor_mq5_init(void);
int sensor_mq5_read_raw(void);
