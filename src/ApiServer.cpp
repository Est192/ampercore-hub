#include "ApiServer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "ConfigManager.h"
#include "WebPortal.h"
#include "MQTTManager.h"

extern AsyncWebServer server;
extern ConfigManager configManager;
extern MQTTManager mqttManager;

extern String cachedNetworks;
extern std::vector<String> scannedNetworks;

void ApiServer::begin()
{
    server.on(
        "/api/status",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String json =
                "{"
                "\"status\":\"online\","
                "\"ip\":\"" +
                WiFi.localIP().toString() +
                "\","
                "\"ssid\":\"" +
                WiFi.SSID() +
                "\""
                "}";

            request->send(
                200,
                "application/json",
                json
            );
        }
    );

    server.on(
        "/api/config",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String json =
                "{"
                "\"hubName\":\"" +
                configManager.getHubName() +
                "\","
                "\"ssid\":\"" +
                configManager.getSSID() +
                "\","
                "\"ip\":\"" +
                WiFi.localIP().toString() +
                "\""
                "}";

            request->send(
                200,
                "application/json",
                json
            );
        }
    );

    server.on(
        "/api/networks",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String json = "{";

            json +=
                "\"count\":" +
                String(
                    scannedNetworks.size()
                );

            json +=
                ",\"networks\":[";

            for (
                size_t i = 0;
                i < scannedNetworks.size();
                i++
            )
            {
                json +=
                    "\"" +
                    scannedNetworks[i] +
                    "\"";

                if (
                    i <
                    scannedNetworks.size() - 1
                )
                {
                    json += ",";
                }
            }

            json += "]}";

            request->send(
                200,
                "application/json",
                json
            );
        }
    );

    server.on(
        "/api/device",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            uint64_t chipid =
                ESP.getEfuseMac();

            String hubId =
                "AMP-HUB-" +
                String(
                    (uint32_t)(
                        chipid >> 24
                    ),
                    HEX
                );

            String json =
                "{"
                "\"hubId\":\"" +
                hubId +
                "\","
                "\"chip\":\"ESP32\","
                "\"cores\":2,"
                "\"flashMB\":4,"
                "\"heap\":" +
                String(
                    ESP.getFreeHeap()
                ) +
                ",\"uptime\":" +
                String(
                    millis() / 1000
                ) +
                "}";

            request->send(
                200,
                "application/json",
                json
            );
        }
    );

    server.on(
        "/api/mqtt",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String json = "{";

            json += "\"connected\":";
            json +=
                mqttManager.isConnected()
                    ? "true"
                    : "false";

            json +=
                ",\"broker\":\"" +
                mqttManager.getBroker() +
                "\"";

            json +=
                ",\"port\":" +
                String(
                    mqttManager.getPort()
                );

            json += "}";

            request->send(
                200,
                "application/json",
                json
            );
        }
    );

    server.on(
        "/api/reboot",
        HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                "{\"result\":\"rebooting\"}"
            );

            delay(500);

            ESP.restart();
        }
    );

    server.on(
        "/api/reset",
        HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                "{\"result\":\"resetting\"}"
            );

            configManager.clear();

            delay(500);

            ESP.restart();
        }
    );

    Serial.println(
        "API Server iniciado"
    );
}
