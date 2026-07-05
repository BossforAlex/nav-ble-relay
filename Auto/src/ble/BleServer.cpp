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
 * 用户需求：手机端做 MAC 白名单过滤，ESP32 端不做限制。
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
// 静态实例指针（用于静态回调中访问实例）
// ============================================================
static BleServer* sInstance = nullptr;

// ============================================================
// BleServer 实现
// ============================================================

void BleServer::begin(const char* deviceName) {
    sInstance = this;

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
        NimBLECharacteristic* chr = service->createCharacteristic(
            desc.uuid,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
        );
        // WRITE 方向不需要 CCCD 描述符
        chr->setCallbacks(writeCb);
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 已注册特征值: %s (%s) props=WRITE|WRITE_NR\n",
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
    Serial.printf("  特征值:    WRITE | WRITE_NO_RESPONSE（5 个）\n");
    for (const auto& desc : chars) {
        Serial.printf("             - %-9s  %s\n", desc.name, desc.uuid);
    }
    Serial.printf("  广播状态:  已启动\n");
    Serial.println("══════════════════════════════════════════════════════");
    Serial.printf("[BLE] ✓ 等待手机连接...\n");
}

void BleServer::loop() {
    // 当前无需在 loop 中做特殊处理
    // 连接状态由回调更新；数据由 onWrite 回调上报
}

void BleServer::onConnect(NimBLEServer* pServer) {
    connectedDeviceCount++;
    Serial.println();
    Serial.println("─────────────── 手机连接事件 ───────────────");
    Serial.printf("  连接数:    %d\n", connectedDeviceCount);
    Serial.printf("  MTU:       %d 字节\n", NimBLEDevice::getMTU());
    Serial.println("──────────────────────────────────────────");
    Serial.printf("[BLE] ✓ 手机已连接，准备接收数据\n");
    (void)pServer;  // 避免未使用警告
}

void BleServer::onDisconnect(NimBLEServer* /*pServer*/) {
    if (connectedDeviceCount > 0) connectedDeviceCount--;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 手机已断开（当前连接数: %d）\n", connectedDeviceCount);
    }
    // 断开后继续广播，允许其他手机连接
    if (server != nullptr && started) {
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        if (adv != nullptr) {
            adv->start();
        }
    }
}

void BleServer::onWrite(NimBLECharacteristic* pChar) {
    if (pChar == nullptr) return;
    if (dataCallback == nullptr) return;

    std::string value = pChar->getValue();
    if (value.empty()) return;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
    size_t len = value.size();
    const char* uuid = pChar->getUUID().toString().c_str();

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 收到手机写入 %u 字节 | UUID=%s\n", (unsigned)len, uuid);
    }

    dataCallback(uuid, data, len);
}
