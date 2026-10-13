#include "MQTTManager.h"
#include <ArduinoJson.h>
#include "DeviceManager.h"

extern DeviceManager deviceManager;

MQTTManager::MQTTManager()
    : mqttClient(wifiClient)
{
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

void MQTTManager::callback(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    String message;

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
        topic
    );

    Serial.print(
        "PAYLOAD: "
    );

    Serial.println(
        message
    );

    if(
        String(topic) ==
        "ampercore/discovery"
    )
    {
        JsonDocument doc;

        if(
            deserializeJson(
                doc,
                message
            ) ==
            DeserializationError::Ok
        )
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
    }
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