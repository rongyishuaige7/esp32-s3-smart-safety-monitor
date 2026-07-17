#include "alarm_logic.h"
#include "actuator_ctrl.h"
#include "motor_ctrl.h"
#include "servo_ctrl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "alarm";

/*
 * 分时上电防浪涌（有风扇/水泵/舵机任一即启用）：
 *
 * 通用序列（alarm_task 周期 200ms）：
 *   步骤 0        ── 蜂鸣器 + LED 开启
 *   步骤 1~7      ── 维持蜂鸣/LED（等待约 1.6s）
 *   步骤 8        ── 关蜂鸣器 + 启动第一执行器
 *   步骤 9        ── 启动第二执行器（如有）
 *   步骤 10       ── 启动第三执行器（如有）
 *
 * 优先级顺序：舵机 > 风扇 > 水泵
 *   combo(浓烟+气体):  蜂鸣1.6s → 舵机开窗 → 风扇 → 水泵(如LEVEL3)
 *   纯烟雾 LEVEL3:     蜂鸣1.6s → 风扇 → 水泵
 *   纯烟雾 LEVEL2:     蜂鸣1.6s → 风扇
 *   火焰:              蜂鸣1.6s → 水泵
 *
 * s_startup_step 记录已完成的步骤（-1 = 无报警/已复位）。
 */
#define BUZZER_DELAY_STEPS  8   /* 蜂鸣器先响 8×200ms≈1.6s 后再启动下一个执行器 */

static int  s_startup_step   = -1; /* -1: 空闲；否则为分步启动的当前步骤 */
static bool s_running        = false; /* 执行器是否在固定运行期内 */
static int  s_run_ticks_left = 0;  /* 固定运行倒计时，归零时强制关闭 */
static int  s_flame_latch    = 0;  /* 火焰锁存倒计时 */
static int  s_smoke_latch    = 0;  /* 烟雾等级锁存（本次运行周期内只升不降） */

void alarm_logic_apply(app_state_t *s)
{
    int mq2 = s->mq2_raw;
    if (mq2 < 0) { mq2 = 0; }
    int mq5 = s->mq5_raw;
    if (mq5 < 0) { mq5 = 0; }

    /* 火焰锁存：传感器报火焰时重置计时器；计时器>0 时强制视为有火焰 */
    bool flame_raw = s->flame;
    if (flame_raw) {
        s_flame_latch = FLAME_LATCH_COUNT;
    } else if (s_flame_latch > 0) {
        s_flame_latch--;
    }
    bool flame = (s_flame_latch > 0);
    bool gas_danger = (mq5 >= (int)THRESHOLD_GAS_PPM_RAW);

    smoke_level_t sl_raw = SMOKE_LEVEL_NONE;
    if      (mq2 >= (int)THRESHOLD_SMOKE_LEVEL3) { sl_raw = SMOKE_LEVEL_3; }
    else if (mq2 >= (int)THRESHOLD_SMOKE_LEVEL2) { sl_raw = SMOKE_LEVEL_2; }
    else if (mq2 >= (int)THRESHOLD_SMOKE_LEVEL1) { sl_raw = SMOKE_LEVEL_1; }

    /* 烟雾等级锁存：只升不降，报警解除（s_clear_confirm 归零）时才重置 */
    if ((int)sl_raw > s_smoke_latch) {
        s_smoke_latch = (int)sl_raw;
    }
    smoke_level_t sl = (smoke_level_t)s_smoke_latch;

    bool heavy_smoke = (sl >= SMOKE_LEVEL_2); /* 用锁存后的等级判断 */
    bool combo       = gas_danger && heavy_smoke;

    /* ---------- 目标状态（逻辑期望值） ---------- */
    bool want_buzzer  = false;
    bool want_led     = false;
    bool want_pump    = false;
    bool want_fan     = false;
    bool want_win     = false;

    if (flame) {
        want_pump   = true;
        want_buzzer = true;
        want_led    = true;
        sl = SMOKE_LEVEL_3;
    } else if (combo) {
        want_fan    = true;
        want_win    = true;
        want_buzzer = true;
        want_led    = true;
        if (sl >= SMOKE_LEVEL_3) { want_pump = true; }
    } else {
        if (sl >= SMOKE_LEVEL_1) { want_buzzer = true; want_led = true; }
        if (sl >= SMOKE_LEVEL_2) { want_fan    = true; }
        if (sl >= SMOKE_LEVEL_3) { want_pump = true; want_buzzer = true; }
        if (gas_danger)          { want_buzzer = true; want_led = true; }
    }

    bool any_alarm = want_buzzer || want_led || want_pump || want_fan || want_win;
    /* 有任何大功率执行器（风扇/水泵/舵机）就分步上电，避免浪涌 */
    bool need_stagger = want_fan || want_pump || want_win;

    /* ------------------------------------------------------------------ */
    /* 固定运行倒计时：运行中每帧 -1，归零时关闭所有执行器，重回监测状态  */
    /* ------------------------------------------------------------------ */
    if (s_running) {
        s_run_ticks_left--;
        if (s_run_ticks_left <= 0) {
            /* 固定时长到期，强制关闭所有执行器 */
            actuator_buzzer_set(false);
            actuator_led_set(false);
            actuator_pump_set(false);
            motor_fan_set(false);
            servo_window_set_open(false);
            s_startup_step   = -1;
            s_running        = false;
            s_run_ticks_left = 0;
            s_smoke_latch    = 0;
            ESP_LOGI(TAG, "actuator run time expired, all off, resume monitoring");
            goto update_state;
        }
        /* 运行期内推进分步上电（如有） */
        if (s_startup_step >= 0 && s_startup_step < BUZZER_DELAY_STEPS - 1) {
            s_startup_step++;
        } else if (s_startup_step == BUZZER_DELAY_STEPS - 1) {
            /* 蜂鸣器延时结束，关蜂鸣器，启动第一个大功率执行器 */
            actuator_buzzer_set(false);
            if (want_win) {
                servo_window_set_open(true);
                ESP_LOGI(TAG, "stagger: buzzer off, servo open");
            } else if (want_fan) {
                motor_fan_set(true);
                ESP_LOGI(TAG, "stagger: buzzer off, fan on");
            } else if (want_pump) {
                actuator_pump_set(true);
                ESP_LOGI(TAG, "stagger: buzzer off, pump on");
            }
            s_startup_step = BUZZER_DELAY_STEPS;
        } else if (s_startup_step == BUZZER_DELAY_STEPS) {
            /* 第二个大功率执行器 */
            if (want_win) {
                motor_fan_set(want_fan);
                ESP_LOGI(TAG, "stagger: fan=%d", want_fan ? 1 : 0);
            } else if (want_fan && want_pump) {
                actuator_pump_set(true);
                ESP_LOGI(TAG, "stagger: pump on (fan already running)");
            }
            s_startup_step = BUZZER_DELAY_STEPS + 1;
        } else if (s_startup_step == BUZZER_DELAY_STEPS + 1) {
            /* 第三个（仅 combo 且 LEVEL3 同时需要水泵时） */
            if (want_win && want_pump) {
                actuator_pump_set(true);
                ESP_LOGI(TAG, "stagger: pump on (combo+level3)");
            }
            s_startup_step = BUZZER_DELAY_STEPS + 2;
        }
        /* 分步完成后维持，直到计时结束 */
        goto update_state;
    }

    /* ------------------------------------------------------------------ */
    /* 非运行期：检测到报警条件则触发，开始固定运行                        */
    /* ------------------------------------------------------------------ */
    if (!any_alarm) {
        /* 无报警，保持空闲 */
        goto update_state;
    }

    /* 触发：启动固定运行计时器，分步上电 */
    s_running        = true;
    s_run_ticks_left = ACTUATOR_RUN_TICKS;
    ESP_LOGI(TAG, "alarm triggered: run for %d ticks (%dms), stagger=%d",
             ACTUATOR_RUN_TICKS, ACTUATOR_RUN_TICKS * 200, need_stagger ? 1 : 0);

    if (!need_stagger) {
        /* 仅蜂鸣器+LED（LEVEL1 / 纯气体）：立即启动，显式关闭所有大功率 */
        actuator_buzzer_set(want_buzzer);
        actuator_led_set(want_led);
        actuator_pump_set(false);
        motor_fan_set(false);
        servo_window_set_open(false);
        s_startup_step = BUZZER_DELAY_STEPS + 3;
    } else {
        /*
         * 分步上电序列（每帧推进一步，alarm_task 周期 200ms）：
         *
         * combo:   蜂鸣器+LED → 等1.6s → 关蜂鸣+舵机开窗 → 风扇 → 水泵(如需)
         * 非combo: 蜂鸣器+LED → 等1.6s → 关蜂鸣+风扇 → 水泵(如需)
         */
        s_startup_step = 0;
        actuator_buzzer_set(want_buzzer);
        actuator_led_set(want_led);
        ESP_LOGI(TAG, "stagger step0: buzzer/led, next actuator after %d frames", BUZZER_DELAY_STEPS);
    }
    /* 首帧触发后直接进入 update_state，下一帧起由 s_running 驱动分步推进 */

update_state:
    {
        /*
         * 安全防护：不在分步启动阶段的、且逻辑不需要的执行器，每帧强制关闭。
         * 防止 GPIO 电平意外漂移导致水泵/风扇异常开启。
         */
        if (!s_running) {
            actuator_buzzer_set(false);
            actuator_led_set(false);
            actuator_pump_set(false);
            motor_fan_set(false);
            servo_window_set_open(false);
        } else if (s_startup_step > BUZZER_DELAY_STEPS + 2) {
            /*
             * 分步已完成：条件升级时补开执行器，条件消退时关闭。
             * 解决"触发时为 LEVEL1 只开蜂鸣，运行期内升到 LEVEL2 但风扇
             *  未被启动"的问题——因为分步序列已跳过，需要在此补开。
             */
            want_pump ? actuator_pump_set(true)         : actuator_pump_set(false);
            want_fan  ? motor_fan_set(true)             : motor_fan_set(false);
            want_win  ? servo_window_set_open(true)     : servo_window_set_open(false);
        }

        ui_state_t ui = UI_STATE_SAFE;
        if      (flame)                  { ui = UI_STATE_FIRE;  }
        else if (combo || gas_danger)    { ui = UI_STATE_GAS;   }
        else if (sl >= SMOKE_LEVEL_1)    { ui = UI_STATE_SMOKE; }

        s->smoke_level       = sl;
        s->gas_danger        = gas_danger;
        s->combo_vent_priority = combo;
        s->ui_state          = ui;
        s->buzzer_on         = want_buzzer;
        s->led_on            = want_led;
        s->pump_on           = want_pump;
        s->fan_on            = want_fan;
        s->window_open       = want_win;

        /* 有报警或运行中时每帧输出状态，方便串口排查 */
        if (any_alarm || s_running) {
            ESP_LOGI(TAG, "mq2=%d mq5=%d fl=%d sm=%d step=%d run=%d -> Bz=%d L=%d F=%d P=%d W=%d",
                     mq2, mq5, flame ? 1 : 0, (int)sl, s_startup_step, s_running ? 1 : 0,
                     want_buzzer, want_led, want_fan, want_pump, want_win);
        }
    }
    return;

}
