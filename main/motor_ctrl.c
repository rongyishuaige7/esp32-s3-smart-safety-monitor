#include "motor_ctrl.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "motor";

esp_err_t motor_fan_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_L9110_IA_GPIO) | (1ULL << PIN_L9110_IB_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(PIN_L9110_IA_GPIO, 0);
    gpio_set_level(PIN_L9110_IB_GPIO, 0);
    ESP_LOGI(TAG, "L9110 fan IA=%d IB=%d", PIN_L9110_IA_GPIO, PIN_L9110_IB_GPIO);
    return ESP_OK;
}

void motor_fan_set(bool on)
{
    if (on) {
        gpio_set_level(PIN_L9110_IA_GPIO, 1);
        gpio_set_level(PIN_L9110_IB_GPIO, 0);
    } else {
        gpio_set_level(PIN_L9110_IA_GPIO, 0);
        gpio_set_level(PIN_L9110_IB_GPIO, 0);
    }
}
