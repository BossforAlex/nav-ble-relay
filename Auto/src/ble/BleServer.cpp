#include "BleServer.h"

#include "esp_log.h"
#include "esp_task_wdt.h"

/**
 * @file BleServer.cpp
 * @brief ESP32 BLE GATT Server 实现
 *
 * v0.5.7 重构（完全匹配开源参考库 alexanderlavrushko/BLE-HUD-navigation-ESP32）：
 *   - CHAR_POLL 从 NOTIFY 改为 INDICATE（属性 + 加 BLE2902 描述符）
 *   - loop() 中改用 chrPoll->indicate()（无参，发送当前 value 字段）
 *   - 1.4.1 NimBLE 没有 notifyValue()，必须用 indicate() API
 *
 * 架构：
 *   - 1 Service (UUID 0xFFE0)
 *     - CHAR_DATA (0xFFE1, WRITE | WRITE_NR)：手机写入 JSON 导航数据
 *     - CHAR_POLL (0xFFE2, INDICATE + BLE2902)：ESP32 每 2 秒发一次空 indicate poll
 *
 * 关键设计（修复 Interrupt wdt 超时）：
 *   回调中只设置标志位 + 拷贝数据到环形缓冲区，
 *   所有重活（Serial 打印、JSON 解析、回调上层）由 loop() 处理。
 *   notify 发送也由 loop() 处理，避免在回调中阻塞协议栈。
 */

// ============================================================
// 内部辅助：连接状态回调类
// ============================================================
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleServer* parent) : mParent(parent) {}

    void onConnect(NimBLEServer* pServer) override {
        mParent->onConnect(pServer);
    }

    void onDisconnect(NimBLEServer* pServer) override {
        mParent->onDisconnect(pServer);
    }

private:
    BleServer* mParent;
};

// ============================================================
// 内部辅助：特征值写入回调类
//
// 只挂一个回调给 CHAR_DATA（唯一可写特征值）。
// ============================================================
class CharWriteCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit CharWriteCallbacks(BleServer* parent) : mParent(parent) {}

    void onWrite(NimBLECharacteristic* pChar) override {
        mParent->onWrite(pChar);
    }

private:
    BleServer* mParent;
};

// ============================================================
// BleServer 实现
// ============================================================

bool BleServer::begin(const char* deviceName) {
    // 关闭 BLE 安全 / 配对 / 加密要求
    // 关键：NimBLE 默认要求 Secure Connection (LESC) 配对，
    // 很多手机 / Android 版本会触发 SEC_REQ_EVT 协商但不接受 LESC，
    // 导致 smp_calculate_link_key_from_long_term_key 失败，写入被拒。
    // 关闭后可无加密通信，简化连接（用户场景：本地近距离）。
    NimBLEDevice::setSecurityAuth(false, false, false);
    NimBLEDevice::setSecurityInitKey(0);
    NimBLEDevice::setSecurityRespKey(0);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    // 关键：禁用 BLE 默认的 CCCD 自动订阅行为
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

    // v0.7.1: BLE 初始化重试机制
    // 非 USB 供电时，电源不稳定可能导致 BLE 控制器初始化失败。
    // 重试最多 3 次，每次间隔 500ms 让电源稳定。
    bool bleOk = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("[BLE] 初始化 BLE 协议栈 (第 %d/3 次)...\n", attempt);
        bleOk = NimBLEDevice::init(deviceName);
        if (bleOk) {
            Serial.printf("[BLE] ✓ BLE 协议栈初始化成功\n");
            break;
        }
        Serial.printf("[BLE] ✗ BLE 初始化失败 (第 %d/3 次)，500ms 后重试...\n", attempt);
        delay(500);
        esp_task_wdt_reset();
    }

    if (!bleOk) {
        Serial.println("[BLE] ✗✗✗ BLE 初始化全部失败！请检查供电或重启设备");
        started = false;
        return false;
    }

    // 发射功率适当调整，确保与各种手机兼容
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);

    // 请求较大 MTU，减少分包
    NimBLEDevice::setMTU(517);

    // 短暂延时让协议栈就绪
    delay(200);

    // 创建 GATT Server
    server = NimBLEDevice::createServer();
    if (server == nullptr) {
        Serial.println("[BLE] 创建 GATT Server 失败");
        started = false;
        return false;
    }
    ServerCallbacks* serverCb = new ServerCallbacks(this);
    server->setCallbacks(serverCb);

    // 创建主服务
    service = server->createService(BleUUID::SERVICE);

    // ── 特征值 1：CHAR_DATA（手机 → ESP32，WRITE | WRITE_NR） ──
    // 只设 WRITE 属性，不再设 NOTIFY。手机写入的 JSON 由 NavParser 解析。
    chrData = service->createCharacteristic(
        BleUUID::CHAR_DATA,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    CharWriteCallbacks* writeCb = new CharWriteCallbacks(this);
    chrData->setCallbacks(writeCb);

    // ── 特征值 2：CHAR_POLL（ESP32 → 手机，INDICATE） ──
    // 与开源参考库 alexanderlavrushko/BLE-HUD-navigation-ESP32 完全一致：
    //   - 属性 INDICATE（不是 NOTIFY）—— phone 收到需要回 ACK
    //   - CCCD 描述符 (0x2902) 由 NimBLE 自动创建，不可手动创建
    //     （NimBLE 1.4.x 在 createCharacteristic 时检测到 INDICATE/NOTIFY 属性会
    //       自动添加 CCCD，手动 createDescriptor("2902") 会触发 assert）
    //   - 每 2 秒发一次空 indicate，手机收到后把最新数据写回 CHAR_DATA
    chrPoll = service->createCharacteristic(
        BleUUID::CHAR_POLL,
        NIMBLE_PROPERTY::INDICATE
    );
    chrPoll->setValue("");  // 初始为空字符串，每次 indicate() 发 ""
    // 不需要 setCallbacks（手机只读不写）

    // 启动服务
    service->start();

    // 启动广播（让手机能扫描并连接）
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BleUUID::SERVICE);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);  // iPhone 兼容
    adv->setMaxPreferred(0x12);
    adv->start();

    started = true;

    // v0.7.1: 初始化成功后才关闭 NimBLE 内部日志 spam
    // 之前把日志抑制放在 init 前，导致看不到 BLE 初始化失败的错误信息
    esp_log_level_set("NimBLE", ESP_LOG_NONE);
    esp_log_level_set("NimBLEServer", ESP_LOG_NONE);
    esp_log_level_set("NimBLEService", ESP_LOG_NONE);
    esp_log_level_set("NimBLECharacteristic", ESP_LOG_NONE);
    esp_log_level_set("NimBLEAdvertising", ESP_LOG_NONE);
    esp_log_level_set("NimBLEDevice", ESP_LOG_NONE);
    esp_log_level_set("NimBLEUtils", ESP_LOG_NONE);
    esp_log_level_set("NimBLERemoteService", ESP_LOG_NONE);
    esp_log_level_set("NimBLERemoteCharacteristic", ESP_LOG_NONE);
    esp_log_level_set("NimBLEClient", ESP_LOG_NONE);
    esp_log_level_set("NimBLEScan", ESP_LOG_NONE);
    esp_log_level_set("*", ESP_LOG_WARN);

    // 初始化活动时间戳
    _lastActivityMs.store(millis());
    _lastPollSentMs.store(0);

    // 打印详细的启动验证信息
    Serial.println();
    Serial.println("═══════════════ BLE GATT Server 启动验证 v0.5.8 ═══════════════");
    Serial.printf("  设备名:    %s\n", deviceName);
    Serial.printf("  设备地址:  %s\n",
                  NimBLEDevice::getAddress().toString().c_str());
    Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
    Serial.printf("  加密/配对: 已禁用 (sm_bonding=0 sm_mitm=0 sm_sc=0)\n");
    Serial.printf("  服务 UUID: %s\n", BleUUID::SERVICE);
    Serial.printf("  特征值 1:  %s  (WRITE | WRITE_NR)  ← 手机写 JSON 导航数据\n",
                  BleUUID::CHAR_DATA);
    Serial.printf("  特征值 2:  %s  (INDICATE + BLE2902) → ESP32 每 2s indicate poll\n",
                  BleUUID::CHAR_POLL);
    Serial.printf("  广播状态:  已启动\n");
    Serial.println("════════════════════════════════════════════════════════════");
    Serial.printf("[BLE] ✓ 等待手机连接...\n");
    Serial.flush();  // 强制刷新串口，确保用户立即看到
    return true;
}

void BleServer::loop() {
    // ── 1. 处理连接事件 ──
    if (_connectPending.load()) {
        _connectPending.store(false);
        Serial.println();
        Serial.println("─────────────── 手机连接事件 ───────────────");
        Serial.printf("  连接数:    %d\n", _pendingConnCount.load());
        Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
        Serial.println("──────────────────────────────────────────");
        Serial.printf("[BLE] ✓ 手机已连接，准备接收数据\n");
        Serial.flush();
        // 连接后重置活动计时器（让 poll 立即开始工作）
        _lastActivityMs.store(millis());
    }
    if (_disconnectPending.load()) {
        _disconnectPending.store(false);
        Serial.printf("[BLE] 手机已断开（当前连接数: %d）\n",
                      _pendingConnCount.load());
        // 断开后继续广播，允许其他手机连接
        // 仅在广播已停止时才重启，避免 "Advertising already active" 警告 spam
        if (server != nullptr && started) {
            NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
            if (adv != nullptr && !adv->isAdvertising()) {
                adv->start();
            }
        }
        Serial.flush();
    }

    // ── 2. 消费写入事件队列 ──
    int head = _writeHead.load();
    int tail = _writeTail.load();
    while (tail != head) {
        // 拷贝事件数据到栈上（避免持锁时调用 dataCallback 长时间阻塞）
        WriteEvent ev;
        portENTER_CRITICAL(&_writeMux);
        ev = _writeBuf[tail];
        int nextTail = (tail + 1) % kMaxWriteEvents;
        _writeTail.store(nextTail);
        portEXIT_CRITICAL(&_writeMux);
        tail = nextTail;

        // 在锁外调用 dataCallback（避免持锁时间过长导致 BLE 任务入队阻塞）
        if (dataCallback) {
            dataCallback(ev.data, ev.len);
        }
        // 重新读 head
        head = _writeHead.load();
    }

    // ── 3. ESP32 → 手机 poll 节流（INDICATE） ──
    // 检测：距离上次收到数据（或上次 poll）超过 _pollIntervalMs → 发一次空 indicate
    // 与开源参考库一致：用 indicate() 发送当前 value 字段（这里为空字符串）
    if (!isConnected() || chrPoll == nullptr) return;

    const uint32_t nowMs = millis();
    const uint32_t interval = _pollIntervalMs.load();
    if (interval == 0) return;  // 0 = 禁用 poll

    const uint32_t lastActivity = _lastActivityMs.load();
    const uint32_t elapsed = nowMs - lastActivity;
    if (elapsed < interval) return;

    // 距上次 poll 也需要超过 interval（避免快速重试）
    const uint32_t lastPoll = _lastPollSentMs.load();
    if (nowMs - lastPoll < interval) return;

    // 发送 indicate（INDICATE 比 NOTIFY 多一次 ACK，但更可靠）
    // 关键修复：NimBLE 1.4.1 没有 notifyValue()，必须用 indicate() / notify()
    // 字段 value 保持空字符串（手机只需要"有新数据请发"的信号）
    //
    // v0.5.11 修复：indicate() 会阻塞等待手机 ACK，若手机响应慢可能阻塞数秒。
    // 在调用前主动喂狗，防止长时间阻塞触发 Task WDT。
    // NimBLE 内部 indicate() 有超时机制（约 1 秒），但仍需喂狗保护。
    esp_task_wdt_reset();
    chrPoll->indicate();
    _lastPollSentMs.store(nowMs);
    _lastActivityMs.store(nowMs);  // poll 本身也算一次活动
    _pollSentCount.fetch_add(1);
}

void BleServer::enqueueWrite(const uint8_t* data, size_t len) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文（CPU1）中被调用
    // 必须用 portMUX 锁保护 _writeBuf[] 与 _writeHead 的并发访问
    // 避免 CPU0 (loop) 消费时读到错乱的 head → 数组越界 → WDT
    if (data == nullptr || len == 0) return;

    int copyLen = (int)len;
    if (copyLen > 520) copyLen = 520;

    portENTER_CRITICAL(&_writeMux);
    int head = _writeHead.load();
    int tail = _writeTail.load();
    int nextHead = (head + 1) % kMaxWriteEvents;

    if (nextHead == tail) {
        // 队列满：丢弃最旧的事件（覆盖 tail）
        tail = (tail + 1) % kMaxWriteEvents;
        _writeTail.store(tail);
        _droppedEventCount.fetch_add(1);
    }

    WriteEvent& ev = _writeBuf[head];
    memcpy(ev.data, data, copyLen);
    ev.len = (uint16_t)copyLen;

    // 内存屏障：确保 _writeBuf 写入对其他 CPU 可见后再发布 _writeHead
    _writeHead.store(nextHead);
    _writeEventCount.fetch_add(1);

    portEXIT_CRITICAL(&_writeMux);
}

void BleServer::onConnect(NimBLEServer* pServer) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文（CPU1）执行
    // 不能调用 NimBLEDevice::getMTU() / Serial.printf / 任何阻塞操作
    // 仅用 atomic 计数器 + atomic 标志位，由 loop() 在主任务上下文中处理
    int cnt = connectedDeviceCount.fetch_add(1) + 1;
    _pendingConnCount.store(cnt);
    _connectPending.store(true);
    (void)pServer;
}

void BleServer::onDisconnect(NimBLEServer* /*pServer*/) {
    // ⚠️ 同 onConnect，不能做阻塞操作
    int cur = connectedDeviceCount.load();
    if (cur > 0) {
        connectedDeviceCount.store(cur - 1);
        _pendingConnCount.store(cur - 1);
    }
    _disconnectPending.store(true);
    // 关键：断开后自动重启广播（参考开源库设计）
    // 这里只能设标志位，真正的 startAdvertising 由 loop() 处理
}

void BleServer::onWrite(NimBLECharacteristic* pChar) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文（CPU1）执行
    // 不能调用 Serial.printf / 任何阻塞操作
    // 仅获取值 + 入队（enqueueWrite 内部已加 portMUX 锁），
    // 由 loop() 在主任务上下文中处理
    //
    // v0.5.11 修复：避免 std::string 拷贝（在 BLE 任务栈上分配）
    // 改用 pChar->getValue() 返回的临时对象直接入队数据指针
    if (pChar == nullptr) return;
    if (dataCallback == nullptr) return;

    // 获取特征值（返回 std::string 临时对象，数据在堆上）
    std::string value = pChar->getValue();
    if (value.empty()) return;

    enqueueWrite(reinterpret_cast<const uint8_t*>(value.data()),
                 value.size());

    // 重置活动计时器（loop 中的 poll 看到就会停止发 poll）
    _lastActivityMs.store(millis());
}
