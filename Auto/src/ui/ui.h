#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "fonts/fonts.h"

extern lv_obj_t *ui_Screen1;

// 顶部：道路名称（左上）
extern lv_obj_t *ui_RoadNameLabel;
// 顶部：车道指引容器（居中蓝条）
extern lv_obj_t *ui_LaneContainer;
// 左侧：转向大箭头
extern lv_obj_t *ui_TurnArrow;
// 左侧：路口距离文字（"725米"）
extern lv_obj_t *ui_DistanceLabel;
// 中央：当前车速（超大数字 "0"）
extern lv_obj_t *ui_SpeedLabel;
extern lv_obj_t *ui_SpeedUnitLabel;
// 右上：限速圆圈 + 数字
extern lv_obj_t *ui_LimitSign;
extern lv_obj_t *ui_LimitLabel;
// 右下：链条/电子眼图标（v0.6.6 重新设计：用 canvas 绘制自定义链条）
extern lv_obj_t *ui_ChainIcon;
// 左上：BLE 连接状态小点
extern lv_obj_t *ui_BleDot;
// 左下角：剩余总路程/时间（v0.6.6 新增）
extern lv_obj_t *ui_RouteInfoLabel;

void ui_init(void);

// v0.6.6: 程序化绘制链条图标（每次初始化后 / 链条改变时调用）
void ui_refresh_chain_icon(void);

#ifdef __cplusplus
}
#endif

#endif
