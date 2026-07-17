#pragma once

#include "esp_err.h"

/**
 * @brief 启动本地 Wi-Fi SoftAP 与 HTTP 状态界面。
 *
 * 公开源码不包含固定密码：热点名称和密码只从本地、被 Git 忽略的
 * local_config.h 读取。没有本地配置时，网络服务不会启动。
 * HTTP：
 *   GET /          → 内嵌的 index.html
 *   GET /api/status → 服务端推送（SSE），约 1 秒一条 JSON
 */
esp_err_t web_server_start(void);
