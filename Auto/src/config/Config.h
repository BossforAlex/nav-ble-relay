#pragma once

/**
 * @file Config.h
 * @brief 项目级常量与配置开关
 *
 * 所有与硬件、BLE UUID、调试行为相关的常量统一放在此处，
 * 便于后期修改与移植到其他 ESP32 平台。
 */

#include <Arduino.h>

// ===================== 项目信息 =====================
#define PROJECT_NAME    "AutoNavDisplay"
#define PROJECT_VERSION "0.5.7"

// ===================== 串口配置 =====================
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

// ===================== BLE 配置 =====================
// ESP32 端 BLE GATT 服务与特征值 UUID
// 必须与 Flutter 端 ble_constants.dart 中定义的 UUID 保持一致
//
// v0.5.7 重构（参考 alexanderlavrushko/BLE-HUD-navigation-ESP32 极简模式）：
//   1 个 Service + 2 个 Characteristic：
//   - CHAR_DATA：WRITE | WRITE_NR，手机 → ESP32，导航数据（JSON）
//   - CHAR_POLL：INDICATE + BLE2902，ESP32 → 手机，poll 请求（2 秒无活动则发空 indicate）
//
// 原 5 个特征值 (GUIDE/DRIVE/TMC/STATE/LOCATION) 合并为 1 个 JSON 通道，
// 通过 JSON 中的 "type" 字段路由到 NavParser 各个子解析器。

namespace BleUUID {
    constexpr const char* SERVICE   = "0000FFE0-0000-1000-8000-00805F9B34FB";
    constexpr const char* CHAR_DATA = "0000FFE1-0000-1000-8000-00805F9B34FB"; // 手机 → ESP32（WRITE | WRITE_NR）
    constexpr const char* CHAR_POLL = "0000FFE2-0000-1000-8000-00805F9B34FB"; // ESP32 → 手机（INDICATE + BLE2902）
}

// ===================== 屏幕配置 =====================
// 默认使用串口虚拟屏幕（安全模式，无 TFT 时不会崩溃）
// 连接 ST7789 TFT 屏幕后，将此处改为 0 或在 platformio.ini 的 S3 环境
// build_flags 中添加 -D SCREEN_SERIAL_ONLY=0
#ifndef SCREEN_SERIAL_ONLY
#define SCREEN_SERIAL_ONLY 1
#endif

// TFT 屏幕默认分辨率，运行时会自适应
#ifndef TFT_WIDTH
#define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 240
#endif

// HUD 配色（与视频中的 HUD 风格一致：高对比、夜间友好）
namespace HudColor {
    constexpr uint16_t BG       = 0x0000;  // 黑底
    constexpr uint16_t PRIMARY  = 0x07FF;  // 青色（主信息）
    constexpr uint16_t ACCENT   = 0x07E0;  // 绿色（车速）
    constexpr uint16_t WARN     = 0xFD20;  // 橙色（警告）
    constexpr uint16_t DANGER   = 0xF800;  // 红色（限速/超速）
    constexpr uint16_t DIM      = 0x4208;  // 暗灰（次要信息）
    constexpr uint16_t WHITE    = 0xFFFF;
    constexpr uint16_t YELLOW   = 0xFFE0;
}

// ===================== 功能开关 =====================
namespace Feature {
    constexpr bool ENABLE_PHYSICAL_SCREEN = true;   // 启用真实 TFT 屏幕
    constexpr bool ENABLE_ANIMATION = true;          // 转向箭头脉冲动画
    constexpr bool BLE_AUTO_RECONNECT = true;        // 自动重连 BLE
    constexpr uint32_t BLE_SCAN_TIMEOUT_MS = 5000;   // BLE 扫描超时
    constexpr bool LOW_POWER_WHEN_IDLE = false;      // 空闲降频
    constexpr uint32_t SCREEN_REFRESH_MS = 200;     // 屏幕刷新间隔（~5fps）
}

// ===================== 调试开关 =====================
namespace Debug {
    constexpr bool LOG_BLE_RAW      = true;
    constexpr bool LOG_RENDER_STATE = false;
    constexpr bool LOG_ANIMATION    = false;
    constexpr bool LOG_SYSTEM       = true;
}
