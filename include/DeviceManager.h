#pragma once

#include <Arduino.h>
#include <vector>

#include "Device.h"

struct DeviceHealthSnapshot
{
    String id;
    unsigned long lastSeen;
    bool hasTelemetry;
};

class DeviceManager
{
public:

    void addDevice(
        const Device& device
    );

    bool deviceExists(
        const String& id
    );

    bool removeDevice(
        const String& id
    );

    Device* findDevice(
        const String& id
    );

    bool updateTelemetry(
        const String& id,
        const Telemetry& telemetry
    );

    int count();

    String getDevicesJson();

    String getDeviceJson(
        const String& id
    );

    std::vector<DeviceHealthSnapshot> getHealthSnapshot();

private:

    std::vector<Device> devices;
};