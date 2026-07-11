#pragma once

/**
 * @file fonts.h
 * @brief v0.9.0: 自定义箭头字体声明
 *
 * 字体由 scripts/generate_fonts.sh 生成：
 *   - arrows_20/48:  DejaVu Sans 箭头专用
 *
 * CJK 中文字体已移除，改为由手机端 Flutter App 预渲染为位图传输。
 * ESP32 固件仅保留箭头符号和 ASCII 数字/单位字符。
 */
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t arrows_20;
extern const lv_font_t arrows_48;

#ifdef __cplusplus
}
#endif