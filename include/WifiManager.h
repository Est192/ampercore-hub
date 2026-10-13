#pragma once

#include <Arduino.h>

class WifiManager
{
public:

    /*
     * Inicialización general
     */
    void begin();

    /*
     * Portal de configuración
     */
    void startAccessPoint();

    /*
     * Conexión a red doméstica
     */
    bool connectToWiFi(
        const String& ssid,
        const String& password
    );

    /*
     * Estado actual
     */
    bool isConnected();

    /*
     * Información
     */
    String getLocalIP();

    String getSSID();

    /*
     * Recuperación
     */
    void reconnect();

    /*
     * Estado AP
     */
    bool isAPMode();

private:

    bool apMode = false;

    String connectedSSID = "";
};