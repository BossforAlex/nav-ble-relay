#include "ScreenTFT.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <math.h>
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// v0.6.2: ILI9341 横屏 320x240 HUD 导航显示
// 屏幕：MSP2807 2.8" SPI ILI9341
// 接线：CS=10 DC=2 RST=4 MOSI=11 SCK=12 BL=6
//
// v0.6.2 移除 sprite 帧缓冲，直接绘制到 TFT。
// 避免 PSRAM 依赖，兼容 ESP32-S3 SuperMini 克隆板。
// 使用 startWrite()/endWrite() 批量 SPI 事务减少闪烁。
// ══════════════════════════════════════════════════════════════

ScreenTFT::ScreenTFT()
    : tft() {
}

// ── 初始化 ────────────────────────────────────────────────

bool ScreenTFT::init() {
    // 上电后等待电源稳定
    delay(300);

    tft.init();
    tft.setRotation(1);  // 横屏 320x240

    // 验证 TFT 是否响应
    if (tft.width() == 0 || tft.height() == 0) {
        Serial.println("[Screen] TFT 未检测到（检查 5V 供电 + SPI 接线）");
        return false;
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
#endif

    // 清屏
    tft.fillScreen(HudColor::BG);

    mInited = true;
    Serial.printf("[Screen] ILI9341 TFT 初始化完成 %dx%d 横屏 (直接绘制)\n", W, H);
    return true;
}

// ── 主循环入口 ────────────────────────────────────────────

void ScreenTFT::update() {
    unsigned long now = millis();
    if (now - mLastRenderMs < Feature::SCREEN_REFRESH_MS) return;
    mLastRenderMs = now;
    mFrameCounter++;
    renderFrame();
}

void ScreenTFT::setNavState(const Nav::NavState& state) {
    mState = state;
}

void ScreenTFT::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[Screen] BLE %s\n", connected ? "已连接" : "已断开");
    }
}

void ScreenTFT::log(const char* msg) {
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[Screen] %s\n", msg);
    }
}

// ── 帧渲染 ────────────────────────────────────────────────

void ScreenTFT::renderFrame() {
    if (!mInited) return;

    tft.startWrite();

    if (mFrameCounter == 1) {
        drawBootScreen();
        tft.endWrite();
        return;
    }

    drawBackground();

    bool nav = (mState.mapState == Nav::MapState::Navigating);

    if (nav && mState.guide.valid) {
        drawTopBar();
        drawTurnArrow();
        drawDistance();
        drawSpeedPanel();
        drawLaneBar();
        drawTmcBar();
        drawRoadName();
        drawBottomBar();
    } else if (mState.mapState == Nav::MapState::Arrived) {
        drawTopBar();
        tft.setTextColor(HudColor::ACCENT, HudColor::BG);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Arrived", W / 2, H / 2, 4);
    } else {
        drawIdleScreen();
    }

    tft.endWrite();
}

// ── 背景 ──────────────────────────────────────────────────

void ScreenTFT::drawBackground() {
    tft.fillScreen(HudColor::BG);
}

// ── 启动画面 ──────────────────────────────────────────────

void ScreenTFT::drawBootScreen() {
    tft.fillScreen(HudColor::BG);
    int cx = W / 2, cy = H / 2;

    // 外框
    tft.drawRect(8, 8, W - 16, H - 16, HudColor::PRIMARY);
    tft.drawRect(12, 12, W - 24, H - 24, HudColor::DIM);

    // 导航箭头
    int sz = 30;
    tft.fillTriangle(cx, cy - sz, cx - sz, cy + sz / 2,
                     cx + sz, cy + sz / 2, HudColor::PRIMARY);
    tft.fillRect(cx - 5, cy + sz / 2, 10, 20, HudColor::PRIMARY);

    // 版本文字
    tft.setTextColor(HudColor::DIM, HudColor::BG);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("AutoNavDisplay v" PROJECT_VERSION, cx, H - 12, 2);

    // 进度条
    int barY = H - 32;
    tft.drawRect(18, barY, W - 36, 5, HudColor::DIM);
    tft.fillRect(20, barY + 1, (W - 40) / 3, 3, HudColor::ACCENT);
    tft.fillRect(20 + (W - 40) / 3, barY + 1, (W - 40) / 3, 3, HudColor::PRIMARY);
}

// ── 顶部状态栏 ────────────────────────────────────────────

void ScreenTFT::drawTopBar() {
    tft.fillRect(0, 0, W, TOP_BAR_H, HudColor::DIM);

    // BLE 状态点
    int dotR = 4;
    int dotX = 10, dotY = TOP_BAR_H / 2;
    tft.fillCircle(dotX, dotY, dotR,
                   mBleConnected ? HudColor::ACCENT : HudColor::DANGER);

    // 导航状态
    tft.setTextColor(HudColor::WHITE, HudColor::DIM);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(ScreenRenderer::mapStateLabel(mState.mapState),
                   dotX + dotR + 6, dotY, 2);

    // 方位角
    tft.setTextDatum(MR_DATUM);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s ^", bearingLabel(mState.location.bearing));
    tft.drawString(buf, W - 6, dotY, 2);
}

// ── 转向箭头（主区域左侧） ────────────────────────────────

void ScreenTFT::drawTurnArrow() {
    int cx = 100;
    int cy = MAIN_AREA_Y + MAIN_AREA_H / 2 + 10;
    int sz = 40;

    // 动画脉冲
    uint16_t color = HudColor::PRIMARY;
    if (Feature::ENABLE_ANIMATION && mFrameCounter % 6 < 3) {
        color = HudColor::WHITE;
    }

    drawArrowIcon(cx, cy, sz, mState.guide.icon, color);
}

void ScreenTFT::drawArrowIcon(int cx, int cy, int sz, int icon, uint16_t color) {
    float angle = 0;
    switch (icon) {
        case 1: case 9:                      angle = 0;           break;
        case 2:                              angle = -PI / 2;      break;
        case 3:                              angle = PI / 2;       break;
        case 4:                              angle = -PI / 4;      break;
        case 5:                              angle = PI / 4;       break;
        case 6:                              angle = -3 * PI / 4;  break;
        case 7:                              angle = 3 * PI / 4;   break;
        case 8: case 19:                     angle = PI;           break;
        case 15: // 到达
            tft.setTextColor(HudColor::ACCENT, HudColor::BG);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("END", cx, cy, 4);
            return;
        default: return;
    }

    auto rotX = [&](int x, int y) {
        float dx = x - cx, dy = y - cy;
        return (int)(dx * cosf(angle) - dy * sinf(angle) + cx);
    };
    auto rotY = [&](int x, int y) {
        float dx = x - cx, dy = y - cy;
        return (int)(dx * sinf(angle) + dy * cosf(angle) + cy);
    };

    int sw = sz / 5, sh = sz * 2 / 3;
    int hw = sz / 2, hh = sz / 3;

    // 杆（矩形 → 两个三角形）
    int x0 = cx - sw / 2, y0 = cy + hh / 2;
    int x1 = cx + sw / 2, y1 = cy + hh / 2;
    int x2 = cx + sw / 2, y2 = cy + hh / 2 - sh;
    int x3 = cx - sw / 2, y3 = cy + hh / 2 - sh;
    tft.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x1, y1), rotY(x1, y1),
                     rotX(x2, y2), rotY(x2, y2), color);
    tft.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x2, y2), rotY(x2, y2),
                     rotX(x3, y3), rotY(x3, y3), color);

    // 箭头头
    int hx0 = cx - hw / 2, hy0 = cy - sh + hh / 2;
    int hx1 = cx + hw / 2, hy1 = cy - sh + hh / 2;
    int hx2 = cx,          hy2 = cy - sh + hh / 2 - hh;
    tft.fillTriangle(rotX(hx0, hy0), rotY(hx0, hy0), rotX(hx1, hy1), rotY(hx1, hy1),
                     rotX(hx2, hy2), rotY(hx2, hy2), color);
}

// ── 距离文本（箭头下方） ──────────────────────────────────

void ScreenTFT::drawDistance() {
    int y = MAIN_AREA_Y + MAIN_AREA_H - 10;
    int cx = 100;

    char buf[32];
    if (mState.guide.distanceText[0]) {
        snprintf(buf, sizeof(buf), "%s", mState.guide.distanceText);
    } else {
        ScreenRenderer::formatDistance(mState.guide.segRemainDis, buf, sizeof(buf));
    }

    uint16_t color = HudColor::PRIMARY;
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 200) color = HudColor::WARN;
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 50)  color = HudColor::DANGER;

    tft.setTextColor(color, HudColor::BG);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(buf, cx, y, 4);
}

// ── 速度面板（右侧：限速圆 + 当前车速） ──────────────────

void ScreenTFT::drawSpeedPanel() {
    int cx = 260, cy = MAIN_AREA_Y + MAIN_AREA_H / 2;

    // 限速圆
    if (mState.guide.limitedSpeed > 0) {
        bool over = mState.guide.curSpeed > mState.guide.limitedSpeed;
        drawSpeedLimitCircle(cx, cy - 15, 30, mState.guide.limitedSpeed, over);
    }

    // 当前车速
    tft.setTextColor(HudColor::ACCENT, HudColor::BG);
    tft.setTextDatum(TC_DATUM);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", mState.guide.curSpeed);
    tft.drawString(buf, cx, cy + 25, 4);

    tft.setTextColor(HudColor::DIM, HudColor::BG);
    tft.drawString("km/h", cx, cy + 55, 2);
}

void ScreenTFT::drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool over) {
    uint16_t ring = over ? HudColor::DANGER : HudColor::WARN;
    tft.drawCircle(cx, cy, r, ring);
    tft.drawCircle(cx, cy, r - 1, ring);
    tft.fillCircle(cx, cy, r - 2, HudColor::BG);
    tft.setTextColor(over ? HudColor::DANGER : HudColor::WHITE, HudColor::BG);
    tft.setTextDatum(MC_DATUM);
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", speed);
    tft.drawString(buf, cx, cy, 2);
}

// ── 车道指引 ──────────────────────────────────────────────

void ScreenTFT::drawLaneBar() {
    tft.fillRect(0, LANE_BAR_Y, W, LANE_BAR_H, HudColor::BG);

    int laneCount = mState.driveWay.laneCount;
    if (!mState.driveWay.enabled || laneCount == 0) {
        if (mState.guide.cameraDist > 0) {
            char buf[24];
            snprintf(buf, sizeof(buf), "CAM %dm", mState.guide.cameraDist);
            tft.setTextColor(HudColor::WARN, HudColor::BG);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(buf, W - 6, LANE_BAR_Y + LANE_BAR_H / 2, 2);
        }
        return;
    }

    if (laneCount > Nav::MAX_LANES) laneCount = Nav::MAX_LANES;

    int laneW = 32;
    int laneH = 18;
    int totalW = laneCount * (laneW + 2);
    int startX = (W - totalW) / 2;
    if (startX < 6) startX = 6;

    for (int i = 0; i < laneCount; i++) {
        int lx = startX + i * (laneW + 2);
        int ly = LANE_BAR_Y + (LANE_BAR_H - laneH) / 2;
        int backIcon = mState.driveWay.lanes[i].backIcon;

        uint16_t color = HudColor::PRIMARY;
        tft.fillRoundRect(lx, ly, laneW, laneH, 3, color);

        tft.setTextColor(HudColor::BG, color);
        tft.setTextDatum(MC_DATUM);
        const char* label = "";
        switch (backIcon) {
            case 0: label = "^"; break;
            case 1: label = "<"; break;
            case 2: label = "<^"; break;
            case 3: label = ">"; break;
            case 4: label = "^>"; break;
            case 5: label = "U"; break;
            case 6: label = "<>"; break;
            case 7: label = "<^>"; break;
        }
        tft.drawString(label, lx + laneW / 2, ly + laneH / 2, 2);
    }

    if (mState.guide.cameraDist > 0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "CAM %dm", mState.guide.cameraDist);
        tft.setTextColor(HudColor::WARN, HudColor::BG);
        tft.setTextDatum(MR_DATUM);
        tft.drawString(buf, W - 4, LANE_BAR_Y + LANE_BAR_H / 2, 2);
    }
}

// ── 路况光柱 ──────────────────────────────────────────────

void ScreenTFT::drawTmcBar() {
    if (!mState.tmc.enabled || mState.tmc.segmentCount == 0) return;

    int totalDist = mState.tmc.totalDistance;
    if (totalDist <= 0) return;

    int barW = W - 12;
    int x = 6;

    for (int i = 0; i < mState.tmc.segmentCount && i < Nav::MAX_TMC_SEGMENTS; i++) {
        int segW = (int)((float)mState.tmc.segments[i].distance / totalDist * barW);
        if (segW < 1) segW = 1;

        uint16_t color = HudColor::DIM;
        switch (mState.tmc.segments[i].status) {
            case 1: color = HudColor::ACCENT; break;
            case 2: color = HudColor::YELLOW; break;
            case 3: color = HudColor::WARN;   break;
            case 4: color = HudColor::DANGER; break;
        }
        tft.fillRect(x, TMC_BAR_Y, segW, TMC_BAR_H, color);
        x += segW;
    }
}

// ── 道路名称 ──────────────────────────────────────────────

void ScreenTFT::drawRoadName() {
    char buf[80] = {0};
    if (mState.guide.intersection[0]) {
        snprintf(buf, sizeof(buf), "%s", mState.guide.intersection);
    } else if (mState.guide.nextRoadName[0]) {
        snprintf(buf, sizeof(buf), "%s -> %s",
                 mState.guide.curRoadName, mState.guide.nextRoadName);
    } else if (mState.guide.curRoadName[0]) {
        snprintf(buf, sizeof(buf), "%s", mState.guide.curRoadName);
    }

    if (buf[0]) {
        tft.setTextColor(HudColor::WHITE, HudColor::BG);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(buf, W / 2, ROAD_BAR_Y + 2, 2);
    }
}

// ── 底部信息栏 ────────────────────────────────────────────

void ScreenTFT::drawBottomBar() {
    if (mState.guide.routeRemainDis > 0 || mState.guide.routeRemainTime > 0) {
        char disBuf[16], timeBuf[16];
        ScreenRenderer::formatDistance(mState.guide.routeRemainDis, disBuf, sizeof(disBuf));
        ScreenRenderer::formatTime(mState.guide.routeRemainTime, timeBuf, sizeof(timeBuf));

        char buf[40];
        snprintf(buf, sizeof(buf), "%s / %s", disBuf, timeBuf);
        tft.setTextColor(HudColor::DIM, HudColor::BG);
        tft.setTextDatum(BL_DATUM);
        tft.drawString(buf, 6, H - 4, 2);
    }

    if (mState.location.valid) {
        tft.setTextColor(HudColor::DIM, HudColor::BG);
        tft.setTextDatum(BR_DATUM);
        char buf[24];
        snprintf(buf, sizeof(buf), "GPS:%d", mState.location.bearing);
        tft.drawString(buf, W - 6, H - 4, 2);
    }
}

// ── 空闲画面 ──────────────────────────────────────────────

void ScreenTFT::drawIdleScreen() {
    drawTopBar();

    tft.setTextColor(HudColor::DIM, HudColor::BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Waiting for", W / 2, H / 2 - 16, 4);
    tft.drawString("Navigation Data", W / 2, H / 2 + 16, 4);

    // 呼吸灯
    if (mFrameCounter % 4 < 2) {
        tft.fillCircle(W / 2, H / 2 + 50, 5, HudColor::PRIMARY);
    }
}

// ── 工具函数 ──────────────────────────────────────────────

const char* ScreenTFT::bearingLabel(int deg) {
    if (deg < 0) return "?";
    if (deg < 23 || deg >= 338) return "N";
    if (deg < 68)  return "NE";
    if (deg < 113) return "E";
    if (deg < 158) return "SE";
    if (deg < 203) return "S";
    if (deg < 248) return "SW";
    if (deg < 293) return "W";
    return "NW";
}

const char* ScreenTFT::arrowLabel(int icon) {
    switch (icon) {
        case 1:  return "^";
        case 2:  return "<";
        case 3:  return ">";
        case 4:  return "<^";
        case 5:  return "^>";
        case 6:  return "<v";
        case 7:  return "v>";
        case 8:  return "U";
        case 15: return "END";
        default: return "?";
    }
}