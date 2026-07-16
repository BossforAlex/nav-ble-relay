#include "ScreenConsole.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <stdio.h>

/**
 * ScreenConsole —— 串口直通显示模式（纯展示层）
 *
 * 实现 Screen 纯展示接口，仅在数据更新时打印结构化日志。
 * 方便后期更换不同屏幕驱动（TFT/OLED/串口）。
 */

bool ScreenConsole::init() {
    mLastPrintedMs = 0;
    mInitialized = true;
    Serial.println("[Screen] 串口直通显示模式已启用");
    return true;
}

void ScreenConsole::update() {
    // 串口模式无需刷新
}

void ScreenConsole::log(const char* msg) {
    Serial.printf("[System] %s\n", msg);
}

/* ── 批量更新 ──────────────────────────────────────────────── */

void ScreenConsole::setNavState(const Nav::NavState& state) {
    mState = state;
    unsigned long now = millis();
    if (now - mLastPrintedMs < 50) return;
    mLastPrintedMs = now;
    printNavStateLine();
}

void ScreenConsole::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    mLastPrintedMs = 0;
}

/* ── 单字段展示（串口模式下仅打印日志） ───────────────────── */

void ScreenConsole::showArrow(int amapIcon) {
    const char* names[] = {"左转","直行","右转","左前掉头","左前方","右前方",
                           "左后方","右后方","调头","延续","","","","","","到达",
                           "","","","旧调头","环岛"};
    const char* name = (amapIcon >= 0 && amapIcon <= 20) ? names[amapIcon] : "?";
    Serial.printf("[SCREEN] 箭头: %s (icon=%d)\n", name, amapIcon);
}

void ScreenConsole::showDistance(const char* text) {
    Serial.printf("[SCREEN] 距离: %s\n", text);
}

void ScreenConsole::showSpeed(int speed) {
    Serial.printf("[SCREEN] 时速: %d km/h\n", speed);
}

void ScreenConsole::showSpeedLimit(int limit, bool overSpeed) {
    Serial.printf("[SCREEN] 限速: %d %s\n", limit, overSpeed ? "(超速!)" : "");
}

void ScreenConsole::showRoadName(const char* name) {
    Serial.printf("[SCREEN] 路名: %s\n", name ? name : "(null)");
}

void ScreenConsole::showLanes(int count, const int* backIcons, int /*turnIcon*/) {
    Serial.printf("[SCREEN] 车道: %d 条", count);
    if (backIcons) {
        for (int i = 0; i < count; i++) {
            Serial.printf(" [%d]", backIcons[i]);
        }
    }
    Serial.println();
}

void ScreenConsole::showRouteInfo(const char* text) {
    Serial.printf("[SCREEN] 全程: %s\n", text);
}

void ScreenConsole::showIdle() {
    Serial.printf("[SCREEN] 空闲\n");
}

/* ── 核心导航数据打印 ──────────────────────────────────────── */

void ScreenConsole::printNavStateLine() {
    char disBuf[16] = {0};
    char routeDisBuf[16] = {0};
    char routeTimeBuf[16] = {0};
    char cameraBuf[24] = {0};

    ScreenRenderer::formatDistance(mState.guide.segRemainDis, disBuf, sizeof(disBuf));
    ScreenRenderer::formatDistance(mState.guide.routeRemainDis, routeDisBuf, sizeof(routeDisBuf));
    ScreenRenderer::formatTime(mState.guide.routeRemainTime, routeTimeBuf, sizeof(routeTimeBuf));

    if (mState.guide.cameraDist > 0) {
        char camDisBuf[16];
        ScreenRenderer::formatDistance(mState.guide.cameraDist, camDisBuf, sizeof(camDisBuf));
        snprintf(cameraBuf, sizeof(cameraBuf), "CAM=%s/%d", camDisBuf, mState.guide.cameraSpeed);
    }

    char intersection[64] = "--";
    if (mState.guide.intersection[0]) {
        snprintf(intersection, sizeof(intersection), "%s", mState.guide.intersection);
    } else if (mState.guide.nextRoadName[0]) {
        snprintf(intersection, sizeof(intersection), "-> %s", mState.guide.nextRoadName);
    }

    Serial.printf(
        "[NAV] 转向=%d 路口=%s 距下一路口=%s "
        "车速=%dkm/h 限速=%dkm/h "
        "全程=%s/%s %s\n",
        mState.guide.icon,
        intersection,
        mState.guide.distanceText[0] ? mState.guide.distanceText : disBuf,
        mState.guide.curSpeed,
        mState.guide.limitedSpeed,
        routeDisBuf,
        routeTimeBuf,
        cameraBuf
    );
}