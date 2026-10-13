#include <Arduino.h>
#include <WiFi.h>

#include "WifiManager.h"
#include "WebPortal.h"

WifiManager wifiManager;

WebPortal webPortal;

void setup()
{
    Serial.begin(115200);

    wifiManager.startAccessPoint();

    webPortal.begin();

    Serial.println();
    Serial.println("AMPERCORE HUB");
    Serial.println("IP:");
    Serial.println(WiFi.softAPIP());
}

void loop()
{
}