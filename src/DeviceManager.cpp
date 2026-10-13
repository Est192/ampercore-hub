#include "DeviceManager.h"

void DeviceManager::addDevice(
    const Device& device
)
{
    if(
        deviceExists(
            device.id
        )
    )
    {
        return;
    }

    devices.push_back(
        device
    );

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
    for(
        auto& device :
        devices
    )
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
        auto& device :
        devices
    )
    {
        if(device.id == id)
        {
            return &device;
        }
    }

    return nullptr;
}

int DeviceManager::count()
{
    return devices.size();
}

String DeviceManager::getDevicesJson()
{
    String json =
        "{"
        "\"count\":" +
        String(
            devices.size()
        ) +
        ",\"devices\":[";

    for(
        size_t i = 0;
        i < devices.size();
        i++
    )
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

        if(
            i <
            devices.size() - 1
        )
        {
            json += ",";
        }
    }

    json += "]}";

    return json;
}