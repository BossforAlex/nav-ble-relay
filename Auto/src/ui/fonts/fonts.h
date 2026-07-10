#pragma once

/**
 * @file fonts.h
 * @brief v0.6.9: 自定义 CJK 字体 + 箭头字体声明
 *
 * 字体由 scripts/generate_fonts.sh 生成：
 *   - cjk_14/20/24: DroidSansFallback (CJK) + DejaVu Sans (箭头) 合并
 *   - arrows_20/48:  DejaVu Sans 箭头专用
 */
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t cjk_14;
extern const lv_font_t cjk_20;
extern const lv_font_t cjk_24;
extern const lv_font_t arrows_20;
extern const lv_font_t arrows_48;

#ifdef __cplusplus
}
#endif