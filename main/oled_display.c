#include "oled_display.h"
#include "font.h"
#include "app_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "oled";

#define OLED_W          128
#define OLED_H          64
#define OLED_BUFSIZE    ((OLED_W * OLED_H) / 8)

static uint8_t s_fb[OLED_BUFSIZE];
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_i2c_dev;

static esp_err_t i2c_write_raw(const uint8_t *data, size_t len)
{
    /* 超时略长，便于异步队列排空 */
    return i2c_master_transmit(s_i2c_dev, data, len, pdMS_TO_TICKS(200));
}

static esp_err_t oled_cmd(uint8_t c)
{
    uint8_t buf[2] = {0x00, c};
    return i2c_write_raw(buf, 2);
}

static esp_err_t oled_init_ssd1306(void)
{
    /* 初始化命令合并为一次 I2C 传输，避免队列溢出。
     * 首字节 0x00 为控制字节，其后均为命令。 */
    static const uint8_t init_seq[] = {
        0x00,       /* 控制：命令流 */
        0xAE,       /* 关显示 */
        0xD5, 0x80, /* 时钟分频 / 振荡频率 */
        0xA8, 0x3F, /* 多路复用比 64 */
        0xD3, 0x00, /* 显示偏移 0 */
        0x40,       /* 起始行 0 */
        0x8D, 0x14, /* 电荷泵 */
        0x20, 0x00, /* 水平寻址模式 */
        0xA1,       /* 段重映射 */
        0xC8,       /* COM 扫描方向 */
        0xDA, 0x12, /* COM 引脚 */
        0x81, 0xCF, /* 对比度 */
        0xD9, 0xF1, /* 预充电周期 */
        0xDB, 0x40, /* VCOMH */
        0xA4,       /* 从 RAM 显示 */
        0xA6,       /* 正常显示（非反色） */
        0xAF,       /* 开显示 */
    };
    esp_err_t e = i2c_master_transmit(s_i2c_dev, init_seq, sizeof(init_seq),
                                      pdMS_TO_TICKS(500));
    return e;
}

static void fb_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

static void fb_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) {
        return;
    }
    uint16_t idx = (uint16_t)x + (uint16_t)(y / 8) * OLED_W;
    uint8_t mask = (uint8_t)(1u << (y & 7));
    if (on) {
        s_fb[idx] |= mask;
    } else {
        s_fb[idx] &= (uint8_t)~mask;
    }
}

static void draw_line(int x0, int y0, int x1, int y1, bool on)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_set_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_char(int x, int y, char c)
{
    const unsigned char *g = font_glyph((uint8_t)c);
    for (int col = 0; col < GLCDFONT_W; col++) {
        uint8_t line = g[col];
        for (int row = 0; row < 8; row++, line >>= 1) {
            if (line & 1) {
                fb_set_pixel(x + col, y + row, true);
            }
        }
    }
}

static void draw_str(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x, y, *s++);
        x += GLCDFONT_W + 1;
        if (x > OLED_W - GLCDFONT_W) {
            break;
        }
    }
}

static esp_err_t oled_flush(void)
{
    /* 列、页地址范围一次写入 */
    static const uint8_t addr_seq[] = {
        0x00,       /* 控制：命令流 */
        0x21, 0x00, 0x7F,  /* 列地址 0～127 */
        0x22, 0x00, 0x07,  /* 页地址 0～7 */
    };
    esp_err_t e = i2c_master_transmit(s_i2c_dev, addr_seq, sizeof(addr_seq),
                                      pdMS_TO_TICKS(200));
    if (e != ESP_OK) {
        return e;
    }

    /* 整帧显存一次传输：1 字节控制（0x40=数据流）+ 1024 字节像素 */
    static uint8_t tx[1 + OLED_BUFSIZE];
    tx[0] = 0x40;
    memcpy(&tx[1], s_fb, OLED_BUFSIZE);
    e = i2c_master_transmit(s_i2c_dev, tx, sizeof(tx), pdMS_TO_TICKS(500));
    return e;
}

esp_err_t oled_display_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_OLED_SDA_GPIO,
        .scl_io_num = PIN_OLED_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        /* 0 = 同步模式，无异步队列，避免溢出 */
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true},
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = OLED_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_i2c_dev));

    /* 等待 OLED 上电稳定后再发初始化 */
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t err = oled_init_ssd1306();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ssd1306 init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 初始化后立即清屏，避免花屏 */
    fb_clear();
    (void)oled_flush();

    ESP_LOGI(TAG, "SSD1306 OK (I2C addr 0x%02X)", OLED_I2C_ADDR);
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * 雷达：全屏布局，与 ESP32S3_Radar_Test 一致：
 *   圆心 CX=64、CY=63、半径 R=60，三圈、30/60/90/120/150° 辐条，
 *   角度数字 30～180、12 段扫尾、障碍物点与后方阴影、右上角状态与距离。
 * -------------------------------------------------------------------------- */
#define RADAR_CX      64
#define RADAR_CY      63
#define RADAR_R       60

static void draw_radar_full(const app_state_t *s)
{
    /* --- 三条同心半圆弧 --- */
    const int rings[3] = { RADAR_R / 3, RADAR_R * 2 / 3, RADAR_R };
    for (int ri = 0; ri < 3; ri++) {
        int r = rings[ri];
        for (int a = 0; a <= 180; a += 3) {
            float rad = (float)a * (float)M_PI / 180.0f;
            int x = RADAR_CX + (int)(cosf(rad) * (float)r);
            int y = RADAR_CY - (int)(sinf(rad) * (float)r);
            fb_set_pixel(x, y, true);
        }
    }

    /* --- 30°/60°/90°/120°/150° 辐条（虚线采样） --- */
    const int spokes[] = {30, 60, 90, 120, 150};
    for (int si = 0; si < 5; si++) {
        float rad = (float)spokes[si] * (float)M_PI / 180.0f;
        int dx = (int)(cosf(rad) * (float)RADAR_R);
        int dy = (int)(sinf(rad) * (float)RADAR_R);
        /* 虚线：t 从 0 到 1 采样 */
        for (int t8 = 0; t8 <= 8; t8++) {
            float t = (float)t8 / 8.0f;
            int px = RADAR_CX + (int)((float)dx * t);
            int py = RADAR_CY - (int)((float)dy * t);
            fb_set_pixel(px, py, true);
        }
    }

    /* --- 角度标注 30/60/90/120/150/180（逐个硬编码，避开 HUD 区域）
     *
     * 屏幕坐标参考（圆心 64,63，半径 60）：
     *   弧端 = (CX + cos(deg)*60, CY - sin(deg)*60)
     *   30°→(116,33)  60°→(94,11)  90°→(64,3)
     *  120°→(34,11)  150°→(12,33) 180°→(4,63)
     *
     * HUD 区域：x=80~127, y=0~19（SAFE/ALARM + 距离）
     * 60° 弧端 x≈94，右侧会闯入 HUD，因此标签放弧端左侧上方。
     * ---------------------------------------------------------------- */
    {
        /* {标签文字, 左上角 x, 左上角 y} */
        static const struct { const char *txt; int8_t x; int8_t y; } lp[] = {
            { "30",  117, 26 },  /* 弧端右侧，紧贴屏幕右边 */
            { "60",   72,  2 },  /* 弧端左侧，远离 HUD（x<84） */
            { "90",   57,  0 },  /* 正顶居中，x=57~68 不进 HUD */
            {"120",   20,  2 },  /* 弧端左侧 */
            {"150",    0, 26 },  /* 靠左边缘 */
            {"180",    0, 54 },  /* 基线左上方 */
        };
        for (int li = 0; li < 6; li++) {
            draw_str((int)lp[li].x, (int)lp[li].y, lp[li].txt);
        }
    }

    /* --- 底边基线 --- */
    draw_line(0, RADAR_CY, OLED_W - 1, RADAR_CY, true);

    /* --- 扫描尾迹（由旧到新，越新越密） --- */
    for (int i = 0; i < RADAR_TRAIL_LEN; i++) {
        /* 尾迹索引：按 (head+i)%LEN，越旧 step 越大、点越疏 */
        int idx  = (s->radar_trail_head + i) % RADAR_TRAIL_LEN;
        int step = RADAR_TRAIL_LEN - i;
        float trad = s->radar_trail[idx] * (float)M_PI / 180.0f;
        for (int r = step; r <= RADAR_R; r += step) {
            int px = RADAR_CX + (int)(cosf(trad) * (float)r);
            int py = RADAR_CY - (int)(sinf(trad) * (float)r);
            fb_set_pixel(px, py, true);
        }
    }

    /* --- 当前扫描线（实线） --- */
    float ang = s->radar_sweep_deg;
    if (ang < 0.f) { ang = 0.f; }
    if (ang > 180.f) { ang = 180.f; }
    float rad = ang * (float)M_PI / 180.0f;
    int ex = RADAR_CX + (int)(cosf(rad) * (float)RADAR_R);
    int ey = RADAR_CY - (int)(sinf(rad) * (float)RADAR_R);
    draw_line(RADAR_CX, RADAR_CY, ex, ey, true);

    /* --- 障碍物点（按距离映射到半径，量程 0～HCSR04_MAX_DISTANCE_CM） --- */
    float d = s->distance_cm;
    if (d > 0.f && d < HCSR04_MAX_DISTANCE_CM) {
        float ratio = d / HCSR04_MAX_DISTANCE_CM;
        if (ratio > 1.f) { ratio = 1.f; }
        int pr = (int)(ratio * (float)RADAR_R);
        /* 障碍物后方阴影：扫描角 ±6°，从障碍物外沿到外缘，稀疏点阵 */
        for (int da = -6; da <= 6; da++) {
            float shadow_deg = ang + (float)da;
            if (shadow_deg < 0.f || shadow_deg > 180.f) {
                continue;
            }
            float srad = shadow_deg * (float)M_PI / 180.0f;
            for (int rr = pr + 1; rr <= RADAR_R; rr++) {
                int sx = RADAR_CX + (int)(cosf(srad) * (float)rr);
                int sy = RADAR_CY - (int)(sinf(srad) * (float)rr);
                if (((sx + sy) % 3) == 0) {
                    fb_set_pixel(sx, sy, true);
                }
            }
        }
        int px = RADAR_CX + (int)(cosf(rad) * (float)pr);
        int py = RADAR_CY - (int)(sinf(rad) * (float)pr);
        /* 9×9 实心方块 + 十字准心，更加醒目 */
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                fb_set_pixel(px + dx, py + dy, true);
            }
        }
        /* 十字准心延伸线，增强可见性 */
        for (int ext = -6; ext <= 6; ext++) {
            fb_set_pixel(px + ext, py, true);
            fb_set_pixel(px, py + ext, true);
        }
    }

    /* --- 右上角状态与距离（列约 80～127，行 0～19） --- */
    const char *status_str;
    switch (s->ui_state) {
        case UI_STATE_GAS:   status_str = "!!ALARM"; break;
        case UI_STATE_SMOKE: status_str = "!!ALARM"; break;
        case UI_STATE_FIRE:  status_str = "!!ALARM"; break;
        default:             status_str = "  SAFE "; break;
    }
    draw_str(84, 0, status_str);

    char dbuf[10];
    if (d > 0.f) {
        snprintf(dbuf, sizeof(dbuf), "%.0fcm", d);
    } else {
        snprintf(dbuf, sizeof(dbuf), "---cm");
    }
    draw_str(84, 10, dbuf);
}

void oled_display_render(const app_state_t *s)
{
    fb_clear();
    draw_radar_full(s);
    (void)oled_flush();
}

void oled_display_msg(const char *line1, const char *line2)
{
    fb_clear();
    if (line1) {
        draw_str(0, 20, line1);
    }
    if (line2) {
        draw_str(0, 36, line2);
    }
    (void)oled_flush();
}
