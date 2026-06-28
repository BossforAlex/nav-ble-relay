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
#define PROJECT_VERSION "0.3.1"

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

// ===================== 功能开关 =====================
namespace Feature {
    // 当前阶段关闭真实屏幕，使用串口虚拟屏幕输出
    constexpr bool ENABLE_PHYSICAL_SCREEN = false;

    // 是否启用 iOS Watch 风格的动画效果（无屏幕时在串口打印动画帧）
    constexpr bool ENABLE_ANIMATION = true;

    // 是否自动重连 BLE
    constexpr bool BLE_AUTO_RECONNECT = true;

    // BLE 扫描超时（毫秒）
    constexpr uint32_t BLE_SCAN_TIMEOUT_MS = 5000;

    // 无导航数据时是否降低刷新频率（省电）
    constexpr bool LOW_POWER_WHEN_IDLE = false;
}

// ===================== 调试开关 =====================
namespace Debug {
    constexpr bool LOG_BLE_RAW      = true;  // 打印原始 BLE JSON
    constexpr bool LOG_RENDER_STATE = true;  // 打印渲染状态
    constexpr bool LOG_ANIMATION    = true;  // 打印动画帧信息
    constexpr bool LOG_SYSTEM       = true;  // 打印系统/连接日志
}
