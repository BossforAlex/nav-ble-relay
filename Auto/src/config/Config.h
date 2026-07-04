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
#define PROJECT_VERSION "0.4.0"

// ===================== 串口配置 =====================
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

// ===================== BLE 配置 =====================
// Android 端 BLE GATT 服务与特征值 UUID
// 必须与 BleGattServer.kt 中定义的 UUID 保持一致

// 设备名前缀：Android 端广播名与 ESP32 本地名均使用此前缀，
// 双方仅在识别到对方名称以此前缀开头时才建立/接受连接。
constexpr const char* DEVICE_NAME_PREFIX = "ICA";

namespace BleUUID {
    constexpr const char* SERVICE       = "0000FFE0-0000-1000-8000-00805F9B34FB";
    constexpr const char* CHAR_GUIDE    = "0000FFE1-0000-1000-8000-00805F9B34FB"; // 导航引导信息
    constexpr const char* CHAR_DRIVE    = "0000FFE2-0000-1000-8000-00805F9B34FB"; // 车道信息
    constexpr const char* CHAR_TMC      = "0000FFE3-0000-1000-8000-00805F9B34FB"; // 路况光柱
    constexpr const char* CHAR_STATE    = "0000FFE4-0000-1000-8000-00805F9B34FB"; // 导航状态
    constexpr const char* CHAR_LOCATION = "0000FFE5-0000-1000-8000-00805F9B34FB"; // 定位信息
}

// ===================== 屏幕配置 =====================
#ifndef SCREEN_SERIAL_ONLY
#define SCREEN_SERIAL_ONLY 0
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
