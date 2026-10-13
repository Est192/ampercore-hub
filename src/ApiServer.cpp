#include "ApiServer.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

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
        "/api/reboot",
        HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            request->send(
                200,
                "application/json",
                "{\"result\":\"rebooting\"}"
            );

            delay(1000);

            ESP.restart();
        }
    );

    Serial.println(
        "API Server iniciado"
    );
}