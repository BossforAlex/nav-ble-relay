#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLEAdvertising.h>
#include <functional>
#include "config/Config.h"
#include <esp_task_wdt.h>

class BleServer : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    using DataCallback = std::function<void(const char* uuid, const uint8_t* data, size_t len)>;

    BleServer() = default;

    void begin(const char* deviceName);
    void setDataCallback(DataCallback cb) { dataCallback = cb; }
    void loop();
    bool isConnected() const { return connected; }

    // BLEServerCallbacks
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

    // BLECharacteristicCallbacks
    void onWrite(BLECharacteristic* pCharacteristic) override;

private:
    BLEServer* server = nullptr;
    BLEAdvertising* advertising = nullptr;
    DataCallback dataCallback;
    bool connected = false;
    bool needRestartAdvertising = false;
    unsigned long lastAdvertisingRestartMs = 0;

    struct CharDesc {
        const char* name;
        const char* uuid;
        BLECharacteristic* characteristic;
    };
    CharDesc chars[5] = {
        {"Guide",    BleUUID::CHAR_GUIDE,    nullptr},
        {"DriveWay", BleUUID::CHAR_DRIVE,    nullptr},
        {"Tmc",      BleUUID::CHAR_TMC,      nullptr},
        {"State",    BleUUID::CHAR_STATE,    nullptr},
        {"Location", BleUUID::CHAR_LOCATION, nullptr}
    };
};
