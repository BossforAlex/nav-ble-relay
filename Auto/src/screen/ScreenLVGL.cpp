#include "ScreenLVGL.h"
#include "config/Config.h"
#include "ScreenRenderer.h"
#include <stdio.h>
#include <string.h>

// ══════════════════════════════════════════════════════════════
// v0.6.6: 基于 LVGL 的现代化 HUD 显示
// 移植自 https://github.com/BossforAlex/LVGL-NAV
// 完全对照 a.jpg 重新设计布局
// ══════════════════════════════════════════════════════════════

ScreenLVGL::ScreenLVGL() = default;

static ScreenLVGL* sInstance = nullptr;

// ── 显示刷新回调（静态转发到实例） ──────────────────────────

void ScreenLVGL::onFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    sInstance->mTft.startWrite();
    sInstance->mTft.setAddrWindow(area->x1, area->y1, w, h);
    sInstance->mTft.pushColors((uint16_t*)&color_p->full, w * h, true);
    sInstance->mTft.endWrite();

    lv_disp_flush_ready(disp);
}

// ── 初始化 ────────────────────────────────────────────────

bool ScreenLVGL::init() {
    delay(300);

    sInstance = this;

    mTft.init();
    mTft.setRotation(1);  // 横屏 320x240
    mTft.fillScreen(TFT_BLACK);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
#endif

    // 检查分辨率
    int w = mTft.width();
    int h = mTft.height();
    if (w == 0 || h == 0) {
        Serial.println("[ScreenLVGL] TFT 未检测到（检查 5V 供电 + SPI 接线）");
        return false;
    }

    lv_init();

    // 分配显示缓冲（内部 RAM 或 PSRAM）
    mBuf = (lv_color_t*)heap_caps_malloc(
        sizeof(lv_color_t) * LV_BUF_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (mBuf == nullptr) {
        mBuf = (lv_color_t*)ps_malloc(sizeof(lv_color_t) * LV_BUF_SIZE);
    }
    if (mBuf == nullptr) {
        Serial.println("[ScreenLVGL] LVGL 显示缓冲分配失败");
        return false;
    }

    lv_disp_draw_buf_init(&mDrawBuf, mBuf, nullptr, LV_BUF_SIZE);

    lv_disp_drv_init(&mDispDrv);
    mDispDrv.hor_res = 320;
    mDispDrv.ver_res = 240;
    mDispDrv.flush_cb = onFlush;
    mDispDrv.draw_buf = &mDrawBuf;
    lv_disp_drv_register(&mDispDrv);

    ui_init();
    showIdleScreen();

    mInited = true;
    Serial.printf("[ScreenLVGL] ILI9341 %dx%d + LVGL v%d.%d.%d 初始化完成\n",
                  w, h, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return true;
}

// ── 主循环入口 ────────────────────────────────────────────

void ScreenLVGL::update() {
    if (!mInited) return;
    // v0.6.6 关键修复：绝不能因为暂时没数据就抹掉屏幕！
    // 用户反馈 v0.6.5 的 3 秒超时机制会导致导航过程中"等待数据"反复闪现，
    // 严重干扰视线。新策略：
    //   1) BLE INDICATE poll 由 BleServer::loop() 主动触发（每 1s）
    //   2) 手机收到 INDICATE → 立即把最新一次广播数据写回 CHAR_DATA
    //   3) 屏幕始终显示最后接收到的 NavState（包括超速提示等所有细节）
    //   4) 只有重启 / 显式调用 setNavState(idle) 才会切到空闲画面
    lv_timer_handler();
}

void ScreenLVGL::setNavState(const Nav::NavState& state) {
    mState = state;
    mLastNavMs = millis();
    applyNavState();
}

void ScreenLVGL::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    updateBleDot();
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[ScreenLVGL] BLE %s\n", connected ? "已连接" : "已断开");
    }
}

void ScreenLVGL::log(const char* msg) {
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[ScreenLVGL] %s\n", msg);
    }
}

// ── 更新 BLE 连接状态点 ────────────────────────────────────

void ScreenLVGL::updateBleDot() {
    if (!mInited) return;
    // BLE 已连接：绿色；未连接：灰色
    if (mBleConnected) {
        lv_obj_set_style_bg_color(ui_BleDot, lv_color_hex(0x00E676), 0);
    } else {
        lv_obj_set_style_bg_color(ui_BleDot, lv_color_hex(0x666666), 0);
    }
    lv_obj_invalidate(ui_BleDot);
}

// ── 距离格式化（米 → "725米" / "1.2km"） ──────────────────

void ScreenLVGL::formatDistance(int meters, char* out, size_t outSize) {
    if (meters < 0) meters = 0;
    if (meters < 1000) {
        snprintf(out, outSize, "%d米", meters);
    } else {
        // 显示一位小数 km
        int km = meters / 1000;
        int dec = (meters % 1000) / 100;
        snprintf(out, outSize, "%d.%dkm", km, dec);
    }
}

void ScreenLVGL::formatRouteInfo(char* out, size_t outSize) {
    int routeDis = mState.guide.routeRemainDis;
    int routeTime = mState.guide.routeRemainTime;
    if (routeDis <= 0 && routeTime <= 0) {
        out[0] = '\0';
        return;
    }
    char disBuf[24];
    if (routeDis > 0) {
        if (routeDis < 1000) {
            snprintf(disBuf, sizeof(disBuf), "%dm", routeDis);
        } else {
            int km = routeDis / 1000;
            int dec = (routeDis % 1000) / 100;
            snprintf(disBuf, sizeof(disBuf), "%d.%dkm", km, dec);
        }
    } else {
        disBuf[0] = '\0';
    }
    char timeBuf[16];
    if (routeTime > 0) {
        int mins = (routeTime + 30) / 60;
        if (mins >= 60) {
            snprintf(timeBuf, sizeof(timeBuf), "%dh%dm", mins / 60, mins % 60);
        } else {
            snprintf(timeBuf, sizeof(timeBuf), "%d分钟", mins);
        }
    } else {
        timeBuf[0] = '\0';
    }
    if (disBuf[0] && timeBuf[0]) {
        snprintf(out, outSize, "剩 %s · %s", disBuf, timeBuf);
    } else if (disBuf[0]) {
        snprintf(out, outSize, "剩 %s", disBuf);
    } else {
        snprintf(out, outSize, "剩 %s", timeBuf);
    }
}

// ── 更新 UI 控件 ──────────────────────────────────────────

void ScreenLVGL::applyNavState() {
    if (!mInited) return;

    // v0.6.6: 不再因为 !nav 或 !valid 就回退到 idle 画面。
    // 屏幕始终显示最后接收到的 NavState（断流也不擦屏）。
    // 如果是真正的初始空状态（mState.lastUpdateMs == 0），才显示一次 idle
    if (mState.lastUpdateMs == 0) {
        showIdleScreen();
        return;
    }

    // 道路名称
    if (mState.guide.curRoadName[0]) {
        lv_label_set_text(ui_RoadNameLabel, mState.guide.curRoadName);
    } else {
        lv_label_set_text_static(ui_RoadNameLabel, "导航中");
    }

    // 转向箭头
    setTurnSymbol(mState.guide.icon);

    // 距离
    char distBuf[32];
    if (mState.guide.distanceText[0]) {
        snprintf(distBuf, sizeof(distBuf), "%s", mState.guide.distanceText);
    } else {
        formatDistance(mState.guide.segRemainDis, distBuf, sizeof(distBuf));
    }
    lv_label_set_text(ui_DistanceLabel, distBuf);

    // 车速（始终显示，含 0）
    char speedBuf[8];
    snprintf(speedBuf, sizeof(speedBuf), "%d", mState.guide.curSpeed);
    lv_label_set_text(ui_SpeedLabel, speedBuf);

    // 限速
    if (mState.guide.limitedSpeed > 0) {
        char limitBuf[8];
        snprintf(limitBuf, sizeof(limitBuf), "%d", mState.guide.limitedSpeed);
        lv_label_set_text(ui_LimitLabel, limitBuf);
        lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);

        // 超速时圆圈变亮红
        bool over = mState.guide.curSpeed > mState.guide.limitedSpeed;
        lv_obj_set_style_border_color(ui_LimitSign,
            lv_color_hex(over ? 0xFF1744 : 0xD32F2F), 0);
    } else {
        lv_label_set_text_static(ui_LimitLabel, "");
        lv_obj_add_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
    }

    // 车道
    setLaneArrows();

    // 剩余全程信息
    char routeBuf[64];
    formatRouteInfo(routeBuf, sizeof(routeBuf));
    lv_label_set_text(ui_RouteInfoLabel, routeBuf);
}

// ── 设置车道箭头 ──────────────────────────────────────────

void ScreenLVGL::setLaneArrows() {
    // 清除现有子对象
    lv_obj_clean(ui_LaneContainer);

    // 车道数量：若导航中无 driveWay.enabled 或 laneCount==0，
    // 仍显示 3 个默认直行箭头（与 a.jpg 默认态一致）
    int laneCount = mState.driveWay.enabled ? mState.driveWay.laneCount : 0;
    if (laneCount == 0) {
        // 默认 3 车道直行
        laneCount = 3;
    }
    if (laneCount > Nav::MAX_LANES) laneCount = Nav::MAX_LANES;

    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    // a.jpg 显示 6 个箭头（每个 ~28px 宽，间距 6px），填满 212px
    // 实际 laneCount 由数据决定，若 laneCount>6 则每箭头更窄
    int totalW = 212 - 8;  // 留 4px 边距
    int arrowW = totalW / laneCount;
    if (arrowW > 32) arrowW = 32;
    int startX = (212 - arrowW * laneCount) / 2;
    if (startX < 2) startX = 2;

    for (int i = 0; i < laneCount; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int backIcon = (i < mState.driveWay.laneCount)
                        ? mState.driveWay.lanes[i].backIcon : 1;

        // backIcon 映射：
        //   0 = 左转  1 = 直行  2 = 右转  3 = 左+直  4 = 直+右
        //   5 = 左+直+右（保留/未用） 6 = 调头  7 = ?
        const char* symbol = LV_SYMBOL_UP;
        if (backIcon == 0) symbol = LV_SYMBOL_LEFT;
        else if (backIcon == 1) symbol = LV_SYMBOL_UP;
        else if (backIcon == 2) symbol = LV_SYMBOL_RIGHT;
        else if (backIcon == 3) symbol = LV_SYMBOL_LEFT;  // 左+直 → 简化左
        else if (backIcon == 4) symbol = LV_SYMBOL_RIGHT;  // 直+右 → 简化右
        else if (backIcon == 6) symbol = LV_SYMBOL_DOWN;  // 调头

        lv_label_set_text_static(arrow, symbol);
        lv_obj_set_style_text_color(arrow, lv_color_white(), 0);
        // 字体大小根据箭头宽度自适应
        const lv_font_t* font = &lv_font_montserrat_20;
        if (arrowW <= 18) font = &lv_font_montserrat_14;
        else if (arrowW <= 24) font = &lv_font_montserrat_20;
        else font = &lv_font_montserrat_24;
        lv_obj_set_style_text_font(arrow, font, 0);
        lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(arrow, arrowW, 30);
        lv_obj_set_pos(arrow, startX + i * arrowW, 3);
    }
}

// ── 设置转向图标（a.jpg fork 风格：左上箭头） ──────────────

void ScreenLVGL::setTurnSymbol(int icon) {
    // 高德 AmapAuto icon 编号 → LVGL symbol 映射
    // 与开源参考库 BossforAlex/LVGL-NAV 保持一致
    const char* symbol = LV_SYMBOL_UP;

    switch (icon) {
        case 0:  symbol = LV_SYMBOL_LEFT;     break;  // 左转
        case 1:  symbol = LV_SYMBOL_UP;       break;  // 直行
        case 2:  symbol = LV_SYMBOL_RIGHT;    break;  // 右转
        case 3:  symbol = LV_SYMBOL_LEFT;     break;  // 左前方掉头
        case 4:  symbol = LV_SYMBOL_LEFT;     break;  // 左前方（左前）
        case 5:  symbol = LV_SYMBOL_RIGHT;    break;  // 右前方（右前）
        case 6:  symbol = LV_SYMBOL_LEFT;     break;  // 左后方（rare）
        case 7:  symbol = LV_SYMBOL_RIGHT;    break;  // 右后方（rare）
        case 8:  symbol = LV_SYMBOL_DOWN;     break;  // 调头
        case 9:  symbol = LV_SYMBOL_UP;       break;  // 继续直行
        case 15: symbol = LV_SYMBOL_OK;       break;  // 到达目的地
        case 19: symbol = LV_SYMBOL_DOWN;     break;  // 调头（旧版）
        case 20: symbol = LV_SYMBOL_REFRESH;  break;  // 环岛
        default: symbol = LV_SYMBOL_UP;       break;  // 默认直行
    }

    lv_label_set_text_static(ui_TurnArrow, symbol);
}

// ── 空闲画面（仅初始 / 重置时调用一次） ──────────────────

void ScreenLVGL::showIdleScreen() {
    if (!mInited) return;

    lv_label_set_text_static(ui_RoadNameLabel, "");
    lv_label_set_text_static(ui_TurnArrow, "");
    lv_label_set_text_static(ui_DistanceLabel, "");
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_label_set_text_static(ui_LimitLabel, "");
    lv_obj_add_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(ui_LaneContainer);
    lv_label_set_text_static(ui_RouteInfoLabel, "");
    updateBleDot();
}
