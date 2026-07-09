#pragma once

/**
 * @file ScreenTFT.h
 * @brief ILI9341 TFT 横屏 HUD 导航显示 (v0.6.3)
 *
 * 目标屏幕：MSP2807 2.8" ILI9341 SPI TFT (320x240)
 * 接线方式：ESP32-S3 SuperMini HSPI 引脚
 *           CS=10 DC=2 RST=4 MOSI=11 SCK=12 BL=6
 *
 * v0.6.3: 恢复 sprite 离屏帧缓冲（PSRAM），双缓冲消除画面闪烁。
 * PSRAM 已确认正常工作（2MB AP_3v3）。
 * 全分辨率 sprite 320x240x2=153KB，使用 ps_malloc 分配在 PSRAM。
 *
 * 横屏布局 (320x240)：
 * ┌──────────────────────────────────────────────────────┐
 * │ ● BLE │ 导航中 │ 帧:1234 │ 方位:北 ↑              │ top bar
 * ├──────────────────────────────────────────────────────┤
 * │  ← 左转                  ┌──────────┐              │
 * │                          │ 限速  80 │              │ main
 * │       300m               │ 当前  78 │              │
 * │                          │  km/h    │              │
 * │                          └──────────┘              │
 * │ 车道: [↑] [↑] [→]                         CAM    │ lanes
 * │██████████▓▓▓▓░░░░░░░░  (路况光柱)                    │ tmc bar
 * │  中山路 → 人民路                                     │ road
 * │  全程: 3.2 km / 5 min        纬度 39.9042          │ bottom
 * └──────────────────────────────────────────────────────┘
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

    // ── 布局常量（横屏 320x240） ──
    static constexpr int TOP_BAR_H  = 22;
    static constexpr int LANE_BAR_H = 24;
    static constexpr int TMC_BAR_H  = 8;
    static constexpr int MAIN_AREA_Y = TOP_BAR_H;
    static constexpr int MAIN_AREA_H = 142;  // 22..164
    static constexpr int LANE_BAR_Y  = MAIN_AREA_Y + MAIN_AREA_H;
    static constexpr int TMC_BAR_Y   = LANE_BAR_Y + LANE_BAR_H;
    static constexpr int ROAD_BAR_Y  = TMC_BAR_Y + TMC_BAR_H + 2;

    // ── 渲染子模块 ──
    void renderFrame();
    void drawBootScreen();
    void drawBackground();
    void drawTopBar();
    void drawTurnArrow();
    void drawDistance();
    void drawSpeedPanel();    // 限速圆 + 当前车速
    void drawLaneBar();
    void drawTmcBar();
    void drawRoadName();
    void drawBottomBar();
    void drawIdleScreen();

    // ── 工具函数 ──
    void drawArrowIcon(int cx, int cy, int size, int icon, uint16_t color);
    void drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool overSpeed);
    const char* bearingLabel(int deg);
    const char* arrowLabel(int icon);
};