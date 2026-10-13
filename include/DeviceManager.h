#pragma once

#include <Arduino.h>
#include <vector>

struct Device
{
    String id;
    String name;
    String type;
    String state;
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

    int count();

    String getDevicesJson();

private:

    std::vector<Device> devices;
};