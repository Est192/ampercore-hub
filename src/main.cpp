#include <Arduino.h>
#include <WiFi.h>

#include "WifiManager.h"
#include "WebPortal.h"
#include "ConfigManager.h"

WifiManager wifiManager;
WebPortal webPortal;
ConfigManager configManager;

TaskHandle_t wifiScanTaskHandle = NULL;

void wifiScanTask(void * parameter)
{
    while(true)
    {
        scanNetworks();

        vTaskDelay(
            pdMS_TO_TICKS(30000)
        );
    }
}

void setup()
{
    Serial.begin(115200);

    configManager.begin();

    Serial.println(
        "Iniciando WiFi..."
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

    Serial.println(
        "Iniciando portal..."
    );

    webPortal.begin();

    Serial.println();
    Serial.println(
        "AMPERCORE HUB"
    );

    Serial.print(
        "IP AP: "
    );

    Serial.println(
        WiFi.softAPIP()
    );

    Serial.println();
    Serial.println(
        "CONFIGURACION ACTUAL"
    );

    Serial.println(
        configManager.getSSID()
    );

    Serial.println(
        configManager.getPassword()
    );

    Serial.println(
        configManager.getHubName()
    );
}

void loop()
{
}