#include "DeviceManager.h"

void DeviceManager::addDevice(
    const Device& device
)
{
    if(deviceExists(device.id))
    {
        return;
    }

    devices.push_back(device);

    Serial.print(
        "Dispositivo registrado: "
    );

    Serial.println(
        device.name
    );
}

bool DeviceManager::deviceExists(
    const String& id
)
{
    for(auto& device : devices)
    {
        if(device.id == id)
        {
            return true;
        }
    }

    return false;
}

bool DeviceManager::removeDevice(
    const String& id
)
{
    for(
        auto it = devices.begin();
        it != devices.end();
        ++it
    )
    {
        if(it->id == id)
        {
            devices.erase(it);

            return true;
        }
    }

    return false;
}

Device* DeviceManager::findDevice(
    const String& id
)
{
    for(
        size_t i = 0;
        i < devices.size();
        ++i
    )
    {
        if(devices[i].id == id)
        {
            return &devices[i];
        }
    }

    return nullptr;
}

bool DeviceManager::updateTelemetry(
    const String& id,
    const Telemetry& telemetry
)
{
    Device* device =
        findDevice(id);

    if(device == nullptr)
    {
        Serial.print(
            "updateTelemetry() failed: device not found -> "
        );
        Serial.println(
            id
        );
        return false;
    }

    device->telemetry =
        telemetry;

    Serial.print(
        "Telemetry saved for device "
    );
    Serial.println(
        id
    );

    return true;
}

int DeviceManager::count()
{
    return devices.size();
}

String DeviceManager::getDevicesJson()
{
    String json =
        "{\"count\":" +
        String(devices.size()) +
        ",\"devices\":[";

    for(size_t i = 0;
        i < devices.size();
        i++)
    {
        json +=
            "{"
            "\"id\":\"" +
            devices[i].id +
            "\","
            "\"name\":\"" +
            devices[i].name +
            "\","
            "\"type\":\"" +
            devices[i].type +
            "\","
            "\"state\":\"" +
            devices[i].state +
            "\""
            "}";

        if(i < devices.size() - 1)
        {
            json += ",";
        }
    }

    json += "]}";

    return json;
}

String DeviceManager::getDeviceJson(
    const String& id
)
{
    Device* device =
        findDevice(id);

    if(device == nullptr)
    {
        return "{}";
    }

    String json = "{";

    json +=
        "\"id\":\"" +
        device->id +
        "\"";

    json +=
        ",\"name\":\"" +
        device->name +
        "\"";

    json +=
        ",\"type\":\"" +
        device->type +
        "\"";

    json +=
        ",\"state\":\"" +
        device->state +
        "\"";

    json +=
        ",\"voltage\":" +
        String(device->telemetry.voltage);

    json +=
        ",\"current\":" +
        String(device->telemetry.current);

    json +=
        ",\"power\":" +
        String(device->telemetry.power);

    json +=
        ",\"energy\":" +
        String(device->telemetry.energy);

    json += "}";

    return json;
}