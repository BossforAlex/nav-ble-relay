#include "ScreenTFT.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <math.h>
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// v0.6.0: ILI9341 横屏 320x240 HUD 导航显示
// 屏幕：MSP2807 2.8" SPI ILI9341
// 接线：CS=10 DC=2 RST=4 MOSI=11 SCK=12 BL=6
// ══════════════════════════════════════════════════════════════

ScreenTFT::ScreenTFT()
    : tft(), sprite(&tft) {
}

// ── 初始化 ────────────────────────────────────────────────

bool ScreenTFT::init() {
    tft.init();
    tft.setRotation(1);  // 横屏 320x240
    mW = tft.width();
    mH = tft.height();

    if (mW == 0 || mH == 0) {
        Serial.println("[Screen] TFT 未检测到");
        return false;
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

    // 创建离屏 sprite（双缓冲，避免闪烁）
    sprite.setColorDepth(16);
    sprite.setTextWrap(false);
    uint16_t* buf = (uint16_t*)sprite.createSprite(mW, mH);
    if (buf == nullptr) {
        Serial.println("[Screen] sprite 帧缓冲分配失败");
        return false;
    }

    mInited = true;
    mSpriteOk = true;
    Serial.printf("[Screen] ILI9341 TFT 初始化完成 %dx%d 横屏\n", mW, mH);
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
    if (!mInited || !mSpriteOk) return;

    if (mFrameCounter == 0) {
        drawBootScreen();
        sprite.pushSprite(0, 0);
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
        sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Arrived", mW / 2, mH / 2, 4);
    } else {
        drawIdleScreen();
    }

    sprite.pushSprite(0, 0);
}

// ── 背景 ──────────────────────────────────────────────────

void ScreenTFT::drawBackground() {
    sprite.fillSprite(HudColor::BG);
}

// ── 启动画面 ──────────────────────────────────────────────

void ScreenTFT::drawBootScreen() {
    sprite.fillSprite(HudColor::BG);
    int cx = mW / 2, cy = mH / 2;

    // 外框
    sprite.drawRect(8, 8, mW - 16, mH - 16, HudColor::PRIMARY);
    sprite.drawRect(12, 12, mW - 24, mH - 24, HudColor::DIM);

    // 导航箭头
    int sz = 30;
    sprite.fillTriangle(cx, cy - sz, cx - sz, cy + sz / 2,
                        cx + sz, cy + sz / 2, HudColor::PRIMARY);
    sprite.fillRect(cx - 5, cy + sz / 2, 10, 20, HudColor::PRIMARY);

    // 版本文字
    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("AutoNavDisplay v" PROJECT_VERSION, cx, mH - 12, 2);

    // 进度条
    int barY = mH - 32;
    sprite.drawRect(18, barY, mW - 36, 5, HudColor::DIM);
    sprite.fillRect(20, barY + 1, (mW - 40) / 3, 3, HudColor::ACCENT);
    sprite.fillRect(20 + (mW - 40) / 3, barY + 1, (mW - 40) / 3, 3, HudColor::PRIMARY);
}

// ── 顶部状态栏 ────────────────────────────────────────────

void ScreenTFT::drawTopBar() {
    sprite.fillRect(0, 0, mW, TOP_BAR_H, HudColor::DIM);

    // BLE 状态点
    int dotR = 4;
    int dotX = 10, dotY = TOP_BAR_H / 2;
    sprite.fillCircle(dotX, dotY, dotR,
                      mBleConnected ? HudColor::ACCENT : HudColor::DANGER);

    // 导航状态 + 帧计数
    sprite.setTextColor(HudColor::WHITE, HudColor::DIM);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString(ScreenRenderer::mapStateLabel(mState.mapState),
                      dotX + dotR + 6, dotY, 2);

    // 方位角
    sprite.setTextDatum(MR_DATUM);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s ^", bearingLabel(mState.location.bearing));
    sprite.drawString(buf, mW - 6, dotY, 2);
}

// ── 转向箭头（主区域左侧） ────────────────────────────────

void ScreenTFT::drawTurnArrow() {
    int cx = 100;  // 左侧区域中心
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
    // 高德 ICON 到角度映射
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
            sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString("END", cx, cy, 4);
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
    sprite.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x1, y1), rotY(x1, y1),
                        rotX(x2, y2), rotY(x2, y2), color);
    sprite.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x2, y2), rotY(x2, y2),
                        rotX(x3, y3), rotY(x3, y3), color);

    // 箭头头
    int hx0 = cx - hw / 2, hy0 = cy - sh + hh / 2;
    int hx1 = cx + hw / 2, hy1 = cy - sh + hh / 2;
    int hx2 = cx,          hy2 = cy - sh + hh / 2 - hh;
    sprite.fillTriangle(rotX(hx0, hy0), rotY(hx0, hy0), rotX(hx1, hy1), rotY(hx1, hy1),
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

    sprite.setTextColor(color, HudColor::BG);
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString(buf, cx, y, 4);
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
    sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", mState.guide.curSpeed);
    sprite.drawString(buf, cx, cy + 25, 4);

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.drawString("km/h", cx, cy + 55, 2);
}

void ScreenTFT::drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool over) {
    uint16_t ring = over ? HudColor::DANGER : HudColor::WARN;
    sprite.drawCircle(cx, cy, r, ring);
    sprite.drawCircle(cx, cy, r - 1, ring);
    sprite.fillCircle(cx, cy, r - 2, HudColor::BG);
    sprite.setTextColor(over ? HudColor::DANGER : HudColor::WHITE, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", speed);
    sprite.drawString(buf, cx, cy, 2);
}

// ── 车道指引 ──────────────────────────────────────────────

void ScreenTFT::drawLaneBar() {
    sprite.fillRect(0, LANE_BAR_Y, mW, LANE_BAR_H, HudColor::BG);

    int laneCount = mState.driveWay.laneCount;
    if (!mState.driveWay.enabled || laneCount == 0) {
        // 无车道数据时显示电子眼
        if (mState.guide.cameraDist > 0) {
            char buf[24];
            snprintf(buf, sizeof(buf), "CAM %dm", mState.guide.cameraDist);
            sprite.setTextColor(HudColor::WARN, HudColor::BG);
            sprite.setTextDatum(MR_DATUM);
            sprite.drawString(buf, mW - 6, LANE_BAR_Y + LANE_BAR_H / 2, 2);
        }
        return;
    }

    if (laneCount > 8) laneCount = 8;

    int laneW = 32;
    int laneH = 18;
    int totalW = laneCount * (laneW + 2);
    int startX = (mW - totalW) / 2;
    if (startX < 6) startX = 6;

    for (int i = 0; i < laneCount; i++) {
        int lx = startX + i * (laneW + 2);
        int ly = LANE_BAR_Y + (LANE_BAR_H - laneH) / 2;
        int backIcon = mState.driveWay.lanes[i].backIcon;

        uint16_t color = HudColor::PRIMARY;
        sprite.fillRoundRect(lx, ly, laneW, laneH, 3, color);

        sprite.setTextColor(HudColor::BG, color);
        sprite.setTextDatum(MC_DATUM);
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
        sprite.drawString(label, lx + laneW / 2, ly + laneH / 2, 2);
    }

    // 电子眼（右侧）
    if (mState.guide.cameraDist > 0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "CAM %dm", mState.guide.cameraDist);
        sprite.setTextColor(HudColor::WARN, HudColor::BG);
        sprite.setTextDatum(MR_DATUM);
        sprite.drawString(buf, mW - 4, LANE_BAR_Y + LANE_BAR_H / 2, 2);
    }
}

// ── 路况光柱 ──────────────────────────────────────────────

void ScreenTFT::drawTmcBar() {
    if (!mState.tmc.enabled || mState.tmc.segmentCount == 0) return;

    int totalDist = mState.tmc.totalDistance;
    if (totalDist <= 0) return;

    int barW = mW - 12;
    int startX = 6;
    int x = startX;

    for (int i = 0; i < mState.tmc.segmentCount && i < MAX_TMC_SEGMENTS; i++) {
        int segW = (int)((float)mState.tmc.segments[i].distance / totalDist * barW);
        if (segW < 1) segW = 1;

        uint16_t color = HudColor::DIM;
        switch (mState.tmc.segments[i].status) {
            case 1: color = HudColor::ACCENT; break;  // 畅通
            case 2: color = HudColor::YELLOW; break;  // 缓行
            case 3: color = HudColor::WARN;   break;  // 拥堵
            case 4: color = HudColor::DANGER; break;  // 严重拥堵
        }
        sprite.fillRect(x, TMC_BAR_Y, segW, TMC_BAR_H, color);
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
        sprite.setTextColor(HudColor::WHITE, HudColor::BG);
        sprite.setTextDatum(TC_DATUM);
        sprite.drawString(buf, mW / 2, ROAD_BAR_Y + 2, 2);
    }
}

// ── 底部信息栏 ────────────────────────────────────────────

void ScreenTFT::drawBottomBar() {
    int y = mH - BOTTOM_BAR_H;

    // 全程剩余距离/时间
    if (mState.guide.routeRemainDis > 0 || mState.guide.routeRemainTime > 0) {
        char disBuf[16], timeBuf[16];
        ScreenRenderer::formatDistance(mState.guide.routeRemainDis, disBuf, sizeof(disBuf));
        ScreenRenderer::formatTime(mState.guide.routeRemainTime, timeBuf, sizeof(timeBuf));

        char buf[40];
        snprintf(buf, sizeof(buf), "%s / %s", disBuf, timeBuf);
        sprite.setTextColor(HudColor::DIM, HudColor::BG);
        sprite.setTextDatum(BL_DATUM);
        sprite.drawString(buf, 6, mH - 4, 2);
    }

    // GPS 坐标（简化显示精度）
    if (mState.location.valid) {
        sprite.setTextColor(HudColor::DIM, HudColor::BG);
        sprite.setTextDatum(BR_DATUM);
        char buf[24];
        snprintf(buf, sizeof(buf), "GPS:%d", mState.location.bearing);
        sprite.drawString(buf, mW - 6, mH - 4, 2);
    }
}

// ── 空闲画面 ──────────────────────────────────────────────

void ScreenTFT::drawIdleScreen() {
    drawTopBar();

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Waiting for", mW / 2, mH / 2 - 16, 4);
    sprite.drawString("Navigation Data", mW / 2, mH / 2 + 16, 4);

    // 呼吸灯
    if (mFrameCounter % 4 < 2) {
        sprite.fillCircle(mW / 2, mH / 2 + 50, 5, HudColor::PRIMARY);
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