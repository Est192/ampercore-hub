#include <Arduino.h>
#include <WiFi.h>

#include "ApiServer.h"
#include "ConfigManager.h"
#include "DeviceManager.h"
#include "DiagnosticsManager.h"
#include "HealthMonitor.h"
#include "MQTTManager.h"
#include "WebPortal.h"
#include "WifiManager.h"

DeviceManager deviceManager;
WifiManager wifiManager;
ConfigManager configManager;
WebPortal webPortal;
ApiServer apiServer;
MQTTManager mqttManager;
DiagnosticsManager diagnosticsManager;
HealthMonitor healthMonitor;

TaskHandle_t wifiScanTaskHandle = nullptr;
unsigned long lastHeartbeat = 0;

/*
 * System flow: initialize diagnostics first so startup and subsystem events use
 * the same sink. WiFi and portal can operate in setup mode; MQTT is enabled
 * when station mode has a connection. The scan task only requests WiFi scans.
 */
void wifiScanTask(void* parameter)
{
    (void)parameter;
    unsigned long lastPeriodicScan = 0;

    while(true)
    {
        const unsigned long now = millis();
        const bool manualScanRequested = takeNetworkScanRequest();
        const bool periodicScanDue =
            wifiManager.isAPMode() && now - lastPeriodicScan >= 30000;

        if(manualScanRequested || periodicScanDue)
        {
            lastPeriodicScan = now;
            scanNetworks();
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void setup()
{
    Serial.begin(115200);
    diagnosticsManager.begin();
    healthMonitor.begin(diagnosticsManager, mqttManager, deviceManager);
    diagnosticsManager.info("SYSTEM", "AMPERCORE HUB Version 0.3.0-alpha iniciando");

    configManager.begin();
    wifiManager.begin();

    bool wifiConnected = false;
    if(configManager.isConfigured())
    {
        diagnosticsManager.info("CONFIG", "Configuracion WiFi encontrada");
        wifiConnected = wifiManager.connectToWiFi(
            configManager.getSSID(),
            configManager.getPassword()
        );

        if(wifiConnected)
        {
            scanNetworks();
        }
    }

    if(!wifiConnected)
    {
        diagnosticsManager.info("SYSTEM", "Iniciando modo de configuracion");
        wifiManager.startAccessPoint();
        scanNetworks();
    }

    const BaseType_t scanTaskResult = xTaskCreatePinnedToCore(
        wifiScanTask,
        "WiFiScanTask",
        4096,
        nullptr,
        1,
        &wifiScanTaskHandle,
        1
    );

    if(scanTaskResult != pdPASS)
    {
        diagnosticsManager.error("SYSTEM", "No se pudo iniciar WiFiScanTask");
    }

    webPortal.begin();
    apiServer.begin();

    if(wifiConnected)
    {
        mqttManager.begin();
        mqttManager.connect();
        diagnosticsManager.info("SYSTEM", "Modo de operacion; IP: " + wifiManager.getLocalIP());
    }
    else
    {
        diagnosticsManager.info("SYSTEM", "Modo de configuracion; IP AP: " + WiFi.softAPIP().toString());
    }

    healthMonitor.noteMqttLoop();
    diagnosticsManager.info("SYSTEM", "Sistema listo");
}

void loop()
{
    mqttManager.loop();
    healthMonitor.loop();

    if(millis() - lastHeartbeat > 30000)
    {
        lastHeartbeat = millis();

        const String payload =
            "{\"heap\":" + String(ESP.getFreeHeap()) +
            ",\"uptime\":" + String(millis() / 1000) + "}";

        if(mqttManager.publish("ampercore/hub/heartbeat", payload))
        {
            if(diagnosticsManager.shouldLogHeartbeat())
            {
                diagnosticsManager.info("SYSTEM", "Heartbeat MQTT enviado");
            }
        }
    }

    if(!wifiManager.isAPMode() && !wifiManager.isConnected())
    {
        diagnosticsManager.warning("WIFI", "Conexion WiFi perdida");
        wifiManager.reconnect();
    }

    delay(5000);
}