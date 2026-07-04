#pragma once

/**
 * @file ScreenConsole.h
 * @brief 串口直通显示模式
 *
 * 不模拟虚拟屏幕（无 ASCII 框图），仅在数据更新时打印结构化数据。
 * 用户需求：串口只显示真实交互数据，TFT 显示交由 ScreenTFT 负责。
 */

#include "Screen.h"

class ScreenConsole : public Screen {
public:
    bool init() override;
    void update() override;
    void setNavState(const Nav::NavState& state) override;
    void setBleConnected(bool connected) override;
    void log(const char* msg) override;

private:
    Nav::NavState mState;
    bool mBleConnected = false;
    bool mInitialized = false;
    unsigned long mLastPrintedMs = 0;
    unsigned long mLastUpdateMs = 0;
    unsigned long mFrameCounter = 0;

    void printNavStateLine();
};
