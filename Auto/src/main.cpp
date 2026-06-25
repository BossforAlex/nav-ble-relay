/**
 * @file main.cpp
 * @brief ESP32-C3 Super Mini 导航显示主程序
 *
 * 架构：
 *   main.cpp (组合器)
 *   ├─ ble::BleClient   负责 BLE 扫描/连接/订阅
 *   ├─ nav::NavParser   负责 JSON -> 结构化数据
 *   ├─ Screen (抽象)    负责显示输出
 *      └─ ScreenConsole  当前阶段：串口虚拟屏幕
 *
 * 后续接入 OLED/LCD/TFT 时，只需：
 *   1. 新增 ScreenOled 继承 Screen；
 *   2. 把 screen 实例替换为 ScreenOled；
 *   3. 复用 ScreenRenderer 中的标签/格式化函数。
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

#include "config/Config.h"
#include "ble/BleClient.h"
#include "nav/NavParser.h"
#include "screen/ScreenConsole.h"

// ===================== 全局对象 =====================
static BleClient sBleClient;
static ScreenConsole sScreen;
static Nav::NavState sNavState;

// BLE 数据接收缓冲区（静态，避免堆碎片）
static char sJsonBuffer[1024];

// ===================== BLE 数据回调 =====================
static void onBleData(const char* uuid, const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

    // 拷贝并截断，确保是合法 C 字符串；防止异常字符导致 printf 越界
    size_t copyLen = len < sizeof(sJsonBuffer) - 1 ? len : sizeof(sJsonBuffer) - 1;
    memcpy(sJsonBuffer, data, copyLen);
    sJsonBuffer[copyLen] = '\0';

    if (Debug::LOG_BLE_RAW) {
        Serial.printf("[BLE][%s] len=%u raw=%s\n", uuid, (unsigned)len, sJsonBuffer);
    }

    bool ok = false;
    if (strstr(uuid, BleUUID::CHAR_GUIDE)) {
        ok = Nav::parseGuideInfo(sJsonBuffer, sNavState.guide);
    } else if (strstr(uuid, BleUUID::CHAR_DRIVE)) {
        ok = Nav::parseDriveWayInfo(sJsonBuffer, sNavState.driveWay);
    } else if (strstr(uuid, BleUUID::CHAR_TMC)) {
        ok = Nav::parseTmcInfo(sJsonBuffer, sNavState.tmc);
    } else if (strstr(uuid, BleUUID::CHAR_LOCATION)) {
        ok = Nav::parseLocationInfo(sJsonBuffer, sNavState.location);
    } else if (strstr(uuid, BleUUID::CHAR_STATE)) {
        // 状态报文可能是简单整数，也可能是 JSON {"state":1}
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, sJsonBuffer);
        int state = err ? atoi(sJsonBuffer) : doc["state"] | doc["data"]["state"] | -1;
        sNavState.mapState = Nav::parseMapState(state);
        ok = true;
    }

    if (ok) {
        sNavState.lastUpdateMs = millis();
        sScreen.setNavState(sNavState);
    } else if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE][%s] 数据解析失败，请检查 JSON 格式或启用简化模式\n", uuid);
    }
}

// ===================== 初始化 =====================
void setup() {
    Serial.begin(SERIAL_BAUD);

    // 等待串口就绪，但最多 2 秒；无串口连接时也不阻塞启动
    while (!Serial && millis() < 2000) { delay(10); }
    delay(300);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.printf("║  %s v%s\n", PROJECT_NAME, PROJECT_VERSION);
    #ifdef BOARD_NAME
    Serial.printf("║  Board: %s\n", BOARD_NAME);
    #endif
    Serial.println("╚══════════════════════════════════════════╝");

    // 初始化虚拟屏幕
    sScreen.init();
    sScreen.log("系统启动，准备连接蓝牙...");

    // 初始化 BLE
    sBleClient.begin("ESP32-C3-Nav");
    sBleClient.setDataCallback(onBleData);

    // 如需指定 Android 设备 MAC，取消下行注释（替换为真实 MAC）
    // sBleClient.setTargetAddress("AA:BB:CC:DD:EE:FF");
}

// ===================== 主循环 =====================
void loop() {
    sBleClient.loop();
    sScreen.setBleConnected(sBleClient.isConnected());
    sScreen.update();

    // 空闲时降低循环频率，减少 CPU 占用
    if (Feature::LOW_POWER_WHEN_IDLE &&
        sNavState.mapState != Nav::MapState::Navigating) {
        delay(50);
    } else {
        delay(10);
    }
}
