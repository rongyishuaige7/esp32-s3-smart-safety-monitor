#pragma once

#include "app_config.h"
#include "esp_err.h"

esp_err_t oled_display_init(void);
void oled_display_render(const app_state_t *s);

/** 显示两行文字（自检阶段、任务启动前使用）。 */
void oled_display_msg(const char *line1, const char *line2);
