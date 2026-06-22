#pragma once

/**
 * @file NavParser.h
 * @brief 导航 JSON 数据解析接口
 */

#include "NavData.h"

namespace Nav {

// 解析各类导航 JSON 报文
bool parseGuideInfo(const char* json, GuideInfo& out);
bool parseDriveWayInfo(const char* json, DriveWayInfo& out);
bool parseTmcInfo(const char* json, TmcSegmentInfo& out);
bool parseLocationInfo(const char* json, LocationInfo& out);

// 解析地图状态（简单整数或 JSON）
MapState parseMapState(int state);

} // namespace Nav
