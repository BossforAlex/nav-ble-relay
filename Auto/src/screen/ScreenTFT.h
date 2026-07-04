#pragma once

/**
 * @file ScreenTFT.h
 * @brief ST7789 TFT 屏幕 HUD 风格实现
 *
 * 参考《小屏手机手搓hud》视频的 HUD 设计理念：
 * - 高对比黑底配色，夜间行车友好
 * - 大号转向箭头 + 距离 + 车速为核心信息
 * - 屏幕自适应：根据 TFT_WIDTH/TFT_HEIGHT 自动缩放字体和布局
 * - 限速标志、车道指引、路况光柱辅助显示
 */

#include "Screen.h"
#include "nav/NavData.h"
#include <TFT_eSPI.h>

class ScreenTFT : public Screen {
public:
    ScreenTFT();
    ~ScreenTFT() override = default;

    bool init() override;
    void update() override;
    void setNavState(const Nav::NavState& state) override;
    void setBleConnected(bool connected) override;
    void log(const char* msg) override;

private:
    TFT_eSPI tft;
    TFT_eSprite sprite;      // 双缓冲离屏渲染，避免闪烁

    Nav::NavState mState;
    bool mBleConnected = false;
    unsigned long mLastRenderMs = 0;
    unsigned long mFrameCounter = 0;
    bool mInited = false;
    bool mSpriteOk = false;  // sprite 帧缓冲是否分配成功

    // 屏幕尺寸（运行时获取）
    int mWidth = 240;
    int mHeight = 240;

    // 自适应缩放因子（基于 240x240 基准）
    float mScale = 1.0f;

    // 计算缩放因子
    void computeScale();

    // HUD 渲染各模块
    void renderFrame();
    void drawBackground();
    void drawTopBar();              // BLE 状态 + 时间
    void drawTurnArrow();           // 大号转向箭头
    void drawDistance();            // 路口距离
    void drawRoadName();            // 当前/下一道路名
    void drawSpeed();              // 车速 + 限速标志
    void drawLanes();               // 车道指引
    void drawTmcBar();              // 路况光柱
    void drawRouteInfo();           // 全程剩余
    void drawCamera();              // 电子眼
    void drawIdleScreen();         // 无导航时显示
    // 启动画面：纯几何图形，不使用 drawString（避免 S3 字体渲染 crash）
    void drawBootScreen();  // 启动画面

    // 工具
    int scale(int v) const { return (int)(v * mScale); }
    uint8_t scaleFont(uint8_t f) const;

    // 箭头绘制
    void drawArrow(int cx, int cy, int size, int icon, uint16_t color);
    // 限速标志
    void drawSpeedLimit(int cx, int cy, int radius, int speed, bool overSpeed);
};
