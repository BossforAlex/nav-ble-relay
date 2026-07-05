#include "BleServer.h"

/**
 * @file BleServer.cpp
 * @brief ESP32 BLE GATT Server 实现
 *
 * ESP32 作为 GATT Server（外设），广播 AutoNavDisplay 名称，
 * 等待手机（Flutter GATT Client）连接并写入 JSON 数据。
 *
 * 特征值方向：WRITE | WRITE_NO_RESPONSE
 *   - 手机连接后通过 writeCharacteristic 推送数据
 *   - ESP32 端在 onWrite 中接收并通过 dataCallback 上报
 *
 * 用户需求：手机端做 MAC 白名单过滤，ESP32 端不做限制。
 */

// ============================================================
// 内部辅助：连接状态回调类
// ============================================================
class ServerCallbacks : public BLEServerCallbacks {
public:
    explicit ServerCallbacks(BleServer* parent) : mParent(parent) {}

    void onConnect(BLEServer* pServer) override {
        mParent->onConnect(pServer);
    }

    void onDisconnect(BLEServer* pServer) override {
        mParent->onDisconnect(pServer);
    }

private:
    BleServer* mParent;
};

// ============================================================
// 内部辅助：特征值写入回调类
// ============================================================
class CharWriteCallbacks : public BLECharacteristicCallbacks {
public:
    explicit CharWriteCallbacks(BleServer* parent) : mParent(parent) {}

    void onWrite(BLECharacteristic* pChar) override {
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

    // 初始化 BLE 协议栈
    BLEDevice::init(deviceName);

    // 发射功率适当调整，确保与各种手机兼容
    BLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);

    // 关闭 BLE 安全 / 配对 / 加密要求
    // 关键：ESP32 Arduino BLE 默认要求 Secure Connection (LESC) 配对，
    // 很多手机 / Android 版本会触发 SEC_REQ_EVT 协商但不接受 LESC，
    // 导致 smp_calculate_link_key_from_long_term_key 失败，写入被拒。
    // 关闭后可无加密通信，简化连接（用户场景：本地局域网近距离）。
    BLEDevice::setSecurityAuth(false, false, false);
    // 不分发任何密钥（init=0 表示不请求对方分发任何加密密钥）
    BLEDevice::setSecurityInitKey(0);
    BLEDevice::setSecurityRespKey(0);
    // IO 能力设为 0x05 = BLE_SM_IO_NO_INPUT_NO_OUTPUT，避免配对时弹出配对码
    // 注：使用数值常量以兼容不同 NimBLE 版本的命名差异
    BLEDevice::setSecurityIOCap(0x05);

    // 请求较大 MTU，减少分包
    BLEDevice::setMTU(517);

    // 短暂延时让协议栈就绪
    delay(200);

    // 创建 GATT Server
    server = BLEDevice::createServer();
    if (server == nullptr) {
        Serial.println("[BLE] 创建 GATT Server 失败");
        return;
    }
    ServerCallbacks* serverCb = new ServerCallbacks(this);
    server->setCallbacks(serverCb);

    // 双重保险：在 server 上也设置一次（部分库版本对 setSecurityAuth 的响应时序不同）
    server->setSecurityAuth(false, false, false);

    // 创建主服务
    service = server->createService(BLEUUID(BleUUID::SERVICE));

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
        BLECharacteristic* chr = service->createCharacteristic(
            BLEUUID(desc.uuid),
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
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
    BLEAdvertising* adv = server->getAdvertising();
    adv->addServiceUUID(BLEUUID(BleUUID::SERVICE));
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);  // iPhone 兼容
    adv->setMaxPreferred(0x12);
    adv->start();

    started = true;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] GATT Server 已启动，设备名=%s，关闭加密，等待手机连接...\n",
                      deviceName);
    }
}

void BleServer::loop() {
    // 当前无需在 loop 中做特殊处理
    // 连接状态由回调更新；数据由 onWrite 回调上报
}

void BleServer::onConnect(BLEServer* /*pServer*/) {
    connectedDeviceCount++;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 手机已连接（当前连接数: %d）\n", connectedDeviceCount);
    }
}

void BleServer::onDisconnect(BLEServer* /*pServer*/) {
    if (connectedDeviceCount > 0) connectedDeviceCount--;
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 手机已断开（当前连接数: %d）\n", connectedDeviceCount);
    }
    // 断开后继续广播，允许其他手机连接
    if (server != nullptr && started) {
        BLEAdvertising* adv = server->getAdvertising();
        if (adv != nullptr) {
            adv->start();
        }
    }
}

void BleServer::onWrite(BLECharacteristic* pChar) {
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
