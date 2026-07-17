#pragma once

#include "app_config.h"

/**
 * @brief 根据 @p s 中传感器读数计算等级，驱动执行器并更新 @p s 中的输出状态。
 */
void alarm_logic_apply(app_state_t *s);
