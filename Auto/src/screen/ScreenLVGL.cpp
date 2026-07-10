#include "ScreenLVGL.h"
#include "config/Config.h"
#include "ScreenRenderer.h"
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// v0.6.5: 基于 LVGL 的现代化 HUD 显示
// 移植自 https://github.com/BossforAlex/LVGL-NAV
// 复用现有 TFT_eSPI 接线，分辨率 320x240
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
    lv_timer_handler();

    // 导航数据超时：如果 3 秒没有新数据，回到空闲画面
    if (mState.lastUpdateMs > 0 && millis() - mState.lastUpdateMs > 3000) {
        if (mState.mapState == Nav::MapState::Navigating) {
            Nav::NavState idle = Nav::NavState();
            setNavState(idle);
            showIdleScreen();
        }
    }
}

void ScreenLVGL::setNavState(const Nav::NavState& state) {
    mState = state;
    mLastNavMs = millis();
    applyNavState();
}

void ScreenLVGL::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[ScreenLVGL] BLE %s\n", connected ? "已连接" : "已断开");
    }
}

void ScreenLVGL::log(const char* msg) {
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[ScreenLVGL] %s\n", msg);
    }
}

// ── 更新 UI 控件 ──────────────────────────────────────────

void ScreenLVGL::applyNavState() {
    if (!mInited) return;

    bool nav = (mState.mapState == Nav::MapState::Navigating);

    if (!nav || !mState.guide.valid) {
        showIdleScreen();
        return;
    }

    // 道路名称
    if (mState.guide.curRoadName[0]) {
        char roadBuf[80];
        snprintf(roadBuf, sizeof(roadBuf), "%s", mState.guide.curRoadName);
        lv_label_set_text(ui_RoadNameLabel, roadBuf);
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
        ScreenRenderer::formatDistance(mState.guide.segRemainDis, distBuf, sizeof(distBuf));
    }
    lv_label_set_text(ui_DistanceLabel, distBuf);

    // 车速
    char speedBuf[8];
    snprintf(speedBuf, sizeof(speedBuf), "%d", mState.guide.curSpeed);
    lv_label_set_text(ui_SpeedLabel, speedBuf);

    // 限速
    if (mState.guide.limitedSpeed > 0) {
        char limitBuf[8];
        snprintf(limitBuf, sizeof(limitBuf), "%d", mState.guide.limitedSpeed);
        lv_label_set_text(ui_LimitLabel, limitBuf);
        lv_obj_clear_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);

        bool over = mState.guide.curSpeed > mState.guide.limitedSpeed;
        lv_obj_set_style_border_color(ui_LimitSign,
            lv_color_hex(over ? 0xFF0000 : 0xD32F2F), 0);
    } else {
        lv_obj_add_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
    }

    // 车道
    setLaneArrows();

    // 摄像头
    if (mState.guide.cameraDist > 0) {
        lv_obj_clear_flag(ui_CameraIcon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_CameraIcon, LV_OBJ_FLAG_HIDDEN);
    }
}

// ── 设置车道箭头 ──────────────────────────────────────────

void ScreenLVGL::setLaneArrows() {
    // 清除现有子对象
    lv_obj_clean(ui_LaneContainer);

    int laneCount = mState.driveWay.enabled ? mState.driveWay.laneCount : 0;
    if (laneCount == 0) {
        lv_obj_add_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (laneCount > Nav::MAX_LANES) laneCount = Nav::MAX_LANES;

    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    int totalW = laneCount * 30 - 2;
    int startX = (212 - totalW) / 2;
    if (startX < 4) startX = 4;

    for (int i = 0; i < laneCount; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int backIcon = mState.driveWay.lanes[i].backIcon;

        const char* symbol = LV_SYMBOL_UP;
        if (backIcon == 1) symbol = LV_SYMBOL_LEFT;
        else if (backIcon == 3) symbol = LV_SYMBOL_RIGHT;
        else if (backIcon == 5) symbol = LV_SYMBOL_REFRESH;

        lv_label_set_text_static(arrow, symbol);
        lv_obj_set_style_text_color(arrow, lv_color_white(), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(arrow, 28, 30);
        lv_obj_set_pos(arrow, startX + i * 30, 4);
    }
}

// ── 设置转向图标 ──────────────────────────────────────────

void ScreenLVGL::setTurnSymbol(int icon) {
    const char* symbol = LV_SYMBOL_UP;

    switch (icon) {
        case 1: case 9:                     symbol = LV_SYMBOL_UP;       break;
        case 2: case 6:                     symbol = LV_SYMBOL_LEFT;     break;
        case 3: case 7:                     symbol = LV_SYMBOL_RIGHT;    break;
        case 4:                             symbol = LV_SYMBOL_LEFT;     break;  // 左前
        case 5:                             symbol = LV_SYMBOL_RIGHT;    break;  // 右前
        case 8: case 19:                    symbol = LV_SYMBOL_DOWN;     break;  // 调头
        case 15:                            symbol = LV_SYMBOL_OK;       break;  // 到达
        case 20:                            symbol = LV_SYMBOL_REFRESH;  break;  // 环岛
        default:                            symbol = LV_SYMBOL_UP;       break;
    }

    lv_label_set_text_static(ui_TurnArrow, symbol);
}

// ── 空闲画面 ──────────────────────────────────────────────

void ScreenLVGL::showIdleScreen() {
    if (!mInited) return;

    lv_label_set_text_static(ui_RoadNameLabel, "等待导航数据");
    lv_label_set_text_static(ui_TurnArrow, "?");
    lv_label_set_text_static(ui_DistanceLabel, "--");
    lv_label_set_text_static(ui_SpeedLabel, "0");
    lv_label_set_text_static(ui_LimitLabel, "--");
    lv_obj_add_flag(ui_LimitSign, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_CameraIcon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(ui_LaneContainer);
    lv_obj_add_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);
}
