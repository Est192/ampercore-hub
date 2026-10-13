#include <Arduino.h>
#include <WiFi.h>
#include "WifiManager.h"
#include "ConfigManager.h"
#include "WebPortal.h"
#include "ApiServer.h"
#include "MQTTManager.h"
#include "DeviceManager.h"

DeviceManager deviceManager;
WifiManager wifiManager;
ConfigManager configManager;
WebPortal webPortal;
ApiServer apiServer;
MQTTManager mqttManager;
TaskHandle_t wifiScanTaskHandle = NULL;

unsigned long lastHeartbeat = 0;

void wifiScanTask(void * parameter)
{
    while(true)
    {
        if(wifiManager.isAPMode())
        {
            scanNetworks();
        }

        vTaskDelay(
            pdMS_TO_TICKS(30000)
        );
    }
}

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println(
        "================================"
    );
    Serial.println(
        "AMPERCORE HUB"
    );
    Serial.println(
        "Version 0.2.0"
    );
    Serial.println(
        "================================"
    );
    Serial.println();

    configManager.begin();

    wifiManager.begin();

    bool wifiConnected = false;

    Serial.println(
        "CONFIGURACION ACTUAL"
    );

    Serial.print("SSID: ");
    Serial.println(
        configManager.getSSID()
    );

    Serial.print("Hub: ");
    Serial.println(
        configManager.getHubName()
    );

    if(configManager.isConfigured())
    {
        Serial.println();
        Serial.println(
            "Configuracion encontrada"
        );

        wifiConnected =
            wifiManager.connectToWiFi(
                configManager.getSSID(),
                configManager.getPassword()
            );

        if (wifiConnected)
        {
            Serial.println(
                "Escaneo inicial de redes..."
            );
            scanNetworks();
        }
    }

    if(!wifiConnected)
    {
        Serial.println();
        Serial.println(
            "Iniciando modo configuracion"
        );

        wifiManager.startAccessPoint();

        scanNetworks();

        xTaskCreatePinnedToCore(
            wifiScanTask,
            "WiFiScanTask",
            4096,
            NULL,
            1,
            &wifiScanTaskHandle,
            1
        );
    }

    webPortal.begin();

    apiServer.begin();

      if(wifiConnected)
      {
          mqttManager.begin();

          mqttManager.connect();
      }

    Serial.println();

    if(wifiConnected)
    {
        Serial.println(
            "MODO OPERACION"
        );

        Serial.print("SSID: ");
        Serial.println(
            wifiManager.getSSID()
        );

        Serial.print("IP: ");
        Serial.println(
            wifiManager.getLocalIP()
        );
    }
    else
    {
        Serial.println(
            "MODO CONFIGURACION"
        );

        Serial.print("IP AP: ");
        Serial.println(
            WiFi.softAPIP()
        );
    }

    Serial.println();
    Serial.println(
        "Sistema listo"
    );
}

void loop()
{
  mqttManager.loop();

  if(
    millis() - lastHeartbeat >
    30000
    )
    {
        lastHeartbeat = millis();

        String payload =
            "{"
            "\"heap\":" +
            String(
                ESP.getFreeHeap()
            ) +
            ",\"uptime\":" +
            String(
                millis()/1000
            ) +
            "}";

        mqttManager.publish(
            "ampercore/hub/heartbeat",
            payload
        );

        Serial.println(
            "Heartbeat MQTT enviado"
        );
    }

    if(
        !wifiManager.isAPMode()
        &&
        !wifiManager.isConnected()
    )
    {
        Serial.println(
            "Conexion perdida"
        );

        wifiManager.reconnect();
    }

    delay(5000);
}