#include "BleServer.h"

// 直接修改 NimBLE host 的安全配置。
// 原因：ESP32 Arduino BLE 2.0.0 库不暴露 setSecurityAuth / setSecurityInitKey 等
// 公共 API，但底层基于 NimBLE，ble_hs_cfg 是公开的 host 全局结构。
extern "C" {
#include "host/ble_hs.h"
#include "host/ble_sm.h"
}

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

    // 在 BLEDevice::init() 之前预先配置 NimBLE 安全策略
    // 关键：BLEDevice::init() 内部会调用 nimble_port_init() -> ble_hs_init()，
    // ble_hs_init() 会立刻把 ble_hs_cfg 复制到 host 内部状态。
    // 因此必须在 init() 之前修改 ble_hs_cfg 才有效。
    ble_hs_cfg.sm_bonding = 0;          // 不持久化绑定
    ble_hs_cfg.sm_mitm = 0;             // 不要求 MITM 保护
    ble_hs_cfg.sm_sc = 0;               // 不要求 LESC（Secure Connection）
    ble_hs_cfg.sm_io_cap = 5;           // BLE_SM_IO_NO_INPUT_NO_OUTPUT
    ble_hs_cfg.sm_our_key_dist = 0;     // 我们不请求对方分发任何密钥
    ble_hs_cfg.sm_their_key_dist = 0;   // 我们也不分发任何密钥
    if (Debug::LOG_SYSTEM) {
        Serial.println("[BLE] 预配置 NimBLE: sm_bonding=0 sm_mitm=0 sm_sc=0");
    }

    // 初始化 BLE 协议栈（内部会调用 ble_hs_init() 复制上面配置）
    BLEDevice::init(deviceName);

    // 发射功率适当调整，确保与各种手机兼容
    BLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);

    // 请求较大 MTU，减少分包
    BLEDevice::setMTU(517);

    // 双重保险：init 之后再次通过 setter 强制设置
    // 这些 setter 会修改 host 内部活动状态（不仅是 ble_hs_cfg）
    ble_sm_set_bonding(0);
    ble_sm_set_mitm(0);
    ble_sm_set_sc(0);

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
