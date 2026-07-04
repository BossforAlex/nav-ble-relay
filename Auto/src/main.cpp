/**
 * @file main.cpp
 * @brief ESP32-S3 Super Mini 导航 BLE 接收器主程序
 *
 * 架构：
 *   main.cpp (组合器)
 *   ├─ ble::BleClient   ESP32 作为 GATT Client，扫描名为 ICA 的 Android 设备
 *   │                    订阅其 GATT Server 推送的 5 个特征值通知
 *   ├─ nav::NavParser   负责 JSON -> 结构化数据
 *   ├─ Screen (抽象)    负责显示输出
 *      ├─ ScreenTFT     ESP32-S3: ST7789 TFT HUD 显示（需外接屏幕）
 *      └─ ScreenConsole 默认：串口直通显示真实数据（无模拟）
 *
 * BLE 方向（关键）：
 *   - Android Flutter APP: GATT Server，广播名为 "ICA"，向 5 个特征值 notify 推送 JSON
 *   - ESP32 (本端):       GATT Client，扫描 "ICA" 设备名，连接后订阅通知
 *   - 双方设备名：ESP32 用 "AutoNavDisplay"（自身），手机用 "ICA"（被搜索）
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <esp_task_wdt.h>

#include "config/Config.h"
#include "ble/BleClient.h"
#include "nav/NavParser.h"

#if SCREEN_SERIAL_ONLY
  #include "screen/ScreenConsole.h"
  static ScreenConsole sScreen;
#else
  #include "screen/ScreenTFT.h"
  static ScreenTFT sScreen;
#endif

static BleClient sBleClient;
static Nav::NavState sNavState;

// BLE 数据接收缓冲区（静态，避免堆碎片）
static char sJsonBuffer[1024];

// ===================== BLE 数据回调 =====================
static void onBleData(const char* uuid, const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

    size_t copyLen = len < sizeof(sJsonBuffer) - 1 ? len : sizeof(sJsonBuffer) - 1;
    memcpy(sJsonBuffer, data, copyLen);
    sJsonBuffer[copyLen] = '\0';

    // 默认总是打印原始 bytes（用户要求"串口显示真实交互数据"）
    Serial.printf("[BLE] 收到 %u 字节 | UUID=%s | data=%s\n",
                  (unsigned)len, uuid, sJsonBuffer);

    if (Debug::LOG_BLE_RAW) {
        Serial.printf("[BLE][%s] HEX:", uuid);
        for (size_t i = 0; i < len && i < 32; i++) {
            Serial.printf(" %02X", data[i]);
        }
        if (len > 32) Serial.print(" ...");
        Serial.println();
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
    } else {
        Serial.printf("[BLE][%s] 解析失败：%s\n", uuid, sJsonBuffer);
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
    Serial.printf("║  Role:  BLE Client (扫描 '%s')  \n", DEVICE_NAME_PREFIX);
    Serial.printf("║  Mode:  %s\n",
#if SCREEN_SERIAL_ONLY
                  "串口直通显示"
#else
                  "ST7789 TFT HUD"
#endif
    );
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.flush();

    // 启动阶段主动喂狗
    esp_task_wdt_reset();

    // 初始化屏幕
    bool screenOk = sScreen.init();
    if (!screenOk) {
        Serial.println("[Screen] 屏幕初始化失败，回退到串口直通");
    }
    sScreen.log("系统启动，准备扫描蓝牙设备...");

    // 初始化 BLE 客户端（注意：ESP32 自身用 AutoNavDisplay 名，
    // 扫描名为 "ICA" 的 Android 设备并连接）
    sBleClient.begin(PROJECT_NAME);  // ESP32 本地名为 AutoNavDisplay
    sBleClient.setDataCallback(onBleData);
}

void loop() {
    sBleClient.loop();
    sScreen.setBleConnected(sBleClient.isConnected());
    sScreen.update();

    if (Feature::LOW_POWER_WHEN_IDLE &&
        sNavState.mapState != Nav::MapState::Navigating) {
        delay(50);
    } else {
        delay(10);
    }
}
