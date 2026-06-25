#include "ScreenConsole.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <stdio.h>

bool ScreenConsole::init() {
    if (Debug::LOG_SYSTEM) {
        Serial.println("[Screen] 串口虚拟屏幕初始化完成");
    }
    return true;
}

void ScreenConsole::update() {
    // 每 500ms 渲染一帧，模拟真实屏幕的刷新率
    unsigned long now = millis();
    if (now - mLastRenderMs < 500) return;
    mLastRenderMs = now;
    mFrameCounter++;

    renderFrame();
    if (Feature::ENABLE_ANIMATION) {
        renderAnimation();
    }
}

void ScreenConsole::setNavState(const Nav::NavState& state) {
    mState = state;
}

void ScreenConsole::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    Serial.printf("[Screen] BLE %s\n", connected ? "已连接" : "已断开");
}

void ScreenConsole::log(const char* msg) {
    Serial.printf("[Screen] %s\n", msg);
}

void ScreenConsole::renderFrame() {
    char disBuf[32];
    char timeBuf[32];

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║         AutoNavDisplay 虚拟屏幕         ║");
    Serial.printf ("║  BLE: %-10s  帧: %-6lu        ║\n",
                   mBleConnected ? "已连接" : "未连接", mFrameCounter);
    Serial.println("╠════════════════════════════════════════╣");

    // 1. 路口信息（高德设计规范：当前道路 → 下一道路）
    Serial.printf("路口信息: %s\n",
                  mState.guide.intersection[0] ? mState.guide.intersection : "--");

    // 2. 转向指示（iOS Watch 风格核心信息：大箭头 + 距离）
    char turnArt[256];
    ScreenRenderer::renderTurnAscii(mState.guide.icon, turnArt, sizeof(turnArt));
    Serial.println(turnArt);

    // 3. 下一路口距离 / 转向标签：优先使用 Android 预格式化的显示文本
    if (mState.guide.distanceText[0]) {
        Serial.printf("路口距离: %s\n", mState.guide.distanceText);
    } else {
        ScreenRenderer::formatDistance(mState.guide.segRemainDis, disBuf, sizeof(disBuf));
        Serial.printf("路口距离: %s\n", disBuf);
    }
    if (mState.guide.turnLabel[0]) {
        Serial.printf("转向: %s\n", mState.guide.turnLabel);
    }

    // 4. 车道指引
    if (mState.driveWay.enabled && mState.driveWay.laneCount > 0) {
        Serial.println("车道指引:");
        for (int i = 0; i < mState.driveWay.laneCount; i++) {
            char laneArt[64];
            ScreenRenderer::renderLaneAscii(mState.driveWay.lanes[i].backIcon, laneArt, sizeof(laneArt));
            Serial.printf("  %s\n", laneArt);
        }
    }

    // 5. 速度与限速
    Serial.printf("车速: %d km/h  限速: %d km/h\n",
                  mState.guide.curSpeed, mState.guide.limitedSpeed);

    // 6. 全程剩余
    ScreenRenderer::formatDistance(mState.guide.routeRemainDis, disBuf, sizeof(disBuf));
    ScreenRenderer::formatTime(mState.guide.routeRemainTime, timeBuf, sizeof(timeBuf));
    Serial.printf("全程剩余: %s / %s\n", disBuf, timeBuf);

    // 7. 电子眼与红绿灯
    if (mState.guide.cameraDist > 0) {
        ScreenRenderer::formatDistance(mState.guide.cameraDist, disBuf, sizeof(disBuf));
        Serial.printf("电子眼: %s 限速%d\n", disBuf, mState.guide.cameraSpeed);
    }
    if (mState.guide.trafficLightNum > 0) {
        Serial.printf("红绿灯: %d 个\n", mState.guide.trafficLightNum);
    }

    // 8. 路况概览
    if (mState.tmc.enabled && mState.tmc.segmentCount > 0) {
        Serial.print("路况: ");
        for (int i = 0; i < mState.tmc.segmentCount; i++) {
            Serial.printf("%s ", ScreenRenderer::tmcStatusLabel(mState.tmc.segments[i].status));
        }
        Serial.println();
    }

    Serial.println("╚════════════════════════════════════════╝");
}

void ScreenConsole::renderAnimation() {
    // 模拟 iOS Watch 导航的脉冲动效：在串口打印当前脉冲相位
    static uint8_t phase = 0;
    phase = (phase + 1) % 4;
    const char* pulse[] = {"●   ", " ●  ", "  ● ", "   ●"};
    if (Debug::LOG_ANIMATION) {
        Serial.printf("[Anim] 转向脉冲相位: %s (frame=%lu)\n", pulse[phase], mFrameCounter);
    }
}
