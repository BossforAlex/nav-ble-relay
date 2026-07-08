#pragma once

/**
 * @file BleServer.h
 * @brief ESP32 BLE GATT Server 封装（基于 NimBLE-Arduino 库）
 *
 * v0.5.7 重构：参考 alexanderlavrushko/BLE-HUD-navigation-ESP32 的极简设计
 *   - 把 CHAR_POLL 从 NOTIFY 改为 INDICATE（与开源库完全一致）
 *   - 增加 BLE2902 描述符（indicate 必需，客户端通过它订阅/确认）
 *   - 移除 nimBLE 1.4.1 不存在的 notifyValue() 调用，改回 indicate() API
 *
 * 架构：
 *   1 个 Service + 2 个 Characteristic：
 *     - CHAR_DATA (WRITE | WRITE_NR)：手机 → ESP32 写入导航数据 JSON
 *     - CHAR_POLL (INDICATE + BLE2902)：ESP32 → 手机 poll 请求（每 2 秒发一次空指示）
 *
 * 交互模式（轮询，与开源库一致）：
 *   - ESP32 在 loop() 中检测：距离上次收到数据 > 2 秒 → 通过 CHAR_POLL 发空 indicate
 *   - 手机收到 indicate → 立刻把最新的导航数据写一次 CHAR_DATA
 *   - 收到数据后 ESP32 重置计时器
 *   - 整个交互纯文本 0 spam：去除了 NimBLE 内部 "subscribe event" 大量日志
 *
 * 关键设计（修复 Interrupt wdt 超时 + IDLE stack canary + CPU0/CPU1 竞态）：
 *   NimBLE 的 onConnect/onDisconnect/onWrite 回调运行在 BLE 协议栈
 *   任务上下文（通常为 CPU1 高优先级任务）。在回调中执行任何阻塞
 *   操作（如 Serial.printf 大量打印、调用 NimBLEDevice::getMTU()、
 *   JSON 解析）都会阻塞协议栈，导致：
 *     - Interrupt wdt timeout on CPU1
 *     - Stack canary / watchpoint triggered (IDLE0/1)
 *
 *   解决方案：
 *     1. 回调中只做最小工作（设置标志位 + 在 portMUX 锁保护下入队）
 *     2. 所有重活（Serial 打印、JSON 解析、回调上层）由 loop() 在主任务
 *        上下文中处理
 *     3. 共享队列用 portMUX_TYPE 自旋锁保护
 *     4. 标志位用 std::atomic<int> / std::atomic<bool> 替代 volatile
 *
 *   notify 发送也要避免在回调中调用 notifyValue()（同样会阻塞协议栈），
 *   所以 poll 通知也在 loop() 中发送，标志位在 BLE 任务中设置。
 *
 * 用户需求：
 *   - 批量开发场景，ESP32 端不做任何 MAC 限制
 *   - 等待任意手机连接，被动接收数据 + 主动 poll 拉新数据
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <functional>
#include "config/Config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

class BleServer {
public:
    // 回调签名：data=原始字节（来自手机写入的 JSON），len=长度
    // 不再需要 uuid 参数——只有 1 个 write char
    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;

    BleServer() = default;

    // 初始化 BLE 并启动 GATT Server + 广播
    // deviceName: ESP32 本地广播名（如 "AutoNavDisplay"）
    void begin(const char* deviceName);

    // 注册数据回调
    void setDataCallback(DataCallback cb) { dataCallback = cb; }

    // 主循环中调用：消费事件队列，调用 dataCallback，发送 poll 通知
    // 必须在 setup() 之外的 loop() 中频繁调用，避免 BLE 任务回调阻塞
    void loop();

    // 当前是否有手机已连接
    bool isConnected() const { return connectedDeviceCount.load() > 0; }

    // 当前已连接手机数
    int getConnectedCount() const { return connectedDeviceCount.load(); }

    // 配置 poll 间隔（毫秒）。0 = 禁用 poll。默认 2000ms
    void setPollIntervalMs(uint32_t ms) { _pollIntervalMs.store(ms); }

private:
    DataCallback dataCallback;
    NimBLEServer* server = nullptr;
    NimBLEService* service = nullptr;
    NimBLECharacteristic* chrData = nullptr;  // 手机 → ESP32 写入
    NimBLECharacteristic* chrPoll = nullptr;  // ESP32 → 手机 poll (indicate)
    bool started = false;
    std::atomic<int> connectedDeviceCount{0};

    // ── 事件队列（避免在 BLE 回调中调用 Serial / callback） ──
    struct WriteEvent {
        uint16_t len;           // 数据长度
        // 数据缓冲区：足够容纳 MTU=517 的写入；超长截断
        uint8_t data[520];
    };

    static constexpr int kMaxWriteEvents = 16;
    WriteEvent _writeBuf[kMaxWriteEvents];
    std::atomic<int> _writeHead{0};   // BLE 回调写入位置
    std::atomic<int> _writeTail{0};   // loop() 消费位置

    // 用于保护 _writeBuf[] 的自旋锁
    portMUX_TYPE _writeMux = portMUX_INITIALIZER_UNLOCKED;

    // 连接/断开事件（标志位形式）
    std::atomic<bool> _connectPending{false};
    std::atomic<bool> _disconnectPending{false};
    std::atomic<int> _pendingConnCount{0};

    // poll 节流：每 _pollIntervalMs 毫秒无活动就发一次空通知
    // 单位毫秒，0 = 禁用
    std::atomic<uint32_t> _pollIntervalMs{2000};
    std::atomic<uint32_t> _lastActivityMs{0};    // 上次收到数据 / 通知的 millis()
    std::atomic<uint32_t> _lastPollSentMs{0};   // 上次发 poll 的 millis()

    // 把一个写入事件入队（在 BLE 回调中调用，必须无阻塞）
    void enqueueWrite(const uint8_t* data, size_t len);

    // 总写入事件计数器（用于日志诊断）
    std::atomic<uint32_t> _writeEventCount{0};
    std::atomic<uint32_t> _droppedEventCount{0};
    std::atomic<uint32_t> _pollSentCount{0};     // 累计发送的 poll 次数

public:
    // 内部回调方法（供内部回调类访问）
    // 注意：这些函数在 BLE 协议栈任务上下文中执行，不能做阻塞操作
    void onConnect(NimBLEServer* pServer);
    void onDisconnect(NimBLEServer* pServer);
    void onWrite(NimBLECharacteristic* pChar);
};
