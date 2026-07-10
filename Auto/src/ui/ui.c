#include "ui.h"
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// v0.6.5: LVGL UI 初始化
// 移植自 https://github.com/BossforAlex/LVGL-NAV
// 适配到 AutoNavDisplay 项目：
//   - 分辨率 320x240（ILI9341 2.8" 屏）
//   - 保留原 UI 元素：路名、车道、转向箭头、距离、车速、限速、摄像头
//   - 增加底部区域显示路况光柱和全程信息（由 ScreenLVGL 动态更新）
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
lv_obj_t *ui_CameraIcon;

// 样式
static lv_style_t style_bg;
static lv_style_t style_text_light;
static lv_style_t style_text_big;
static lv_style_t style_lane_blue;
static lv_style_t style_limit_red;

// 摄像头符号模拟（LVGL 没有摄像头 symbol，用 VIDEO 符号替代）
static const char* CAMERA_SYMBOL = LV_SYMBOL_VIDEO;

void ui_init(void)
{
    // ── 全局样式 ──
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x0A0A0A));
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);

    lv_style_init(&style_text_light);
    lv_style_set_text_color(&style_text_light, lv_color_hex(0xE0E0E0));

    lv_style_init(&style_text_big);
    lv_style_set_text_color(&style_text_big, lv_color_white());
    lv_style_set_text_font(&style_text_big, &lv_font_montserrat_48);

    lv_style_init(&style_lane_blue);
    lv_style_set_bg_color(&style_lane_blue, lv_color_hex(0x1565C0));
    lv_style_set_bg_opa(&style_lane_blue, LV_OPA_COVER);
    lv_style_set_radius(&style_lane_blue, 6);
    lv_style_set_border_width(&style_lane_blue, 0);

    lv_style_init(&style_limit_red);
    lv_style_set_bg_color(&style_limit_red, lv_color_black());
    lv_style_set_bg_opa(&style_limit_red, LV_OPA_COVER);
    lv_style_set_border_color(&style_limit_red, lv_color_hex(0xD32F2F));
    lv_style_set_border_width(&style_limit_red, 3);
    lv_style_set_radius(&style_limit_red, LV_RADIUS_CIRCLE);

    // ── 主屏幕 ──
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_add_style(ui_Screen1, &style_bg, 0);

    // 道路名称（左上角）
    ui_RoadNameLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_RoadNameLabel, "等待导航数据");
    lv_obj_add_style(ui_RoadNameLabel, &style_text_light, 0);
    lv_obj_set_style_text_font(ui_RoadNameLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(ui_RoadNameLabel, 12, 10);

    // 车道指引容器（顶部居中，原项目尺寸 212x38）
    ui_LaneContainer = lv_obj_create(ui_Screen1);
    lv_obj_set_size(ui_LaneContainer, 212, 38);
    lv_obj_set_pos(ui_LaneContainer, 54, 6);
    lv_obj_add_style(ui_LaneContainer, &style_lane_blue, 0);
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_LaneContainer, 0, 0);

    // 转向箭头（左侧，大）
    ui_TurnArrow = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_TurnArrow, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(ui_TurnArrow, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_TurnArrow, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(ui_TurnArrow, 44, 60);

    // 距离（箭头下方）
    ui_DistanceLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_DistanceLabel, "725米");
    lv_obj_add_style(ui_DistanceLabel, &style_text_light, 0);
    lv_obj_set_style_text_font(ui_DistanceLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(ui_DistanceLabel, 36, 116);

    // 车速数字（中央偏右，大）
    ui_SpeedLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_obj_add_style(ui_SpeedLabel, &style_text_big, 0);
    lv_obj_set_pos(ui_SpeedLabel, 156, 54);

    // 车速单位
    ui_SpeedUnitLabel = lv_label_create(ui_Screen1);
    lv_label_set_text_static(ui_SpeedUnitLabel, "km/h");
    lv_obj_add_style(ui_SpeedUnitLabel, &style_text_light, 0);
    lv_obj_set_style_text_font(ui_SpeedUnitLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui_SpeedUnitLabel, 172, 112);

    // 右侧容器（限速 + 摄像头）
    lv_obj_t *right_cont = lv_obj_create(ui_Screen1);
    lv_obj_set_size(right_cont, 60, 120);
    lv_obj_set_pos(right_cont, 258, 20);
    lv_obj_set_style_bg_opa(right_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_cont, 0, 0);
    lv_obj_clear_flag(right_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(right_cont, 0, 0);

    // 限速圆圈
    ui_LimitSign = lv_obj_create(right_cont);
    lv_obj_set_size(ui_LimitSign, 44, 44);
    lv_obj_set_pos(ui_LimitSign, 8, 0);
    lv_obj_add_style(ui_LimitSign, &style_limit_red, 0);
    lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_SCROLLABLE);

    ui_LimitLabel = lv_label_create(ui_LimitSign);
    lv_label_set_text_static(ui_LimitLabel, "80");
    lv_obj_center(ui_LimitLabel);
    lv_obj_set_style_text_color(ui_LimitLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_LimitLabel, &lv_font_montserrat_20, 0);

    // 摄像头图标
    ui_CameraIcon = lv_label_create(right_cont);
    lv_label_set_text_static(ui_CameraIcon, CAMERA_SYMBOL);
    lv_obj_set_style_text_color(ui_CameraIcon, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_CameraIcon, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(ui_CameraIcon, 16, 56);

    lv_disp_load_scr(ui_Screen1);
}
