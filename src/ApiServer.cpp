#include "ApiServer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "ConfigManager.h"
#include "WebPortal.h"
#include "MQTTManager.h"
#include "DeviceManager.h"
#include "DiagnosticsManager.h"
#include "HealthMonitor.h"

extern AsyncWebServer server;

extern ConfigManager configManager;
extern MQTTManager mqttManager;
extern DeviceManager deviceManager;
extern DiagnosticsManager diagnosticsManager;
extern HealthMonitor healthMonitor;

extern String cachedNetworks;
extern std::vector<String> scannedNetworks;

namespace
{
void registerDeviceCommandEndpoint(const char* path, const char* action)
{
    server.on(
        path,
        HTTP_POST,
        [action](AsyncWebServerRequest* request)
        {
            const bool idInBody = request->hasParam("id", true);
            const bool idInQuery = request->hasParam("id");

            if(!idInBody && !idInQuery)
            {
                request->send(400, "application/json", "{\"error\":\"missing id\"}");
                return;
            }

            const String id = request->getParam("id", idInBody)->value();
            if(id.isEmpty())
            {
                request->send(400, "application/json", "{\"error\":\"empty id\"}");
                return;
            }

            if(!deviceManager.deviceExists(id))
            {
                diagnosticsManager.error("API", "Comando solicitado para dispositivo inexistente: " + id);
                request->send(404, "application/json", "{\"error\":\"device not found\"}");
                return;
            }

            if(mqttManager.sendCommand(id, action))
            {
                request->send(200, "application/json", "{\"result\":\"ok\"}");
            }
            else
            {
                request->send(503, "application/json", "{\"error\":\"command publish failed\"}");
            }
        }
    );
}
}

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
            String json =
                "{\"count\":" +
                String(scannedNetworks.size()) +
                ",\"networks\":[";

            for(
                size_t i = 0;
                i < scannedNetworks.size();
                i++
            )
            {
                json +=
                    "\"" +
                    scannedNetworks[i] +
                    "\"";

                if(
                    i < scannedNetworks.size() - 1
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
        "/api/devices",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                deviceManager.getDevicesJson()
            );
        }
    );

    server.on(
        "/api/deviceinfo",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            if(
                !request->hasParam("id")
            )
            {
                request->send(
                    400,
                    "application/json",
                    "{\"error\":\"missing id\"}"
                );

                return;
            }

            String id =
                request
                    ->getParam("id")
                    ->value();

            if(!deviceManager.deviceExists(id))
            {
                request->send(
                    404,
                    "application/json",
                    "{\"error\":\"device not found\"}"
                );

                return;
            }

            request->send(
                200,
                "application/json",
                deviceManager.getDeviceJson(
                    id
                )
            );
        }
    );

    server.on(
        "/api/diagnostics",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                diagnosticsManager
                    .getDiagnosticsJson(
                        mqttManager.isConnected(),
                        WiFi.isConnected()
                    )
            );
        }
    );

    server.on(
        "/api/logs",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                diagnosticsManager
                    .getLogsJson()
            );
        }
    );

    server.on(
        "/api/health",
        HTTP_GET,
        [](AsyncWebServerRequest* request)
        {
            request->send(
                200,
                "application/json",
                healthMonitor.getHealthJson()
            );
        }
    );

    server.on(
        "/api/health/events",
        HTTP_GET,
        [](AsyncWebServerRequest* request)
        {
            request->send(
                200,
                "application/json",
                healthMonitor.getHealthEventsJson()
            );
        }
    );

    registerDeviceCommandEndpoint("/api/device/on", "turn_on");
    registerDeviceCommandEndpoint("/api/device/off", "turn_off");
    registerDeviceCommandEndpoint("/api/device/reboot", "reboot");

    server.on(
        "/api/reboot",
        HTTP_POST,
        [](AsyncWebServerRequest* request)
        {
            diagnosticsManager.warning("API", "Reinicio solicitado por API");
            request->send(200, "application/json", "{\"result\":\"rebooting\"}");
            delay(500);
            ESP.restart();
        }
    );

    server.on(
        "/api/reset",
        HTTP_POST,
        [](AsyncWebServerRequest* request)
        {
            diagnosticsManager.warning("API", "Restablecimiento de configuracion solicitado por API");
            request->send(200, "application/json", "{\"result\":\"resetting\"}");
            configManager.clear();
            delay(500);
            ESP.restart();
        }
    );

    diagnosticsManager.info("API", "API Server iniciado");
}