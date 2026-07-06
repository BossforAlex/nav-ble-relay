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
        // 仅使用 WRITE（writeWithResponse），更可靠：
        //   - 有 ACK 确认，避免丢包
        //   - 触发 onWrite 回调明确
        //   - 不触发 NimBLE 的 subscribe 事件误判
        // （不再使用 WRITE_NR，因为它会被部分协议栈误判为 subscribe）
        NimBLECharacteristic* chr = service->createCharacteristic(
            desc.uuid,
            NIMBLE_PROPERTY::WRITE
        );
        chr->setCallbacks(writeCb);
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 已注册特征值: %s (%s) props=WRITE\n",
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
    Serial.printf("  特征值:    WRITE (5 个，强制 writeWithResponse)\n");
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
    if (_connectPending) {
        _connectPending = false;
        Serial.println();
        Serial.println("─────────────── 手机连接事件 ───────────────");
        Serial.printf("  连接数:    %d\n", _pendingConnCount);
        Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
        Serial.println("──────────────────────────────────────────");
        Serial.printf("[BLE] ✓ 手机已连接，准备接收数据\n");
    }
    if (_disconnectPending) {
        _disconnectPending = false;
        Serial.printf("[BLE] 手机已断开（当前连接数: %d）\n", _pendingConnCount);
        // 断开后继续广播，允许其他手机连接
        // 仅在广播已停止时才重启，避免 "Advertising already active" 警告 spam
        if (server != nullptr && started) {
            NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
            if (adv != nullptr && !adv->isAdvertising()) {
                adv->start();
            }
        }
    }

    // 消费写入事件队列
    while (_writeTail != _writeHead) {
        const WriteEvent& ev = _writeBuf[_writeTail];
        if (dataCallback) {
            dataCallback(ev.uuid, ev.data, ev.len);
        }
        _writeTail = (_writeTail + 1) % kMaxWriteEvents;
    }
}

void BleServer::enqueueWrite(const char* uuid, const uint8_t* data, size_t len) {
    // 计算下一个写入位置
    int nextHead = (_writeHead + 1) % kMaxWriteEvents;
    if (nextHead == _writeTail) {
        // 队列满：丢弃最旧的事件（覆盖 tail）
        _writeTail = (_writeTail + 1) % kMaxWriteEvents;
    }
    WriteEvent& ev = _writeBuf[_writeHead];
    // 拷贝 UUID（含结尾 \0）
    strncpy(ev.uuid, uuid ? uuid : "?", sizeof(ev.uuid) - 1);
    ev.uuid[sizeof(ev.uuid) - 1] = '\0';
    // 拷贝数据（超长截断）
    size_t copyLen = len;
    if (copyLen > sizeof(ev.data)) copyLen = sizeof(ev.data);
    memcpy(ev.data, data, copyLen);
    ev.len = (uint16_t)copyLen;
    _writeHead = nextHead;
}

void BleServer::onConnect(NimBLEServer* pServer) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文中执行
    // 不能调用 NimBLEDevice::getMTU() / Serial.printf / 任何阻塞操作
    // 仅设置标志位 + 计数器，由 loop() 在主任务上下文中处理
    connectedDeviceCount++;
    _pendingConnCount = connectedDeviceCount;
    _connectPending = true;
    (void)pServer;
}

void BleServer::onDisconnect(NimBLEServer* /*pServer*/) {
    // ⚠️ 同 onConnect，不能做阻塞操作
    if (connectedDeviceCount > 0) connectedDeviceCount--;
    _pendingConnCount = connectedDeviceCount;
    _disconnectPending = true;
}

void BleServer::onWrite(NimBLECharacteristic* pChar) {
    // ⚠️ 此函数在 BLE 协议栈任务上下文中执行
    // 不能调用 Serial.printf / 任何阻塞操作
    // 仅获取值 + 入队，由 loop() 在主任务上下文中处理
    if (pChar == nullptr) return;
    if (dataCallback == nullptr) return;

    std::string value = pChar->getValue();
    if (value.empty()) return;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
    size_t len = value.size();
    // UUID toString() 返回 std::string，但注意不能在此上下文中调用复杂操作
    // NimBLE 中 toString() 是简单字符串拼接，相对安全
    const char* uuid = pChar->getUUID().toString().c_str();

    enqueueWrite(uuid, data, len);
}
