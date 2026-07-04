/**
 * @file main.cpp
 * @brief ESP32-S3 Super Mini 导航 HUD 显示主程序
 *
 * 架构：
 *   main.cpp (组合器)
 *   ├─ ble::BleServer   负责 BLE 接收手机写入的导航数据
 *   ├─ nav::NavParser   负责 JSON -> 结构化数据
 *   ├─ Screen (抽象)    负责显示输出
 *      ├─ ScreenTFT     ESP32-S3: ST7789 TFT HUD 显示
 *      └─ ScreenConsole ESP32-C3: 串口虚拟屏幕（向后兼容）
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <esp_task_wdt.h>

#include "config/Config.h"
#include "ble/BleServer.h"
#include "nav/NavParser.h"

#if SCREEN_SERIAL_ONLY
  #include "screen/ScreenConsole.h"
  static ScreenConsole sScreen;
#else
  #include "screen/ScreenTFT.h"
  static ScreenTFT sScreen;
#endif

static BleServer sBleServer;
static Nav::NavState sNavState;

// BLE 数据接收缓冲区（静态，避免堆碎片）
static char sJsonBuffer[1024];

// ===================== BLE 数据回调 =====================
static void onBleData(const char* uuid, const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

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

    // 等待串口就绪，但最多 1.5 秒
    while (!Serial && millis() < 1500) { delay(10); }
    delay(300);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.printf("║  %s v%s\n", PROJECT_NAME, PROJECT_VERSION);
    #ifdef BOARD_NAME
    Serial.printf("║  Board: %s\n", BOARD_NAME);
    #endif
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.flush();

    // 启动阶段主动喂狗
    esp_task_wdt_reset();

    // 初始化屏幕
    sScreen.init();
    sScreen.log("系统启动，准备连接蓝牙...");

    // 初始化 BLE
    sBleServer.begin(DEVICE_NAME_PREFIX);
    sBleServer.setDataCallback(onBleData);
}

// ===================== 主循环 =====================
void loop() {
    sBleServer.loop();
    sScreen.setBleConnected(sBleServer.isConnected());
    sScreen.update();

    if (Feature::LOW_POWER_WHEN_IDLE &&
        sNavState.mapState != Nav::MapState::Navigating) {
        delay(50);
    } else {
        delay(10);
    }
}
