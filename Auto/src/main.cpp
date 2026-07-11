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
  // v0.6.5: 使用 LVGL 现代化 UI
  #include "screen/ScreenLVGL.h"
  static ScreenLVGL sScreen;
#endif

static BleServer sBleServer;
static Nav::NavState sNavState;

// v0.6.8: BLE 数据到达后只更新 NavState，不直接调 LVGL API。
// 由 loop() 在消费完所有 BLE 事件后统一刷屏一次，避免：
//   1) LVGL 非线程安全调用（BLE 回调可能在不同上下文）
//   2) 每包都 applyNavState + lv_timer_handler 导致栈暴涨 → WDT panic
static bool sNavDirty = false;

// v0.9.1: 批量更新防抖 —— 50ms 内收到多包数据时，只应用最后一次
static unsigned long sNavDirtySince = 0;

// BLE 数据接收缓冲区（静态，避免堆碎片）
// v0.5.7：单 char 通道，JSON 带 "type" 字段（guide/drive/tmc/state/location）
static char sJsonBuffer[2048];

// v0.6.4: 分片重组缓冲区
// Flutter 端当 JSON > 400B 时，自动拆分为带 [0xAA, idx, total, ...data] 头的 chunk
// ESP32 端在此重组后解析，对上层 NavParser 透明
static uint8_t sChunkBuf[2048];
static size_t  sChunkLen = 0;
static int     sChunkTotal = 0;
static int     sChunkReceived = 0;
static unsigned long sChunkStartMs = 0;

// v0.9.1: 分片重组状态保护（防止 BLE 回调上下文与主 loop 竞态）
static portMUX_TYPE sChunkMux = portMUX_INITIALIZER_UNLOCKED;

// ===================== BLE 数据回调 =====================
// v0.5.7 重构：单 write char + INDICATE poll，JSON 用 "type" 字段路由
// 期望格式：{"type": "guide"|"drive"|"tmc"|"state"|"location", "ts": ..., "data": {...}}
//
// v0.6.4: 支持分片重组。检测首字节 0xAA 为分片消息，
// 按 chunk index 顺序重组，收齐后按完整 JSON 解析。
static void onBleData(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

    // ── v0.6.4: 分片消息检测 ──
    if (len >= 3 && data[0] == 0xAA) {
        int idx   = (int)data[1];
        int total = (int)data[2];
        size_t chunkDataLen = len - 3;
        const uint8_t* chunkData = data + 3;

        // 超时保护：如果 5 秒内没收齐，丢弃旧缓冲
        if (sChunkTotal > 0 && (millis() - sChunkStartMs > 5000)) {
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片超时，丢弃 %d/%d\n", sChunkReceived, sChunkTotal);
            }
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
        }

        // v0.6.6: 新消息开始 (idx=0) → 总是重置缓冲
        // 这是关键修复：之前如果 chunk 错乱会丢弃整个当前状态，
        // 导致 Flutter 端 _onPollReceived 用 Future.wait() 并行 5 个 write
        // 时，guide+drive 分片交错后两个消息全丢
        if (idx == 0) {
            // 第一片：重置缓冲
            sChunkLen = 0;
            sChunkTotal = total;
            sChunkReceived = 0;
            sChunkStartMs = millis();
        }

        // v0.6.6 改进:分片错乱时保守处理
        //   - 如果新片是 idx=0 (新消息开始) → 上面已重置，此处一定一致
        //   - 如果新片是 idx>0 但和当前状态不匹配
        //       A) 当前 sChunkTotal==0 (已被超时/前次错乱清空) → 重新开始
        //       B) 当前 sChunkTotal>0 (在处理另一条消息中) → 丢弃这个新片
        //          因为这条消息属于另一条并发消息，保留当前状态更重要
        if (total != sChunkTotal || idx != sChunkReceived) {
            if (sChunkTotal == 0 && idx > 0) {
                // 状态已空，新片是中间片 → 重新开始（按 idx 推断 total=idx+1 不安全，直接丢弃）
                if (Debug::LOG_BLE_RAW) {
                    Serial.printf("[BLE] 分片孤立 (idx=%d, 无前置), 丢弃\n", idx);
                }
                sChunkTotal = 0;
                sChunkReceived = 0;
                sChunkLen = 0;
                return;
            }
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片错乱 (期望 %d/%d, 收到 %d/%d), 丢弃新片保留当前消息\n",
                              sChunkReceived, sChunkTotal, idx, total);
            }
            // 关键:不重置 sChunkTotal 等,保留当前消息进度
            return;
        }

        // 追加到重组缓冲
        if (sChunkLen + chunkDataLen < sizeof(sChunkBuf)) {
            memcpy(sChunkBuf + sChunkLen, chunkData, chunkDataLen);
            sChunkLen += chunkDataLen;
            sChunkReceived++;
        } else {
            Serial.printf("[BLE] 分片缓冲溢出 (%u + %u > %u)\n",
                          (unsigned)sChunkLen, (unsigned)chunkDataLen, (unsigned)sizeof(sChunkBuf));
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
            return;
        }

        // 收齐所有分片 → 复制到 sJsonBuffer 继续解析
        if (sChunkReceived >= sChunkTotal) {
            size_t copyLen = sChunkLen < sizeof(sJsonBuffer) - 1
                             ? sChunkLen : sizeof(sJsonBuffer) - 1;
            memcpy(sJsonBuffer, sChunkBuf, copyLen);
            sJsonBuffer[copyLen] = '\0';

            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片重组完成 (%d 片, %u 字节)\n",
                              sChunkTotal, (unsigned)sChunkLen);
            }

            // 重置分片状态
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;

            // 继续向下解析（不 return）
        } else {
            return; // 等待更多分片
        }
    } else {
        // 非分片消息：直接拷贝到 sJsonBuffer
        size_t copyLen = len < sizeof(sJsonBuffer) - 1 ? len : sizeof(sJsonBuffer) - 1;
        memcpy(sJsonBuffer, data, copyLen);
        sJsonBuffer[copyLen] = '\0';
    }

    // 解析 JSON 顶层
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, sJsonBuffer);
    if (err) {
        if (Debug::LOG_BLE_RAW) {
            Serial.printf("[BLE] ✗ JSON 解析失败: %s\n", err.c_str());
        }
        return;
    }

    const char* type = doc["type"] | "";
    if (type[0] == '\0') {
        if (Debug::LOG_BLE_RAW) {
            Serial.printf("[BLE] ✗ 缺少 type 字段: %s\n", sJsonBuffer);
        }
        return;
    }
    size_t jsonLen = strlen(sJsonBuffer);

    // 根据 type 路由到对应 NavParser
    bool ok = false;
    if (strcmp(type, "guide") == 0) {
        ok = Nav::parseGuideInfo(sJsonBuffer, sNavState.guide);
    } else if (strcmp(type, "drive") == 0) {
        ok = Nav::parseDriveWayInfo(sJsonBuffer, sNavState.driveWay);
    } else if (strcmp(type, "tmc") == 0) {
        ok = Nav::parseTmcInfo(sJsonBuffer, sNavState.tmc);
    } else if (strcmp(type, "location") == 0) {
        ok = Nav::parseLocationInfo(sJsonBuffer, sNavState.location);
    } else if (strcmp(type, "state") == 0) {
        int state = doc["data"]["EXTRA_STATE"] | doc["data"]["state"] | -1;
        sNavState.mapState = Nav::parseMapState(state);
        ok = true;
    } else {
        if (Debug::LOG_BLE_RAW) {
            Serial.printf("[BLE] ✗ 未知 type: %s\n", type);
        }
        return;
    }

    if (ok) {
            sNavState.lastUpdateMs = millis();
            sNavDirty = true;  // v0.6.8: 只标记，loop() 统一刷屏
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] ✓ %s (%d 字节)\n", type, jsonLen);
            }
        } else {
        Serial.printf("[BLE] ✗ %s 解析失败: %s\n", type, sJsonBuffer);
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
    Serial.printf("██  FW: v%s  (%s %s)\n", PROJECT_VERSION, __DATE__, __TIME__);
    Serial.println("██████████████████████████████████████████████");
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
                  "ILI9341 TFT 横屏"
#endif
    );
    Serial.println("╚══════════════════════════════════════════════╝");
    delay(50);

    // 启动阶段主动喂狗
    esp_task_wdt_reset();

    // v0.8.1: BLE 初始化必须放在屏幕之前！
    // 原因：USB CDC 未连接主机时，后台任务会抢占 BLE 射频初始化资源。
    // 屏幕背光启动瞬间电流尖峰进一步恶化 → BLE 控制器初始化失败。
    // 先启动 BLE（电源最干净），再启动屏幕。
    // 上电后等待 1000ms 让电源和 USB 栈完全稳定。
    delay(1000);
    esp_task_wdt_reset();

    bool bleOk = sBleServer.begin(PROJECT_NAME);
    if (!bleOk) {
        Serial.println("[main] BLE 初始化失败！设备将无法接收导航数据");
    }
    sBleServer.setDataCallback(onBleData);
    sBleServer.setPollIntervalMs(500);

    // v0.9.1: BLE 初始化后，增加 SPI 总线稳定等待时间
    // NimBLE 射频活动会干扰 HSPI 总线，需要更长的稳定时间
    delay(1500);
    esp_task_wdt_reset();

#if SCREEN_SERIAL_ONLY == 0
    // v0.9.1: 重新初始化 SPI 总线，清除 BLE 射频干扰残留
    SPI.end();
    delay(50);
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    SPI.setFrequency(40000000);
    delay(50);

    // 手动硬件复位 TFT（RST=4），确保 ILI9341 在干净状态下重新初始化
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);  // ILI9341 规格要求复位后至少 120ms 才能接收命令
#endif

    // ── BLE 就绪 + TFT 复位后再初始化屏幕 ──
    bool screenOk = sScreen.init();
    if (!screenOk) {
        Serial.println("[Screen] 屏幕初始化失败，回退到串口直通");
    }
    if (bleOk) {
        sScreen.log("系统启动，等待手机 BLE 连接...");
    } else {
        sScreen.log("BLE 初始化失败，请重启设备");
    }
    delay(50);
}

void loop() {
    esp_task_wdt_reset();

    // v0.9.1: 分段处理，每步之后喂狗，防止单步过久触发 WDT
    sBleServer.loop();
    esp_task_wdt_reset();

    sScreen.setBleConnected(sBleServer.isConnected());
    esp_task_wdt_reset();

    if (sNavDirty) {
        sNavDirty = false;
        sScreen.setNavState(sNavState);
        esp_task_wdt_reset();  // applyNavState 可能耗时较长
    }

    sScreen.update();
    esp_task_wdt_reset();

    delay(5);
}
