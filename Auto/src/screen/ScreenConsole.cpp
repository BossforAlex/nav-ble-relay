#include "ScreenConsole.h"
#include "ScreenRenderer.h"
#include "config/Config.h"
#include <stdio.h>

/**
 * ScreenConsole —— 串口直通显示模式
 *
 * 设计目标（用户需求）：
 *   - 不模拟虚拟屏幕（不输出 ASCII 框图、不做"渲染"）
 *   - 串口直接显示真实的交互数据：
 *     1) BLE 接收到的原始 JSON（Debug::LOG_BLE_RAW 启用时）
 *     2) 解析后的结构化字段（路口距离/车速/限速/电子眼等）
 *   - 接 TFT 屏幕时 ScreenTFT 负责显示渲染
 *
 * 输出策略：
 *   - 数据变化时才打印（与变化频率解耦）
 *   - 单行紧凑输出，便于 grep / 解析
 */

bool ScreenConsole::init() {
    mLastPrintedMs = 0;
    mLastUpdateMs = 0;
    mInitialized = true;
    Serial.println("[Screen] 串口直通显示模式已启用（仅显示数据，无模拟渲染）");
    return true;
}

void ScreenConsole::update() {
    if (!mInitialized) return;
    // 不做"刷新率模拟"，仅依赖 setNavState/setBleConnected 主动推送。
    // 此方法保留以满足 Screen 接口。
}

void ScreenConsole::setNavState(const Nav::NavState& state) {
    mState = state;
    mLastUpdateMs = millis();
    // 数据更新即打印（节流：避免同毫秒重复）
    unsigned long now = millis();
    if (now - mLastPrintedMs < 50) return;
    mLastPrintedMs = now;
    printNavStateLine();
}

void ScreenConsole::setBleConnected(bool connected) {
    if (mBleConnected == connected) return;
    mBleConnected = connected;
    // BLE 状态变化必须立即打印
    mLastPrintedMs = 0;
    Serial.printf("[BLE] 状态变化: %s\n", connected ? "已连接" : "已断开");
}

void ScreenConsole::log(const char* msg) {
    Serial.printf("[System] %s\n", msg);
}

void ScreenConsole::printNavStateLine() {
    // 单行紧凑输出：核心导航数据
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

    // 路口信息：当前路 → 下一路
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
