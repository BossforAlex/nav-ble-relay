#include "BleServer.h"

void BleServer::begin(const char* deviceName) {
    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 正在初始化 BLE 协议栈...");
    BLEDevice::init(deviceName);

    // C3 部分核心版本对 P9 发射功率兼容性不佳，降为 P6 确保稳定
    BLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);

    // 配置 BLE 安全：不绑定、无 IO 能力，使用 Just Works 加密，避免 Android 写入时反复检查安全标志
    static BLESecurity bleSecurity;
    bleSecurity.setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
    bleSecurity.setCapability(ESP_IO_CAP_NONE);

    // 短暂延时让 BLE 协议栈就绪，同时喂狗避免初始化耗时触发看门狗复位
    delay(300);
    esp_task_wdt_reset();

    server = BLEDevice::createServer();
    server->setCallbacks(this);

    BLEUUID serviceUuid(BleUUID::SERVICE);
    BLEService* service = server->createService(serviceUuid);

    for (auto& desc : chars) {
        desc.characteristic = service->createCharacteristic(
            BLEUUID(desc.uuid),
            BLECharacteristic::PROPERTY_WRITE
        );
        desc.characteristic->setAccessPermissions(ESP_GATT_PERM_WRITE);
        desc.characteristic->setCallbacks(this);
    }

    service->start();

    advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);

    if (Debug::LOG_SYSTEM) {
        Serial.printf("[BLE] GATT Server 已启动，设备名: %s\n", deviceName);
        Serial.println("[BLE] 开始广播，等待手机连接...");
    }

    BLEDevice::startAdvertising();
    esp_task_wdt_reset();
}

void BleServer::onConnect(BLEServer* /*pServer*/) {
    connected = true;
    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 手机已连接");
}

void BleServer::onDisconnect(BLEServer* /*pServer*/) {
    connected = false;
    needRestartAdvertising = true;
    lastAdvertisingRestartMs = millis();
    if (Debug::LOG_SYSTEM) Serial.println("[BLE] 手机已断开，将重新广播");
}

void BleServer::onWrite(BLECharacteristic* pCharacteristic) {
    if (!dataCallback) return;

    std::string uuid = pCharacteristic->getUUID().toString();
    std::string value = pCharacteristic->getValue();
    dataCallback(uuid.c_str(),
                 reinterpret_cast<const uint8_t*>(value.data()),
                 value.length());
}

void BleServer::loop() {
    if (needRestartAdvertising && millis() - lastAdvertisingRestartMs > 500) {
        needRestartAdvertising = false;
        if (Debug::LOG_SYSTEM) Serial.println("[BLE] 重新启动广播");
        BLEDevice::startAdvertising();
    }
}
