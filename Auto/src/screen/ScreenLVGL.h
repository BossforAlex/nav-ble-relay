#pragma once

/**
 * @file ScreenLVGL.h
 * @brief v0.9.0: 基于 LVGL 的现代化 HUD 显示（CJK 字体已移除）
 *
 * 移植自 https://github.com/BossforAlex/LVGL-NAV
 * 适配到现有项目：
 *   - 保留 Screen 抽象接口
 *   - 复用现有 TFT_eSPI 接线（CS=10, DC=2, RST=4, BL=6, HSPI）
 *   - 分辨率 320x240
 *   - 由 NavState 驱动 UI 更新
 *
 * v0.9.0 关键变更：
 *   - 移除 CJK 中文字体（~7.4MB），固件体积大幅缩减
 *   - 所有文本使用 LVGL 内置 Montserrat 字体（ASCII 数字/单位）
 *   - 中文路名等由手机端 Flutter App 预渲染为位图传输
 *   - 路名显示占位符 "---"，其余信息（箭头/速度/距离/限速）正常工作
 */

#include "Screen.h"
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui/ui.h"

class ScreenLVGL : public Screen {
public:
    ScreenLVGL();

    bool init() override;
    void update() override;
    void setNavState(const Nav::NavState& state) override;
    void setBleConnected(bool connected) override;
    void log(const char* msg) override;

private:
    TFT_eSPI mTft;
    Nav::NavState mState;
    bool mBleConnected = false;
    bool mInited = false;
    unsigned long mLastRenderMs = 0;
    unsigned long mLastNavMs = 0;

    // LVGL 显示缓冲
    static constexpr int LV_BUF_SIZE = 320 * 240 / 10;
    lv_color_t* mBuf = nullptr;
    lv_disp_draw_buf_t mDrawBuf;
    lv_disp_drv_t mDispDrv;

    // 内部方法
    static void onFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    void applyNavState();
    void setLaneArrows();
    void setTurnSymbol(int icon);
    void showIdleScreen();
    void formatDistance(int meters, char* out, size_t outSize);
    void formatRouteInfo(char* out, size_t outSize);
    void updateBleDot();
};
