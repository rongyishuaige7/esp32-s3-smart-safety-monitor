#pragma once

#include "esp_err.h"

esp_err_t sensor_mq2_init(void);
int sensor_mq2_read_raw(void);
