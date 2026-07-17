#include "actuator_ctrl.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "actuator";

/* 0=关 1=出水(INB) 2=抽水(INA)，仅状态变化时打日志，避免空闲轮询刷屏 */
static uint8_t s_pump_cmd = 0;

static void setup_out(gpio_num_t pin, int initial)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(pin, initial);
}

esp_err_t actuator_ctrl_init(void)
{
    /* 蜂鸣器：低电平响，高电平静音；上电输出高电平保持静音 */
    setup_out(PIN_BUZZER_GPIO, 1);
    setup_out(PIN_LED_ALARM_GPIO, 0);

    /* 水泵驱动板（INA/INB 双线控制，上电默认关闭） */
    setup_out(PIN_PUMP_INA_GPIO, 0);
    setup_out(PIN_PUMP_INB_GPIO, 0);

    ESP_LOGI(TAG, "GPIO actuators OK (buzzer, LED, pump INA=%d INB=%d)",
             PIN_PUMP_INA_GPIO, PIN_PUMP_INB_GPIO);
    return ESP_OK;
}

void actuator_buzzer_set(bool on)
{
    /* 低电平有效：低=响，高=静音 */
    gpio_set_level(PIN_BUZZER_GPIO, on ? 0 : 1);
}

void actuator_led_set(bool on)
{
    gpio_set_level(PIN_LED_ALARM_GPIO, on ? 1 : 0);
}

void actuator_pump_set(bool on)
{
    /* INB=1 INA=0 → 出水（报警灭火方向）；均 0 → 停止 */
    uint8_t next = on ? 1u : 0u;
    if (next != s_pump_cmd) {
        ESP_LOGI(TAG, "pump -> %s (INB=%d)", on ? "ON" : "OFF", on ? 1 : 0);
        s_pump_cmd = next;
    }
    gpio_set_level(PIN_PUMP_INA_GPIO, 0);
    gpio_set_level(PIN_PUMP_INB_GPIO, on ? 1 : 0);
}

void actuator_pump_set_drain(bool on)
{
    /* INA=1 INB=0 → 抽水（自检/排水方向）；均 0 → 停止 */
    uint8_t next = on ? 2u : 0u;
    if (next != s_pump_cmd) {
        ESP_LOGI(TAG, "pump(drain) -> %s (INA=%d)", on ? "ON" : "OFF", on ? 1 : 0);
        s_pump_cmd = next;
    }
    gpio_set_level(PIN_PUMP_INA_GPIO, on ? 1 : 0);
    gpio_set_level(PIN_PUMP_INB_GPIO, 0);
}
