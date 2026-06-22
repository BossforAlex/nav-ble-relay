#pragma once

/**
 * @file ScreenConsole.h
 * @brief 串口虚拟屏幕实现
 *
 * 无真实屏幕时，通过串口以“帧”的形式输出当前导航 UI 状态，
 * 便于验证协议解析、渲染逻辑和动画状态。
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
    unsigned long mLastRenderMs = 0;
    unsigned long mFrameCounter = 0;

    void renderFrame();
    void renderAnimation();
};
