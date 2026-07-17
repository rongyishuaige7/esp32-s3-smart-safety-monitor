#include "web_server.h"
#include "app_config.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>

#if __has_include("local_config.h")
#include "local_config.h"
#else
#include "local_config.example.h"
#endif

static const char *TAG = "web";

/* 内嵌 HTML：构建时由 embed_web.py 生成（见 CMakeLists.txt / PlatformIO 脚本） */
extern const uint8_t index_html_start[];
extern const uint32_t index_html_len;

/* -------------------------------------------------------------------------- */
/*  GET /  — 根路径返回页面                                                   */
/* -------------------------------------------------------------------------- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t len = (size_t)index_html_len;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)index_html_start, (ssize_t)len);
}

/* -------------------------------------------------------------------------- */
/*  GET /api/status  — 服务端推送（SSE）                                      */
/* -------------------------------------------------------------------------- */
static esp_err_t status_sse_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char buf[512];

    /* 客户端断开或发送失败则退出循环 */
    while (true) {
        app_state_lock();
        app_state_t snap = g_app_state;
        app_state_unlock();

        const char *state_str;
        switch (snap.ui_state) {
            case UI_STATE_GAS:   state_str = "GAS";   break;
            case UI_STATE_SMOKE: state_str = "SMOKE"; break;
            case UI_STATE_FIRE:  state_str = "FIRE";  break;
            default:             state_str = "SAFE";  break;
        }

        int n = snprintf(buf, sizeof(buf),
            "data: {"
            "\"state\":\"%s\","
            "\"mq2\":%d,"
            "\"mq5\":%d,"
            "\"flame\":%s,"
            "\"dist\":%.1f,"
            "\"smoke_level\":%d,"
            "\"gas_danger\":%s,"
            "\"buzzer\":%s,"
            "\"led\":%s,"
            "\"pump\":%s,"
            "\"fan\":%s,"
            "\"window\":%s"
            "}\n\n",
            state_str,
            snap.mq2_raw,
            snap.mq5_raw,
            snap.flame       ? "true" : "false",
            snap.distance_cm > 0.f ? (double)snap.distance_cm : 0.0,
            (int)snap.smoke_level,
            snap.gas_danger  ? "true" : "false",
            snap.buzzer_on   ? "true" : "false",
            snap.led_on      ? "true" : "false",
            snap.pump_on     ? "true" : "false",
            snap.fan_on      ? "true" : "false",
            snap.window_open ? "true" : "false"
        );

        if (n <= 0 || n >= (int)sizeof(buf)) {
            break;
        }

        esp_err_t err = httpd_resp_send_chunk(req, buf, n);
        if (err != ESP_OK) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  浏览器自动请求图标等：返回 204，减少无意义 404 日志                         */
/* -------------------------------------------------------------------------- */
static esp_err_t not_found_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  HTTP 服务器                                                               */
/* -------------------------------------------------------------------------- */
static void start_http_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size      = 8192;
    cfg.max_open_sockets   = 7;   /* 页面、SSE、favicon 等可同时连接 */
    cfg.recv_wait_timeout  = 5;   /* 秒：普通请求接收超时 */
    cfg.send_wait_timeout  = 60;  /* 秒：SSE 长连接需较长发送超时 */
    cfg.lru_purge_enable   = true; /* 连接数满时淘汰最久未用的空闲连接 */

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }

    httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = NULL,
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t api_status = {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = status_sse_handler,
        .user_ctx  = NULL,
    };
    httpd_register_uri_handler(server, &api_status);

    /* 对常见图标路径返回 204 */
    static const char *const stub_uris[] = {
        "/favicon.ico",
        "/apple-touch-icon.png",
        "/apple-touch-icon-precomposed.png",
    };
    static const httpd_uri_t stub = {
        .method   = HTTP_GET,
        .handler  = not_found_handler,
        .user_ctx = NULL,
    };
    for (int i = 0; i < 3; i++) {
        httpd_uri_t u = stub;
        u.uri = stub_uris[i];
        httpd_register_uri_handler(server, &u);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", cfg.server_port);
}

/* -------------------------------------------------------------------------- */
/*  WiFi SoftAP（热点）                                                       */
/* -------------------------------------------------------------------------- */
static bool web_server_has_local_network_config(void)
{
    return strlen(SAFETY_MONITOR_AP_SSID) > 0 &&
           strlen(SAFETY_MONITOR_AP_PASSWORD) >= 8;
}

static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = SAFETY_MONITOR_AP_SSID,
            .ssid_len       = (uint8_t)strlen(SAFETY_MONITOR_AP_SSID),
            .channel        = SAFETY_MONITOR_AP_CHANNEL,
            .password       = SAFETY_MONITOR_AP_PASSWORD,
            .max_connection = SAFETY_MONITOR_AP_MAX_STA,
            .authmode       = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started (credentials suppressed; local IP is ESP-IDF default)");
}

/* -------------------------------------------------------------------------- */
/*  对外入口                                                                  */
/* -------------------------------------------------------------------------- */
esp_err_t web_server_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if (!web_server_has_local_network_config()) {
        ESP_LOGW(TAG, "local_config.h is missing or incomplete; local HTTP status page is disabled");
        return ESP_OK;
    }

    wifi_init_softap();
    start_http_server();
    return ESP_OK;
}
