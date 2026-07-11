#include "ScreenLVGL.h"
#include "config/Config.h"
#include "ScreenRenderer.h"
#include <stdio.h>
#include <string.h>

// ══════════════════════════════════════════════════════════════
// v0.9.0: 三栏黄金比例布局（参照 UI.TXT 设计规范）
//   CJK 中文字体已移除，中文路名等由 Flutter App 预渲染位图传输。
//   左 (100px): 导航信息 — 路名(占位) / 转向箭头 / 剩余距离
//   中 (140px): 时速 + 车道 — Flexbox 车道条 / 大字时速 / km/h
//   右 ( 65px): 状态 — 限速红圈 / 连接图标
//   底: 全程剩余信息 (ASCII 数字)
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

    mInited = true;
    showIdleScreen();

    Serial.printf("[ScreenLVGL] ILI9341 %dx%d + LVGL v%d.%d.%d 初始化完成\n",
                  w, h, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return true;
}

// ── 主循环入口 ────────────────────────────────────────────

void ScreenLVGL::update() {
    if (!mInited) return;
    // v0.6.8 关键修复: lv_tick_inc() 必须调用，否则 lv_timer_handler()
    // 内部 lv_tick_get() 始终返回 0 → 判断时间未到 → 直接 return 不刷新！
    lv_tick_inc(5);
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

// ── 距离格式化（米 → "725 m" / "1.2 km"） ──────────────────

void ScreenLVGL::formatDistance(int meters, char* out, size_t outSize) {
    if (meters < 0) meters = 0;
    if (meters < 1000) {
        snprintf(out, outSize, "%d m", meters);
    } else {
        int km = meters / 1000;
        int dec = (meters % 1000) / 100;
        snprintf(out, outSize, "%d.%d km", km, dec);
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
            snprintf(timeBuf, sizeof(timeBuf), "%dmin", mins);
        }
    } else {
        timeBuf[0] = '\0';
    }
    if (disBuf[0] && timeBuf[0]) {
        snprintf(out, outSize, "%s . %s", disBuf, timeBuf);
    } else if (disBuf[0]) {
        snprintf(out, outSize, "%s", disBuf);
    } else {
        snprintf(out, outSize, "%s", timeBuf);
    }
}

// ── 更新 UI 控件 ──────────────────────────────────────────

void ScreenLVGL::applyNavState() {
    if (!mInited) return;

    if (mState.lastUpdateMs == 0) {
        showIdleScreen();
        return;
    }

    Serial.printf("[ScreenLVGL] applyNavState: speed=%d icon=%d road=%s dist=%d\n",
                  mState.guide.curSpeed, mState.guide.icon,
                  mState.guide.curRoadName[0] ? mState.guide.curRoadName : "(none)",
                  mState.guide.segRemainDis);

    // 道路名称（v0.9.0: CJK 字体已移除，显示占位符）
    // 后续由 Flutter App 预渲染位图传输真实路名
    if (mState.guide.curRoadName[0]) {
        lv_label_set_text_static(ui_RoadNameLabel, "---");
    } else {
        lv_label_set_text_static(ui_RoadNameLabel, "NAV");
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

// ── 设置车道箭头（Flexbox 自动布局） ──────────────────────────

void ScreenLVGL::setLaneArrows() {
    // 清除现有子对象
    lv_obj_clean(ui_LaneContainer);

    // 车道数量
    int laneCount = mState.driveWay.enabled ? mState.driveWay.laneCount : 0;
    if (laneCount == 0) {
        laneCount = 4;  // 默认 4 车道直行
    }
    if (laneCount > Nav::MAX_LANES) laneCount = Nav::MAX_LANES;

    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    // Flexbox 自动布局，无需手动计算位置。
    // 每个箭头宽度自适应，容器 132px 由 Flexbox 均匀分配。
    for (int i = 0; i < laneCount; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int backIcon = (i < mState.driveWay.laneCount)
                        ? mState.driveWay.lanes[i].backIcon : 1;

        const char* symbol = "↑";
        if (backIcon == 0) symbol = "←";
        else if (backIcon == 1) symbol = "↑";
        else if (backIcon == 2) symbol = "→";
        else if (backIcon == 3) symbol = "↰";  // 左+直
        else if (backIcon == 4) symbol = "↱";  // 直+右
        else if (backIcon == 6) symbol = "↶";  // 调头

        lv_label_set_text_static(arrow, symbol);
        lv_obj_set_style_text_color(arrow, lv_color_white(), 0);
        lv_obj_set_style_text_font(arrow, &arrows_20, 0);
        lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
        // Flexbox 子元素：宽度自适应，高度撑满容器
        lv_obj_set_height(arrow, 24);
    }
}

// ── 设置转向图标（a.jpg fork 风格：左上箭头） ──────────────

void ScreenLVGL::setTurnSymbol(int icon) {
    // 高德 AmapAuto icon 编号 → Unicode 箭头映射
    // v0.6.9: 使用 arrows_48 字体中的 Unicode 箭头，比 LVGL 内置符号更清晰易读
    //
    // 箭头映射:
    //   ← 左转  ↑ 直行  → 右转  ↓ 调头
    //   ↰ 左前  ↱ 右前  ↶ 左后/左调头  ↷ 右后/右调头
    //   ◎ 环岛  ★ 到达
    const char* symbol = "↑";

    switch (icon) {
        case 0:  symbol = "←";  break;  // 左转
        case 1:  symbol = "↑";  break;  // 直行
        case 2:  symbol = "→";  break;  // 右转
        case 3:  symbol = "↶";  break;  // 左前方掉头
        case 4:  symbol = "↰";  break;  // 左前方
        case 5:  symbol = "↱";  break;  // 右前方
        case 6:  symbol = "↶";  break;  // 左后方
        case 7:  symbol = "↷";  break;  // 右后方
        case 8:  symbol = "↷";  break;  // 调头
        case 9:  symbol = "↑";  break;  // 继续直行
        case 15: symbol = "★";  break;  // 到达目的地
        case 19: symbol = "↷";  break;  // 调头（旧版）
        case 20: symbol = "◎";  break;  // 环岛
        default: symbol = "↑";  break;  // 默认直行
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
