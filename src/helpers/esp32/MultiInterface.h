#pragma once
#include "helpers/BaseSerialInterface.h"

class MultiInterface : public BaseSerialInterface {
public:
    BaseSerialInterface* ble = nullptr;
    BaseSerialInterface* wifi = nullptr;

    enum class Mode : uint8_t { BLE, WIFI };
    Mode mode = Mode::BLE;

    void setBLE(BaseSerialInterface* iface) { ble = iface; }
    void setWiFi(BaseSerialInterface* iface) { wifi = iface; }

    void setMode(Mode m) { mode = m; }

    BaseSerialInterface* active() const {
        if (mode == Mode::BLE) return ble;
        return wifi;
    }

    // REQUIRED BY BaseSerialInterface
    void enable() override {
        if (active()) active()->enable();
    }

    void disable() override {
        if (active()) active()->disable();
    }

    bool isEnabled() const override {
        return active() ? active()->isEnabled() : false;
    }

    bool isConnected() const override {
        return active() ? active()->isConnected() : false;
    }

    bool isWriteBusy() const override {
        return active() ? active()->isWriteBusy() : false;
    }

    size_t writeFrame(const uint8_t src[], size_t len) override {
        return active() ? active()->writeFrame(src, len) : 0;
    }

    size_t checkRecvFrame(uint8_t dest[]) override {
        return active() ? active()->checkRecvFrame(dest) : 0;
    }
};
