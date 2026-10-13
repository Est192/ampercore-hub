#include "WifiManager.h"

#include <WiFi.h>

void WifiManager::begin()
{
    WiFi.mode(WIFI_OFF);

    delay(500);
}

void WifiManager::startAccessPoint()
{
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        "Ampercore_Setup",
        "ampercore123"
    );

    apMode = true;

    Serial.println();
    Serial.println("=== ACCESS POINT ===");

    Serial.print("SSID: ");
    Serial.println("Ampercore_Setup");

    Serial.print("IP: ");
    Serial.println(
        WiFi.softAPIP()
    );
}

bool WifiManager::connectToWiFi(
    const String& ssid,
    const String& password
)
{
    Serial.println();
    Serial.println(
        "Conectando a WiFi..."
    );

    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );

    unsigned long startTime =
        millis();

    while(
        WiFi.status()
        != WL_CONNECTED
        &&
        millis() - startTime
        < 15000
    )
    {
        Serial.print(".");
        delay(500);
    }

    if(
        WiFi.status()
        ==
        WL_CONNECTED
    )
    {
        connectedSSID =
            ssid;

        apMode = false;

        Serial.println();
        Serial.println(
            "WiFi conectado"
        );

        Serial.print(
            "IP: "
        );

        Serial.println(
            WiFi.localIP()
        );

        return true;
    }

    Serial.println();
    Serial.println(
        "Error de conexión"
    );

    return false;
}

bool WifiManager::isConnected()
{
    return
        WiFi.status()
        ==
        WL_CONNECTED;
}

String WifiManager::getLocalIP()
{
    if(isConnected())
    {
        return
            WiFi.localIP()
            .toString();
    }

    return
        WiFi.softAPIP()
        .toString();
}

String WifiManager::getSSID()
{
    return connectedSSID;
}

void WifiManager::reconnect()
{
    if(isConnected())
    {
        return;
    }

    Serial.println(
        "Reconectando..."
    );

    WiFi.reconnect();
}

bool WifiManager::isAPMode()
{
    return apMode;
}