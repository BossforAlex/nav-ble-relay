#include "ui.h"
#include "fonts/fonts.h"
#include <stdio.h>
#include <string.h>

// ══════════════════════════════════════════════════════════════
// v0.6.6: LVGL UI 初始化
// 完全对照 a.jpg 参考图重新设计
//   - 左上：道路名（白色，~20px）
//   - 顶部居中：蓝色车道条（多个白色上箭头）
//   - 左中：超大白色转向箭头（fork 风格）
//   - 左下：路口距离 "725米"（白色，~24px）
//   - 中央：超大白色车速数字（48px）
//   - 右上：红圈限速牌（带数字）
//   - 右下：白色链条图标
//   - 左下角：剩余全程信息
//   - 背景：近黑色 #0A0A0A
// ══════════════════════════════════════════════════════════════

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

// 样式
static lv_style_t style_bg;
static lv_style_t style_text_white;        // 白色文字
static lv_style_t style_text_light;        // 浅灰文字
static lv_style_t style_text_huge;         // 超大白色文字（车速）
static lv_style_t style_lane_blue;         // 蓝色车道背景
static lv_style_t style_limit_red;         // 红色限速圈
static lv_style_t style_ble_dot;           // BLE 状态点

// v0.6.6: 自定义链条 icon 像素缓存（28x16，RGB565）
#define CHAIN_W 28
#define CHAIN_H 16
static uint8_t chain_pix[CHAIN_W * CHAIN_H * 2];  // RGB565
static lv_img_dsc_t chain_img_dsc;

// 内部静态函数：画一个椭圆环到 chain_pix
static void draw_chain_ring(int cx, int cy, int r, int thickness) {
    for (int y = 0; y < CHAIN_H; y++) {
        for (int x = 0; x < CHAIN_W; x++) {
            int dx = x - cx;
            int dy = (y - cy) * 2;  // 拉伸因子 2 让圆变椭圆以适应 28x16
            int dist_sq = dx * dx + dy * dy;
            int r_outer = r + thickness / 2;
            int r_inner = r - thickness / 2 + 1;
            if (dist_sq <= r_outer * r_outer && dist_sq >= r_inner * r_inner) {
                int idx = (y * CHAIN_W + x) * 2;
                chain_pix[idx + 0] = 0xFF;     // RGB565 白: 0xFFFF (低字节)
                chain_pix[idx + 1] = 0xFF;     // RGB565 白: 0xFFFF (高字节)
            }
        }
    }
}

// 在屏幕上生成链条图标（用 chain_pix 像素缓存 → lv_img_dsc → ui_ChainIcon）
void ui_refresh_chain_icon(void) {
    // 清空
    memset(chain_pix, 0, sizeof(chain_pix));

    // 画两个互锁的环
    draw_chain_ring(7, 8, 4, 2);
    draw_chain_ring(19, 8, 4, 2);

    // 配置 img descriptor
    chain_img_dsc.header.always_zero = 0;
    chain_img_dsc.header.w = CHAIN_W;
    chain_img_dsc.header.h = CHAIN_H;
    chain_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565
    chain_img_dsc.data = chain_pix;
    chain_img_dsc.data_size = sizeof(chain_pix);

    lv_img_set_src(ui_ChainIcon, &chain_img_dsc);
    lv_obj_invalidate(ui_ChainIcon);
}

void ui_init(void)
{
    // ── 全局样式 ──
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x0A0A0A));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bg, 0);

    lv_style_init(&style_text_white);
    lv_style_set_text_color(&style_text_white, lv_color_white());
    lv_style_set_text_align(&style_text_white, LV_TEXT_ALIGN_LEFT);

    lv_style_init(&style_text_light);
    lv_style_set_text_color(&style_text_light, lv_color_hex(0xE0E0E0));

    lv_style_init(&style_text_huge);
    lv_style_set_text_color(&style_text_huge, lv_color_white());
    lv_style_set_text_font(&style_text_huge, &lv_font_montserrat_48);

    lv_style_init(&style_lane_blue);
    lv_style_set_bg_color(&style_lane_blue, lv_color_hex(0x1565C0));
    lv_style_set_bg_opa(&style_lane_blue, LV_OPA_COVER);
    lv_style_set_radius(&style_lane_blue, 6);
    lv_style_set_border_width(&style_lane_blue, 0);

    lv_style_init(&style_limit_red);
    lv_style_set_bg_color(&style_limit_red, lv_color_hex(0x1A0000));  // 暗红底
    lv_style_set_bg_opa(&style_limit_red, LV_OPA_COVER);
    lv_style_set_border_color(&style_limit_red, lv_color_hex(0xD32F2F));
    lv_style_set_border_width(&style_limit_red, 3);
    lv_style_set_radius(&style_limit_red, LV_RADIUS_CIRCLE);

    lv_style_init(&style_ble_dot);
    lv_style_set_bg_color(&style_ble_dot, lv_color_hex(0x444444));
    lv_style_set_bg_opa(&style_ble_dot, LV_OPA_COVER);
    lv_style_set_radius(&style_ble_dot, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_ble_dot, 0);

    // ── 主屏幕 ──
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_add_style(ui_Screen1, &style_bg, 0);
    lv_obj_set_size(ui_Screen1, 320, 240);

    // ── 左上角：BLE 状态指示点（极小装饰） ──
    ui_BleDot = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_BleDot, 6, 6);
    lv_obj_set_pos(ui_BleDot, 4, 4);
    lv_obj_add_style(ui_BleDot, &style_ble_dot, 0);
    lv_obj_clear_flag(ui_BleDot, LV_OBJ_FLAG_SCROLLABLE);

    // ── 顶部：道路名称（左上，避开 BLE 点） ──
    ui_RoadNameLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_RoadNameLabel, "");
    lv_obj_add_style(ui_RoadNameLabel, &style_text_white, 0);
    lv_obj_set_style_text_font(ui_RoadNameLabel, &cjk_20, 0);
    lv_obj_set_pos(ui_RoadNameLabel, 14, 6);
    lv_obj_set_width(ui_RoadNameLabel, 130);
    lv_label_set_long_mode(ui_RoadNameLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // ── 顶部居中：蓝色车道指引容器 ──
    // 位置：(54, 4)，尺寸：(212, 36)，圆角蓝条
    ui_LaneContainer = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_LaneContainer, 212, 36);
    lv_obj_set_pos(ui_LaneContainer, 54, 4);
    lv_obj_add_style(ui_LaneContainer, &style_lane_blue, 0);
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_LaneContainer, 0, 0);

    // ── 右上：限速圆圈 ──
    // 位置：(266, 6)，尺寸：(48, 48)，红边圆圈
    ui_LimitSign = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_LimitSign, 48, 48);
    lv_obj_set_pos(ui_LimitSign, 266, 6);
    lv_obj_add_style(ui_LimitSign, &style_limit_red, 0);
    lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_SCROLLABLE);

    ui_LimitLabel = lv_label_create(ui_LimitSign);
    lv_label_set_text_static(ui_LimitLabel, "");
    lv_obj_center(ui_LimitLabel);
    lv_obj_set_style_text_color(ui_LimitLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_LimitLabel, &cjk_20, 0);

    // ── 左侧中：超大转向箭头 ──
    // 位置：(0, 60)，尺寸：(110, 110)，纯白色 fork 风格箭头
    ui_TurnArrow = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_TurnArrow, "");
    lv_obj_set_style_text_color(ui_TurnArrow, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_TurnArrow, &arrows_48, 0);
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_TurnArrow, 110, 110);
    lv_obj_set_pos(ui_TurnArrow, 0, 60);

    // ── 左侧下：路口距离 "725米" ──
    // 位置：(8, 178)，白色，~24px
    ui_DistanceLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_DistanceLabel, "");
    lv_obj_set_style_text_color(ui_DistanceLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_DistanceLabel, &cjk_24, 0);
    lv_obj_set_style_text_align(ui_DistanceLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_DistanceLabel, 110, 36);
    lv_obj_set_pos(ui_DistanceLabel, 4, 178);

    // ── 中央：超大车速数字 "0" ──
    // 位置：(110, 60)，尺寸：(160, 130)，48 字号
    ui_SpeedLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_obj_add_style(ui_SpeedLabel, &style_text_huge, 0);
    lv_obj_set_style_text_align(ui_SpeedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_SpeedLabel, 160, 100);
    lv_obj_set_pos(ui_SpeedLabel, 110, 60);

    // 车速单位 km/h（车速数字右下角，小字）
    ui_SpeedUnitLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_SpeedUnitLabel, "km/h");
    lv_obj_set_style_text_color(ui_SpeedUnitLabel, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_font(ui_SpeedUnitLabel, &cjk_14, 0);
    lv_obj_set_pos(ui_SpeedUnitLabel, 196, 162);

    // ── 右下：链条图标（v0.6.6 新增，参考 a.jpg） ──
    // 28x16 像素，水平居中放在 248~276 x 170~200
    ui_ChainIcon = lv_img_create(ui_Screen1);
    lv_obj_set_pos(ui_ChainIcon, 248, 168);
    lv_obj_set_size(ui_ChainIcon, CHAIN_W, CHAIN_H);
    // 初始画一次（空数据时也能看见链条，确认 UI 状态）
    ui_refresh_chain_icon();

    // ── 左下角：剩余全程信息 "全程 12.5km / 25分钟" ──
    ui_RouteInfoLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_RouteInfoLabel, "");
    lv_obj_set_style_text_color(ui_RouteInfoLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(ui_RouteInfoLabel, &cjk_14, 0);
    lv_obj_set_pos(ui_RouteInfoLabel, 4, 222);
    lv_obj_set_size(ui_RouteInfoLabel, 200, 16);

    lv_disp_load_scr(ui_Screen1);
}
