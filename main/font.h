/**
 * @file font.h
 * @brief 5×7 ASCII 字模（Adafruit glcdfont）及 OLED 取字访问。
 */
#pragma once

#include <stdint.h>

extern const unsigned char glcdfont[];

#define GLCDFONT_FIRST  0
#define GLCDFONT_LAST   255
#define GLCDFONT_W      5
#define GLCDFONT_H      8

static inline const unsigned char *font_glyph(uint8_t c)
{
    /* 完整 0～255 表；不可打印字符回退为空格 0x20 */
    if (c < 32) {
        c = 32;
    }
    return &glcdfont[c * GLCDFONT_W];
}
