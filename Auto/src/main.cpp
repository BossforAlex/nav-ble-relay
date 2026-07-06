/**
 * @file main.cpp
 * @brief ESP32-S3 Super Mini 导航 BLE 接收器主程序
 *
 * 架构（用户最新需求）：
 *   main.cpp (组合器)
 *   ├─ ble::BleServer   ESP32 作为 GATT Server（外设）
 *   │                    广播 AutoNavDisplay 名称，等待手机（Flutter GATT Client）连接
 *   │                    接收手机通过 WRITE/WRITE_NO_RESPONSE 写入的 JSON 数据
 *   ├─ nav::NavParser   负责 JSON -> 结构化数据
 *   └─ Screen (抽象)    负责显示输出
 *      ├─ ScreenTFT     ESP32-S3: ST7789 TFT HUD 显示（需外接屏幕）
 *      └─ ScreenConsole 默认：串口直通显示真实数据（无模拟）
 *
 * BLE 方向（关键）：
 *   - Android Flutter APP: GATT Client，扫描名为 "AutoNavDisplay" 的 ESP32 设备
 *     并在 MAC 白名单匹配后连接，写入 5 个特征值
 *   - ESP32 (本端):        GATT Server，自身名 "AutoNavDisplay"，被手机连接
 *   - 手机端做 MAC 白名单限制（用户需求：手机蓝牙连接数较多，只对授权 ESP32 推数据）
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

    // 串口直接显示手机发来的真实交互数据（用户需求：不模拟虚拟屏幕）
    Serial.printf("[BLE] 手机写入 %u 字节 | UUID=%s | data=%s\n",
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
    delay(500);  // 确保串口芯片完全就绪

    // 打印醒目的版本标识（用户需求：确保上电即看到完整输出，防止串口丢数据）
    Serial.println();
    Serial.println();
    Serial.println("██████████████████████████████████████████████");
    Serial.printf("██  %s v%s\n", PROJECT_NAME, PROJECT_VERSION);
    Serial.printf("██  FW: v0.5.2  (%s %s)\n", __DATE__, __TIME__);
    Serial.println("██████████████████████████████████████████████");
    Serial.flush();
    delay(50);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.printf("║  %s v%s\n", PROJECT_NAME, PROJECT_VERSION);
    #ifdef BOARD_NAME
    Serial.printf("║  Board: %s\n", BOARD_NAME);
    #endif
    Serial.printf("║  Role:  BLE GATT Server (等待手机连接)\n");
    Serial.printf("║  Name:  %s\n", PROJECT_NAME);
    Serial.printf("║  Mode:  %s\n",
#if SCREEN_SERIAL_ONLY
                  "串口直通显示"
#else
                  "ST7789 TFT HUD"
#endif
    );
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.flush();
    delay(50);

    // 启动阶段主动喂狗
    esp_task_wdt_reset();

    // 初始化屏幕
    bool screenOk = sScreen.init();
    if (!screenOk) {
        Serial.println("[Screen] 屏幕初始化失败，回退到串口直通");
    }
    sScreen.log("系统启动，等待手机 BLE 连接...");
    Serial.flush();
    delay(50);

    // 初始化 BLE GATT Server
    sBleServer.begin(PROJECT_NAME);  // ESP32 广播名为 AutoNavDisplay
    sBleServer.setDataCallback(onBleData);
    Serial.flush();
}

void loop() {
    // 喂狗：避免长循环触发 WDT 超时
    esp_task_wdt_reset();

    // 消费 BLE 事件队列（在主任务上下文中处理串口打印、JSON 解析）
    sBleServer.loop();
    sScreen.setBleConnected(sBleServer.isConnected());
    sScreen.update();

    // 短延时，让 BLE 任务有足够 CPU 时间
    delay(5);
}
