#include "BleClient.h"
#include <BLEScan.h>

// ============================================================
// 内部辅助：广告设备回调
// ============================================================
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    explicit AdvertisedDeviceCallbacks(BleClient* client) : mClient(client) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        const std::string addr = advertisedDevice.getAddress().toString();
        const int rssi = advertisedDevice.getRSSI();
        const bool hasName = advertisedDevice.haveName();
        const std::string name = hasName ? advertisedDevice.getName() : "";
        const bool hasSvc = advertisedDevice.haveServiceUUID();

        // 打印所有扫描结果，便于诊断蓝牙名/UUID匹配问题
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 扫描结果: %s RSSI=%d name=%s hasName=%d hasSvc=%d\n",
                          addr.c_str(), rssi,
                          name.empty() ? "(none)" : name.c_str(),
                          hasName ? 1 : 0, hasSvc ? 1 : 0);
        }

        // 只连接名称以 DEVICE_NAME_PREFIX（ICA）开头的 Android 设备
        if (!hasName || name.empty()) return;
        if (name.compare(0, strlen(DEVICE_NAME_PREFIX), DEVICE_NAME_PREFIX) != 0) {
            if (Debug::LOG_SYSTEM) {
                Serial.printf("[BLE] 跳过非目标设备: %s\n", name.c_str());
            }
            return;
        }

        // 注意：服务端（Android GATT Server）不一定 advertise service UUID，
        // 仅靠 name 匹配即可，避免因 hasSvc=false 而漏掉合法目标设备。
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 匹配目标设备: %s (RSSI=%d, hasSvc=%d)\n",
                          addr.c_str(), rssi, hasSvc ? 1 : 0);
        }

        if (mClient->targetMac.empty() || addr == mClient->targetMac) {
            mClient->targetMac = addr;
            mClient->targetAddrType = advertisedDevice.getAddressType();
            mClient->doConnect = true;
            mClient->stopScan();
        }
    }

private:
    BleClient* mClient;
};

// ============================================================
// 内部辅助：连接状态回调
// ============================================================
class ClientCallbacks : public BLEClientCallbacks {
public:
    explicit ClientCallbacks(BleClient* client) : mClient(client) {}

    void onConnect(BLEClient* /*pClient*/) override {
        mClient->connected = true;
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 已连接");
    }

    void onDisconnect(BLEClient* /*pClient*/) override {
        mClient->connected = false;
        mClient->client = nullptr;
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 已断开，将尝试重连");
    }

    void onMtuChanged(BLEClient* pClient, int mtu) override {
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] MTU 协商完成: %d 字节\n", mtu);
        }
    }

private:
    BleClient* mClient;
};

// ============================================================
// BleClient 实现
// ============================================================
BleClient* BleClient::sInstance = nullptr;

void BleClient::begin(const char* deviceName) {
    sInstance = this;

    BLEDevice::init(deviceName);

    // C3 部分核心版本对 P9 发射功率兼容性不佳，降为 P6 确保稳定
    BLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);

    // 短暂延时让 BLE 协议栈完全就绪，避免后续 getScan() 返回空指针
    delay(300);

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 初始化完成，设备名: %s\n", deviceName);
    }

    // 扫描器与回调只创建一次，避免 startScan 反复 new 造成内存碎片
    scan = BLEDevice::getScan();
    if (scan == nullptr) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 获取扫描器失败，将重试");
        return;
    }
    advertisedCallbacks = new AdvertisedDeviceCallbacks(this);
    clientCallbacks = new ClientCallbacks(this);
    scan->setAdvertisedDeviceCallbacks(advertisedCallbacks, false);
    // 降低扫描间隔/窗口，提高部分 C3 核心版本与 Android 广播的兼容性
    scan->setInterval(160);
    scan->setWindow(80);
    scan->setActiveScan(true);

    // 不在 setup() 中立即启动扫描，避免部分 core 版本 startScan 阻塞导致看门狗复位。
    // 扫描由 loop() 在 3 秒后自动开始。
    isScanning = false;
}

void BleClient::startScan() {
    if (isScanning || scan == nullptr) return;

    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 开始扫描...");
    // 本版本 BLEScan::start 返回 BLEScanResults，不能按 bool 判断；
    // 只要没有正在扫描，就直接启动，并通过 isScanning 防止重复调用。
    scan->start(Feature::BLE_SCAN_TIMEOUT_MS / 1000, false);
    isScanning = true;
}

void BleClient::stopScan() {
    if (!isScanning || scan == nullptr) return;
    scan->stop();
    isScanning = false;
}

bool BleClient::connectToServer() {
    if (targetMac.empty()) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 目标 MAC 为空，重新扫描");
        return false;
    }

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 正在连接 %s (type=%d) ...\n",
                      targetMac.c_str(), static_cast<int>(targetAddrType));
    }

    client = BLEDevice::createClient();
    client->setClientCallbacks(clientCallbacks);

    BLEAddress address(targetMac);
    if (!client->connect(address, targetAddrType)) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 连接失败");
        client = nullptr;
        connected = false;
        return false;
    }

    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 连接成功，开始 MTU 协商...");

    // ESP32 Arduino BLE 库不暴露 requestMTU()；MTU 在连接/MTU-change 回调中
    // 异步设置（见 ClientCallbacks::onMtuChanged）。先读取当前协商值。
    int mtu = client->getMTU();
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 当前 MTU: %d 字节（默认=23）\n", mtu);
    }
    if (mtu < 100) {
        if (Debug::LOG_SYSTEM) {
            Serial.println("[BLE] 警告：MTU 较小，JSON 大包可能被截断");
            Serial.println("[BLE] 提示：需确认 Android 端有 GATT_MTU 协商或调高");
        }
    }

    BLERemoteService* service = client->getService(BLEUUID(BleUUID::SERVICE));
    if (!service) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 未找到服务");
        client->disconnect();
        client = nullptr;
        return false;
    }

    subscribeCharacteristics(service);
    return true;
}

void BleClient::subscribeCharacteristics(BLERemoteService* service) {
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

    for (const auto& desc : chars) {
        BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(desc.uuid));
        if (!chr) {
            if (Debug::LOG_SYSTEM) Serial.printf("[BLE] 特征值不存在: %s\n", desc.name);
            continue;
        }
        if (!chr->canNotify()) {
            if (Debug::LOG_SYSTEM) Serial.printf("[BLE] 特征值不支持通知: %s\n", desc.name);
            continue;
        }
        chr->registerForNotify(notifyCallback);
        if (Debug::LOG_SYSTEM) Serial.printf("[BLE] 已订阅: %s\n", desc.name);
    }
}

void BleClient::notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool /*isNotify*/) {
    if (!sInstance || !sInstance->dataCallback) return;
    const char* uuid = pChar->getUUID().toString().c_str();
    sInstance->dataCallback(uuid, pData, length);
}

void BleClient::loop() {
    if (doConnect) {
        doConnect = false;
        if (!connectToServer()) {
            lastReconnectAttemptMs = millis();
            startScan();
        }
    }

    if (!connected && !doConnect && Feature::BLE_AUTO_RECONNECT) {
        unsigned long now = millis();
        if (now - lastReconnectAttemptMs > 3000) {
            lastReconnectAttemptMs = now;
            startScan();
        }
    }
}
