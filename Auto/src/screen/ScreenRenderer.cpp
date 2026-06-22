#include "ScreenRenderer.h"
#include <stdio.h>
#include <string.h>

namespace ScreenRenderer {

const char* turnIconLabel(int icon) {
    switch (icon) {
        case 0:  return "未定义";
        case 1:  return "直行";
        case 2:  return "左转";
        case 3:  return "右转";
        case 4:  return "左前方";
        case 5:  return "右前方";
        case 6:  return "左后方";
        case 7:  return "右后方";
        case 8:  return "左转掉头";
        case 9:  return "直行";
        case 10: return "到达途经点";
        case 11: return "进入环岛";
        case 12: return "驶出环岛";
        case 13: return "服务区";
        case 14: return "收费站";
        case 15: return "到达目的地";
        case 16: return "隧道";
        case 17: return "环岛(左行)";
        case 18: return "出环岛(左行)";
        case 19: return "右转掉头";
        case 20: return "顺行";
        default: return "未知";
    }
}

const char* laneBackIconLabel(int backIcon) {
    switch (backIcon) {
        case 0: return "直行";
        case 1: return "左转";
        case 2: return "直行+左转";
        case 3: return "右转";
        case 4: return "直行+右转";
        case 5: return "左转掉头";
        case 6: return "左转+右转";
        case 7: return "直行+左转+右转";
        default: return "?";
    }
}

const char* mapStateLabel(Nav::MapState state) {
    switch (state) {
        case Nav::MapState::Idle:       return "空闲";
        case Nav::MapState::Navigating: return "导航中";
        case Nav::MapState::Arrived:    return "已到达";
        case Nav::MapState::Paused:     return "已暂停";
        default: return "未知";
    }
}

const char* tmcStatusLabel(int status) {
    switch (status) {
        case 1: return "畅通";
        case 2: return "缓行";
        case 3: return "拥堵";
        case 4: return "严重拥堵";
        default: return "无数据";
    }
}

void formatDistance(int meters, char* out, size_t outLen) {
    if (meters >= 1000) {
        snprintf(out, outLen, "%.1fkm", meters / 1000.0f);
    } else {
        snprintf(out, outLen, "%dm", meters);
    }
}

void formatTime(int seconds, char* out, size_t outLen) {
    int m = seconds / 60;
    int s = seconds % 60;
    if (m > 0) {
        snprintf(out, outLen, "%d分%02d秒", m, s);
    } else {
        snprintf(out, outLen, "%02d秒", s);
    }
}

void renderTurnAscii(int icon, char* out, size_t outLen) {
    // 简单 ASCII 占位，后续可扩展为更精美的矢量描述
    const char* label = turnIconLabel(icon);
    if (icon == 1 || icon == 9) {
        snprintf(out, outLen,
            "    |    \n"
            "    |    \n"
            "    |    \n"
            "   / \\   \n"
            "  /   \\  \n"
            " %s", label);
    } else if (icon == 2) {
        snprintf(out, outLen,
            "  /      \n"
            " /       \n"
            "|        \n"
            " \\       \n"
            "  \\      \n"
            " %s", label);
    } else if (icon == 3) {
        snprintf(out, outLen,
            "      \\  \n"
            "       \\ \n"
            "        |\n"
            "       / \n"
            "      /  \n"
            " %s", label);
    } else if (icon == 8) {
        snprintf(out, outLen,
            "  |      \n"
            "  |      \n"
            "  |      \n"
            "  U      \n"
            "   <---- \n"
            " %s", label);
    } else {
        snprintf(out, outLen, "[图标 %d]\n%s", icon, label);
    }
}

void renderLaneAscii(int backIcon, char* out, size_t outLen) {
    const char* label = laneBackIconLabel(backIcon);
    char symbol[32];
    switch (backIcon) {
        case 0: snprintf(symbol, sizeof(symbol), " ^ "); break;
        case 1: snprintf(symbol, sizeof(symbol), " < "); break;
        case 2: snprintf(symbol, sizeof(symbol), "<^ "); break;
        case 3: snprintf(symbol, sizeof(symbol), " > "); break;
        case 4: snprintf(symbol, sizeof(symbol), "^> "); break;
        case 5: snprintf(symbol, sizeof(symbol), " U<"); break;
        case 6: snprintf(symbol, sizeof(symbol), "<> "); break;
        case 7: snprintf(symbol, sizeof(symbol), "<^>"); break;
        default: snprintf(symbol, sizeof(symbol), " ? "); break;
    }
    snprintf(out, outLen, "[%s] %s", symbol, label);
}

} // namespace ScreenRenderer
