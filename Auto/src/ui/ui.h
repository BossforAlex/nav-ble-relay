#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "fonts/fonts.h"

// ══════════════════════════════════════════════════════════════
// v0.8.0: 三栏黄金比例布局
//   左 (100px): 路名 → 转向箭头 → 距离
//   中 (140px): 车道指引条 (Flexbox) → 大字时速 → km/h
//   右 ( 65px): 限速红圈 → 连接状态图标
// ══════════════════════════════════════════════════════════════

extern lv_obj_t *ui_Screen1;

// 左侧导航栏
extern lv_obj_t *ui_RoadNameLabel;   // 路名（顶部居中）
extern lv_obj_t *ui_TurnArrow;       // 转向大箭头（中央）
extern lv_obj_t *ui_DistanceLabel;   // 剩余距离（底部）

// 中间时速区
extern lv_obj_t *ui_LaneContainer;   // 车道指引容器（Flexbox 弹性布局）
extern lv_obj_t *ui_SpeedLabel;      // 大字时速数字
extern lv_obj_t *ui_SpeedUnitLabel;  // 速度单位 km/h

// 右侧状态区
extern lv_obj_t *ui_LimitSign;       // 限速圆圈背景
extern lv_obj_t *ui_LimitLabel;      // 限速数字
extern lv_obj_t *ui_ChainIcon;       // 蓝牙连接状态图标

// 全局装饰
extern lv_obj_t *ui_BleDot;          // BLE 连接状态小点（左上角）
extern lv_obj_t *ui_RouteInfoLabel;  // 剩余全程信息（底部横跨）

void ui_init(void);

// v0.6.6: 程序化绘制链条图标（每次初始化后 / 链条改变时调用）
void ui_refresh_chain_icon(void);

#ifdef __cplusplus
}
#endif

#endif
