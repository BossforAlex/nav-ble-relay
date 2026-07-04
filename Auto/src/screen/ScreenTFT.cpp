#include "ScreenTFT.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <math.h>

ScreenTFT::ScreenTFT()
    : tft(), sprite(&tft) {
}

bool ScreenTFT::init() {
    tft.init();
    mWidth = tft.width();
    mHeight = tft.height();
    computeScale();

    // 背光
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

    tft.setRotation(0);  // 竖屏
    tft.fillScreen(HudColor::BG);

    sprite.setColorDepth(16);
    sprite.createSprite(mWidth, mHeight);

    mInited = true;
    drawBootScreen("Starting...");

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[Screen] TFT 初始化完成 %dx%d scale=%.2f\n", mWidth, mHeight, mScale);
    }
    return true;
}

void ScreenTFT::computeScale() {
    // 以 240x240 为基准计算缩放
    int minDim = mWidth < mHeight ? mWidth : mHeight;
    mScale = (float)minDim / 240.0f;
    if (mScale < 0.5f) mScale = 0.5f;
    if (mScale > 2.0f) mScale = 2.0f;
}

uint8_t ScreenTFT::scaleFont(uint8_t f) const {
    int s = (int)(f * mScale);
    if (s < 1) s = 1;
    if (s > 8) s = 8;
    return (uint8_t)s;
}

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

void ScreenTFT::renderFrame() {
    if (!mInited) return;

    drawBackground();

    bool navigating = (mState.mapState == Nav::MapState::Navigating);

    if (navigating && mState.guide.valid) {
        drawTopBar();
        drawTurnArrow();
        drawDistance();
        drawRoadName();
        drawSpeed();
        drawLanes();
        drawTmcBar();
        drawRouteInfo();
        drawCamera();
    } else if (mState.mapState == Nav::MapState::Arrived) {
        drawTopBar();
        sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextSize(scaleFont(4));
        sprite.drawString("Arrived", mWidth / 2, mHeight / 2);
    } else {
        drawIdleScreen();
    }

    // 推送到屏幕
    sprite.pushSprite(0, 0);
}

void ScreenTFT::drawBackground() {
    sprite.fillSprite(HudColor::BG);
}

void ScreenTFT::drawTopBar() {
    int barH = scale(20);
    sprite.fillRect(0, 0, mWidth, barH, HudColor::DIM);

    // BLE 状态点
    int dotR = scale(4);
    int dotX = scale(12);
    int dotY = barH / 2;
    sprite.fillCircle(dotX, dotY, dotR, mBleConnected ? HudColor::ACCENT : HudColor::DANGER);

    // 设备名
    sprite.setTextColor(HudColor::WHITE, HudColor::DIM);
    sprite.setTextDatum(ML_DATUM);
    sprite.setTextSize(scaleFont(1));
    sprite.drawString(DEVICE_NAME_PREFIX, dotX + dotR + scale(4), dotY);

    // 帧计数
    sprite.setTextDatum(MR_DATUM);
    sprite.setTextColor(HudColor::PRIMARY, HudColor::DIM);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", mFrameCounter);
    sprite.drawString(buf, mWidth - scale(4), dotY);
}

void ScreenTFT::drawTurnArrow() {
    int cx = mWidth / 2;
    int cy = scale(20) + scale(50);
    int size = scale(45);
    uint16_t color = HudColor::PRIMARY;

    // 动画脉冲
    if (Feature::ENABLE_ANIMATION) {
        uint8_t phase = mFrameCounter % 4;
        if (phase == 0) color = HudColor::PRIMARY;
        else if (phase == 2) color = HudColor::WHITE;
    }

    drawArrow(cx, cy, size, mState.guide.icon, color);
}

void ScreenTFT::drawArrow(int cx, int cy, int size, int icon, uint16_t color) {
    // 根据高德 ICON 定义绘制对应箭头
    // 简化实现：用旋转角度绘制主箭头
    float angle = 0;  // 弧度，0 = 向上

    switch (icon) {
        case 1: case 9: angle = 0; break;           // 直行
        case 2: angle = -PI / 2; break;              // 左转
        case 3: angle = PI / 2; break;               // 右转
        case 4: angle = -PI / 4; break;              // 左前方
        case 5: angle = PI / 4; break;               // 右前方
        case 6: angle = -3 * PI / 4; break;          // 左后方
        case 7: angle = 3 * PI / 4; break;           // 右后方
        case 8: case 19: angle = PI; break;          // 掉头
        case 15: // 到达目的地 - 绘制旗帜
            sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
            sprite.setTextDatum(MC_DATUM);
            sprite.setTextSize(scaleFont(4));
            sprite.drawString("END", cx, cy);
            return;
        default: angle = 0; break;
    }

    // 绘制粗箭头（杆 + 箭头头）
    int shaftW = size / 6;
    int shaftH = size * 2 / 3;
    int headW = size / 2;
    int headH = size / 3;

    // 用三角形构建向上箭头，再旋转
    // 杆
    int x0 = cx - shaftW / 2, y0 = cy + headH / 2;
    int x1 = cx + shaftW / 2, y1 = cy + headH / 2;
    int x2 = cx + shaftW / 2, y2 = cy + headH / 2 - shaftH;
    int x3 = cx - shaftW / 2, y3 = cy + headH / 2 - shaftH;

    // 箭头头（三角形）
    int hx0 = cx - headW / 2, hy0 = cy - shaftH + headH / 2;
    int hx1 = cx + headW / 2, hy1 = cy - shaftH + headH / 2;
    int hx2 = cx, hy2 = cy - shaftH + headH / 2 - headH;

    // 旋转各点
    auto rotate = [&](int px, int py) -> void {
        float dx = px - cx;
        float dy = py - cy;
        float rx = dx * cosf(angle) - dy * sinf(angle) + cx;
        float ry = dx * sinf(angle) + dy * cosf(angle) + cy;
        // 直接绘制不方便，改为用 fillTriangle 组合
    };

    // 简化：直接用 TFT 三角形，手动计算旋转后坐标
    auto rotX = [&](int px, int py) -> int {
        float dx = px - cx;
        float dy = py - cy;
        return (int)(dx * cosf(angle) - dy * sinf(angle) + cx);
    };
    auto rotY = [&](int px, int py) -> int {
        float dx = px - cx;
        float dy = py - cy;
        return (int)(dx * sinf(angle) + dy * cosf(angle) + cy);
    };

    // 杆（矩形 -> 两个三角形）
    sprite.fillTriangle(
        rotX(x0, y0), rotY(x0, y0),
        rotX(x1, y1), rotY(x1, y1),
        rotX(x2, y2), rotY(x2, y2),
        color);
    sprite.fillTriangle(
        rotX(x0, y0), rotY(x0, y0),
        rotX(x2, y2), rotY(x2, y2),
        rotX(x3, y3), rotY(x3, y3),
        color);

    // 箭头头
    sprite.fillTriangle(
        rotX(hx0, hy0), rotY(hx0, hy0),
        rotX(hx1, hy1), rotY(hx1, hy1),
        rotX(hx2, hy2), rotY(hx2, hy2),
        color);
}

void ScreenTFT::drawDistance() {
    int y = scale(20) + scale(100);
    char distBuf[32];

    if (mState.guide.distanceText[0]) {
        strncpy(distBuf, mState.guide.distanceText, sizeof(distBuf) - 1);
        distBuf[sizeof(distBuf) - 1] = '\0';
    } else {
        ScreenRenderer::formatDistance(mState.guide.segRemainDis, distBuf, sizeof(distBuf));
    }

    // 距离接近时变色提醒
    uint16_t color = HudColor::PRIMARY;
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 200) {
        color = HudColor::WARN;
    }
    if (mState.guide.segRemainDis > 0 && mState.guide.segRemainDis < 50) {
        color = HudColor::DANGER;
    }

    sprite.setTextColor(color, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.setTextSize(scaleFont(6));
    sprite.drawString(distBuf, mWidth / 2, y);
}

void ScreenTFT::drawRoadName() {
    int y = scale(20) + scale(135);

    // 路口信息：当前路 -> 下一道路
    char roadBuf[80] = {0};
    if (mState.guide.intersection[0]) {
        strncpy(roadBuf, mState.guide.intersection, sizeof(roadBuf) - 1);
    } else if (mState.guide.nextRoadName[0]) {
        snprintf(roadBuf, sizeof(roadBuf), "-> %s", mState.guide.nextRoadName);
    }

    if (roadBuf[0]) {
        sprite.setTextColor(HudColor::WHITE, HudColor::BG);
        sprite.setTextDatum(TC_DATUM);
        sprite.setTextSize(scaleFont(2));
        // 截断过长文字
        sprite.drawString(roadBuf, mWidth / 2, y);
    }
}

void ScreenTFT::drawSpeed() {
    // 车速显示在左下，限速标志在右下
    int speedY = mHeight - scale(45);
    int speedX = scale(10);

    // 车速
    sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
    sprite.setTextDatum(BL_DATUM);
    sprite.setTextSize(scaleFont(5));
    char speedBuf[12];
    snprintf(speedBuf, sizeof(speedBuf), "%d", mState.guide.curSpeed);
    sprite.drawString(speedBuf, speedX, mHeight - scale(4));

    // "km/h" 标签
    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextSize(scaleFont(1));
    sprite.drawString("km/h", speedX, mHeight - scale(4) - scaleFont(5) * 8);

    // 限速标志
    if (mState.guide.limitedSpeed > 0) {
        int cx = mWidth - scale(35);
        int cy = mHeight - scale(30);
        int radius = scale(20);
        bool overSpeed = mState.guide.curSpeed > mState.guide.limitedSpeed;
        drawSpeedLimit(cx, cy, radius, mState.guide.limitedSpeed, overSpeed);
    }
}

void ScreenTFT::drawSpeedLimit(int cx, int cy, int radius, int speed, bool overSpeed) {
    uint16_t ringColor = overSpeed ? HudColor::DANGER : HudColor::WARN;
    // 外环
    sprite.drawCircle(cx, cy, radius, ringColor);
    sprite.drawCircle(cx, cy, radius - 1, ringColor);
    // 内圈黑色
    sprite.fillCircle(cx, cy, radius - 2, HudColor::BG);
    // 速度数字
    sprite.setTextColor(overSpeed ? HudColor::DANGER : HudColor::WHITE, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(scaleFont(2));
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", speed);
    sprite.drawString(buf, cx, cy);
}

void ScreenTFT::drawLanes() {
    if (!mState.driveWay.enabled || mState.driveWay.laneCount == 0) return;

    int laneY = mHeight - scale(70);
    int laneAreaW = mWidth - scale(20);
    int laneCount = mState.driveWay.laneCount;
    if (laneCount > 8) laneCount = 8;
    int laneW = laneAreaW / laneCount;
    int laneH = scale(12);
    int startX = scale(10);

    for (int i = 0; i < laneCount; i++) {
        int lx = startX + i * laneW;
        int backIcon = mState.driveWay.lanes[i].backIcon;

        // 根据车道类型绘制不同图标
        uint16_t color = HudColor::PRIMARY;
        sprite.fillRect(lx, laneY, laneW - 2, laneH, color);

        // 在车道块上绘制方向标记
        sprite.setTextColor(HudColor::BG, color);
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextSize(scaleFont(1));
        const char* label = "";
        switch (backIcon) {
            case 0: label = "^"; break;       // 直行
            case 1: label = "<"; break;        // 左转
            case 2: label = "<^"; break;       // 直行+左转
            case 3: label = ">"; break;        // 右转
            case 4: label = "^>"; break;       // 直行+右转
            case 5: label = "U"; break;        // 掉头
            case 6: label = "<>"; break;       // 左+右
            case 7: label = "<^>"; break;      // 直+左+右
            default: break;
        }
        sprite.drawString(label, lx + laneW / 2, laneY + laneH / 2);
    }
}

void ScreenTFT::drawTmcBar() {
    if (!mState.tmc.enabled || mState.tmc.segmentCount == 0) return;

    int barY = scale(20) + scale(115);
    int barH = scale(6);
    int barW = mWidth - scale(20);
    int startX = scale(10);

    int totalDist = mState.tmc.totalDistance;
    if (totalDist <= 0) return;

    int x = startX;
    for (int i = 0; i < mState.tmc.segmentCount && i < 32; i++) {
        int segW = (int)((float)mState.tmc.segments[i].distance / totalDist * barW);
        if (segW < 1) segW = 1;

        uint16_t color = HudColor::DIM;
        switch (mState.tmc.segments[i].status) {
            case 1: color = HudColor::ACCENT; break;    // 畅通-绿
            case 2: color = HudColor::YELLOW; break;     // 缓行-黄
            case 3: color = HudColor::WARN; break;       // 拥堵-橙
            case 4: color = HudColor::DANGER; break;     // 严重拥堵-红
            default: break;
        }
        sprite.fillRect(x, barY, segW, barH, color);
        x += segW;
    }
}

void ScreenTFT::drawRouteInfo() {
    int y = mHeight - scale(60);
    char disBuf[32];
    char timeBuf[32];
    ScreenRenderer::formatDistance(mState.guide.routeRemainDis, disBuf, sizeof(disBuf));
    ScreenRenderer::formatTime(mState.guide.routeRemainTime, timeBuf, sizeof(timeBuf));

    char info[64];
    snprintf(info, sizeof(info), "%s / %s", disBuf, timeBuf);

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.setTextSize(scaleFont(1));
    sprite.drawString(info, mWidth / 2, y);
}

void ScreenTFT::drawCamera() {
    if (mState.guide.cameraDist <= 0) return;

    int camY = mHeight - scale(50);
    int camX = mWidth / 2;

    char distBuf[32];
    ScreenRenderer::formatDistance(mState.guide.cameraDist, distBuf, sizeof(distBuf));

    sprite.setTextColor(HudColor::WARN, HudColor::BG);
    sprite.setTextDatum(TC_DATUM);
    sprite.setTextSize(scaleFont(2));
    char buf[40];
    snprintf(buf, sizeof(buf), "CAM %s", distBuf);
    sprite.drawString(buf, camX, camY);
}

void ScreenTFT::drawIdleScreen() {
    // 无导航时显示等待画面
    drawTopBar();

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(scaleFont(2));
    sprite.drawString("Waiting for", mWidth / 2, mHeight / 2 - scale(15));
    sprite.drawString("navigation", mWidth / 2, mHeight / 2 + scale(15));

    // 闪烁的圆点表示等待
    if (mFrameCounter % 2 == 0) {
        sprite.fillCircle(mWidth / 2, mHeight / 2 + scale(40), scale(4), HudColor::PRIMARY);
    }
}

void ScreenTFT::drawBootScreen(const char* msg) {
    tft.fillScreen(HudColor::BG);
    tft.setTextColor(HudColor::PRIMARY, HudColor::BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(scaleFont(2));
    tft.drawString(PROJECT_NAME, mWidth / 2, mHeight / 2 - scale(20));
    tft.setTextColor(HudColor::DIM, HudColor::BG);
    tft.setTextSize(scaleFont(1));
    tft.drawString("v" PROJECT_VERSION, mWidth / 2, mHeight / 2 + scale(10));
    if (msg) {
        tft.drawString(msg, mWidth / 2, mHeight / 2 + scale(30));
    }
}
