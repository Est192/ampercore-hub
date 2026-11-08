#include "WifiManager.h"

#include <WiFi.h>

#include "DiagnosticsManager.h"

extern DiagnosticsManager diagnosticsManager;

void WifiManager::begin()
{
    WiFi.mode(WIFI_OFF);

    delay(500);
    diagnosticsManager.info("WIFI", "WiFi Manager iniciado");
}

void WifiManager::startAccessPoint()
{
    WiFi.mode(WIFI_AP);

    if(!WiFi.softAP(
        "Ampercore_Setup",
        "ampercore123"
    ))
    {
        diagnosticsManager.error("WIFI", "No se pudo iniciar el punto de acceso");
        return;
    }

    apMode = true;

    diagnosticsManager.info(
        "WIFI",
        "Punto de acceso Ampercore_Setup iniciado en " + WiFi.softAPIP().toString()
    );
}

bool WifiManager::connectToWiFi(
    const String& ssid,
    const String& password
)
{
    diagnosticsManager.info("WIFI", "Conectando a la red configurada");

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

        diagnosticsManager.info(
            "WIFI",
            "WiFi conectado; IP: " + WiFi.localIP().toString()
        );

        return true;
    }

    diagnosticsManager.error("WIFI", "No se pudo conectar a la red configurada");

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

    diagnosticsManager.incrementWifiReconnects();
    diagnosticsManager.warning("WIFI", "Intentando reconectar WiFi");
    WiFi.reconnect();
}

bool WifiManager::isAPMode()
{
    return apMode;
}