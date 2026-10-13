#include "WifiManager.h"

#include <WiFi.h>

void WifiManager::startAccessPoint()
{
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        "Ampercore_Setup",
        "ampercore123"
    );
}