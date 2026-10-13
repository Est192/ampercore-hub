#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DeviceManager.h"

extern DeviceManager deviceManager;

void processDiscoveryMessage(
    const String& payload
);

class MQTTManager
{
public:

    MQTTManager();

    void begin();

    void loop();

    bool connect();

    bool isConnected();

    bool publish(
        const String& topic,
        const String& payload
    );

    void subscribe(
        const String& topic
    );

    String getBroker();

    int getPort();

private:

    WiFiClient wifiClient;

    PubSubClient mqttClient;

    String broker =
        "broker.hivemq.com";

    int port = 1883;

    String clientId;

    unsigned long lastReconnectAttempt = 0;

    static void callback(
        char* topic,
        byte* payload,
        unsigned int length
    );
};
