#pragma once

/**
 * @file ScreenLVGL.h
 * @brief v0.9.0: 基于 LVGL 的 HUD 显示 —— 纯展示层
 *
 * 移植自 https://github.com/BossforAlex/LVGL-NAV
 * 适配到现有项目：
 *   - 继承 Screen 纯展示接口
 *   - 复用现有 TFT_eSPI 接线（HSPI: CS=10, DC=2, RST=4, BL=6）
 *   - 分辨率 320x240，三栏黄金比例布局
 *   - 静态 UI 框架由 ui.c 提供，本层仅负责数据→控件映射
 *
 * 关键变更 v0.9.0：
 *   - 移除 CJK 中文字体（~7.4MB），所有文本用 LVGL 内置 Montserrat
 *   - 添加单字段 showXxx() 方法，方便未来 Flutter 端直接推送单字段
 *   - 方向箭头遵循高德 AmapAuto SDK 官方 icon 定义
 *   - 车道 backIcon 遵循 AmapAuto 定义
 */

#include "Screen.h"
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui/ui.h"

class ScreenLVGL : public Screen {
public:
    ScreenLVGL();

    // ── Screen 接口实现 ──
    bool init() override;
    void update() override;
    void log(const char* msg) override;

    void setNavState(const Nav::NavState& state) override;
    void setBleConnected(bool connected) override;

    // ── 单字段展示方法（纯展示层） ──
    void showArrow(int amapIcon) override;
    void showDistance(const char* text) override;
    void showSpeed(int speed) override;
    void showSpeedLimit(int limit, bool overSpeed) override;
    void showRoadName(const char* name) override;
    void showLanes(int count, const int* backIcons) override;
    void showRouteInfo(const char* text) override;
    void showIdle() override;

private:
    TFT_eSPI mTft;
    Nav::NavState mState;
    bool mBleConnected = false;
    bool mInited = false;

    // LVGL 显示缓冲
    // v0.9.1: 增大 LVGL 缓冲（配合 lv_conf.h）
    static constexpr int LV_BUF_SIZE = 320 * 240 / 4;
    lv_color_t* mBuf = nullptr;
    lv_disp_draw_buf_t mDrawBuf;
    lv_disp_drv_t mDispDrv;

    // 内部方法
    static void onFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    void applyNavState();
    void updateBleDot();
    void formatDistance(int meters, char* out, size_t outSize);
    void formatRouteInfo(char* out, size_t outSize);

    // v0.9.1: TFT 初始化验证与重试
    bool tftInitWithRetry(int maxRetries = 3);
};