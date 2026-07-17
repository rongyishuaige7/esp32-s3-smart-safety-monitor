#include "radar_scan.h"
#include "sensor_ultrasonic.h"
#include "servo_ctrl.h"

void radar_scan_tick(app_state_t *s)
{
    float d = sensor_ultrasonic_measure_cm();
    s->distance_cm = d;

    /* 推进扫描前，将当前角度写入尾迹环形缓冲 */
    s->radar_trail[s->radar_trail_head] = s->radar_sweep_deg;
    s->radar_trail_head = (uint8_t)((s->radar_trail_head + 1) % RADAR_TRAIL_LEN);

    if (s->radar_sweep_dir == 0) {
        s->radar_sweep_dir = 1;
    }
    s->radar_sweep_deg += (float)(s->radar_sweep_dir) * 2.0f;

    if (s->radar_sweep_deg >= 180.f) {
        s->radar_sweep_deg = 180.f;
        s->radar_sweep_dir = -1;
    } else if (s->radar_sweep_deg <= 0.f) {
        s->radar_sweep_deg = 0.f;
        s->radar_sweep_dir = 1;
    }

    /* 驱动雷达舵机到当前扫描角，实现超声波物理扫描 */
    servo_radar_set_angle(s->radar_sweep_deg);
}
