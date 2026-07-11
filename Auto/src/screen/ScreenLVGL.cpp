#include "ScreenLVGL.h"
#include "config/Config.h"
#include <stdio.h>
#include <string.h>
#include <esp_task_wdt.h>

/* ══════════════════════════════════════════════════════════════
 * v0.9.0 — 纯展示层（参照 ui.txt 设计规范）
 *
 * 架构：
 *   ui.c: 静态 UI 框架（创建控件，不包含数据逻辑）
 *   ScreenLVGL: 数据→控件映射层（NavState → lv_label_set_text）
 *   main.cpp: 桥接层（BLE 数据 → NavParser → Screen 方法）
 *
 * 高德导航方向箭头 icon 映射（AmapAuto SDK）：
 *   icon  0: 左转     ←
 *   icon  1: 直行     ↑
 *   icon  2: 右转     →
 *   icon  3: 左前方掉头  ↶
 *   icon  4: 左前方    ↰
 *   icon  5: 右前方    ↱
 *   icon  6: 左后方    ↶
 *   icon  7: 右后方    ↷
 *   icon  8: 调头     ↷
 *   icon  9: 延续直行   ↑
 *   icon 15: 到达目的地  ★
 *   icon 19: 调头(旧版) ↷
 *   icon 20: 环岛     ◎
 *
 * 车道 backIcon 映射 (AmapAuto)：
 *   0: 左转    1: 直行    2: 右转
 *   3: 左+直   4: 直+右   6: 调头
 * ══════════════════════════════════════════════════════════════ */

ScreenLVGL::ScreenLVGL() = default;
static ScreenLVGL* sInstance = nullptr;

/* ── 显示刷新回调 ──────────────────────────────────────────── */

void ScreenLVGL::onFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    sInstance->mTft.startWrite();
    sInstance->mTft.setAddrWindow(area->x1, area->y1, w, h);
    sInstance->mTft.pushColors((uint16_t*)&color_p->full, w * h, true);
    sInstance->mTft.endWrite();
    lv_disp_flush_ready(disp);
}

/* ── v0.9.1: TFT 初始化验证与重试 ──────────────────────────── */

bool ScreenLVGL::tftInitWithRetry(int maxRetries) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("[ScreenLVGL] TFT 初始化 (第 %d/%d 次)...\n", attempt, maxRetries);

        // 每次重试前重新初始化 SPI 总线
        SPI.end();
        delay(100);
        SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
        SPI.setFrequency(40000000);
        delay(50);

        mTft.init();
        mTft.setRotation(1);

        // 验证：通过屏幕填充测试显示是否响应
        mTft.fillScreen(TFT_BLACK);
        delay(10);
        mTft.fillScreen(TFT_RED);
        delay(50);
        mTft.fillScreen(TFT_BLACK);

        int w = mTft.width();
        int h = mTft.height();
        if (w == 320 && h == 240) {
            Serial.printf("[ScreenLVGL] ✓ TFT 初始化成功 (第 %d 次)\n", attempt);
            return true;
        }

        Serial.printf("[ScreenLVGL] ✗ TFT 尺寸异常 (%dx%d), 重试...\n", w, h);
        delay(300);
        esp_task_wdt_reset();
    }
    return false;
}

/* ── 初始化 ────────────────────────────────────────────────── */

bool ScreenLVGL::init() {
    delay(300);
    sInstance = this;

    // v0.9.1: 使用带重试的 TFT 初始化
    if (!tftInitWithRetry(3)) {
        Serial.println("[ScreenLVGL] TFT 初始化全部失败！检查供电和 SPI 接线");
        return false;
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
#endif

    int w = mTft.width();
    int h = mTft.height();
    if (w == 0 || h == 0) {
        Serial.println("[ScreenLVGL] TFT 未检测到（检查 5V 供电 + SPI 接线）");
        return false;
    }

    lv_init();

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
    showIdle();

    Serial.printf("[ScreenLVGL] ILI9341 %dx%d + LVGL v%d.%d.%d 初始化完成\n",
                  w, h, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return true;
}

/* ── 主循环 ────────────────────────────────────────────────── */

void ScreenLVGL::update() {
    if (!mInited) return;
    lv_tick_inc(5);
    lv_timer_handler();
}

void ScreenLVGL::log(const char* msg) {
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[ScreenLVGL] %s\n", msg);
    }
}

/* ── 批量更新（当前 loop 使用） ─────────────────────────────── */

void ScreenLVGL::setNavState(const Nav::NavState& state) {
    mState = state;
    applyNavState();
}

void ScreenLVGL::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    updateBleDot();
}

/* ══════════════════════════════════════════════════════════════
 * 单字段展示方法（纯展示层，未来 Flutter 直接推送时使用）
 * ══════════════════════════════════════════════════════════════ */

void ScreenLVGL::showArrow(int amapIcon) {
    if (!mInited) return;
    const char* symbol = "↑";
    switch (amapIcon) {
        case 0:  symbol = "←";  break;  // 左转
        case 1:  symbol = "↑";  break;  // 直行
        case 2:  symbol = "→";  break;  // 右转
        case 3:  symbol = "↶";  break;  // 左前方掉头
        case 4:  symbol = "↰";  break;  // 左前方
        case 5:  symbol = "↱";  break;  // 右前方
        case 6:  symbol = "↶";  break;  // 左后方
        case 7:  symbol = "↷";  break;  // 右后方
        case 8:  symbol = "↷";  break;  // 调头
        case 9:  symbol = "↑";  break;  // 延续直行
        case 15: symbol = "★";  break;  // 到达目的地
        case 19: symbol = "↷";  break;  // 调头（旧版）
        case 20: symbol = "◎";  break;  // 环岛
        default: symbol = "↑";  break;
    }
    // v0.9.1: 使用 lv_label_set_text 而非 set_text_static
    // set_text_static 在字体回退时可能引用已释放的指针
    lv_label_set_text(ui_TurnArrow, symbol);
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_invalidate(ui_TurnArrow);
}

void ScreenLVGL::showDistance(const char* text) {
    if (!mInited) return;
    lv_label_set_text(ui_DistanceLabel, text);
}

void ScreenLVGL::showSpeed(int speed) {
    if (!mInited) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", speed);
    lv_label_set_text(ui_SpeedLabel, buf);
}

void ScreenLVGL::showSpeedLimit(int limit, bool overSpeed) {
    if (!mInited) return;
    if (limit > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", limit);
        lv_label_set_text(ui_LimitLabel, buf);
        lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(ui_LimitSign,
            lv_color_hex(overSpeed ? 0xFF1744 : 0xD32F2F), 0);
    } else {
        lv_label_set_text_static(ui_LimitLabel, "");
        lv_obj_add_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
    }
}

void ScreenLVGL::showRoadName(const char* name) {
    if (!mInited) return;
    if (name && name[0]) {
        lv_label_set_text(ui_RoadNameLabel, name);
    } else {
        lv_label_set_text_static(ui_RoadNameLabel, "NAV");
    }
}

void ScreenLVGL::showLanes(int count, const int* backIcons) {
    if (!mInited) return;
    lv_obj_clean(ui_LaneContainer);
    if (count <= 0) {
        count = 4;
    }
    if (count > 8) count = 8;
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    // v0.9.1: 计算每个车道标签的宽度（Flexbox 均分）
    int laneW = (132 - 4) / count;  // 132=容器宽, 减去内边距

    for (int i = 0; i < count; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int bi = backIcons ? backIcons[i] : 1;
        const char* sym = "↑";
        if (bi == 0) sym = "←";
        else if (bi == 1) sym = "↑";
        else if (bi == 2) sym = "→";
        else if (bi == 3) sym = "↰";
        else if (bi == 4) sym = "↱";
        else if (bi == 6) sym = "↶";

        // v0.9.1: 使用 lv_label_set_text 确保字体正确渲染
        lv_label_set_text(arrow, sym);
        lv_obj_set_style_text_color(arrow, lv_color_white(), 0);
        lv_obj_set_style_text_font(arrow, &arrows_20, 0);
        lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
        // v0.9.1: 设置固定宽度防止文字被裁剪
        lv_obj_set_width(arrow, laneW > 20 ? laneW : 20);
        lv_obj_set_height(arrow, 24);
    }
}

void ScreenLVGL::showRouteInfo(const char* text) {
    if (!mInited) return;
    lv_label_set_text(ui_RouteInfoLabel, text);
}

void ScreenLVGL::showIdle() {
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

/* ── BLE 状态点颜色 ────────────────────────────────────────── */

void ScreenLVGL::updateBleDot() {
    if (!mInited) return;
    lv_color_t c = mBleConnected ? lv_color_hex(0x00E676) : lv_color_hex(0x666666);
    lv_obj_set_style_bg_color(ui_BleDot, c, 0);
    lv_obj_invalidate(ui_BleDot);
}

/* ── 距离格式化（米 → "725 m" / "1.2 km"） ────────────────── */

void ScreenLVGL::formatDistance(int meters, char* out, size_t outSize) {
    if (meters < 0) meters = 0;
    if (meters < 1000) {
        snprintf(out, outSize, "%d m", meters);
    } else {
        snprintf(out, outSize, "%d.%d km", meters / 1000, (meters % 1000) / 100);
    }
}

void ScreenLVGL::formatRouteInfo(char* out, size_t outSize) {
    int dis = mState.guide.routeRemainDis;
    int time = mState.guide.routeRemainTime;
    if (dis <= 0 && time <= 0) { out[0] = '\0'; return; }

    char db[24] = "", tb[16] = "";
    if (dis > 0) {
        if (dis < 1000) snprintf(db, sizeof(db), "%dm", dis);
        else snprintf(db, sizeof(db), "%d.%dkm", dis / 1000, (dis % 1000) / 100);
    }
    if (time > 0) {
        int mins = (time + 30) / 60;
        if (mins >= 60) snprintf(tb, sizeof(tb), "%dh%dm", mins / 60, mins % 60);
        else snprintf(tb, sizeof(tb), "%dmin", mins);
    }

    if (db[0] && tb[0]) snprintf(out, outSize, "%s . %s", db, tb);
    else if (db[0]) snprintf(out, outSize, "%s", db);
    else snprintf(out, outSize, "%s", tb);
}

/* ── 批量应用导航状态（通过单字段方法实现） ────────────────── */

void ScreenLVGL::applyNavState() {
    if (!mInited) return;

    if (mState.lastUpdateMs == 0) {
        showIdle();
        return;
    }

    // 路名（v0.9.0: 占位符，待 Flutter 端预渲染位图传输）
    showRoadName("---");

    // 转向箭头
    showArrow(mState.guide.icon);

    // 距离
    char distBuf[32];
    if (mState.guide.distanceText[0]) {
        snprintf(distBuf, sizeof(distBuf), "%s", mState.guide.distanceText);
    } else {
        formatDistance(mState.guide.segRemainDis, distBuf, sizeof(distBuf));
    }
    showDistance(distBuf);

    // 车速
    showSpeed(mState.guide.curSpeed);

    // 限速
    bool over = mState.guide.curSpeed > mState.guide.limitedSpeed;
    showSpeedLimit(mState.guide.limitedSpeed, over);

    // 车道
    int laneCount = mState.driveWay.enabled ? mState.driveWay.laneCount : 0;
    if (laneCount == 0) laneCount = 4;
    if (laneCount > 8) laneCount = 8;
    int backIcons[8];
    for (int i = 0; i < laneCount; i++) {
        backIcons[i] = (i < mState.driveWay.laneCount)
                       ? mState.driveWay.lanes[i].backIcon : 1;
    }
    showLanes(laneCount, backIcons);

    // 全程信息
    char routeBuf[64];
    formatRouteInfo(routeBuf, sizeof(routeBuf));
    showRouteInfo(routeBuf);
}