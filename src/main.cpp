#include <Arduino.h>
#include <WiFi.h>

#include "WifiManager.h"
#include "WebPortal.h"
#include "ConfigManager.h"

WifiManager wifiManager;
WebPortal webPortal;
ConfigManager configManager;

TaskHandle_t wifiScanTaskHandle = NULL;

/*
 * Tarea de escaneo WiFi
 */
void wifiScanTask(void *parameter)
{
    while (true)
    {
        if (wifiManager.isAPMode())
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
    Serial.println("================================");
    Serial.println("AMPERCORE HUB");
    Serial.println("Version 0.1.3");
    Serial.println("================================");
    Serial.println();

    /*
     * Inicializar configuracion
     */
    configManager.begin();

    /*
     * Inicializar WiFi
     */
    wifiManager.begin();

    Serial.println("CONFIGURACION ACTUAL");

    Serial.print("SSID: ");
    Serial.println(
        configManager.getSSID()
    );

    Serial.print("Hub: ");
    Serial.println(
        configManager.getHubName()
    );

    Serial.println();

    bool wifiConnected = false;

    /*
     * Intentar conectar si existe configuracion
     */
    if (configManager.isConfigured())
    {
        Serial.println(
            "Configuracion encontrada"
        );

        wifiConnected =
            wifiManager.connectToWiFi(
                configManager.getSSID(),
                configManager.getPassword()
            );
    }

    /*
     * Si no conecta levantar AP
     */
    if (!wifiConnected)
    {
        Serial.println();
        Serial.println(
            "Iniciando modo configuracion..."
        );

        wifiManager.startAccessPoint();

        /*
         * Escaneo inicial
         */
        scanNetworks();

        /*
         * Escaneo periodico
         */
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

    /*
     * Portal web siempre disponible
     */
    webPortal.begin();

    Serial.println();

    if (wifiConnected)
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
    /*
     * Reconexion automatica
     */
    if (
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