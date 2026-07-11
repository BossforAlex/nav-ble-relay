#include "ui.h"
#include "fonts/fonts.h"
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 * v0.9.0 — 静态 UI 框架（参照 ui.txt 设计规范）
 *
 * 屏幕只负责展示，不包含任何数据解析逻辑。
 * 所有动态数据由外部软件层（ScreenLVGL）通过直接设置控件属性写入。
 * 控件句柄通过 extern 声明暴露给外部，方便后期更换不同屏幕驱动。
 *
 * 构建块：样式表 → 三栏面板 → 内部控件 → 加载屏幕
 * ══════════════════════════════════════════════════════════════ */

/* ── 控件句柄（外部可写） ──────────────────────────────────── */

lv_obj_t *ui_Screen1;
lv_obj_t *ui_RoadNameLabel;
lv_obj_t *ui_LaneContainer;
lv_obj_t *ui_TurnArrow;
lv_obj_t *ui_DistanceLabel;
lv_obj_t *ui_SpeedLabel;
lv_obj_t *ui_SpeedUnitLabel;
lv_obj_t *ui_LimitSign;
lv_obj_t *ui_LimitLabel;
lv_obj_t *ui_ChainIcon;
lv_obj_t *ui_BleDot;
lv_obj_t *ui_RouteInfoLabel;

/* ── 内部面板容器 ──────────────────────────────────────────── */

static lv_obj_t *nav_panel;    // 左侧导航 (100px)
static lv_obj_t *speed_panel;  // 中间时速 (140px)
static lv_obj_t *status_panel; // 右侧状态 (65px)

/* ── 样式表 ────────────────────────────────────────────────── */

static lv_style_t style_bg;
static lv_style_t style_panel;
static lv_style_t style_text_white;
static lv_style_t style_text_light;
static lv_style_t style_text_huge;
static lv_style_t style_lane_bar;
static lv_style_t style_limit_circle;
static lv_style_t style_ble_dot;

/* ── 链条图标像素缓存 ──────────────────────────────────────── */

#define CHAIN_W 28
#define CHAIN_H 16
static uint8_t chain_pix[CHAIN_W * CHAIN_H * 2];  // RGB565
static lv_img_dsc_t chain_img_dsc;

/* ── 内部绘制函数 ──────────────────────────────────────────── */

static void draw_chain_ring(int cx, int cy, int r, int thickness) {
    for (int y = 0; y < CHAIN_H; y++) {
        for (int x = 0; x < CHAIN_W; x++) {
            int dx = x - cx;
            int dy = (y - cy) * 2;
            int dist_sq = dx * dx + dy * dy;
            int r_outer = r + thickness / 2;
            int r_inner = r - thickness / 2 + 1;
            if (dist_sq <= r_outer * r_outer && dist_sq >= r_inner * r_inner) {
                int idx = (y * CHAIN_W + x) * 2;
                chain_pix[idx + 0] = 0xFF;
                chain_pix[idx + 1] = 0xFF;
            }
        }
    }
}

void ui_refresh_chain_icon(void) {
    memset(chain_pix, 0, sizeof(chain_pix));
    draw_chain_ring(7, 8, 4, 2);
    draw_chain_ring(19, 8, 4, 2);

    chain_img_dsc.header.always_zero = 0;
    chain_img_dsc.header.w = CHAIN_W;
    chain_img_dsc.header.h = CHAIN_H;
    chain_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    chain_img_dsc.data = chain_pix;
    chain_img_dsc.data_size = sizeof(chain_pix);

    lv_img_set_src(ui_ChainIcon, &chain_img_dsc);
    lv_obj_invalidate(ui_ChainIcon);
}

/* ══════════════════════════════════════════════════════════════
 * 样式表初始化 —— 参照 ui.txt 的设计规范
 * ══════════════════════════════════════════════════════════════ */

static void init_ui_styles(void) {
    // 主屏幕底色（深黑背景）
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x0A0A0A));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);

    // 透明面板（用于三栏区域划分，无边框无内边距）
    lv_style_init(&style_panel);
    lv_style_set_bg_opa(&style_panel, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_panel, 0);
    lv_style_set_pad_all(&style_panel, 0);

    // 标准白色文本
    lv_style_init(&style_text_white);
    lv_style_set_text_color(&style_text_white, lv_color_white());
    lv_style_set_text_align(&style_text_white, LV_TEXT_ALIGN_CENTER);

    // 浅灰色文本（辅助文字）
    lv_style_init(&style_text_light);
    lv_style_set_text_color(&style_text_light, lv_color_hex(0xB0B0B0));

    // 超大时速字体（48px Montserrat）
    lv_style_init(&style_text_huge);
    lv_style_set_text_color(&style_text_huge, lv_color_white());
    lv_style_set_text_font(&style_text_huge, &lv_font_montserrat_48);

    // 车道指引条（深蓝色圆角背景）
    lv_style_init(&style_lane_bar);
    lv_style_set_bg_color(&style_lane_bar, lv_color_hex(0x1565C0));
    lv_style_set_bg_opa(&style_lane_bar, LV_OPA_COVER);
    lv_style_set_radius(&style_lane_bar, 6);
    lv_style_set_border_width(&style_lane_bar, 0);
    lv_style_set_pad_all(&style_lane_bar, 2);

    // 限速标志（白底红圈黑字，完美正圆）
    lv_style_init(&style_limit_circle);
    lv_style_set_bg_color(&style_limit_circle, lv_color_white());
    lv_style_set_bg_opa(&style_limit_circle, LV_OPA_COVER);
    lv_style_set_border_color(&style_limit_circle, lv_color_hex(0xDC143C));
    lv_style_set_border_width(&style_limit_circle, 3);
    lv_style_set_radius(&style_limit_circle, LV_RADIUS_CIRCLE);
    lv_style_set_text_color(&style_limit_circle, lv_color_black());

    // BLE 状态点（圆形，默认灰色）
    lv_style_init(&style_ble_dot);
    lv_style_set_bg_color(&style_ble_dot, lv_color_hex(0x444444));
    lv_style_set_bg_opa(&style_ble_dot, LV_OPA_COVER);
    lv_style_set_radius(&style_ble_dot, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_ble_dot, 0);
}

/* ══════════════════════════════════════════════════════════════
 * 左侧导航面板 (100px) —— 路名 / 转向箭头 / 剩余距离
 * ══════════════════════════════════════════════════════════════ */

static void create_left_nav_panel(void) {
    nav_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(nav_panel, &style_panel, 0);
    lv_obj_set_size(nav_panel, 100, 240);
    lv_obj_set_pos(nav_panel, 5, 0);
    lv_obj_clear_flag(nav_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 路名标签（顶部居中，20px 字体，支持滚动）
    ui_RoadNameLabel = lv_label_create(nav_panel);
    lv_obj_add_style(ui_RoadNameLabel, &style_text_white, 0);
    lv_obj_set_style_text_font(ui_RoadNameLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(ui_RoadNameLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_RoadNameLabel, 100, 24);
    lv_obj_set_pos(ui_RoadNameLabel, 0, 8);
    lv_label_set_long_mode(ui_RoadNameLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // 转向箭头（中央，arrows_48 字体，含高德全 icon 映射字符）
    ui_TurnArrow = lv_label_create(nav_panel);
    lv_obj_set_style_text_color(ui_TurnArrow, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_TurnArrow, &arrows_48, 0);
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_TurnArrow, 100, 100);
    lv_obj_set_pos(ui_TurnArrow, 0, 60);
    // v0.9.1: 确保 label 内容居中于自身区域
    lv_obj_align(ui_TurnArrow, LV_ALIGN_CENTER, 0, 0);

    // 剩余距离（底部居中，24px 字体）
    ui_DistanceLabel = lv_label_create(nav_panel);
    lv_obj_set_style_text_color(ui_DistanceLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_DistanceLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(ui_DistanceLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_DistanceLabel, 100, 32);
    lv_obj_set_pos(ui_DistanceLabel, 0, 195);
}

/* ══════════════════════════════════════════════════════════════
 * 中间时速面板 (140px) —— 车道指引 / 大字时速 / km/h
 * ══════════════════════════════════════════════════════════════ */

static void create_center_speed_panel(void) {
    speed_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(speed_panel, &style_panel, 0);
    lv_obj_set_size(speed_panel, 140, 240);
    lv_obj_set_pos(speed_panel, 110, 0);
    lv_obj_clear_flag(speed_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 车道指引条（顶部，Flexbox 弹性布局自动均匀排列）
    ui_LaneContainer = lv_obj_create(speed_panel);
    lv_obj_add_style(ui_LaneContainer, &style_lane_bar, 0);
    lv_obj_set_size(ui_LaneContainer, 132, 28);
    lv_obj_set_pos(ui_LaneContainer, 4, 8);
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_LaneContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_LaneContainer,
        LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 大字时速数字（中央，48px Montserrat）
    ui_SpeedLabel = lv_label_create(speed_panel);
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_obj_add_style(ui_SpeedLabel, &style_text_huge, 0);
    lv_obj_set_style_text_align(ui_SpeedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_SpeedLabel, 140, 100);
    lv_obj_set_pos(ui_SpeedLabel, 0, 55);

    // 时速单位 "km/h"（时速下方，14px 浅灰色）
    ui_SpeedUnitLabel = lv_label_create(speed_panel);
    lv_label_set_text_static(ui_SpeedUnitLabel, "km/h");
    lv_obj_set_style_text_color(ui_SpeedUnitLabel, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_font(ui_SpeedUnitLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui_SpeedUnitLabel, 48, 155);
}

/* ══════════════════════════════════════════════════════════════
 * 右侧状态面板 (65px) —— 限速红圈 / 连接状态图标
 * ══════════════════════════════════════════════════════════════ */

static void create_right_status_panel(void) {
    status_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(status_panel, &style_panel, 0);
    lv_obj_set_size(status_panel, 65, 240);
    lv_obj_set_pos(status_panel, 255, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 限速圆圈（顶部，白底红圈黑字，50x50 正圆）
    ui_LimitSign = lv_obj_create(status_panel);
    lv_obj_add_style(ui_LimitSign, &style_limit_circle, 0);
    lv_obj_set_size(ui_LimitSign, 50, 50);
    lv_obj_set_pos(ui_LimitSign, 7, 10);
    lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_SCROLLABLE);

    ui_LimitLabel = lv_label_create(ui_LimitSign);
    lv_label_set_text_static(ui_LimitLabel, "");
    lv_obj_center(ui_LimitLabel);
    lv_obj_set_style_text_font(ui_LimitLabel, &lv_font_montserrat_20, 0);

    // 链条连接图标（底部居中，28x16 自绘像素图）
    ui_ChainIcon = lv_img_create(status_panel);
    lv_obj_set_pos(ui_ChainIcon, 18, 175);
    lv_obj_set_size(ui_ChainIcon, CHAIN_W, CHAIN_H);
    ui_refresh_chain_icon();
}

/* ══════════════════════════════════════════════════════════════
 * 底部：剩余全程信息（横跨左下角）
 * ══════════════════════════════════════════════════════════════ */

static void create_bottom_route_info(void) {
    ui_RouteInfoLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_RouteInfoLabel, "");
    lv_obj_set_style_text_color(ui_RouteInfoLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(ui_RouteInfoLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui_RouteInfoLabel, 5, 225);
    lv_obj_set_size(ui_RouteInfoLabel, 250, 16);
}

/* ══════════════════════════════════════════════════════════════
 * 主入口：初始化静态 UI 框架
 * 在 LVGL + TFT 驱动初始化完成后调用一次。
 * 本函数不包含任何数据逻辑，仅创建静态视图骨架。
 * ══════════════════════════════════════════════════════════════ */

void ui_init(void) {
    // 1. 初始化样式表
    init_ui_styles();

    // 2. 创建主屏幕
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_add_style(ui_Screen1, &style_bg, 0);
    lv_obj_set_size(ui_Screen1, 320, 240);

    // 3. BLE 连接状态小点（左上角 6x6）
    ui_BleDot = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_BleDot, 6, 6);
    lv_obj_set_pos(ui_BleDot, 2, 2);
    lv_obj_add_style(ui_BleDot, &style_ble_dot, 0);
    lv_obj_clear_flag(ui_BleDot, LV_OBJ_FLAG_SCROLLABLE);

    // 4. 构建左、中、右三大面板
    create_left_nav_panel();
    create_center_speed_panel();
    create_right_status_panel();

    // 5. 底部全程信息
    create_bottom_route_info();

    // 6. 加载屏幕
    lv_disp_load_scr(ui_Screen1);
}