#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t sensor_flame_init(void);
/** @return 检测到火焰为 true（多数模块 DO 为低电平有效）。 */
bool sensor_flame_read(void);
