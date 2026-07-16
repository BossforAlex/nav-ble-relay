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
    // v0.9.8: 高德导航方向箭头 icon 映射（AmapAuto SDK 官方协议）
    //   0:↑未定义  1:↑直行  2:←左转  3:→右转  4:↖左前方  5:↗右前方
    //   6:↙左后方  7:↘右后方  8:↶左转掉头  9:↑直行  10:★途经点
    //   11:◎环岛  12:◎出环岛  13:★服务区  14:★收费站  15:★到达
    //   16:◎隧道  17:◎环岛(左行)  18:◎出环岛(左行)  19:↷右转掉头  20:↑顺行
    virtual void showArrow(int amapIcon) = 0;
    virtual void showDistance(const char* text) = 0;
    virtual void showSpeed(int speed) = 0;
    virtual void showSpeedLimit(int limit, bool overSpeed) = 0;
    virtual void showRoadName(const char* name) = 0;
    virtual void showLanes(int count, const int* backIcons, int turnIcon = -1) = 0;
    virtual void showRouteInfo(const char* text) = 0;
    virtual void showIdle() = 0;

    // v0.9.3: 背光独立控制（默认无操作，ScreenLVGL 重写）
    virtual void enableBacklight() {}
};