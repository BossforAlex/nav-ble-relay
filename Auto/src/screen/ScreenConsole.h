#pragma once

/**
 * @file ScreenConsole.h
 * @brief 串口直通显示模式（纯展示层）
 *
 * 不模拟虚拟屏幕，仅在数据更新时打印结构化数据。
 * 实现 Screen 纯展示接口，方便后期更换不同屏幕。
 */

#include "Screen.h"

class ScreenConsole : public Screen {
public:
    bool init() override;
    void update() override;
    void log(const char* msg) override;

    void setNavState(const Nav::NavState& state) override;
    void setBleConnected(bool connected) override;

    // 单字段展示方法（串口模式下仅打印结构化日志）
    void showArrow(int amapIcon) override;
    void showDistance(const char* text) override;
    void showSpeed(int speed) override;
    void showSpeedLimit(int limit, bool overSpeed) override;
    void showRoadName(const char* name) override;
    void showLanes(int count, const int* backIcons, int turnIcon = -1) override;
    void showRouteInfo(const char* text) override;
    void showIdle() override;

private:
    Nav::NavState mState;
    bool mBleConnected = false;
    bool mInitialized = false;
    unsigned long mLastPrintedMs = 0;

    void printNavStateLine();
};