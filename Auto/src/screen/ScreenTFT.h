#pragma once

/**
 * @file ScreenTFT.h
 * @brief ILI9341 TFT 横屏 HUD 导航显示 (v0.6.4)
 *
 * 目标屏幕：MSP2807 2.8" ILI9341 SPI TFT (320x240)
 * 接线：ESP32-S3 SuperMini HSPI, CS=10 DC=2 RST=4 MOSI=11 SCK=12 BL=6
 *
 * v0.6.4: iWatch 极简风格 + 20fps 无撕裂渲染
 *   SPI 40MHz → pushSprite ~30ms，留 20ms 绘图时间
 *   大号居中转向箭头 + 距离，清晰道路名称，醒目车速
 *
 * 横屏布局 (320x240)：
 * ┌──────────────────────────────────────┐
 * │  ● NAV                       120 ──┐│  top bar (18px) + 限速圆
 * │                                    ││
 * │               ⬆                    ││  turn arrow (大号居中)
 * │             350 m                  ││  distance
 * │       从 中山路 进入                ││  current road
 * │          人民路                     ││  next road
 * │             78                     ││  speed (大号)
 * │            km/h                    ││
 * │  ████▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░  ││  tmc bar (6px)
 * │  12 min | 3.2 km           N ↑    ││  bottom bar (14px)
 * └──────────────────────────────────────┘
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

    void showArrow(int amapIcon) override;
    void showDistance(const char* text) override;
    void showSpeed(int speed) override;
    void showSpeedLimit(int limit, bool overSpeed) override;
    void showRoadName(const char* name) override;
    void showLanes(int count, const int* backIcons) override;
    void showRouteInfo(const char* text) override;
    void showIdle() override;

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

    // ── 布局常量（横屏 320x240，iWatch 极简风格） ──
    static constexpr int TOP_BAR_H  = 18;
    static constexpr int BTM_BAR_H  = 14;
    static constexpr int TMC_BAR_H  = 6;
    static constexpr int TMC_BAR_Y  = 240 - BTM_BAR_H - TMC_BAR_H - 2;
    static constexpr int BTM_BAR_Y  = TMC_BAR_Y + TMC_BAR_H + 2;

    // 主内容区
    static constexpr int CONTENT_Y  = TOP_BAR_H + 4;
    static constexpr int CONTENT_H  = TMC_BAR_Y - CONTENT_Y - 4;

    // 箭头 + 距离（内容区上半部，居中）
    static constexpr int ARROW_CX   = 160;
    static constexpr int ARROW_CY   = CONTENT_Y + 50;
    static constexpr int ARROW_SZ   = 34;
    static constexpr int DIST_Y     = ARROW_CY + 42;

    // 道路名称（距离下方，居中）
    static constexpr int ROAD_SEP_Y = DIST_Y + 22;
    static constexpr int CUR_ROAD_Y = ROAD_SEP_Y + 12;
    static constexpr int NEXT_ROAD_Y= CUR_ROAD_Y + 17;

    // 车速（道路下方，大号居中）
    static constexpr int SPEED_Y    = NEXT_ROAD_Y + 22;

    // 限速圆圈（右上角）
    static constexpr int LIMIT_CX   = 290;
    static constexpr int LIMIT_CY   = TOP_BAR_H + 18;
    static constexpr int LIMIT_R    = 20;

    // ── 渲染子模块 ──
    void renderFrame();
    void drawBootScreen();
    void drawBackground();
    void drawTopBar();
    void drawSpeedLimit();        // 限速圆圈（右上角）
    void drawArrowAndDistance();  // 转向箭头 + 距离（大号居中）
    void drawRoadNames();         // 当前道路 + 下条道路
    void drawLaneBar();           // 车道指引
    void drawSpeed();             // 车速（大号居中）
    void drawTmcBar();
    void drawBottomBar();
    void drawIdleScreen();

    // ── 工具函数 ──
    void drawArrowIcon(int cx, int cy, int sz, int icon, uint16_t color);
    void drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool over);
    void drawRoundedBtn(int x, int y, int w, int h, int r, uint16_t bg, uint16_t border, const char* text);
    const char* bearingLabel(int deg);
    const char* arrowLabel(int icon);
};