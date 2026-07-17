/**
 * @file main.c
 * @brief ESP32-S3 智能安全监测：FreeRTOS 任务、共享状态、启动流程。
 */
#include "app_config.h"
#include "sensor_adc.h"
#include "sensor_mq2.h"
#include "sensor_mq5.h"
#include "sensor_flame.h"
#include "sensor_ultrasonic.h"
#include "actuator_ctrl.h"
#include "servo_ctrl.h"
#include "motor_ctrl.h"
#include "alarm_logic.h"
#include "oled_display.h"
#include "radar_scan.h"
#include "web_server.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "main";

app_state_t g_app_state;
SemaphoreHandle_t g_app_state_mutex;

/* ------------------------------------------------------------------ */
/*  自检：在创建 FreeRTOS 任务之前执行一次。
 * 会顺序驱动 LED、蜂鸣器、舵机、风扇与水泵；首次上电前务必断开危险负载
 * 或使用受控低压测试条件。 */
/* ------------------------------------------------------------------ */
static void self_test(void)
{
    ESP_LOGI(TAG, "=============================");
    ESP_LOGI(TAG, " ESP32-S3 Self-Test Starting");
    ESP_LOGI(TAG, "=============================");
    oled_display_msg("SELF TEST", "Starting...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* --- 1. LED 闪烁 3 次 --- */
    ESP_LOGI(TAG, "[TEST 1] LED blink x3");
    oled_display_msg("[1] LED", "Blink x3");
    for (int i = 0; i < 3; i++) {
        actuator_led_set(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        actuator_led_set(false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "  -> OK");

    /* --- 2. 蜂鸣器 0.5 秒 --- */
    ESP_LOGI(TAG, "[TEST 2] Buzzer 0.5s");
    oled_display_msg("[2] Buzzer", "Beep 0.5s");
    actuator_buzzer_set(true);
    vTaskDelay(pdMS_TO_TICKS(500));
    actuator_buzzer_set(false);
    ESP_LOGI(TAG, "  -> OK");

    /* --- 3. 舵机 开窗/关窗（慢速） --- */
    ESP_LOGI(TAG, "[TEST 3] Servo sweep 0->90->0");
    oled_display_msg("[3] Servo", "Sweeping...");
    servo_window_set_open(true);
    vTaskDelay(pdMS_TO_TICKS(800));
    servo_window_set_open(false);
    vTaskDelay(pdMS_TO_TICKS(800));
    ESP_LOGI(TAG, "  -> sweep done");

    /* --- 3b. 雷达舵机 0->90->180->0 扫一轮 --- */
    ESP_LOGI(TAG, "[TEST 3b] Radar servo 0->180->0");
    oled_display_msg("[3b] Radar", "0->180->0...");
    for (int a = 0; a <= 180; a += 10) {
        servo_radar_set_angle((float)a);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    for (int a = 180; a >= 0; a -= 10) {
        servo_radar_set_angle((float)a);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    ESP_LOGI(TAG, "  -> radar servo done");

    /* --- 4. 风扇开 2 秒 --- */
    ESP_LOGI(TAG, "[TEST 4] Fan ON 2s");
    oled_display_msg("[4] Fan", "ON 2s -> OFF");
    motor_fan_set(true);
    ESP_LOGI(TAG, "  -> running");
    vTaskDelay(pdMS_TO_TICKS(2000));
    motor_fan_set(false);
    ESP_LOGI(TAG, "  -> OFF");

    /* --- 5. 水泵抽水 1 秒（仅低压受控演示中验证方向） --- */
    ESP_LOGI(TAG, "[TEST 5] Pump drain ON 1s");
    oled_display_msg("[5] Pump", "Drain 1s->OFF");
    actuator_pump_set_drain(true);
    ESP_LOGI(TAG, "  -> ON");
    vTaskDelay(pdMS_TO_TICKS(1000));
    actuator_pump_set_drain(false);
    ESP_LOGI(TAG, "  -> OFF");

    /* --- 6. 超声波测距 3 次 --- */
    ESP_LOGI(TAG, "[TEST 6] Ultrasonic x3");
    oled_display_msg("[6] Ultrasonic", "Measuring...");
    bool us_ok = false;
    for (int i = 0; i < 3; i++) {
        float d = sensor_ultrasonic_measure_cm();
        if (d < 0.f) {
            ESP_LOGW(TAG, "  [%d] timeout", i + 1);
        } else if (d > 400.f) {
            ESP_LOGW(TAG, "  [%d] %.0f cm (out of range)", i + 1, d);
        } else {
            ESP_LOGI(TAG, "  [%d] %.1f cm", i + 1, d);
            us_ok = true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (!us_ok) {
        ESP_LOGW(TAG, "  !! Ultrasonic: no valid reading (check wiring)");
    }

    /* --- 7. 读取传感器一次并打印 --- */
    ESP_LOGI(TAG, "[TEST 7] Sensor read");
    oled_display_msg("[7] Sensors", "Reading...");
    int mq2 = sensor_mq2_read_raw();
    int mq5 = sensor_mq5_read_raw();
    bool fl  = sensor_flame_read();
    ESP_LOGI(TAG, "  MQ2=%d  MQ5=%d  Flame=%s", mq2, mq5, fl ? "YES" : "no");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- 自检结束 --- */
    ESP_LOGI(TAG, "===== Self-test done! Entering monitor loop =====");
    oled_display_msg("Test Done!", "Enter loop...");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void sensor_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(100);
    for (;;) {
        int mq2 = sensor_mq2_read_raw();
        int mq5 = sensor_mq5_read_raw();
        bool fl = sensor_flame_read();

        app_state_lock();
        g_app_state.mq2_raw = mq2;
        g_app_state.mq5_raw = mq5;
        g_app_state.flame = fl;
        app_state_unlock();

        vTaskDelay(period);
    }
}

static void alarm_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(200);
    for (;;) {
        app_state_lock();
        alarm_logic_apply(&g_app_state);
        app_state_unlock();
        vTaskDelay(period);
    }
}

static void radar_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(50);
    for (;;) {
        app_state_lock();
        radar_scan_tick(&g_app_state);
        app_state_unlock();
        vTaskDelay(period);
    }
}

static const char *const k_ui_zh[] = {
    [UI_STATE_SAFE] = "\xe5\xae\x89\xe5\x85\xa8",           /* 安全 */
    [UI_STATE_GAS] = "\xe6\x9c\x89\xe6\xaf\x92\xe6\xb0\x94\xe4\xbd\x93", /* 有毒气体 */
    [UI_STATE_SMOKE] = "\xe7\x83\x9f\xe9\x9b\xbe",           /* 烟雾 */
    [UI_STATE_FIRE] = "\xe7\x81\xab\xe7\x84\xb0",           /* 火焰 */
};

static void display_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(100);
    static int s_last_ui = -1;
    for (;;) {
        app_state_lock();
        if ((int)g_app_state.ui_state != s_last_ui) {
            s_last_ui = (int)g_app_state.ui_state;
            if (s_last_ui >= 0 && s_last_ui < 4) {
                ESP_LOGW(TAG, "OLED状态: %s", k_ui_zh[s_last_ui]);
            }
        }
        oled_display_render(&g_app_state);
        app_state_unlock();
        vTaskDelay(period);
    }
}

void app_main(void)
{
    g_app_state_mutex = xSemaphoreCreateMutex();
    memset(&g_app_state, 0, sizeof(g_app_state));
    g_app_state.mq2_raw = 0;
    g_app_state.mq5_raw = 0;
    g_app_state.distance_cm = -1.f;
    g_app_state.radar_sweep_deg = 0.f;
    g_app_state.radar_trail_head = 0;
    g_app_state.radar_sweep_dir = 1;
    g_app_state.ui_state = UI_STATE_SAFE;

    ESP_LOGI(TAG, "Smart safety monitor starting");

    ESP_ERROR_CHECK(sensor_adc_init());
    ESP_ERROR_CHECK(sensor_mq2_init());
    ESP_ERROR_CHECK(sensor_mq5_init());
    ESP_ERROR_CHECK(sensor_flame_init());
    ESP_ERROR_CHECK(sensor_ultrasonic_init());
    ESP_ERROR_CHECK(actuator_ctrl_init());
    ESP_ERROR_CHECK(servo_window_init());
    ESP_ERROR_CHECK(servo_radar_init());
    ESP_ERROR_CHECK(motor_fan_init());
    ESP_ERROR_CHECK(oled_display_init());

    /* 先自检：在启动 WiFi 前完成 OLED 提示，避免 I2C 与无线初始化互相干扰 */
    self_test();

    /* 自检后再启动 SoftAP 与 HTTP 服务，再创建传感器等任务 */
    ESP_ERROR_CHECK(web_server_start());

    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(alarm_task, "alarm", 4096, NULL, 6, NULL);
    xTaskCreate(radar_task, "radar", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "Tasks created");
}
