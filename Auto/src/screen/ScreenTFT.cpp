#include "ScreenTFT.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <math.h>
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// v0.6.4: iWatch 极简风格 HUD 导航显示
// 屏幕：MSP2807 2.8" ILI9341 (320x240), SPI 40MHz
// 20fps (50ms/frame), pushSprite ~30ms + 渲染 ~20ms
// sprite 双缓冲消除画面撕裂
// ══════════════════════════════════════════════════════════════

ScreenTFT::ScreenTFT()
    : tft(), sprite(&tft) {
}

// ── 初始化 ────────────────────────────────────────────────

bool ScreenTFT::init() {
    delay(300);

    tft.init();
    tft.setRotation(1);

    mW = tft.width();
    mH = tft.height();

    if (mW == 0 || mH == 0) {
        Serial.println("[Screen] TFT 未检测到（检查 5V 供电 + SPI 接线）");
        return false;
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
#endif

    uint16_t* buf = (uint16_t*)sprite.createSprite(mW, mH);
    if (buf == nullptr) {
        Serial.println("[Screen] sprite 分配失败（PSRAM 未启用？）");
        return false;
    }

    sprite.setColorDepth(16);
    sprite.setTextWrap(false);
    sprite.fillScreen(HudColor::BG);

    mInited = true;
    mSpriteOk = true;
    Serial.printf("[Screen] ILI9341 %dx%d 横屏 (v0.6.4, 20fps, SPI40M)\n", mW, mH);
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

    if (mFrameCounter == 1) {
        drawBootScreen();
        sprite.pushSprite(0, 0);
        return;
    }

    drawBackground();

    bool nav = (mState.mapState == Nav::MapState::Navigating);

    if (nav && mState.guide.valid) {
        drawTopBar();
        drawSpeedLimit();
        drawArrowAndDistance();
        drawRoadNames();
        drawLaneBar();
        drawSpeed();
        drawTmcBar();
        drawBottomBar();
    } else if (mState.mapState == Nav::MapState::Arrived) {
        drawTopBar();
        sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("已到达", mW / 2, mH / 2, 4);
    } else {
        drawIdleScreen();
    }

    sprite.pushSprite(0, 0);
}

// ── 背景 ──────────────────────────────────────────────────

void ScreenTFT::drawBackground() {
    sprite.fillScreen(HudColor::BG);
}

// ── 启动画面 ──────────────────────────────────────────────

void ScreenTFT::drawBootScreen() {
    sprite.fillScreen(HudColor::BG);
    int cx = mW / 2, cy = mH / 2;

    sprite.drawRoundRect(10, 10, mW - 20, mH - 20, 8, HudColor::DARK);
    sprite.drawRoundRect(12, 12, mW - 24, mH - 24, 7, HudColor::DIM);

    int sz = 26;
    sprite.fillTriangle(cx, cy - sz, cx - sz, cy + sz / 2,
                        cx + sz, cy + sz / 2, HudColor::PRIMARY);
    sprite.fillRect(cx - 4, cy + sz / 2, 8, 14, HudColor::PRIMARY);

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(BC_DATUM);
    sprite.drawString("AutoNavDisplay v" PROJECT_VERSION, cx, mH - 14, 2);

    int barY = mH - 30;
    sprite.drawRoundRect(20, barY, mW - 40, 4, 2, HudColor::DARK);
    sprite.fillRoundRect(22, barY + 1, (mW - 44) / 3, 2, 1, HudColor::ACCENT);
    sprite.fillRoundRect(22 + (mW - 44) / 3, barY + 1, (mW - 44) / 3, 2, 1, HudColor::PRIMARY);
}

// ── 顶部状态栏 ────────────────────────────────────────────

void ScreenTFT::drawTopBar() {
    sprite.fillRect(0, 0, mW, TOP_BAR_H, HudColor::DARK);

    int dotR = 3, dotX = 7, dotY = TOP_BAR_H / 2;
    sprite.fillCircle(dotX, dotY, dotR,
                      mBleConnected ? HudColor::ACCENT : HudColor::DANGER);

    sprite.setTextColor(HudColor::DIM, HudColor::DARK);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString("NAV", dotX + dotR + 4, dotY, 2);
}

// ── 限速圆圈（右上角） ────────────────────────────────────

void ScreenTFT::drawSpeedLimit() {
    if (mState.guide.limitedSpeed <= 0) return;

    bool over = mState.guide.curSpeed > mState.guide.limitedSpeed;
    drawSpeedLimitCircle(LIMIT_CX, LIMIT_CY, LIMIT_R,
                         mState.guide.limitedSpeed, over);
}

// ── 转向箭头 + 距离（大号居中） ────────────────────────────

void ScreenTFT::drawArrowAndDistance() {
    int icon = mState.guide.icon;

    uint16_t arrowColor = HudColor::PRIMARY;
    if (Feature::ENABLE_ANIMATION && (mFrameCounter % 10) < 5) {
        arrowColor = HudColor::WHITE;
    }

    drawArrowIcon(ARROW_CX, ARROW_CY, ARROW_SZ, icon, arrowColor);

    char buf[32];
    if (mState.guide.distanceText[0]) {
        snprintf(buf, sizeof(buf), "%s", mState.guide.distanceText);
    } else {
        ScreenRenderer::formatDistance(mState.guide.segRemainDis, buf, sizeof(buf));
    }

    uint16_t distColor = HudColor::WHITE;
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 200) distColor = HudColor::WARN;
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 50)  distColor = HudColor::DANGER;

    sprite.setTextColor(distColor, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(buf, ARROW_CX, DIST_Y, 4);
}

// ── 道路名称（iWatch 格式："从 XXX 进入"） ────────────────

void ScreenTFT::drawRoadNames() {
    // 分隔线
    sprite.drawFastHLine(ARROW_CX - 70, ROAD_SEP_Y, 140, HudColor::DARK);

    char curBuf[80] = {0};
    if (mState.guide.curRoadName[0]) {
        snprintf(curBuf, sizeof(curBuf), "从 %s 进入", mState.guide.curRoadName);
    } else {
        snprintf(curBuf, sizeof(curBuf), "沿当前道路行驶");
    }

    sprite.setTextColor(HudColor::WHITE, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(curBuf, ARROW_CX, CUR_ROAD_Y, 2);

    if (mState.guide.nextRoadName[0]) {
        sprite.setTextColor(HudColor::DIM, HudColor::BG);
        sprite.drawString(mState.guide.nextRoadName, ARROW_CX, NEXT_ROAD_Y, 2);
    }
}

// ── 车道指引 ──────────────────────────────────────────────

void ScreenTFT::drawLaneBar() {
    int laneCount = mState.driveWay.laneCount;
    if (!mState.driveWay.enabled || laneCount == 0) return;

    if (laneCount > Nav::MAX_LANES) laneCount = Nav::MAX_LANES;

    int laneW = 26, laneH = 14, gap = 2;
    int totalW = laneCount * (laneW + gap) - gap;
    int startX = (mW - totalW) / 2;
    if (startX < 6) startX = 6;
    int laneY = NEXT_ROAD_Y + 4;

    for (int i = 0; i < laneCount; i++) {
        int lx = startX + i * (laneW + gap);
        int backIcon = mState.driveWay.lanes[i].backIcon;

        uint16_t color = HudColor::PRIMARY;
        sprite.fillRoundRect(lx, laneY, laneW, laneH, 3, color);

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
        sprite.drawString(label, lx + laneW / 2, laneY + laneH / 2, 2);
    }
}

// ── 车速（大号居中） ──────────────────────────────────────

void ScreenTFT::drawSpeed() {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", mState.guide.curSpeed);

    sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(buf, ARROW_CX, SPEED_Y, 6);

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.drawString("km/h", ARROW_CX, SPEED_Y + 26, 2);
}

// ── 路况光柱 ──────────────────────────────────────────────

void ScreenTFT::drawTmcBar() {
    if (!mState.tmc.enabled || mState.tmc.segmentCount == 0) return;

    int totalDist = mState.tmc.totalDistance;
    if (totalDist <= 0) return;

    int barW = mW - 16, x = 8;

    for (int i = 0; i < mState.tmc.segmentCount && i < Nav::MAX_TMC_SEGMENTS; i++) {
        int segW = (int)((float)mState.tmc.segments[i].distance / totalDist * barW);
        if (segW < 1) segW = 1;

        uint16_t color = HudColor::DARK;
        switch (mState.tmc.segments[i].status) {
            case 1: color = HudColor::ACCENT; break;
            case 2: color = HudColor::YELLOW; break;
            case 3: color = HudColor::WARN;   break;
            case 4: color = HudColor::DANGER; break;
        }
        sprite.fillRect(x, TMC_BAR_Y, segW, TMC_BAR_H, color);
        x += segW;
    }
}

// ── 底部信息栏 ────────────────────────────────────────────

void ScreenTFT::drawBottomBar() {
    sprite.drawFastHLine(0, TMC_BAR_Y - 2, mW, HudColor::DARK);

    int textY = BTM_BAR_Y + BTM_BAR_H / 2;

    if (mState.guide.routeRemainDis > 0 || mState.guide.routeRemainTime > 0) {
        char disBuf[16], timeBuf[16];
        ScreenRenderer::formatDistance(mState.guide.routeRemainDis, disBuf, sizeof(disBuf));
        ScreenRenderer::formatTime(mState.guide.routeRemainTime, timeBuf, sizeof(timeBuf));

        char buf[48];
        snprintf(buf, sizeof(buf), "%s | %s", timeBuf, disBuf);
        sprite.setTextColor(HudColor::DIM, HudColor::BG);
        sprite.setTextDatum(BL_DATUM);
        sprite.drawString(buf, 6, textY, 2);
    }

    if (mState.location.valid && mState.location.bearing > 0) {
        sprite.setTextColor(HudColor::DIM, HudColor::BG);
        sprite.setTextDatum(BR_DATUM);
        char buf[16];
        snprintf(buf, sizeof(buf), "%s ^", bearingLabel(mState.location.bearing));
        sprite.drawString(buf, mW - 6, textY, 2);
    }
}

// ── 空闲画面 ──────────────────────────────────────────────

void ScreenTFT::drawIdleScreen() {
    drawTopBar();

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("等待导航数据", mW / 2, mH / 2 - 16, 4);
    sprite.drawString("Waiting for Navigation Data", mW / 2, mH / 2 + 16, 2);

    if (mFrameCounter % 20 < 10) {
        sprite.fillCircle(mW / 2, mH / 2 + 44, 4, HudColor::PRIMARY);
    } else {
        sprite.drawCircle(mW / 2, mH / 2 + 44, 4, HudColor::DARK);
    }
}

// ── 转向箭头绘制（旋转 + 填充三角形） ──────────────────────

void ScreenTFT::drawArrowIcon(int cx, int cy, int sz, int icon, uint16_t color) {
    float angle = 0;
    switch (icon) {
        case 1: case 9:                      angle = 0;           break;
        case 2:                              angle = -PI / 2;     break;
        case 3:                              angle = PI / 2;      break;
        case 4:                              angle = -PI / 4;     break;
        case 5:                              angle = PI / 4;      break;
        case 6:                              angle = -3 * PI / 4; break;
        case 7:                              angle = 3 * PI / 4;  break;
        case 8: case 19:                     angle = PI;          break;
        case 15:
            sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString("END", cx, cy, 4);
            return;
        case 20:
            sprite.drawCircle(cx, cy, sz / 2, color);
            sprite.drawCircle(cx, cy, sz / 2 - 2, color);
            sprite.setTextColor(color, HudColor::BG);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString("O", cx, cy, 2);
            return;
        default:
            sprite.fillCircle(cx, cy, sz / 4, color);
            return;
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

    int x0 = cx - sw / 2, y0 = cy + hh / 2;
    int x1 = cx + sw / 2, y1 = cy + hh / 2;
    int x2 = cx + sw / 2, y2 = cy + hh / 2 - sh;
    int x3 = cx - sw / 2, y3 = cy + hh / 2 - sh;
    sprite.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x1, y1), rotY(x1, y1),
                        rotX(x2, y2), rotY(x2, y2), color);
    sprite.fillTriangle(rotX(x0, y0), rotY(x0, y0), rotX(x2, y2), rotY(x2, y2),
                        rotX(x3, y3), rotY(x3, y3), color);

    int hx0 = cx - hw / 2, hy0 = cy - sh + hh / 2;
    int hx1 = cx + hw / 2, hy1 = cy - sh + hh / 2;
    int hx2 = cx,          hy2 = cy - sh + hh / 2 - hh;
    sprite.fillTriangle(rotX(hx0, hy0), rotY(hx0, hy0), rotX(hx1, hy1), rotY(hx1, hy1),
                        rotX(hx2, hy2), rotY(hx2, hy2), color);
}

// ── 限速圆圈 ──────────────────────────────────────────────

void ScreenTFT::drawSpeedLimitCircle(int cx, int cy, int r, int speed, bool over) {
    uint16_t ring = over ? HudColor::DANGER : HudColor::WARN;
    uint16_t fill = over ? HudColor::DANGER : HudColor::BLUE;

    sprite.fillCircle(cx, cy, r, ring);
    sprite.fillCircle(cx, cy, r - 2, fill);

    sprite.setTextColor(HudColor::WHITE, fill);
    sprite.setTextDatum(MC_DATUM);
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", speed);
    sprite.drawString(buf, cx, cy, 2);
}

// ── 圆角按钮 ──────────────────────────────────────────────

void ScreenTFT::drawRoundedBtn(int x, int y, int w, int h, int r, uint16_t bg, uint16_t border, const char* text) {
    sprite.fillRoundRect(x, y, w, h, r, bg);
    if (border != bg) {
        sprite.drawRoundRect(x, y, w, h, r, border);
    }
    if (text && text[0]) {
        sprite.setTextColor(HudColor::WHITE, bg);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString(text, x + w / 2, y + h / 2, 2);
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