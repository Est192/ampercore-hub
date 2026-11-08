#include "MQTTManager.h"

#include <ArduinoJson.h>

#include "DeviceManager.h"
#include "Telemetry.h"
#include "DiagnosticsManager.h"
#include "HealthMonitor.h"

extern DiagnosticsManager diagnosticsManager;
extern DeviceManager deviceManager;
extern HealthMonitor healthMonitor;

/*
 * MQTT flow: connect() installs Discovery and device subscriptions; callback()
 * dispatches incoming Discovery/Telemetry; publish() counts successful
 * outbound packets, including telemetry and Commands. Commands are published
 * to the per-device command topic and Discovery creates DeviceManager entries.
 */
MQTTManager* MQTTManager::instance = nullptr;

namespace
{
bool registerDiscoveredDevice(const String& payload)
{
    JsonDocument doc;
    const DeserializationError parseError = deserializeJson(doc, payload);

    if(parseError)
    {
        diagnosticsManager.warning("MQTT", "Discovery JSON invalido");
        return false;
    }

    Device device;
    device.id = doc["id"].as<String>();
    device.name = doc["name"].as<String>();
    device.type = doc["type"].as<String>();
    device.state = "online";

    if(device.id.isEmpty() || device.name.isEmpty() || device.type.isEmpty())
    {
        diagnosticsManager.warning("MQTT", "Discovery incompleto: se requieren id, name y type");
        return false;
    }

    deviceManager.addDevice(device);
    return true;
}

String commandPayload(const String& action)
{
    JsonDocument document;
    document["action"] = action;

    String payload;
    serializeJson(document, payload);
    return payload;
}

const char* mqttStateDescription(
    int state
)
{
    switch(state)
    {
        case -4:
            return "connection timeout";

        case -3:
            return "connection lost";

        case -2:
            return "network connection failed";

        case -1:
            return "client disconnected";

        case 1:
            return "incorrect MQTT protocol";

        case 2:
            return "rejected client identifier";

        case 3:
            return "broker unavailable";

        case 4:
            return "bad username or password";

        case 5:
            return "not authorized";

        default:
            return "unknown MQTT state";
    }
}
}

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

    diagnosticsManager.info("MQTT", "MQTT Manager iniciado");
}

bool MQTTManager::connect()
{
    if(mqttClient.connected())
    {
        return true;
    }

    if(WiFi.status() != WL_CONNECTED)
    {
        diagnosticsManager.warning(
            "MQTT",
            "MQTT connection skipped: WiFi disconnected"
        );

        return false;
    }

    IPAddress brokerAddress;

    const bool dnsResolved =
        WiFi.hostByName(
            broker.c_str(),
            brokerAddress
        ) == 1;

    if(!dnsResolved)
    {
        diagnosticsManager.error(
            "MQTT",
            "MQTT DNS lookup failed: " +
            broker
        );

        return false;
    }

    mqttClient.setServer(
        brokerAddress,
        static_cast<uint16_t>(
            port
        )
    );

    WiFiClient tcpProbe;

    if(
        !tcpProbe.connect(
            brokerAddress,
            static_cast<uint16_t>(
                port
            )
        )
    )
    {
        diagnosticsManager.error(
            "MQTT",
            "MQTT TCP probe failed"
        );

        tcpProbe.stop();

        return false;
    }

    tcpProbe.stop();

    delay(100);

    mqttClient.setSocketTimeout(8);

    bool result =
        mqttClient.connect(
            clientId.c_str()
        );

    if(result)
    {
        const bool discoverySubscribed = mqttClient.subscribe(
            "ampercore/discovery"
        );

        const bool devicesSubscribed = mqttClient.subscribe(
            "ampercore/device/#"
        );

        if(!discoverySubscribed || !devicesSubscribed)
        {
            diagnosticsManager.error("MQTT", "Fallo al registrar una o mas subscriptions");
        }

        if(hasConnectedOnce)
        {
            diagnosticsManager.incrementMqttReconnects();
        }
        hasConnectedOnce = true;

        diagnosticsManager.info(
            "MQTT",
            "MQTT conectado"
        );

        diagnosticsManager.info(
            "MQTT",
            "MQTT subscriptions registered"
        );
    }
    else
    {
        const int state =
            mqttClient.state();

        diagnosticsManager.error(
            "MQTT",
            "MQTT Error: " +
            String(state) +
            " (" +
            mqttStateDescription(
                state
            ) +
            ")"
        );
    }

    return result;
}

bool MQTTManager::sendCommand(
    const String& deviceId,
    const String& action
)
{
    String topic =
        "ampercore/device/" +
        deviceId +
        "/command";

    const String payload = commandPayload(action);

    const bool published = publish(
        topic,
        payload
    );

    if(published)
    {
        diagnosticsManager.info("MQTT", "Command enviado: " + action + " -> " + deviceId);
    }

    return published;
}

void MQTTManager::loop()
{
    healthMonitor.noteMqttLoop();

    if(WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if(!mqttClient.connected())
    {
        unsigned long now =
            millis();

        if(
            now -
            lastReconnectAttempt >
            15000
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
    if(!mqttClient.connected())
    {
        diagnosticsManager.warning("MQTT", "Publish omitido: cliente desconectado");
        return false;
    }

    const bool published = mqttClient.publish(
        topic.c_str(),
        payload.c_str()
    );

    if(published)
    {
        diagnosticsManager.incrementMqttMessagesSent();
    }
    else
    {
        diagnosticsManager.error("MQTT", "Fallo al publicar en " + topic);
    }

    return published;
}

void MQTTManager::subscribe(
    const String& topic
)
{
    if(!mqttClient.subscribe(
        topic.c_str()
    ))
    {
        diagnosticsManager.warning("MQTT", "Fallo al suscribirse a " + topic);
    }
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
        !topic.startsWith(prefix)
        ||
        !topic.endsWith(suffix)
    )
    {
        return "";
    }

    return topic.substring(
        prefix.length(),
        topic.length() -
        suffix.length()
    );
}

void MQTTManager::processTelemetryMessage(
    const String& topic,
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
        diagnosticsManager.error(
            "MQTT",
            "Telemetry JSON invalido"
        );

        return;
    }

    String deviceId =
        extractDeviceIdFromTopic(
            topic
        );

    if(deviceId.length() == 0)
    {
        diagnosticsManager.warning(
            "MQTT",
            "Telemetry topic invalido: " +
            topic
        );

        return;
    }

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

    deviceManager.updateTelemetry(deviceId, telemetry);
}

void MQTTManager::handleIncomingMessage(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    diagnosticsManager.incrementMqttMessagesReceived();
    healthMonitor.noteMqttMessageReceived();

    String topicStr =
        String(topic);

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

    if(
        topicStr ==
        "ampercore/discovery"
    )
    {
        registerDiscoveredDevice(message);

        return;
    }

    if(
        topicStr.startsWith(
            "ampercore/device/"
        )
        &&
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
    registerDiscoveredDevice(payload);
}