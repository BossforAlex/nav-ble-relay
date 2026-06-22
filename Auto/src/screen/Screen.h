#pragma once

/**
 * @file Screen.h
 * @brief 屏幕抽象接口
 *
 * 通过抽象层把“显示逻辑”与“具体屏幕驱动”解耦：
 * - 当前阶段使用 ScreenConsole（串口虚拟屏幕）调试；
 * - 后续接入 OLED/LCD/TFT 时只需新增实现类，无需修改主逻辑。
 */

#include "nav/NavData.h"

class Screen {
public:
    virtual ~Screen() = default;

    // 初始化屏幕（或串口）
    virtual bool init() = 0;

    // 主循环中调用，处理动画刷新
    virtual void update() = 0;

    // 设置最新导航状态
    virtual void setNavState(const Nav::NavState& state) = 0;

    // 设置 BLE 连接状态
    virtual void setBleConnected(bool connected) = 0;

    // 输出一条日志/提示
    virtual void log(const char* msg) = 0;
};
