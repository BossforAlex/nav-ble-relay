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
#include <driver/spi_master.h>

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

// v0.9.7: 安全串口输出宏——USB CDC 未连接时 Serial.printf 会阻塞主循环
#define SAFE_SERIAL(fmt, ...) do { if (Serial) Serial.printf(fmt, ##__VA_ARGS__); } while(0)

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

// v0.9.7: 安全串口输出宏——USB CDC 未连接时 Serial.printf 会阻塞主循环
#define SAFE_SERIAL(fmt, ...) do { if (Serial) Serial.printf(fmt, ##__VA_ARGS__); } while(0)

// BLE 数据接收缓冲区（静态，避免堆碎片）
// v0.5.7：单 char 通道，JSON 带 "type" 字段（guide/drive/tmc/state/location）
static char sJsonBuffer[2048];

// ===================== BLE 数据回调 =====================
// v0.5.7 重构：单 write char + INDICATE poll，JSON 用 "type" 字段路由
// 期望格式：{"type": "guide"|"drive"|"tmc"|"state"|"location", "ts": ..., "data": {...}}
//
// v0.6.4: 支持分片重组。检测首字节 0xAA 为分片消息，
// 按 chunk index 顺序重组，收齐后按完整 JSON 解析。
static void onBleData(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

    // ── v0.9.1: 分片消息检测（带锁保护，防止 BLE 回调与主 loop 竞态） ──
    if (len >= 3 && data[0] == 0xAA) {
        int idx   = (int)data[1];
        int total = (int)data[2];
        size_t chunkDataLen = len - 3;
        const uint8_t* chunkData = data + 3;

        portENTER_CRITICAL(&sChunkMux);

        // 超时保护：如果 5 秒内没收齐，丢弃旧缓冲
        if (sChunkTotal > 0 && (millis() - sChunkStartMs > 5000)) {
            if (Debug::LOG_BLE_RAW) {
                SAFE_SERIAL("[BLE] 分片超时，丢弃 %d/%d\n", sChunkReceived, sChunkTotal);
            }
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
        }

        if (idx == 0) {
            sChunkLen = 0;
            sChunkTotal = total;
            sChunkReceived = 0;
            sChunkStartMs = millis();
        }

        if (total != sChunkTotal || idx != sChunkReceived) {
            if (sChunkTotal == 0 && idx > 0) {
                if (Debug::LOG_BLE_RAW) {
                    SAFE_SERIAL("[BLE] 分片孤立 (idx=%d, 无前置), 丢弃\n", idx);
                }
                sChunkTotal = 0;
                sChunkReceived = 0;
                sChunkLen = 0;
                portEXIT_CRITICAL(&sChunkMux);
                return;
            }
            if (Debug::LOG_BLE_RAW) {
                SAFE_SERIAL("[BLE] 分片错乱 (期望 %d/%d, 收到 %d/%d), 丢弃新片\n",
                              sChunkReceived, sChunkTotal, idx, total);
            }
            portEXIT_CRITICAL(&sChunkMux);
            return;
        }

        if (sChunkLen + chunkDataLen < sizeof(sChunkBuf)) {
            memcpy(sChunkBuf + sChunkLen, chunkData, chunkDataLen);
            sChunkLen += chunkDataLen;
            sChunkReceived++;
        } else {
            SAFE_SERIAL("[BLE] 分片缓冲溢出 (%u + %u > %u)\n",
                          (unsigned)sChunkLen, (unsigned)chunkDataLen, (unsigned)sizeof(sChunkBuf));
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
            portEXIT_CRITICAL(&sChunkMux);
            return;
        }

        bool complete = (sChunkReceived >= sChunkTotal);
        size_t copyLen = 0;
        if (complete) {
            copyLen = sChunkLen < sizeof(sJsonBuffer) - 1
                      ? sChunkLen : sizeof(sJsonBuffer) - 1;
            memcpy(sJsonBuffer, sChunkBuf, copyLen);
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
        }
        portEXIT_CRITICAL(&sChunkMux);

        if (complete) {
            sJsonBuffer[copyLen] = '\0';
            if (Debug::LOG_BLE_RAW) {
                SAFE_SERIAL("[BLE] 分片重组完成 (%d 片, %u 字节)\n",
                              total, (unsigned)copyLen);
            }            }
            // 继续向下解析
        } else {
            return;
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
            SAFE_SERIAL("[BLE] ✗ JSON 解析失败: %s\n", err.c_str());
        }
        return;
    }

    const char* type = doc["type"] | "";
    if (type[0] == '\0') {
        if (Debug::LOG_BLE_RAW) {
            SAFE_SERIAL("[BLE] ✗ 缺少 type 字段: %s\n", sJsonBuffer);
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
            SAFE_SERIAL("[BLE] ✗ 未知 type: %s\n", type);
        }
        return;
    }

    if (ok) {
            sNavState.lastUpdateMs = millis();
            sNavDirty = true;  // v0.6.8: 只标记，loop() 统一刷屏
            if (Debug::LOG_BLE_RAW) {
                SAFE_SERIAL("[BLE] ✓ %s (%d 字节)\n", type, jsonLen);
            }
        } else {
        SAFE_SERIAL("[BLE] ✗ %s 解析失败: %s\n", type, sJsonBuffer);
    }
}

// ===================== 初始化 =====================
void setup() {
    // v0.9.4: 关键修复 — 运行时设置 WDT 超时 15 秒
    // 平台配置中的 -D CONFIG_ESP_TASK_WDT_TIMEOUT_S=10 只是编译宏，
    // Arduino 框架在启动时仍调用 esp_task_wdt_init(5, true)，实际超时仅 5 秒。
    // setup() 中 BLE 初始化 + 屏幕初始化需要 ~8-10 秒，必须显式扩大超时。
    esp_task_wdt_init(15, true);
    esp_task_wdt_reset();

    // v0.9.3: 上电后等待电源稳定（冷启动时电池/LDO 需要建立时间）
    delay(800);
    esp_task_wdt_reset();

    Serial.begin(SERIAL_BAUD);

    // 等待串口就绪，但最多 1.5 秒
    while (!Serial && millis() < 1500) { delay(10); }
    esp_task_wdt_reset();  // v0.9.4: 串口等待后喂狗
    delay(500);  // 确保串口芯片完全就绪
    esp_task_wdt_reset();  // v0.9.4: 串口就绪后喂狗

    // 打印醒目的版本标识（用户需求：确保上电即看到完整输出，防止串口丢数据）
    // v0.9.7: 仅当 USB CDC 已连接时才输出，否则 Serial.printf 阻塞 setup()
    if (Serial) {
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
    }

    // 启动阶段主动喂狗
    esp_task_wdt_reset();

    // v0.8.1: BLE 初始化必须放在屏幕之前！
    // 原因：USB CDC 未连接主机时，后台任务会抢占 BLE 射频初始化资源。
    // 屏幕背光启动瞬间电流尖峰进一步恶化 → BLE 控制器初始化失败。
    // 先启动 BLE（电源最干净），再启动屏幕。
    // v0.9.4: 上电后等待 500ms 让电源稳定（原 1000ms 过长）
    delay(500);
    esp_task_wdt_reset();

    bool bleOk = sBleServer.begin(PROJECT_NAME);
    if (!bleOk) {
        if (Serial) Serial.println("[main] BLE 初始化失败！设备将无法接收导航数据");
    }
    sBleServer.setDataCallback(onBleData);
    sBleServer.setPollIntervalMs(500);

    // v0.9.3: BLE 初始化后，增加 SPI 总线稳定等待时间
    // NimBLE 射频活动会干扰 HSPI 总线，需要更长的稳定时间
    // 冷启动（无 USB）时 BLE 射频建立更慢，需要 2500ms+ 稳定
    delay(1500);  // v0.9.4: 原 2500ms，减少以避 WDT 超时
    esp_task_wdt_reset();

#if SCREEN_SERIAL_ONLY == 0
    // v0.9.6: 手动初始化 HSPI 总线（SPI3_HOST），修复冷启动 LCD 不亮
    // 根因：ESP32-S3 ROM 启动时将 FSPI(SPI2) 路由到 GPIO 11/12/10（Flash 引脚），
    // 与 TFT 的 HSPI(SPI3) 引脚冲突。USB 连接时 CDC 初始化意外清除 FSPI 路由，
    // 使 HSPI 能正常工作；冷启动时 FSPI 路由残留，HSPI 初始化失败 → LCD 不亮。
    // 修复：在 TFT 初始化前显式初始化 HSPI，确保总线正确配置。
    {
        spi_bus_config_t busCfg = {};
        busCfg.mosi_io_num = TFT_MOSI;   // 11
        busCfg.miso_io_num = -1;          // ILI9341 只写不读
        busCfg.sclk_io_num = TFT_SCLK;    // 12
        busCfg.quadwp_io_num = -1;
        busCfg.quadhd_io_num = -1;
        busCfg.max_transfer_sz = 320 * 240 * 2 + 8;
        busCfg.flags = SPICOMMON_BUSFLAG_MASTER;
        busCfg.intr_flags = 0;

        esp_err_t ret = spi_bus_initialize(SPI3_HOST, &busCfg, SPI_DMA_CH_AUTO);
        if (ret == ESP_OK) {
            if (Serial) Serial.println("[main] HSPI 总线手动初始化成功");
        } else {
            if (Serial) Serial.printf("[main] ⚠ HSPI 初始化返回 %d (ESP_OK=%d)，TFT_eSPI 将重试\n",
                          ret, ESP_OK);
        }
    }

    // 硬件复位 TFT（RST=4），确保 ILI9341 在干净状态下接收命令
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);  // ILI9341 规格要求复位后至少 120ms 才能接收命令
#endif

    // ── BLE 就绪 + TFT 复位后再初始化屏幕 ──
    bool screenOk = sScreen.init();
    if (!screenOk) {
        if (Serial) Serial.println("[Screen] 屏幕初始化失败，回退到串口直通");
    } else {
        // v0.9.4: 背光前等待 100ms（原 300ms，减少以避 WDT 超时）
        delay(100);
        esp_task_wdt_reset();
        sScreen.enableBacklight();
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

    // v0.9.1: 批量更新防抖 —— 50ms 内收到多包数据时，只应用最后一次
    // 解决 Flutter 端 guide→drive→tmc 顺序发送时的中间态闪烁
    if (sNavDirty) {
        if (sNavDirtySince == 0) {
            sNavDirtySince = millis();
        }
        if (millis() - sNavDirtySince >= 50) {
            sNavDirty = false;
            sNavDirtySince = 0;
            sScreen.setNavState(sNavState);
            esp_task_wdt_reset();  // applyNavState 可能耗时较长
        }
    }

    sScreen.update();
    esp_task_wdt_reset();

    delay(5);
}
