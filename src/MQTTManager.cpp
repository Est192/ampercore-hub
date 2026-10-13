#include "MQTTManager.h"
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "Telemetry.h"

extern DeviceManager deviceManager;

MQTTManager* MQTTManager::instance = nullptr;

MQTTManager::MQTTManager()
    : mqttClient(wifiClient)
{
    instance = this;
}

void MQTTManager::begin()
{
    uint64_t chipid =
        ESP.getEfuseMac();

    clientId =
        "AMPERCORE-HUB-" +
        String(
            (uint32_t)(chipid >> 24),
            HEX
        );

    mqttClient.setServer(
        broker.c_str(),
        port
    );

    mqttClient.setCallback(
        callback
    );

    Serial.println(
        "MQTT Manager iniciado"
    );
}

bool MQTTManager::connect()
{
    if(mqttClient.connected())
    {
        return true;
    }

    Serial.println(
        "Conectando MQTT..."
    );

    bool result =
        mqttClient.connect(
            clientId.c_str()
        );

    if(result)
    {
        Serial.println(
            "MQTT conectado"
        );

        mqttClient.subscribe(
            "ampercore/discovery"
        );

        mqttClient.subscribe(
            "ampercore/device/#"
        );
    }
    else
    {
        Serial.print(
            "MQTT Error: "
        );

        Serial.println(
            mqttClient.state()
        );
    }

    return result;
}

void MQTTManager::loop()
{
    if(!mqttClient.connected())
    {
        unsigned long now =
            millis();

        if(
            now -
            lastReconnectAttempt >
            5000
        )
        {
            lastReconnectAttempt =
                now;

            connect();
        }
    }

    mqttClient.loop();
}

bool MQTTManager::isConnected()
{
    return mqttClient.connected();
}

bool MQTTManager::publish(
    const String& topic,
    const String& payload
)
{
    return mqttClient.publish(
        topic.c_str(),
        payload.c_str()
    );
}

void MQTTManager::subscribe(
    const String& topic
)
{
    mqttClient.subscribe(
        topic.c_str()
    );
}

String MQTTManager::getBroker()
{
    return broker;
}

int MQTTManager::getPort()
{
    return port;
}

String MQTTManager::extractDeviceIdFromTopic(
    const String& topic
)
{
    const String prefix =
        "ampercore/device/";

    const String suffix =
        "/telemetry";

    if(
        !topic.startsWith(prefix) ||
        !topic.endsWith(suffix)
    )
    {
        return "";
    }

    return topic.substring(
        prefix.length(),
        topic.length() - suffix.length()
    );
}

void MQTTManager::processTelemetryMessage(
    const String& topic,
    const String& payload
)
{
    Serial.println(
        "--- Telemetry Processing ---"
    );
    Serial.print(
        "Received topic: "
    );
    Serial.println(
        topic
    );
    Serial.print(
        "Received payload: "
    );
    Serial.println(
        payload
    );

    JsonDocument doc;
    DeserializationError error =
        deserializeJson(
            doc,
            payload
        );

    Serial.print(
        "deserializeJson() result: "
    );
    Serial.println(
        error.c_str()
    );

    if(error != DeserializationError::Ok)
    {
        return;
    }

    String deviceId =
        extractDeviceIdFromTopic(
            topic
        );

    Serial.print(
        "Extracted DeviceId: "
    );
    Serial.println(
        deviceId
    );

    if(deviceId.length() == 0)
    {
        Serial.println(
            "Topic does not match ampercore/device/<id>/telemetry"
        );
        return;
    }

    Device* device =
        deviceManager.findDevice(
            deviceId
        );

    Serial.print(
        "deviceManager.findDevice() result: "
    );
    Serial.println(
        device != nullptr
            ? "FOUND"
            : "NOT FOUND"
    );

    if(device == nullptr)
    {
        return;
    }

    Serial.printf(
        "Telemetry before update: V=%.2f C=%.2f P=%.2f E=%.2f\n",
        device->telemetry.voltage,
        device->telemetry.current,
        device->telemetry.power,
        device->telemetry.energy
    );

    Telemetry telemetry;
    telemetry.voltage =
        doc["voltage"] | 0.0f;
    telemetry.current =
        doc["current"] | 0.0f;
    telemetry.power =
        doc["power"] | 0.0f;
    telemetry.energy =
        doc["energy"] | 0.0f;
    telemetry.timestamp =
        millis();

    bool updated =
        deviceManager.updateTelemetry(
            deviceId,
            telemetry
        );

    Serial.print(
        "updateTelemetry() result: "
    );
    Serial.println(
        updated ? "true" : "false"
    );

    Serial.printf(
        "Telemetry after update: V=%.2f C=%.2f P=%.2f E=%.2f\n",
        device->telemetry.voltage,
        device->telemetry.current,
        device->telemetry.power,
        device->telemetry.energy
    );
}

void MQTTManager::handleIncomingMessage(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    String topicStr =
        String(topic);

    String message = "";

    for(
        unsigned int i = 0;
        i < length;
        i++
    )
    {
        message +=
            (char)payload[i];
    }

    Serial.println();
    Serial.print(
        "TOPIC: "
    );
    Serial.println(
        topicStr
    );
    Serial.print(
        "PAYLOAD: "
    );
    Serial.println(
        message
    );

    if(
        topicStr ==
        "ampercore/discovery"
    )
    {
        JsonDocument doc;

        DeserializationError error =
            deserializeJson(
                doc,
                message
            );

        Serial.print(
            "Discovery deserializeJson() result: "
        );
        Serial.println(
            error.c_str()
        );

        if(error == DeserializationError::Ok)
        {
            Device device;

            device.id =
                doc["id"]
                .as<String>();

            device.name =
                doc["name"]
                .as<String>();

            device.type =
                doc["type"]
                .as<String>();

            device.state =
                "online";

            deviceManager.addDevice(
                device
            );

            Serial.println(
                "Dispositivo descubierto"
            );
        }

        return;
    }

    if(
        topicStr.startsWith(
            "ampercore/device/"
        ) &&
        topicStr.endsWith(
            "/telemetry"
        )
    )
    {
        processTelemetryMessage(
            topicStr,
            message
        );
    }
}

void MQTTManager::callback(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    if(instance == nullptr)
    {
        return;
    }

    instance->handleIncomingMessage(
        topic,
        payload,
        length
    );
}

void processDiscoveryMessage(
    const String& payload
)
{
    JsonDocument doc;

    DeserializationError error =
        deserializeJson(
            doc,
            payload
        );

    if(error)
    {
        Serial.println(
            "JSON invalido"
        );

        return;
    }

    Device device;

    device.id =
        doc["id"]
        .as<String>();

    device.name =
        doc["name"]
        .as<String>();

    device.type =
        doc["type"]
        .as<String>();

    device.state =
        "online";

    deviceManager.addDevice(
        device
    );
}