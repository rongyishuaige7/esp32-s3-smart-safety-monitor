#include "sensor_ultrasonic.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "hcsr04";

esp_err_t sensor_ultrasonic_init(void)
{
    gpio_config_t trig = {
        .pin_bit_mask = (1ULL << PIN_HCSR04_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t echo = {
        .pin_bit_mask = (1ULL << PIN_HCSR04_ECHO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&trig));
    ESP_ERROR_CHECK(gpio_config(&echo));
    gpio_set_level(PIN_HCSR04_TRIG, 0);
    ESP_LOGI(TAG, "HC-SR04 TRIG=%d ECHO=%d", PIN_HCSR04_TRIG, PIN_HCSR04_ECHO);
    return ESP_OK;
}

float sensor_ultrasonic_measure_cm(void)
{
    gpio_set_level(PIN_HCSR04_TRIG, 0);
    esp_rom_delay_us(5);
    gpio_set_level(PIN_HCSR04_TRIG, 1);
    esp_rom_delay_us(15);  /* 15μs 触发脉宽，3.3V 模块更稳，与测试代码一致 */
    gpio_set_level(PIN_HCSR04_TRIG, 0);

    int64_t t_start = esp_timer_get_time();
    while (gpio_get_level(PIN_HCSR04_ECHO) == 0) {
        if ((esp_timer_get_time() - t_start) > HCSR04_TIMEOUT_US) {
            return -1.0f;
        }
    }

    int64_t t_high = esp_timer_get_time();
    while (gpio_get_level(PIN_HCSR04_ECHO) == 1) {
        if ((esp_timer_get_time() - t_high) > HCSR04_TIMEOUT_US) {
            return -1.0f;
        }
    }
    int64_t t_end = esp_timer_get_time();

    float duration_us = (float)(t_end - t_high);
    /* 声速约 343m/s → 0.0343 cm/μs，往返时间折半为单程 */
    float cm = duration_us * 0.0343f / 2.0f;
    if (cm < 2.0f) {
        cm = 2.0f;
    }
    if (cm > HCSR04_MAX_DISTANCE_CM) {
        cm = HCSR04_MAX_DISTANCE_CM;
    }
    return cm;
}
