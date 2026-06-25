#pragma once

/**
 * @file NavData.h
 * @brief 导航数据结构定义
 *
 * 所有字段命名尽量与 Android 端 AmapAutoProtocol.kt 保持一致，
 * 方便两端对照调试。
 */

#include <Arduino.h>

namespace Nav {

// 地图/导航状态
enum class MapState : int8_t {
    Unknown = -1,
    Idle    = 0,
    Navigating = 1,
    Arrived = 2,
    Paused  = 3
};

// ===================== 引导信息 =====================
struct GuideInfo {
    int icon = -1;                 // 转向图标 ID（参见 ICON_MAP）
    char curRoadName[64] = {0};    // 当前道路名
    char nextRoadName[64] = {0};   // 下一道路名
    int routeRemainDis = 0;        // 全程剩余距离（米）
    int routeRemainTime = 0;       // 全程剩余时间（秒）
    int segRemainDis = 0;          // 当前路段剩余距离（米）
    int curSpeed = 0;              // 当前车速（km/h）
    int limitedSpeed = 0;          // 道路限速（km/h）
    int cameraDist = 0;            // 最近电子眼距离（米）
    int cameraType = -1;           // 电子眼类型
    int cameraSpeed = 0;           // 电子眼限速
    int trafficLightNum = 0;       // 红绿灯个数
    bool valid = false;            // 是否有效

    // 当 Android 端开启“ESP32 简化模式”时，会额外下发已经格式化好的显示字符串，
    // 方便 C3 等小内存设备直接显示，无需再次计算。
    char turnLabel[32] = {0};      // 转向简短标签，如“左转”
    char distanceText[32] = {0};   // 路口距离文本，如“350m”
    char intersection[128] = {0};  // 路口信息，如“当前路 → 下一道路”

    void clear() {
        icon = -1;
        curRoadName[0] = '\0';
        nextRoadName[0] = '\0';
        routeRemainDis = 0;
        routeRemainTime = 0;
        segRemainDis = 0;
        curSpeed = 0;
        limitedSpeed = 0;
        cameraDist = 0;
        cameraType = -1;
        cameraSpeed = 0;
        trafficLightNum = 0;
        valid = false;
        turnLabel[0] = '\0';
        distanceText[0] = '\0';
        intersection[0] = '\0';
    }
};

// ===================== 车道信息 =====================
constexpr int MAX_LANES = 8;

struct LaneInfo {
    int number = 0;      // 车道编号
    int backIcon = -1;   // 车道指引图标 0-7
};

struct DriveWayInfo {
    bool enabled = false;
    int size = 0;
    LaneInfo lanes[MAX_LANES];
    int laneCount = 0;

    void clear() {
        enabled = false;
        size = 0;
        laneCount = 0;
        for (int i = 0; i < MAX_LANES; i++) {
            lanes[i] = LaneInfo();
        }
    }
};

// ===================== 路况信息 =====================
constexpr int MAX_TMC_SEGMENTS = 32;

struct TmcSegment {
    int status = -1;     // 路况状态：1畅通 2缓行 3拥堵 4严重拥堵
    int distance = 0;    // 分段距离（米）
};

struct TmcSegmentInfo {
    bool enabled = false;
    int size = 0;
    int totalDistance = 0;
    int residualDistance = 0;
    TmcSegment segments[MAX_TMC_SEGMENTS];
    int segmentCount = 0;

    void clear() {
        enabled = false;
        size = 0;
        totalDistance = 0;
        residualDistance = 0;
        segmentCount = 0;
        for (int i = 0; i < MAX_TMC_SEGMENTS; i++) {
            segments[i] = TmcSegment();
        }
    }
};

// ===================== 定位信息 =====================
struct LocationInfo {
    int bearing = 0;     // 方向角（度）
    int accuracy = 0;    // 精度（米）
    int speed = 0;       // 速度（km/h）
    bool valid = false;

    void clear() {
        bearing = 0;
        accuracy = 0;
        speed = 0;
        valid = false;
    }
};

// ===================== 聚合导航状态 =====================
struct NavState {
    MapState mapState = MapState::Unknown;
    GuideInfo guide;
    DriveWayInfo driveWay;
    TmcSegmentInfo tmc;
    LocationInfo location;
    unsigned long lastUpdateMs = 0;

    void clear() {
        mapState = MapState::Unknown;
        guide.clear();
        driveWay.clear();
        tmc.clear();
        location.clear();
        lastUpdateMs = 0;
    }
};

} // namespace Nav
