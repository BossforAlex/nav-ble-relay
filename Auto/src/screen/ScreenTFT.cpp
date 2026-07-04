#include "ScreenTFT.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <math.h>

ScreenTFT::ScreenTFT()
    : tft(), sprite(&tft) {
}

bool ScreenTFT::init() {
    // 1. 初始化 TFT 硬件
    tft.init();
    mWidth = tft.width();
    mHeight = tft.height();

    // 2. 验证 TFT 是否正常连接（width/height 为 0 说明未检测到屏幕）
    if (mWidth == 0 || mHeight == 0) {
        Serial.println("[Screen] TFT 未检测到(width=0)，回退到串口模式");
        return false;
    }

    computeScale();

    // 背光
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

    tft.setRotation(0);
    tft.fillScreen(HudColor::BG);

    // 3. 创建离屏 sprite（双缓冲，避免闪烁）
    sprite.setColorDepth(16);
    uint16_t* spriteBuf = (uint16_t*)sprite.createSprite(mWidth, mHeight);
    if (spriteBuf == nullptr) {
        // 内存不足，减小 sprite 尺寸再试一次
        int halfW = mWidth / 2;
        int halfH = mHeight / 2;
        spriteBuf = (uint16_t*)sprite.createSprite(halfW, halfH);
        if (spriteBuf == nullptr) {
            Serial.println("[Screen] sprite 帧缓冲分配失败，将禁用渲染");
            // 不设置 mInited，让 renderFrame 直接跳过
            return false;
        }
        Serial.printf("[Screen] sprite 降级为 %dx%d（内存不足）\n", halfW, halfH);
    }

    mInited = true;
    mSpriteOk = true;

    // 4. 启动画面：纯几何图形，不依赖字体渲染
    //    TFT_eSPI 在 ESP32-S3 上 drawString 字体渲染路径存在 StoreProhibited
    //    crash（NULL + 0x10 offset），因此 drawBootScreen 完全使用几何图形
    drawBootScreen();

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
    if (!mInited || !mSpriteOk) return;

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

    // 杆
    int x0 = cx - shaftW / 2, y0 = cy + headH / 2;
    int x1 = cx + shaftW / 2, y1 = cy + headH / 2;
    int x2 = cx + shaftW / 2, y2 = cy + headH / 2 - shaftH;
    int x3 = cx - shaftW / 2, y3 = cy + headH / 2 - shaftH;

    // 箭头头（三角形）
    int hx0 = cx - headW / 2, hy0 = cy - shaftH + headH / 2;
    int hx1 = cx + headW / 2, hy1 = cy - shaftH + headH / 2;
    int hx2 = cx, hy2 = cy - shaftH + headH / 2 - headH;

    // 旋转 lambda
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
        sprite.drawString(roadBuf, mWidth / 2, y);
    }
}

void ScreenTFT::drawSpeed() {
    int speedY = mHeight - scale(45);
    int speedX = scale(10);

    sprite.setTextColor(HudColor::ACCENT, HudColor::BG);
    sprite.setTextDatum(BL_DATUM);
    sprite.setTextSize(scaleFont(5));
    char speedBuf[12];
    snprintf(speedBuf, sizeof(speedBuf), "%d", mState.guide.curSpeed);
    sprite.drawString(speedBuf, speedX, mHeight - scale(4));

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
    sprite.drawCircle(cx, cy, radius, ringColor);
    sprite.drawCircle(cx, cy, radius - 1, ringColor);
    sprite.fillCircle(cx, cy, radius - 2, HudColor::BG);
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

        uint16_t color = HudColor::PRIMARY;
        sprite.fillRect(lx, laneY, laneW - 2, laneH, color);

        sprite.setTextColor(HudColor::BG, color);
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextSize(scaleFont(1));
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
            case 1: color = HudColor::ACCENT; break;
            case 2: color = HudColor::YELLOW; break;
            case 3: color = HudColor::WARN; break;
            case 4: color = HudColor::DANGER; break;
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
    drawTopBar();

    sprite.setTextColor(HudColor::DIM, HudColor::BG);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(scaleFont(2));
    sprite.drawString("Waiting for", mWidth / 2, mHeight / 2 - scale(15));
    sprite.drawString("navigation", mWidth / 2, mHeight / 2 + scale(15));

    if (mFrameCounter % 2 == 0) {
        sprite.fillCircle(mWidth / 2, mHeight / 2 + scale(40), scale(4), HudColor::PRIMARY);
    }
}

// ══════════════════════════════════════════════════════════════
// 启动画面：纯几何图形，不使用 drawString / 字体渲染
// 原因：TFT_eSPI 在 ESP32-S3 上 drawString 字体渲染路径存在
// StoreProhibited crash（NULL + 0x10 offset），根因是 S3 的 SPI
// 总线与经典 ESP32 不同，TFT_eSPI 的字体渲染内部指针未正确初始化。
// 使用 fillTriangle / fillRect / drawRect / fillCircle 替代。
// ══════════════════════════════════════════════════════════════
void ScreenTFT::drawBootScreen() {
    tft.fillScreen(HudColor::BG);

    int cx = mWidth / 2;
    int cy = mHeight / 2;

    // ── 外框（double border） ──
    tft.drawRect(scale(8),  scale(8),
                 mWidth - scale(16), mHeight - scale(16), HudColor::PRIMARY);
    tft.drawRect(scale(12), scale(12),
                 mWidth - scale(24), mHeight - scale(24), HudColor::DIM);

    // ── 中心导航箭头（HUD 风格） ──
    int arrowSize = scale(28);
    int arrowY = cy - scale(15);

    // 箭头头（大三角形）
    int tipX = cx;
    int tipY = arrowY - arrowSize;
    int leftX  = cx - arrowSize;
    int leftY  = arrowY + arrowSize / 2;
    int rightX = cx + arrowSize;
    int rightY = arrowY + arrowSize / 2;
    tft.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, HudColor::PRIMARY);

    // 箭头杆
    int shaftW = scale(8);
    int shaftH = scale(18);
    int shaftBaseY = arrowY + arrowSize / 2;
    tft.fillRect(cx - shaftW / 2, shaftBaseY, shaftW, shaftH, HudColor::PRIMARY);

    // ── 底部进度条（表示启动中） ──
    int barY = mHeight - scale(28);
    int barW = mWidth - scale(36);
    int barX = scale(18);
    int barH = scale(4);
    tft.drawRect(barX, barY, barW, barH, HudColor::DIM);

    // 分三段填充（视觉效果：启动进度）
    int segW = barW / 3;
    tft.fillRect(barX + 2, barY + 1, segW - 2, barH - 2, HudColor::ACCENT);
    tft.fillRect(barX + segW + 2, barY + 1, segW - 2, barH - 2, HudColor::PRIMARY);
    tft.fillRect(barX + segW * 2 + 2, barY + 1, segW - 4, barH - 2, HudColor::DIM);

    // ── 版本标记（右下角小圆点） ──
    tft.fillCircle(mWidth - scale(14), mHeight - scale(14), scale(3), HudColor::DIM);
}