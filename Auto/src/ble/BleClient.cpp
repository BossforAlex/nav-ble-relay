#include "BleClient.h"
#include <BLEScan.h>

// ============================================================
// 内部辅助：广告设备回调
// ============================================================
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    explicit AdvertisedDeviceCallbacks(BleClient* client) : mClient(client) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (Debug::LOG_SYSTEM) {
            Serial.printf("[BLE] 发现设备: %s  RSSI=%d\n",
                          advertisedDevice.getAddress().toString().c_str(),
                          advertisedDevice.getRSSI());
        }

        bool match = false;
        if (advertisedDevice.haveServiceUUID() &&
            advertisedDevice.isAdvertisingService(BLEUUID(BleUUID::SERVICE))) {
            match = true;
        }

        if (!match) return;

        if (mClient->targetMac.empty() || advertisedDevice.getAddress().toString() == mClient->targetMac) {
            BLEDevice::getScan()->stop();
            mClient->targetMac = advertisedDevice.getAddress().toString();
            mClient->doConnect = true;
            if (Debug::LOG_SYSTEM) {
                Serial.printf("[BLE] 匹配目标设备: %s\n", mClient->targetMac.c_str());
            }
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
    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 初始化完成，设备名: %s\n", deviceName);
    }
    startScan();
}

void BleClient::startScan() {
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this), false);
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    // 参数：扫描持续秒数，是否继续上次扫描
    scan->start(Feature::BLE_SCAN_TIMEOUT_MS / 1000, false);
    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 开始扫描...");
}

bool BleClient::connectToServer() {
    if (targetMac.empty()) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 目标 MAC 为空，重新扫描");
        return false;
    }

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] 正在连接 %s ...\n", targetMac.c_str());
    }

    client = BLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks(this));

    BLEAddress address(targetMac);
    if (!client->connect(address)) {
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 连接失败");
        client = nullptr;
        connected = false;
        return false;
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
