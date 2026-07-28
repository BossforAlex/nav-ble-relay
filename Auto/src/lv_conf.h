/**
 * @file lv_conf.h
 * @brief LVGL v8.4 配置（适配 ESP32-S3 + ILI9341 320x240）
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* 颜色深度 16-bit (RGB565) */
#define LV_COLOR_DEPTH     16

/* 显示缓冲区：320*240/10 = 7.7KB，较小以节省内存 */
#define LV_DISP_DEF_REFR_PERIOD    20
#define LV_INDEV_DEF_READ_PERIOD   20

/* 使用 1/10 屏幕大小的单缓冲 */
/* v0.9.1: 从 1/10 屏幕增大到 1/4 屏幕缓冲，减少撕裂和渲染时间 */
#define LV_DISP_BUF_SIZE           (320 * 240 / 4)

/* v0.6.5: 为 LVGL 默认主题能链接，启用被 theme_default 引用的核心 widgets */
#define LV_USE_PERF_MONITOR        0
#define LV_USE_MEM_MONITOR         0
#define LV_USE_ANIMATION           1
#define LV_USE_LABEL               1
#define LV_USE_BTN                 1
#define LV_USE_IMG                 1
#define LV_USE_BTNMATRIX           1
#define LV_USE_BAR                 0
#define LV_USE_SLIDER              0
#define LV_USE_SWITCH              0
#define LV_USE_DROPDOWN            1
#define LV_USE_ROLLER              1
#define LV_USE_TEXTAREA            0
#define LV_USE_CHECKBOX            0
#define LV_USE_LIST                0
#define LV_USE_TABLE               0
#define LV_USE_CHART               0
#define LV_USE_WIN                 0
#define LV_USE_SPAN                0
#define LV_USE_SPINNER             0
#define LV_USE_SPINBOX             0
#define LV_USE_KEYBOARD            0
#define LV_USE_OBJ                 1
#define LV_USE_FLEX                1
#define LV_USE_GRID                0

/* extra widgets：只启用主题需要的，关闭其余以节省 Flash */
#define LV_USE_EXTRA_WIDGETS       1
#define LV_USE_CALENDAR            1
#define LV_USE_CHART               0
#define LV_USE_COLORWHEEL          1
#define LV_USE_IMGBTN              0
#define LV_USE_LED                 1
#define LV_USE_MENU                1
#define LV_USE_METER               1
#define LV_USE_MSGBOX              1
#define LV_USE_SPAN                0
#define LV_USE_SPINBOX             0
#define LV_USE_SPINNER             0
#define LV_USE_TABVIEW             1
#define LV_USE_TILEVIEW            1
#define LV_USE_WIN                 0

/* 默认字体，只加载常用字号 */
#define LV_FONT_MONTSERRAT_12      1
#define LV_FONT_MONTSERRAT_14      1
#define LV_FONT_MONTSERRAT_16      1
#define LV_FONT_MONTSERRAT_20      1
#define LV_FONT_MONTSERRAT_24      1
#define LV_FONT_MONTSERRAT_28      1
#define LV_FONT_MONTSERRAT_32      1
#define LV_FONT_MONTSERRAT_48      1
#define LV_FONT_MONTSERRAT_8       0
#define LV_FONT_MONTSERRAT_10      0
#define LV_FONT_MONTSERRAT_18      1
#define LV_FONT_MONTSERRAT_22      0
#define LV_FONT_MONTSERRAT_26      0
#define LV_FONT_MONTSERRAT_34      0
#define LV_FONT_MONTSERRAT_36      1
#define LV_FONT_MONTSERRAT_40      0
#define LV_FONT_MONTSERRAT_44      0

/* 中文字体需要特殊处理，这里先关闭，后续如需要再加入 */
#define LV_FONT_SIMSUN_16_CJK      0

/* 主题 */
#define LV_USE_THEME_DEFAULT       1
#define LV_THEME_DEFAULT_DARK      1

/* 其他 */
#define LV_USE_USER_DATA           1
#define LV_USE_LARGE_COORD         0
#define LV_USE_LOG                 0

/* v0.9.2: 渲染质量优化 — 减少 TFT 锯齿 */
#define LV_ANTIALIAS               1       /* 全局抗锯齿 */
#define LV_DISP_DEF_REFR_PERIOD    20      /* 刷新周期 20ms */
#define LV_DPI_DEF                 130     /* 默认 DPI（TFT 320x240 实际约 130） */
#define LV_DRAW_COMPLEX            1       /* 启用复杂绘制（阴影/渐变） */
#define LV_SHADOW_CACHE_SIZE       0       /* 无阴影缓存（节省内存） */
#define LV_IMG_CACHE_DEF_SIZE      0       /* 无图片缓存 */
#define LV_GRADIENT_MAX_STOPS      2       /* 渐变最大停止点 */
#define LV_COLOR_MIX_ROUND_OFS     128     /* 颜色混合四舍五入偏移 */
#define LV_COLOR_CHROMA_KEY        lv_color_hex(0x00FF00)  /* 色键 */

/* v0.9.2: 字体渲染质量 */
#define LV_FONT_FMT_TXT_LARGE      0       /* 不存储大字体文本格式 */
#define LV_TXT_ENC                 1       /* 启用 UTF-8 编码 */
#define LV_TXT_BREAK_CHARS         " ,.;:-_)}"  /* 文本换行字符 */
#define LV_TXT_LINE_BREAK_LONG_LEN 0       /* 自动换行阈值（0=禁用） */
#define LV_TXT_COLOR_CMD           "#"     /* 颜色命令前缀 */

#endif /* LV_CONF_H */
