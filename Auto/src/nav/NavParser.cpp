#include "NavParser.h"
#include <ArduinoJson.h>

namespace Nav {

// 安全复制字符串到固定缓冲区
static void safeStrCopy(char* dst, size_t dstLen, const char* src) {
    if (!src || dstLen == 0) return;
    size_t n = strlen(src);
    if (n >= dstLen) n = dstLen - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

bool parseGuideInfo(const char* json, GuideInfo& out) {
    out.clear();
    JsonDocument doc(1024); // 预分配容量，减少堆碎片
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    // Android 端 BLE 传输时把对象序列化在 data 字段里
    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"] : doc.as<JsonObject>();

    out.icon = data["icon"] | data["ICON"] | -1;
    safeStrCopy(out.curRoadName, sizeof(out.curRoadName), data["curRoadName"] | data["CUR_ROAD_NAME"]);
    safeStrCopy(out.nextRoadName, sizeof(out.nextRoadName), data["nextRoadName"] | data["NEXT_ROAD_NAME"]);
    out.routeRemainDis = data["routeRemainDis"] | data["ROUTE_REMAIN_DIS"] | 0;
    out.routeRemainTime = data["routeRemainTime"] | data["ROUTE_REMAIN_TIME"] | 0;
    out.segRemainDis = data["segRemainDis"] | data["SEG_REMAIN_DIS"] | 0;
    out.curSpeed = data["curSpeed"] | data["CUR_SPEED"] | 0;
    out.limitedSpeed = data["limitedSpeed"] | data["LIMITED_SPEED"] | 0;
    out.cameraDist = data["cameraDist"] | data["CAMERA_DIST"] | 0;
    out.cameraType = data["cameraType"] | data["CAMERA_TYPE"] | -1;
    out.cameraSpeed = data["cameraSpeed"] | data["CAMERA_SPEED"] | 0;
    out.trafficLightNum = data["trafficLightNum"] | data["TRAFFIC_LIGHT_NUM"] | 0;
    out.valid = true;
    return true;
}

bool parseDriveWayInfo(const char* json, DriveWayInfo& out) {
    out.clear();
    JsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"] : doc.as<JsonObject>();
    out.enabled = data["drive_way_enabled"] | false;
    out.size = data["drive_way_size"] | 0;

    JsonArray arr = data["drive_way_info"];
    if (!arr) return out.enabled; // 有 enabled 但没有数组也视为成功

    out.laneCount = 0;
    for (JsonObject item : arr) {
        if (out.laneCount >= MAX_LANES) break;
        LaneInfo lane;
        lane.number = item["drive_way_number"] | 0;
        const char* backIconStr = item["drive_way_lane_Back_icon"] | "-1";
        lane.backIcon = atoi(backIconStr);
        out.lanes[out.laneCount++] = lane;
    }
    return true;
}

bool parseTmcInfo(const char* json, TmcSegmentInfo& out) {
    out.clear();
    JsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"] : doc.as<JsonObject>();
    out.enabled = data["tmc_segment_enabled"] | false;
    out.size = data["tmc_segment_size"] | 0;
    out.totalDistance = data["total_distance"] | 0;
    out.residualDistance = data["residual_distance"] | 0;

    JsonArray arr = data["tmc_info"];
    if (!arr) return out.enabled;

    out.segmentCount = 0;
    for (JsonObject item : arr) {
        if (out.segmentCount >= MAX_TMC_SEGMENTS) break;
        TmcSegment seg;
        const char* statusStr = item["tmc_status"] | "-1";
        seg.status = atoi(statusStr);
        const char* distStr = item["tmc_segment_distance"] | "0";
        seg.distance = atoi(distStr);
        out.segments[out.segmentCount++] = seg;
    }
    return true;
}

bool parseLocationInfo(const char* json, LocationInfo& out) {
    out.clear();
    JsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    JsonObject data = doc["data"].is<JsonObject>() ? doc["data"] : doc.as<JsonObject>();
    out.bearing = data["bearing"] | 0;
    out.accuracy = data["accuracy"] | 0;
    out.speed = data["speed"] | 0;
    out.valid = true;
    return true;
}

MapState parseMapState(int state) {
    switch (state) {
        case 0:  return MapState::Idle;
        case 1:  return MapState::Navigating;
        case 2:  return MapState::Arrived;
        case 3:  return MapState::Paused;
        default: return MapState::Unknown;
    }
}

} // namespace Nav
