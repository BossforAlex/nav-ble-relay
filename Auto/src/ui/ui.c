#include "ui.h"
#include "fonts/fonts.h"
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 * v1.0.0 — Night-drive cockpit UI (Material-inspired / TFT driving-first)
 *
 * Design: Deep-space black background, electric-blue instrument glow,
 * high-contrast white numerals, red warning accents.
 * 320x240 landscape, 3-panel: 136px nav / 112px speed / 72px status.
 *
 * v1.0.0 changes:
 *   - Refined color palette: cyan primary (#4FC3F7), amber warnings
 *   - Lane bar: rounded active indicators with glow-like borders
 *   - Speed gauge: monospace numerals with subtle separators
 *   - Status panel: polished limit sign with shadow-like border
 *   - Bottom route bar: subtle top-border separator
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

static lv_obj_t *nav_panel;
static lv_obj_t *speed_panel;
static lv_obj_t *status_panel;

#define SCREEN_W 320
#define SCREEN_H 240
#define NAV_W 136
#define SPEED_W 112
#define STATUS_W 72
#define SPEED_X NAV_W
#define STATUS_X (NAV_W + SPEED_W)

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
static uint8_t chain_pix[CHAIN_W * CHAIN_H * 2];
static lv_img_dsc_t chain_img_dsc;

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
 * 样式表初始化
 * ══════════════════════════════════════════════════════════════ */

static void init_ui_styles(void) {
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);

    lv_style_init(&style_panel);
    lv_style_set_bg_opa(&style_panel, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_panel, 0);
    lv_style_set_pad_all(&style_panel, 0);

    lv_style_init(&style_text_white);
    lv_style_set_text_color(&style_text_white, lv_color_white());
    lv_style_set_text_align(&style_text_white, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_text_light);
    lv_style_set_text_color(&style_text_light, lv_color_hex(0x90A4AE));

    lv_style_init(&style_text_huge);
    lv_style_set_text_color(&style_text_huge, lv_color_white());
    lv_style_set_text_font(&style_text_huge, &lv_font_montserrat_48);

    lv_style_init(&style_lane_bar);
    lv_style_set_bg_color(&style_lane_bar, lv_color_hex(0x0A1018));
    lv_style_set_bg_opa(&style_lane_bar, LV_OPA_COVER);
    lv_style_set_radius(&style_lane_bar, 6);
    lv_style_set_border_width(&style_lane_bar, 1);
    lv_style_set_border_color(&style_lane_bar, lv_color_hex(0x1E2A38));
    lv_style_set_pad_all(&style_lane_bar, 3);

    lv_style_init(&style_limit_circle);
    lv_style_set_bg_color(&style_limit_circle, lv_color_white());
    lv_style_set_bg_opa(&style_limit_circle, LV_OPA_COVER);
    lv_style_set_border_color(&style_limit_circle, lv_color_hex(0xFF5252));
    lv_style_set_border_width(&style_limit_circle, 3);
    lv_style_set_radius(&style_limit_circle, LV_RADIUS_CIRCLE);
    lv_style_set_text_color(&style_limit_circle, lv_color_black());

    lv_style_init(&style_ble_dot);
    lv_style_set_bg_color(&style_ble_dot, lv_color_hex(0x607D8B));
    lv_style_set_bg_opa(&style_ble_dot, LV_OPA_COVER);
    lv_style_set_radius(&style_ble_dot, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_ble_dot, 0);
}

/* ══════════════════════════════════════════════════════════════
 * 左侧导航面板 (136px) — 路名 / 转向箭头 / 剩余距离
 * ══════════════════════════════════════════════════════════════ */

static void create_left_nav_panel(void) {
    nav_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(nav_panel, &style_panel, 0);
    lv_obj_set_size(nav_panel, NAV_W, SCREEN_H);
    lv_obj_set_pos(nav_panel, 0, 0);
    lv_obj_clear_flag(nav_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_RoadNameLabel = lv_label_create(nav_panel);
    lv_obj_add_style(ui_RoadNameLabel, &style_text_white, 0);
    lv_obj_set_style_text_font(ui_RoadNameLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(ui_RoadNameLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_RoadNameLabel, NAV_W, 24);
    lv_obj_set_pos(ui_RoadNameLabel, 0, 4);
    lv_label_set_long_mode(ui_RoadNameLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);

    ui_TurnArrow = lv_label_create(nav_panel);
    lv_obj_set_style_text_color(ui_TurnArrow, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_style_text_font(ui_TurnArrow, &arrows_48, 0);
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_TurnArrow, NAV_W, 96);
    lv_obj_set_pos(ui_TurnArrow, 0, 40);

    ui_DistanceLabel = lv_label_create(nav_panel);
    lv_obj_set_style_text_color(ui_DistanceLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_DistanceLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(ui_DistanceLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_DistanceLabel, NAV_W, 34);
    lv_obj_set_pos(ui_DistanceLabel, 0, 140);
}

/* ══════════════════════════════════════════════════════════════
 * 中间时速面板 (112px) — 车道指引 / 大字时速 / km/h
 * ══════════════════════════════════════════════════════════════ */

static void create_center_speed_panel(void) {
    speed_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(speed_panel, &style_panel, 0);
    lv_obj_set_size(speed_panel, SPEED_W, SCREEN_H);
    lv_obj_set_pos(speed_panel, SPEED_X, 0);
    lv_obj_clear_flag(speed_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_LaneContainer = lv_obj_create(speed_panel);
    lv_obj_add_style(ui_LaneContainer, &style_lane_bar, 0);
    lv_obj_set_size(ui_LaneContainer, SPEED_W - 8, 32);
    lv_obj_set_pos(ui_LaneContainer, 4, 6);
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_LaneContainer,
        LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_LaneContainer,
        LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_SpeedLabel = lv_label_create(speed_panel);
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_obj_add_style(ui_SpeedLabel, &style_text_huge, 0);
    lv_obj_set_style_text_align(ui_SpeedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_SpeedLabel, SPEED_W, 72);
    lv_obj_set_pos(ui_SpeedLabel, 0, 62);

    ui_SpeedUnitLabel = lv_label_create(speed_panel);
    lv_label_set_text_static(ui_SpeedUnitLabel, "km/h");
    lv_obj_set_style_text_color(ui_SpeedUnitLabel, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(ui_SpeedUnitLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(ui_SpeedUnitLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_SpeedUnitLabel, SPEED_W, 20);
    lv_obj_set_pos(ui_SpeedUnitLabel, 0, 130);
}

/* ══════════════════════════════════════════════════════════════
 * 右侧状态面板 (72px) — 限速红圈 / 连接图标
 * ══════════════════════════════════════════════════════════════ */

static void create_right_status_panel(void) {
    status_panel = lv_obj_create(ui_Screen1);
    lv_obj_add_style(status_panel, &style_panel, 0);
    lv_obj_set_size(status_panel, STATUS_W, SCREEN_H);
    lv_obj_set_pos(status_panel, STATUS_X, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_LimitSign = lv_obj_create(status_panel);
    lv_obj_add_style(ui_LimitSign, &style_limit_circle, 0);
    lv_obj_set_size(ui_LimitSign, 52, 52);
    lv_obj_set_pos(ui_LimitSign, 10, 8);
    lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_SCROLLABLE);

    ui_LimitLabel = lv_label_create(ui_LimitSign);
    lv_label_set_text_static(ui_LimitLabel, "");
    lv_obj_center(ui_LimitLabel);
    lv_obj_set_style_text_font(ui_LimitLabel, &lv_font_montserrat_20, 0);

    ui_ChainIcon = lv_img_create(status_panel);
    lv_obj_set_pos(ui_ChainIcon, 22, 76);
    lv_obj_set_size(ui_ChainIcon, CHAIN_W, CHAIN_H);
    ui_refresh_chain_icon();
}

/* ══════════════════════════════════════════════════════════════
 * 底部：剩余全程信息（全宽底部状态行）
 * ══════════════════════════════════════════════════════════════ */

static void create_bottom_route_info(void) {
    ui_RouteInfoLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_RouteInfoLabel, "");
    lv_obj_set_style_text_color(ui_RouteInfoLabel, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(ui_RouteInfoLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(ui_RouteInfoLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui_RouteInfoLabel, 8, 214);
    lv_obj_set_size(ui_RouteInfoLabel, SCREEN_W - 16, 20);
}

/* ══════════════════════════════════════════════════════════════
 * 面板分隔线
 * ══════════════════════════════════════════════════════════════ */

static void create_separators(void) {
    lv_obj_t* sep1 = lv_obj_create(ui_Screen1);
    lv_obj_set_size(sep1, 1, SCREEN_H);
    lv_obj_set_pos(sep1, SPEED_X, 0);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(0x1E2A38), 0);
    lv_obj_set_style_bg_opa(sep1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_clear_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sep2 = lv_obj_create(ui_Screen1);
    lv_obj_set_size(sep2, 1, SCREEN_H);
    lv_obj_set_pos(sep2, STATUS_X, 0);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x1E2A38), 0);
    lv_obj_set_style_bg_opa(sep2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);
}

/* ══════════════════════════════════════════════════════════════
 * 主入口：初始化静态 UI 框架
 * ══════════════════════════════════════════════════════════════ */

void ui_init(void) {
    init_ui_styles();

    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_add_style(ui_Screen1, &style_bg, 0);
    lv_obj_set_size(ui_Screen1, SCREEN_W, SCREEN_H);

    ui_BleDot = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_BleDot, 6, 6);
    lv_obj_set_pos(ui_BleDot, 2, 2);
    lv_obj_add_style(ui_BleDot, &style_ble_dot, 0);
    lv_obj_clear_flag(ui_BleDot, LV_OBJ_FLAG_SCROLLABLE);

    create_left_nav_panel();
    create_center_speed_panel();
    create_right_status_panel();
    create_separators();
    create_bottom_route_info();

    lv_disp_load_scr(ui_Screen1);
}
