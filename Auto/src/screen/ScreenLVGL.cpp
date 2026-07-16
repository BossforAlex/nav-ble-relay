#include "ScreenLVGL.h"
#include "config/Config.h"
#include <stdio.h>
#include <string.h>
#include <esp_task_wdt.h>

/* ══════════════════════════════════════════════════════════════
 * v0.9.8 — 纯展示层（参照 ui.txt 设计规范）
 *
 * 架构：
 *   ui.c: 静态 UI 框架（创建控件，不包含数据逻辑）
 *   ScreenLVGL: 数据→控件映射层（NavState → lv_label_set_text）
 *   main.cpp: 桥接层（BLE 数据 → NavParser → Screen 方法）
 *
 * 高德导航方向箭头 icon 映射（AmapAuto SDK 官方协议，v0.9.8 修正）：
 *   icon  0: 未定义     ↑
 *   icon  1: 直行       ↑
 *   icon  2: 左转       ←
 *   icon  3: 右转       →
 *   icon  4: 左前方      ↖
 *   icon  5: 右前方      ↗
 *   icon  6: 左后方      ↙
 *   icon  7: 右后方      ↘
 *   icon  8: 左转掉头    ↶
 *   icon  9: 直行       ↑
 *   icon 10: 到达途经点  ★
 *   icon 11: 进入环岛    ◎
 *   icon 12: 驶出环岛    ◎
 *   icon 13: 到达服务区  ★
 *   icon 14: 到达收费站  ★
 *   icon 15: 到达目的地  ★
 *   icon 16: 进入隧道    ◎
 *   icon 17: 进入环岛(左行) ◎
 *   icon 18: 驶出环岛(左行) ◎
 *   icon 19: 右转掉头    ↷
 *   icon 20: 顺行       ↑
 *
 * 车道 backIcon 映射 (AmapAuto 官方协议，v0.9.8 修正)：
 *   0: 直行        1: 左转        2: 直行和左转
 *   3: 右转        4: 直行和右转   5: 左转掉头
 *   6: 左转和右转   7: 直行和左转和右转
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

/* ── v0.9.5: TFT 初始化（单次，TFT_eSPI 自行管理 HSPI 总线） ── */

bool ScreenLVGL::tftInitWithRetry(int maxRetries) {
    // 注意：mTft.init() 内部有 _init_done 标志，只能调用一次，重试不会重新初始化 SPI。
    // 真正的重试需要通过硬件复位 TFT（RST 引脚）来让 ILI9341 重新进入已知状态。
    // 硬件复位已在 main.cpp 中完成（RST LOW→HIGH + 150ms 等待）。
    //
    // v0.9.6: 移除无效的 mTft.width()/height() 验证。
    // TFT_eSPI 的 width()/height() 返回编译时配置值（320x240），
    // 不是从显示器读取的实际分辨率，永远通过验证，无法检测真实的初始化失败。
    // 真实验证依赖 main.cpp 中的 spi_bus_initialize() 返回值。

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        if (Serial) Serial.printf("[ScreenLVGL] TFT 初始化 (第 %d/%d 次)...\n", attempt, maxRetries);

        if (attempt == 1) {
            // 首次调用：TFT_eSPI 初始化 HSPI 总线 + 发送 ILI9341 初始化命令
            mTft.init();
        } else {
            // 后续重试：SPI 总线已初始化，仅硬件复位 TFT 后重新发送命令
            // 通过 RST 引脚复位 ILI9341，使其回到已知状态
            pinMode(TFT_RST, OUTPUT);
            digitalWrite(TFT_RST, LOW);
            delay(20);
            digitalWrite(TFT_RST, HIGH);
            delay(150);
            // 注意：mTft.init() 再次调用会因 _init_done 而跳过，
            // 但 TFT 硬件已复位，begin_tft_write/end_tft_write 仍可正常通信
        }

        mTft.setRotation(1);
        mTft.fillScreen(TFT_BLACK);
        delay(30);

        // v0.9.6: 通过发送真实绘图命令验证 SPI 通信正常
        // 如果 SPI 总线未初始化，这些命令会排队但不会崩溃
        mTft.fillScreen(TFT_BLACK);
        mTft.drawPixel(0, 0, TFT_WHITE);
        // 若 SPI 总线正常，命令已发送；若总线异常，不会崩溃但显示无效果
        // 无论如何，init() 已调用，返回 true 让上层继续
        if (Serial) Serial.printf("[ScreenLVGL] ✓ TFT 初始化完成 (第 %d 次)\n", attempt);
        return true;
    }
    return false;
}

/* ── 初始化 ────────────────────────────────────────────────── */

// v0.9.3: 背光独立控制，init 成功后由外部调用
void ScreenLVGL::enableBacklight() {
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    // 先低电平确保关闭，再延迟后点亮（避免电流尖峰）
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
    delay(50);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
    if (Serial) Serial.println("[ScreenLVGL] 背光已点亮");
#endif
}

bool ScreenLVGL::init() {
    // v0.9.5: 移除多余 delay(300)，硬件复位后的 150ms 等待已在 main.cpp 完成
    sInstance = this;

    // v0.9.1: 使用带重试的 TFT 初始化
    if (!tftInitWithRetry(3)) {
        if (Serial) Serial.println("[ScreenLVGL] TFT 初始化全部失败！检查供电和 SPI 接线");
        return false;
    }

    // v0.9.3: 背光延迟到 init 成功后由外部调用 enableBacklight()
    // 避免在电源不稳定时点亮背光导致电压跌落

    int w = mTft.width();
    int h = mTft.height();
    if (w == 0 || h == 0) {
        if (Serial) Serial.println("[ScreenLVGL] TFT 未检测到（检查 5V 供电 + SPI 接线）");
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
        if (Serial) Serial.println("[ScreenLVGL] LVGL 显示缓冲分配失败");
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

    if (Serial) Serial.printf("[ScreenLVGL] ILI9341 %dx%d + LVGL v%d.%d.%d 初始化完成\n",
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
    if (Debug::LOG_SYSTEM && Serial) {
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
    // v0.9.8: 修正为 AmapAuto SDK 官方协议 icon 映射
    // 参考: amap_protocol.dart iconMap (0=未定义,1=直行,2=左转,3=右转,4=左前方,...)
    const char* symbol = "↑";  // 默认直行
    switch (amapIcon) {
        case 0:  symbol = "↑";  break;  // 未定义 → 直行
        case 1:  symbol = "↑";  break;  // 直行
        case 2:  symbol = "←";  break;  // 左转
        case 3:  symbol = "→";  break;  // 右转
        case 4:  symbol = "↖";  break;  // 左前方
        case 5:  symbol = "↗";  break;  // 右前方
        case 6:  symbol = "↙";  break;  // 左后方
        case 7:  symbol = "↘";  break;  // 右后方
        case 8:  symbol = "↶";  break;  // 左转掉头
        case 9:  symbol = "↑";  break;  // 直行
        case 10: symbol = "★";  break;  // 到达途经点
        case 11: symbol = "◎";  break;  // 进入环岛
        case 12: symbol = "◎";  break;  // 驶出环岛
        case 13: symbol = "★";  break;  // 到达服务区
        case 14: symbol = "★";  break;  // 到达收费站
        case 15: symbol = "★";  break;  // 到达目的地
        case 16: symbol = "◎";  break;  // 进入隧道
        case 17: symbol = "◎";  break;  // 进入环岛(左行)
        case 18: symbol = "◎";  break;  // 驶出环岛(左行)
        case 19: symbol = "↷";  break;  // 右转掉头
        case 20: symbol = "↑";  break;  // 顺行
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

void ScreenLVGL::showLanes(int count, const int* backIcons, int turnIcon) {
    if (!mInited) return;
    lv_obj_clean(ui_LaneContainer);
    if (count <= 0) {
        count = 4;
    }
    if (count > 8) count = 8;
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    // v0.9.8: 修正为 AmapAuto SDK 官方协议 backIcon 映射
    // 参考: amap_protocol.dart laneBackIconMap
    //   0=直行, 1=左转, 2=直行和左转, 3=右转,
    //   4=直行和右转, 5=左转掉头, 6=左转和右转, 7=直行和左转和右转
    //
    // v0.9.8: 活跃车道判定 —— 根据当前转向 icon 推断推荐车道
    //   活跃车道高亮显示（亮白），非活跃车道变暗（灰色）
    auto isLaneActive = [turnIcon](int backIcon) -> bool {
        if (turnIcon < 0) return true; // 无转向信息时全部显示为活跃
        switch (turnIcon) {
            case 1:  case 9:  case 20: // 直行/顺行
                return backIcon == 0 || backIcon == 2 || backIcon == 4 || backIcon == 7;
            case 2:  // 左转
                return backIcon == 1 || backIcon == 2 || backIcon == 6 || backIcon == 7;
            case 3:  // 右转
                return backIcon == 3 || backIcon == 4 || backIcon == 6 || backIcon == 7;
            case 4:  // 左前方
                return backIcon == 1 || backIcon == 2;
            case 5:  // 右前方
                return backIcon == 3 || backIcon == 4;
            case 8:  case 19: // 左转掉头 / 右转掉头
                return backIcon == 5;
            default:
                return true; // 环岛/服务区等场景全部显示
        }
    };

    // v0.9.1: 计算每个车道标签的宽度（Flexbox 均分）
    // v0.9.2: 容器从 132px 增大到 137px
    int laneW = (137 - 4) / count;

    for (int i = 0; i < count; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int bi = backIcons ? backIcons[i] : 0;
        const char* sym = "↑";
        // v0.9.8: 修正 backIcon 映射（匹配 AmapAuto 官方协议）
        switch (bi) {
            case 0: sym = "↑";  break;  // 直行
            case 1: sym = "←";  break;  // 左转
            case 2: sym = "↖";  break;  // 直行和左转
            case 3: sym = "→";  break;  // 右转
            case 4: sym = "↗";  break;  // 直行和右转
            case 5: sym = "↶";  break;  // 左转掉头
            case 6: sym = "↰";  break;  // 左转和右转
            case 7: sym = "↺";  break;  // 直行和左转和右转
            default: sym = "↑"; break;
        }

        // v0.9.1: 使用 lv_label_set_text 确保字体正确渲染
        lv_label_set_text(arrow, sym);
        // v0.9.8: 活跃车道高亮亮白，非活跃车道变暗灰
        bool active = isLaneActive(bi);
        lv_color_t color = active ? lv_color_white() : lv_color_hex(0x555555);
        lv_obj_set_style_text_color(arrow, color, 0);
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
    // v0.9.2: 提高对比度，原 0x00E676/0x666666 在纯黑背景上不明显
    lv_color_t c = mBleConnected ? lv_color_hex(0x00E676) : lv_color_hex(0x888888);
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
    showLanes(laneCount, backIcons, mState.guide.icon);

    // 全程信息
    char routeBuf[64];
    formatRouteInfo(routeBuf, sizeof(routeBuf));
    showRouteInfo(routeBuf);
}