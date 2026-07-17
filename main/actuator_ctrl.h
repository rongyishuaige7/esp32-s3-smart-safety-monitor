#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t actuator_ctrl_init(void);

void actuator_buzzer_set(bool on);
void actuator_led_set(bool on);
void actuator_pump_set(bool on);       /* INB=1 → 出水（报警灭火方向） */
void actuator_pump_set_drain(bool on); /* INA=1 → 抽水（自检/排水方向） */
