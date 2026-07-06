#pragma once

/**
 * @file BleServer.h
 * @brief ESP32 BLE GATT Server 封装（基于 NimBLE-Arduino 库）
 *
 * 架构（用户最新需求）：
 *   - 手机（Flutter）作为 GATT Client，主动连接 ESP32
 *   - ESP32 作为 GATT Server，等待手机连接后通过 WRITE/WRITE_NO_RESPONSE
 *     写入 JSON 数据
 *   - ESP32 收到数据后回调给上层（解析、显示）
 *
 * 关键设计（修复 Interrupt wdt 超时 + IDLE stack canary）：
 *   NimBLE 的 onConnect/onDisconnect/onWrite 回调运行在 BLE 协议栈
 *   任务上下文（通常为 CPU1 高优先级任务）。在回调中执行任何阻塞
 *   操作（如 Serial.printf 大量打印、调用 NimBLEDevice::getMTU()、
 *   JSON 解析）都会阻塞协议栈，导致：
 *     - Interrupt wdt timeout on CPU1
 *     - Stack canary / watchpoint triggered (IDLE0)
 *
 *   解决方案：回调中只做最小工作（设置标志位 + 拷贝数据到环形缓冲区），
 *   所有重活（Serial 打印、JSON 解析、回调上层）由 loop() 在主任务
 *   上下文中处理。
 *
 * 用户需求：
 *   - 批量开发场景，ESP32 端不做任何 MAC 限制
 *   - 等待任意手机连接，被动接收数据
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
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

    // 主循环中调用：消费事件队列，调用 dataCallback
    // 必须在 setup() 之外的 loop() 中频繁调用，避免 BLE 任务回调阻塞
    void loop();

    // 当前是否有手机已连接
    bool isConnected() const { return connectedDeviceCount > 0; }

    // 当前已连接手机数
    int getConnectedCount() const { return connectedDeviceCount; }

private:
    DataCallback dataCallback;
    NimBLEServer* server = nullptr;
    NimBLEService* service = nullptr;
    bool started = false;
    volatile int connectedDeviceCount = 0;

    // ── 事件队列（避免在 BLE 回调中调用 Serial / callback） ──
    // 事件类型
    enum class EventType : uint8_t {
        None = 0,
        Connect = 1,
        Disconnect = 2,
        Write = 3,
    };

    // 写入事件数据（环形缓冲区）
    struct WriteEvent {
        char uuid[40];          // UUID 字符串（含结尾 \0）
        uint16_t len;           // 数据长度
        // 数据缓冲区：足够容纳 MTU=517 的写入；超长截断
        uint8_t data[520];
    };

    static constexpr int kMaxWriteEvents = 8;
    WriteEvent _writeBuf[kMaxWriteEvents];
    volatile int _writeHead = 0;   // BLE 回调写入位置
    volatile int _writeTail = 0;   // loop() 消费位置

    // 连接/断开事件（标志位形式，避免重复处理）
    volatile bool _connectPending = false;
    volatile bool _disconnectPending = false;
    volatile int _pendingConnCount = 0;

    // 把一个写入事件入队（在 BLE 回调中调用，必须无阻塞）
    void enqueueWrite(const char* uuid, const uint8_t* data, size_t len);

public:
    // 内部回调方法（供内部回调类 ServerCallbacks / CharWriteCallbacks / CharSubscribeCallbacks 访问）
    // 注意：这些函数在 BLE 协议栈任务上下文中执行，不能做阻塞操作
    void onConnect(NimBLEServer* pServer);
    void onDisconnect(NimBLEServer* pServer);
    void onWrite(NimBLECharacteristic* pChar);
    void onSubscribe(NimBLECharacteristic* pChar, ble_gap_conn_desc* desc, uint16_t subValue);
};
