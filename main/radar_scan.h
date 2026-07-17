#pragma once

#include "app_config.h"

/** 超声波采样 + 软件扫描角，供 OLED 雷达动画使用。 */
void radar_scan_tick(app_state_t *s);
