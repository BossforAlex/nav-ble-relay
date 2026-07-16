#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "fonts/fonts.h"

/* ══════════════════════════════════════════════════════════════
 * v0.9.0 — 静态 UI 框架（参照 ui.txt 设计规范）
 *
 * 架构原则：
 *   1. UI 框架只负责创建控件和设置样式，不包含任何数据解析逻辑
 *   2. 所有动态数据由 ScreenLVGL（软件层）通过直接设置控件属性写入
 *   3. 高德导航方向箭头遵循 AmapAuto SDK 官方 icon 定义
 *   4. 方便后期更换不同屏幕驱动（TFT/OLED/串口等），只需实现 Screen 接口
 *
 * 高德导航方向箭头 icon 映射（AmapAuto SDK）：
 *   icon  0: 左转     ←
 *   icon  1: 直行     ↑
 *   icon  2: 右转     →
 *   icon  3: 左前方掉头  ↶
 *   icon  4: 左前方    ↰
 *   icon  5: 右前方    ↱
 *   icon  6: 左后方    ↶
 *   icon  7: 右后方    ↷
 *   icon  8: 调头     ↷
 *   icon  9: 延续直行   ↑
 *   icon 15: 到达目的地  ★
 *   icon 19: 调头(旧版) ↷
 *   icon 20: 环岛     ◎
 *
 * 车道 backIcon 映射 (AmapAuto)：
 *   0: 左转    1: 直行    2: 右转
 *   3: 左+直   4: 直+右   6: 调头
 *
 * 三栏布局 (320x240)：
 *   +-------------------+-----------------------+---------------+
 *   | 左侧导航 (136px)   | 中间时速 (112px)       | 右侧状态 (72px)|
 *   | [路名占位]         | [车道指引条 28px]      |  [限速80]     |
 *   |                   |                       |               |
 *   |    ( 箭头 )       |      ( 0 )            |               |
 *   |                   |   [大字时速]           |               |
 *   |                   |                       |    ( 链 )     |
 *   | [725 m]           | [km/h]                |               |
 *   +-------------------+-----------------------+---------------+
 *   总宽: 136+112+72 = 320px
 * ══════════════════════════════════════════════════════════════ */

extern lv_obj_t *ui_Screen1;

// 左侧导航栏
extern lv_obj_t *ui_RoadNameLabel;   // 路名（顶部居中，软件层写入）
extern lv_obj_t *ui_TurnArrow;       // 转向大箭头（中央，arrows_48 字体）
extern lv_obj_t *ui_DistanceLabel;   // 剩余距离（底部，软件层写入）

// 中间时速区
extern lv_obj_t *ui_LaneContainer;   // 车道指引容器（Flexbox 弹性布局）
extern lv_obj_t *ui_SpeedLabel;      // 大字时速数字（48px）
extern lv_obj_t *ui_SpeedUnitLabel;  // 速度单位 "km/h"

// 右侧状态区
extern lv_obj_t *ui_LimitSign;       // 限速圆圈背景（白底红圈）
extern lv_obj_t *ui_LimitLabel;      // 限速数字（居中）
extern lv_obj_t *ui_ChainIcon;       // 蓝牙连接状态图标（自绘链条）

// 全局装饰
extern lv_obj_t *ui_BleDot;          // BLE 连接状态小点（左上角 6x6）
extern lv_obj_t *ui_RouteInfoLabel;  // 剩余全程信息（底部，软件层写入）

/**
 * @brief 初始化静态 UI 框架
 *
 * 创建所有控件、设置样式，加载到当前屏幕。
 * 应在 LVGL + TFT 驱动初始化完成后调用。
 * 本函数不包含任何数据逻辑，仅创建静态视图骨架。
 */
void ui_init(void);

/**
 * @brief 刷新链条图标
 *
 * 程序化绘制两个互锁椭圆环，用作 BLE 连接状态图标。
 * 在 ui_init() 中自动调用，也可在需要时手动调用。
 */
void ui_refresh_chain_icon(void);

#ifdef __cplusplus
}
#endif

#endif
