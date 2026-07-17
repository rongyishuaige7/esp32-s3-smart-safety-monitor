/**
 * @file local_config.example.h
 * @brief 公开安全默认值。复制为 local_config.h 后仅在本机填写测试热点信息。
 *
 * local_config.h 被 .gitignore 忽略，且不能用于公网或生产部署。
 */
#pragma once

#define SAFETY_MONITOR_AP_SSID ""
#define SAFETY_MONITOR_AP_PASSWORD ""
#define SAFETY_MONITOR_AP_CHANNEL 6
#define SAFETY_MONITOR_AP_MAX_STA 4
