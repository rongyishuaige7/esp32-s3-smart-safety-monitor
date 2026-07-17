#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t motor_fan_init(void);
void motor_fan_set(bool on);
