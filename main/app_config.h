/**
 * @file app_config.h
 * @brief 引脚映射、阈值、共享应用状态（ESP32-S3 智能安全监测）。
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* -------------------------------------------------------------------------- */
/* GPIO — 传感器                                                              */
/* -------------------------------------------------------------------------- */
#define PIN_MQ2_ADC_GPIO        1   /* ADC1_CH0 */
#define PIN_MQ5_ADC_GPIO        2   /* ADC1_CH1 */
#define PIN_FLAME_GPIO          4
#define PIN_HCSR04_TRIG         5
#define PIN_HCSR04_ECHO         6

/* -------------------------------------------------------------------------- */
/* GPIO — 执行器                                                              */
/* -------------------------------------------------------------------------- */
#define PIN_BUZZER_GPIO         7
#define PIN_LED_ALARM_GPIO      15
#define PIN_PUMP_INA_GPIO       16  /* 水泵驱动板 INA */
#define PIN_PUMP_INB_GPIO       17  /* 水泵驱动板 INB */
#define PIN_L9110_IA_GPIO       18
#define PIN_L9110_IB_GPIO       8
#define PIN_SERVO_WINDOW_GPIO   9   /* LEDC PWM 舵机（开窗） */
#define PIN_SERVO_RADAR_GPIO    10  /* LEDC PWM 舵机（雷达扫描，挂载超声波） */

/* -------------------------------------------------------------------------- */
/* I2C — SSD1306 OLED                                                        */
/* -------------------------------------------------------------------------- */
#define PIN_OLED_SDA_GPIO       11
#define PIN_OLED_SCL_GPIO       12
#define OLED_I2C_ADDR           0x3C
#define OLED_I2C_FREQ_HZ        100000  /* 100kHz，兼容部分 SSD1306 模块 */

/* -------------------------------------------------------------------------- */
/* MQ-2 烟雾阈值（12 位 ADC 原始值 0～4095，按硬件标定）                        */
/* -------------------------------------------------------------------------- */
#define THRESHOLD_SMOKE_LEVEL1  800U
#define THRESHOLD_SMOKE_LEVEL2  1500U
#define THRESHOLD_SMOKE_LEVEL3  2200U

/* 组合规则「浓烟」：烟雾 ≥ 2 级且气体报警同时满足时使用 */
#define THRESHOLD_SMOKE_HEAVY   THRESHOLD_SMOKE_LEVEL2

/* MQ-5 气体阈值（原始 ADC） */
#define THRESHOLD_GAS_PPM_RAW   2000U

/* -------------------------------------------------------------------------- */
/* 超声波                                                                     */
/* -------------------------------------------------------------------------- */
#define HCSR04_MAX_DISTANCE_CM  50.0f
#define HCSR04_TIMEOUT_US       60000  /* 60ms，约 10m 量程，与测试代码一致 */

/* -------------------------------------------------------------------------- */
/* 火焰传感器软件防抖                                                         */
/* sensor_task 周期 100ms；连续 FLAME_DEBOUNCE_COUNT 次采样一致才切换状态。    */
/* 默认 3 → 需持续 300ms 才认定火焰出现/消失，过滤打火机电弧噪声。            */
/* -------------------------------------------------------------------------- */
#define FLAME_DEBOUNCE_COUNT  2

/* -------------------------------------------------------------------------- */
/* 报警解除持续确认帧数                                                        */
/* alarm_task 周期 200ms；连续 ALARM_CLEAR_CONFIRM_COUNT 帧无报警才关执行器。  */
/* 默认 5 → 需持续 1s 无报警才真正关闭，防止信号短暂闪烁导致误关。            */
/* -------------------------------------------------------------------------- */
#define ALARM_CLEAR_CONFIRM_COUNT  5

/* -------------------------------------------------------------------------- */
/* 火焰报警逻辑层锁存时间                                                      */
/* 一旦检测到火焰，alarm_logic 层至少保持报警 FLAME_LATCH_COUNT 帧。           */
/* alarm_task 周期 200ms；默认 25 → 锁存 5s，期间传感器抖动不影响报警状态。   */
/* -------------------------------------------------------------------------- */
#define FLAME_LATCH_COUNT  25

/* -------------------------------------------------------------------------- */
/* 风扇/水泵固定运行时长                                                       */
/* 触发后固定运行 ACTUATOR_RUN_TICKS 帧，之后关闭重新监测，不受传感器读数影响。*/
/* alarm_task 周期 200ms；默认 25 → 固定运行 5s。                             */
/* -------------------------------------------------------------------------- */
#define ACTUATOR_RUN_TICKS  25

/* -------------------------------------------------------------------------- */
/* 执行器分时上电（防浪涌重启）                                                */
/* alarm_task 周期 200ms；每步骤间隔 = ACTUATOR_STAGGER_ALARM_TICKS × 200ms   */
/* 默认 1 → 步骤间隔约 200ms：蜂鸣/LED → +200ms 风扇 → +200ms 舵机           */
/* 若仍重启可调大此值（2 = 400ms 间隔）。                                     */
/* -------------------------------------------------------------------------- */
#define ACTUATOR_STAGGER_ALARM_TICKS  1

/* -------------------------------------------------------------------------- */
/* 雷达尾迹                                                                   */
/* -------------------------------------------------------------------------- */
#define RADAR_TRAIL_LEN  12   /* 尾迹段数，与测试代码一致 */

/* -------------------------------------------------------------------------- */
/* 显示 / 界面状态                                                            */
/* -------------------------------------------------------------------------- */
typedef enum {
    UI_STATE_SAFE = 0,
    UI_STATE_GAS,
    UI_STATE_SMOKE,
    UI_STATE_FIRE,
} ui_state_t;

typedef enum {
    SMOKE_LEVEL_NONE = 0,
    SMOKE_LEVEL_1,
    SMOKE_LEVEL_2,
    SMOKE_LEVEL_3,
} smoke_level_t;

/** 运行时共享状态（由互斥锁保护）。 */
typedef struct {
    int mq2_raw;
    int mq5_raw;
    bool flame;
    float distance_cm;
    float radar_sweep_deg;              /* 0～180，OLED 雷达扫描角（软件） */
    float radar_trail[RADAR_TRAIL_LEN]; /* 尾迹历史角度 */
    uint8_t radar_trail_head;           /* 环形缓冲区头指针 */
    int8_t  radar_sweep_dir;            /* 扫描方向：+1 或 -1 */
    ui_state_t ui_state;
    smoke_level_t smoke_level;
    bool gas_danger;
    bool combo_vent_priority; /* 浓烟+气体：优先开窗+风扇 */
    bool buzzer_on;
    bool led_on;
    bool pump_on;
    bool fan_on;
    bool window_open;         /* true 表示开窗约 90° */
} app_state_t;

extern app_state_t g_app_state;
extern SemaphoreHandle_t g_app_state_mutex;

static inline void app_state_lock(void)
{
    if (g_app_state_mutex) {
        xSemaphoreTake(g_app_state_mutex, portMAX_DELAY);
    }
}

static inline void app_state_unlock(void)
{
    if (g_app_state_mutex) {
        xSemaphoreGive(g_app_state_mutex);
    }
}
