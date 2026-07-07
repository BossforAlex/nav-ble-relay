#include "BleServer.h"

/**
 * @file BleServer.cpp
 * @brief ESP32 BLE GATT Server 实现（基于 NimBLE-Arduino 库）
 *
 * ESP32 作为 GATT Server（外设），广播 AutoNavDisplay 名称，
 * 等待手机（Flutter GATT Client）连接并写入 JSON 数据。
 *
 * 特征值方向：WRITE | WRITE_NO_RESPONSE
 *   - 手机连接后通过 writeCharacteristic 推送数据
 *   - ESP32 端在 onWrite 中接收并通过 dataCallback 上报
 *
 * 关键：禁用 BLE 配对/加密（用户场景：本地近距离无加密传输）
 *   - sm_bonding = 0
 *   - sm_mitm = 0
 *   - sm_sc = 0
 *   - sm_our_key_dist = 0
 *   - sm_their_key_dist = 0
 *   - sm_io_cap = NO_INPUT_NO_OUTPUT
 *
 * 用户需求：批量开发场景，ESP32 端不做任何 MAC 限制。
 *
 * 关键设计（修复 Interrupt wdt 超时）：
 *   回调中只设置标志位 + 拷贝数据到环形缓冲区，
 *   所有重活（Serial 打印、JSON 解析、回调上层）由 loop() 处理。
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
// 关键：Android BLE 协议栈连接后会自动写入 CCCD 尝试订阅通知。
// 如果特征值没有 NOTIFY 属性，CCCD 写入失败 → Android 立即断开连接。
// 因此必须添加 NOTIFY 属性 + onSubscribe 回调（即使不发送任何通知），
// 让 Android 的 CCCD 写入成功，连接才能稳定保持。
// ============================================================
class CharWriteCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit CharWriteCallbacks(BleServer* parent) : mParent(parent) {}

    void onWrite(NimBLECharacteristic* pChar) override {
        mParent->onWrite(pChar);
    }

    void onSubscribe(NimBLECharacteristic* pChar,
                     ble_gap_conn_desc* desc,
                     uint16_t subValue) override {
        mParent->onSubscribe(pChar, desc, subValue);
    }

private:
    BleServer* mParent;
};

// ============================================================
// BleServer 实现
// ============================================================

void BleServer::begin(const char* deviceName) {
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
    // 否则 Android 连接后会立即触发 GATT 通知订阅 → 我们的
    // 特征值用 WRITE 方向无 notify 数据 → 协议栈反复重试 → WDT
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

    // 初始化 BLE 协议栈
    NimBLEDevice::init(deviceName);

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
        return;
    }
    ServerCallbacks* serverCb = new ServerCallbacks(this);
    server->setCallbacks(serverCb);

    // 创建主服务
    service = server->createService(BleUUID::SERVICE);

    // 创建 5 个特征值：WRITE | WRITE_NO_RESPONSE
    struct CharDesc {
        const char* name;
        const char* uuid;
    };
    static const CharDesc chars[] = {
        {"Guide",    BleUUID::CHAR_GUIDE},
        {"DriveWay", BleUUID::CHAR_DRIVE},
        {"Tmc",      BleUUID::CHAR_TMC},
        {"State",    BleUUID::CHAR_STATE},
        {"Location", BleUUID::CHAR_LOCATION},
    };

    CharWriteCallbacks* writeCb = new CharWriteCallbacks(this);
    for (const auto& desc : chars) {
        // 关键修复：WRITE | WRITE_NR | NOTIFY
        //   - WRITE + WRITE_NR：接受手机写入（writeWithResponse + writeWithoutResponse）
        //   - NOTIFY：Android BLE 协议栈连接后会自动写入 CCCD 尝试订阅通知。
        //     如果没有 NOTIFY，CCCD 写入失败 → Android 立即断开连接。
        //     添加 NOTIFY 让 CCCD 写入成功，连接才能稳定保持。
        NimBLECharacteristic* chr = service->createCharacteristic(
            desc.uuid,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
        );
        // 写入回调 + 订阅回调（同一个 setCallbacks 同时处理 onWrite 和 onSubscribe）
        chr->setCallbacks(writeCb);
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 已注册特征值: %s (%s) props=WRITE|WRITE_NR|NOTIFY\n",
                          desc.name, desc.uuid);
        }
    }

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

    // 打印详细的启动验证信息（用户要求：防止初始化出问题）
    Serial.println();
    Serial.println("═══════════════ BLE GATT Server 启动验证 ═══════════════");
    Serial.printf("  设备名:    %s\n", deviceName);
    Serial.printf("  设备地址:  %s\n",
                  NimBLEDevice::getAddress().toString().c_str());
    Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
    Serial.printf("  加密/配对: 已禁用 (sm_bonding=0 sm_mitm=0 sm_sc=0)\n");
    Serial.printf("  服务 UUID: %s\n", BleUUID::SERVICE);
    Serial.printf("  特征值:    WRITE | WRITE_NR | NOTIFY (5 个)\n");
    for (const auto& desc : chars) {
        Serial.printf("             - %-9s  %s\n", desc.name, desc.uuid);
    }
    Serial.printf("  广播状态:  已启动\n");
    Serial.println("══════════════════════════════════════════════════════");
    Serial.printf("[BLE] ✓ 等待手机连接...\n");
    Serial.flush();  // 强制刷新串口，确保用户立即看到
}

void BleServer::loop() {
    // 处理连接事件（避免在 BLE 回调中打印导致 WDT）
    if (_connectPending.load()) {
        _connectPending.store(false);
        Serial.println();
        Serial.println("─────────────── 手机连接事件 ───────────────");
        Serial.printf("  连接数:    %d\n", _pendingConnCount.load());
        Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
        Serial.println("──────────────────────────────────────────");
        Serial.printf("[BLE] ✓ 手机已连接，准备接收数据\n");
        Serial.flush();
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

    // 消费写入事件队列（在 portMUX 锁保护下读 head/tail）
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
            dataCallback(ev.uuid, ev.data, ev.len);
        }
        // 重新读 head
        head = _writeHead.load();
    }
}

void BleServer::enqueueWrite(const char* uuid, const uint8_t* data, size_t len) {
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
    // 拷贝 UUID（含结尾 \0）
    if (uuid != nullptr) {
        strncpy(ev.uuid, uuid, sizeof(ev.uuid) - 1);
    } else {
        ev.uuid[0] = '?';
        ev.uuid[1] = '\0';
    }
    ev.uuid[sizeof(ev.uuid) - 1] = '\0';
    // 拷贝数据
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
}

void BleServer::onWrite(NimBLECharacteristic* pChar) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文（CPU1）执行
    // 不能调用 Serial.printf / 任何阻塞操作
    // 仅获取值 + 入队（enqueueWrite 内部已加 portMUX 锁），
    // 由 loop() 在主任务上下文中处理
    if (pChar == nullptr) return;
    if (dataCallback == nullptr) return;

    // 拷贝数据到栈上，避免 pChar 后续被 NimBLE 复用
    std::string value = pChar->getValue();
    if (value.empty()) return;

    // 拷贝 UUID 到栈上：toString() 返回临时 std::string，
    // 必须在 c_str() 之前用局部变量持有，避免悬垂指针
    std::string uuidStr = pChar->getUUID().toString();

    enqueueWrite(uuidStr.c_str(),
                 reinterpret_cast<const uint8_t*>(value.data()),
                 value.size());
}

void BleServer::onSubscribe(NimBLECharacteristic* pChar,
                            ble_gap_conn_desc* /*desc*/,
                            uint16_t subValue) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文中执行
    // 关键：Android BLE 协议栈连接后会自动写入 CCCD 尝试订阅通知。
    // 此回调接收 CCCD 写入结果（subValue = 0 表示取消订阅，1 表示订阅通知）。
    // 空实现：我们不发送通知，但必须接受订阅请求以保持连接稳定。
    // 不做任何操作（不打印、不阻塞），仅让 NimBLE 处理 CCCD 响应。
    // 如果在此回调中做任何阻塞操作，会触发 CPU1 WDT 超时。
    (void)pChar;
    (void)subValue;
}
