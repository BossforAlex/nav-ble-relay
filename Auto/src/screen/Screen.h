#pragma once

/**
 * @file Screen.h
 * @brief 屏幕抽象接口 —— 纯展示层
 *
 * 架构原则（v0.9.0）：
 *   1. 屏幕只负责展示，不包含任何数据解析逻辑
 *   2. 数据解析由 nav::NavParser 完成（软件层）
 *   3. main.cpp 作为桥接层，将 NavState 转换为 Screen 方法调用
 *   4. 方便后期更换不同屏幕（TFT/OLED/串口），只需实现此接口
 *
 * 两种使用模式：
 *   A) setNavState() 一次性传入整个 NavState（当前 loop 使用）
 *   B) 单独调用 showXxx() 方法（未来 Flutter 端直接推送单字段时使用）
 */

#include "nav/NavData.h"

class Screen {
public:
    virtual ~Screen() = default;

    // ── 生命周期 ──
    virtual bool init() = 0;
    virtual void update() = 0;
    virtual void log(const char* msg) = 0;

    // ── 批量更新（当前 loop 使用） ──
    virtual void setNavState(const Nav::NavState& state) = 0;
    virtual void setBleConnected(bool connected) = 0;

    // ── 单字段更新（未来 Flutter 端直接推送时使用） ──
    // 高德导航方向箭头 icon 映射（AmapAuto SDK）：
    //   0:←左转  1:↑直行  2:→右转  3:↶左前掉头  4:↰左前  5:↱右前
    //   6:↶左后  7:↷右后  8:↷调头  9:↑延续  15:★到达  19:↷调头  20:◎环岛
    virtual void showArrow(int amapIcon) = 0;
    virtual void showDistance(const char* text) = 0;
    virtual void showSpeed(int speed) = 0;
    virtual void showSpeedLimit(int limit, bool overSpeed) = 0;
    virtual void showRoadName(const char* name) = 0;
    virtual void showLanes(int count, const int* backIcons) = 0;
    virtual void showRouteInfo(const char* text) = 0;
    virtual void showIdle() = 0;

    // v0.9.3: 背光独立控制（默认无操作，ScreenLVGL 重写）
    virtual void enableBacklight() {}
};