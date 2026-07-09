#pragma once

/**
 * @file ScreenTFT.h
 * @brief ILI9341 TFT 横屏 HUD 导航显示 (v0.6.4)
 *
 * 目标屏幕：MSP2807 2.8" ILI9341 SPI TFT (320x240)
 * 接线方式：ESP32-S3 SuperMini HSPI 引脚
 *           CS=10 DC=2 RST=4 MOSI=11 SCK=12 BL=6
 *
 * v0.6.4: iWatch 风格现代化 UI。参考 Apple Watch 高德导航设计：
 *   - 大号居中转向箭头 + 距离文字
 *   - 当前道路 / 下条道路
 *   - 车速 + 限速圆圈
 *   - 路况光柱 + 底部信息栏
 *   - 30fps 丝滑渲染，sprite 双缓冲消除撕裂
 *
 * 横屏布局 (320x240)：
 * ┌──────────────────────────────────────────┐
 * │  ● NAV                     ┌──────┐      │  top bar (20px)
 * │                            │ 120  │      │
 * │                            └──────┘      │
 * │              ⬆                           │  turn arrow
 * │            350 m                         │  distance
 * │      从 中山路 进入                       │  current road
 * │         人民路                            │  next road
 * │            78  km/h                      │  speed
 * │  ████▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░   │  tmc bar (6px)
 * │  12 min | 3.2 km                N ↑     │  bottom bar (16px)
 * └──────────────────────────────────────────┘
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
    TFT_eSPI  tft;
    TFT_eSprite sprite = TFT_eSprite(&tft);

    Nav::NavState mState;
    bool mBleConnected = false;
    unsigned long mLastRenderMs = 0;
    unsigned long mFrameCounter = 0;
    bool mInited = false;
    bool mSpriteOk = false;

    int mW = 320;
    int mH = 240;

    // ── 布局常量（横屏 320x240, iWatch 风格） ──
    static constexpr int TOP_BAR_H  = 20;
    static constexpr int BTM_BAR_H  = 16;
    static constexpr int TMC_BAR_H  = 6;
    static constexpr int TMC_BAR_Y  = 240 - BTM_BAR_H - TMC_BAR_H - 2;
    static constexpr int BTM_BAR_Y  = TMC_BAR_Y + TMC_BAR_H + 2;

    // 主内容区：top bar 下方到 tmc bar 上方
    static constexpr int CONTENT_Y  = TOP_BAR_H + 4;
    static constexpr int CONTENT_H  = TMC_BAR_Y - CONTENT_Y - 4;

    // 箭头区域 (内容区上半部)
    static constexpr int ARROW_CX   = 160;
    static constexpr int ARROW_CY   = CONTENT_Y + 55;
    static constexpr int ARROW_SZ   = 36;

    // 距离文字 (箭头下方)
    static constexpr int DIST_Y     = ARROW_CY + 44;

    // 道路名称
    static constexpr int CUR_ROAD_Y = DIST_Y + 28;
    static constexpr int NEXT_ROAD_Y= CUR_ROAD_Y + 18;

    // 速度显示 (右侧)
    static constexpr int SPEED_X    = 260;
    static constexpr int SPEED_Y    = CONTENT_Y + 20;
    static constexpr int LIMIT_R    = 22;

    // ── 渲染子模块 ──
    void renderFrame();
    void drawBootScreen();
    void drawBackground();
    void drawTopBar();
    void drawArrowAndDistance();  // v0.6.4: 合并箭头+距离
    void drawRoadNames();         // 当前道路 + 下条道路
    void drawSpeedPanel();        // 限速圆 + 车速
    void drawTmcBar();
    void drawBottomBar();
    void drawIdleScreen();
    void drawLaneBar();           // 车道指引（保留，导航时显示在 road 下方）

    // ── 工具函数 ──
    void drawArrowIcon(int cx, int cy, int size, int icon, uint16_t color);
    void drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool overSpeed);
    void drawRoundedBtn(int x, int y, int w, int h, int r, uint16_t bg, uint16_t border, const char* text);
    const char* bearingLabel(int deg);
    const char* arrowLabel(int icon);
};