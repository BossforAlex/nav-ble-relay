#pragma once

/**
 * @file BleServer.h
 * @brief ESP32 BLE GATT Server 封装
 *
 * 架构（用户最新需求）：
 *   - 手机（Flutter）作为 GATT Client，主动连接特定 MAC 的 ESP32
 *   - ESP32 作为 GATT Server，等待手机连接后通过 WRITE/WRITE_NO_RESPONSE
 *     写入 JSON 数据
 *   - ESP32 收到数据后回调给上层（解析、显示）
 *
 * 特征值（与 Android 端保持一致）：
 *   - FFE1  引导信息
 *   - FFE2  车道信息
 *   - FFE3  路况光柱
 *   - FFE4  导航状态
 *   - FFE5  定位信息
 *
 * 注意：手机端做 MAC 白名单过滤（用户需求：手机蓝牙连接数较多，只推送给
 * 特定授权的 ESP32 MAC），ESP32 端不做限制，被动接收即可。
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <functional>
#include "config/Config.h"

class BleServer {
public:
    // 回调签名：uuid=特征值 UUID 字符串, data=原始字节, len=长度
    using DataCallback = std::function<void(const char* uuid, const uint8_t* data, size_t len)>;

    BleServer() = default;

    // 初始化 BLE 并启动 GATT Server + 广播
    // deviceName: ESP32 本地广播名（如 "AutoNavDisplay"）
    void begin(const char* deviceName);

    // 注册数据回调
    void setDataCallback(DataCallback cb) { dataCallback = cb; }

    // 主循环中调用（当前无需特殊处理，保留以备未来扩展）
    void loop();

    // 当前是否有手机已连接
    bool isConnected() const { return connectedDeviceCount > 0; }

    // 当前已连接手机数
    int getConnectedCount() const { return connectedDeviceCount; }

private:
    DataCallback dataCallback;
    BLEServer* server = nullptr;
    BLEService* service = nullptr;
    bool started = false;
    int connectedDeviceCount = 0;

    // 内部回调方法（在 .cpp 中以 friend 方式连接）
    void onConnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer);
    void onWrite(BLECharacteristic* pChar);
};
