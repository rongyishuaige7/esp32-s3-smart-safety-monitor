#include "servo_ctrl.h"
#include "app_config.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "servo";

#define SERVO_TIMER           LEDC_TIMER_0
#define SERVO_MODE            LEDC_LOW_SPEED_MODE
#define SERVO_CHANNEL_WIN     LEDC_CHANNEL_0   /* 窗户舵机 */
#define SERVO_CHANNEL_RADAR   LEDC_CHANNEL_1   /* 雷达舵机 */
#define SERVO_DUTY_RES        LEDC_TIMER_13_BIT
#define SERVO_FREQ_HZ         50

/** SG90 脉宽范围（μs）：500 = 0°，1500 = 90°，2500 = 180° */
#define PULSE_US_0DEG         500
#define PULSE_US_90DEG        1500
#define PULSE_US_180DEG       2500
#define PERIOD_US             (1000000 / SERVO_FREQ_HZ)

/** 窗户舵机：约 500 关窗(0°)，约 1500 开窗(90°) */
#define PULSE_US_CLOSED       PULSE_US_0DEG
#define PULSE_US_OPEN         PULSE_US_90DEG

static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    uint32_t max_duty = (1U << 13) - 1; /* 13 位分辨率，最大 8191 */
    return (pulse_us * max_duty) / PERIOD_US;
}

/** 角度 0～180° 转换为对应脉宽 duty */
static uint32_t angle_to_duty(float deg)
{
    if (deg < 0.f)   deg = 0.f;
    if (deg > 180.f) deg = 180.f;
    uint32_t pulse = (uint32_t)(PULSE_US_0DEG + (PULSE_US_180DEG - PULSE_US_0DEG) * deg / 180.f);
    return pulse_to_duty(pulse);
}

/* ------------------------------------------------------------------ */
/*  舵机1：窗户                                                        */
/* ------------------------------------------------------------------ */
esp_err_t servo_window_init(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode = SERVO_MODE,
        .duty_resolution = SERVO_DUTY_RES,
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t ch = {
        .gpio_num = PIN_SERVO_WINDOW_GPIO,
        .speed_mode = SERVO_MODE,
        .channel = SERVO_CHANNEL_WIN,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = pulse_to_duty(PULSE_US_CLOSED),
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_WIN));

    ESP_LOGI(TAG, "Servo window on GPIO%d (50Hz LEDC ch0)", PIN_SERVO_WINDOW_GPIO);
    return ESP_OK;
}

void servo_window_set_open(bool open)
{
    uint32_t pulse = open ? PULSE_US_OPEN : PULSE_US_CLOSED;
    uint32_t duty = pulse_to_duty(pulse);
    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_WIN, duty);
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_WIN);
}

/* ------------------------------------------------------------------ */
/*  舵机2：雷达扫描（超声波固定于此舵机转轴）                           */
/* ------------------------------------------------------------------ */
esp_err_t servo_radar_init(void)
{
    /* TIMER_0 在 servo_window_init() 中已配置，此处只注册通道 */
    ledc_channel_config_t ch = {
        .gpio_num = PIN_SERVO_RADAR_GPIO,
        .speed_mode = SERVO_MODE,
        .channel = SERVO_CHANNEL_RADAR,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = angle_to_duty(0.f),
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_RADAR));

    ESP_LOGI(TAG, "Servo radar on GPIO%d (50Hz LEDC ch1)", PIN_SERVO_RADAR_GPIO);
    return ESP_OK;
}

void servo_radar_set_angle(float deg)
{
    uint32_t duty = angle_to_duty(deg);
    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL_RADAR, duty);
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL_RADAR);
}
