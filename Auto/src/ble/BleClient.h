#pragma once

/**
 * @file BleClient.h
 * @brief ESP32 BLE 客户端封装
 *
 * 职责：扫描 -> 连接 -> 发现服务 -> 订阅所有导航特征值通知 -> 回调给上层
 * 通过 std::function 回调解耦，便于替换为其他通信方式（如 WiFi/Serial）。
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <functional>
#include "config/Config.h"

// 前向声明内部回调类，允许它们访问私有状态
class AdvertisedDeviceCallbacks;
class ClientCallbacks;

class BleClient {
    friend class AdvertisedDeviceCallbacks;
    friend class ClientCallbacks;

public:
    // 回调签名：uuid=特征值 UUID 字符串, data=原始字节, len=长度
    using DataCallback = std::function<void(const char* uuid, const uint8_t* data, size_t len)>;

    BleClient() = default;

    // 初始化 BLE 设备并设置设备名
    void begin(const char* deviceName);

    // 设置目标 Android 设备 MAC（不设置则自动扫描服务 UUID）
    void setTargetAddress(const char* mac) { targetMac = mac ? mac : ""; }

    // 注册数据回调
    void setDataCallback(DataCallback cb) { dataCallback = cb; }

    // 主循环中调用，处理连接/重连/扫描状态机
    void loop();

    // 当前是否已连接
    bool isConnected() const { return connected; }

private:
    bool connectToServer();
    void subscribeCharacteristics(BLERemoteService* service);
    void startScan();
    void stopScan();
    static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);

    // 当前实例指针，供静态回调使用
    static BleClient* sInstance;

    std::string targetMac;
    esp_ble_addr_type_t targetAddrType = BLE_ADDR_TYPE_PUBLIC;
    DataCallback dataCallback;
    BLEClient* client = nullptr;
    BLEScan* scan = nullptr;
    AdvertisedDeviceCallbacks* advertisedCallbacks = nullptr;
    ClientCallbacks* clientCallbacks = nullptr;
    bool connected = false;
    bool doConnect = false;
    bool isScanning = false;
    unsigned long lastReconnectAttemptMs = 0;
};
