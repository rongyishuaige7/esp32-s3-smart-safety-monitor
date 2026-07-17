#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* ---------- 舵机1：窗户开关 ---------- */
esp_err_t servo_window_init(void);
/** @param open true 开窗约 90°，false 关窗约 0°。 */
void servo_window_set_open(bool open);

/* ---------- 舵机2：雷达扫描（挂载超声波传感器） ---------- */
esp_err_t servo_radar_init(void);
/** @param deg 目标角度，0～180°。 */
void servo_radar_set_angle(float deg);
