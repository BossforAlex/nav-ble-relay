#pragma once

/**
 * @file ScreenRenderer.h
 * @brief 屏幕渲染辅助函数
 *
 * 提供转向图标、车道图标、路况、速度等数据的标签与 ASCII 渲染。
 * 这些函数与具体显示设备无关，真实屏幕实现时可直接复用其逻辑。
 */

#include "nav/NavData.h"

namespace ScreenRenderer {

// 转向图标文字
const char* turnIconLabel(int icon);

// 车道指引图标文字
const char* laneBackIconLabel(int backIcon);

// 地图状态文字
const char* mapStateLabel(Nav::MapState state);

// 路况状态文字
const char* tmcStatusLabel(int status);

// 格式化距离：>1000m 显示公里，否则显示米
void formatDistance(int meters, char* out, size_t outLen);

// 格式化时间：秒 -> 分:秒
void formatTime(int seconds, char* out, size_t outLen);

// 把转向图标渲染为 ASCII 艺术（串口调试用）
void renderTurnAscii(int icon, char* out, size_t outLen);

// 把车道图标渲染为 ASCII 艺术
void renderLaneAscii(int backIcon, char* out, size_t outLen);

} // namespace ScreenRenderer
