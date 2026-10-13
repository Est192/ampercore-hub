#include "WifiManager.h"

#include <WiFi.h>

void WifiManager::startAccessPoint()
{
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        "Ampercore_Setup",
        "ampercore123"
    );

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}