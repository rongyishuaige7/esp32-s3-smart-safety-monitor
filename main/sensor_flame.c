#include "sensor_flame.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "flame";

/*
 * 软件防抖：
 *   连续 FLAME_DEBOUNCE_COUNT 次读到同一电平，才切换输出状态。
 *   counter > 0 表示"候选为有火焰"的连续计数；
 *   counter < 0 表示"候选为无火焰"的连续计数。
 *   |counter| 达到阈值时才翻转 s_flame_state。
 *
 *   sensor_task 周期 100ms，默认阈值 3 → 需稳定 300ms 才切换。
 */
static bool s_flame_state   = false;
static int  s_debounce_cnt  = 0;

esp_err_t sensor_flame_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_FLAME_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config: %s", esp_err_to_name(err));
    }
    s_flame_state  = false;
    s_debounce_cnt = 0;
    return err;
}

bool sensor_flame_read(void)
{
    /* 常见火焰模块：低电平表示检测到火焰 */
    bool raw = (gpio_get_level(PIN_FLAME_GPIO) == 0);

    /*
     * 方向反转时归零，同向累计到阈值才切换状态：
     *   检测到火焰：连续 FLAME_DEBOUNCE_COUNT 次低电平 → 切换为有火焰（快速进入）
     *   火焰消失：  连续 FLAME_DEBOUNCE_COUNT 次高电平 → 切换为无火焰（快速退出）
     *
     * 抗抖动依靠上层 alarm_logic 的 ALARM_CLEAR_CONFIRM_COUNT（持续 1s 无报警
     * 才关闭执行器），而非在传感器层延迟退出。
     */
    if (raw) {
        if (s_debounce_cnt < 0) { s_debounce_cnt = 0; }
        s_debounce_cnt++;
        if (s_debounce_cnt >= FLAME_DEBOUNCE_COUNT) {
            s_debounce_cnt = FLAME_DEBOUNCE_COUNT;
            if (!s_flame_state) {
                s_flame_state = true;
                ESP_LOGI(TAG, "flame detected (debounced)");
            }
        }
    } else {
        if (s_debounce_cnt > 0) { s_debounce_cnt = 0; }
        s_debounce_cnt--;
        if (s_debounce_cnt <= -FLAME_DEBOUNCE_COUNT) {
            s_debounce_cnt = -FLAME_DEBOUNCE_COUNT;
            if (s_flame_state) {
                s_flame_state = false;
                ESP_LOGI(TAG, "flame gone (debounced)");
            }
        }
    }

    return s_flame_state;
}
