#include "WebPortal.h"
#include "ConfigManager.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

extern ConfigManager configManager;

String cachedNetworks = "";

/*
 * Escaneo de redes WiFi
 */
void scanNetworks()
{
    Serial.println("Escaneando redes...");

    String tempNetworks = "";

    int count = WiFi.scanNetworks();

    Serial.printf(
        "Redes encontradas: %d\n",
        count
    );

    for (int i = 0; i < count; i++)
    {
        String ssid = WiFi.SSID(i);

        tempNetworks +=
            "<option value=\"" +
            ssid +
            "\">" +
            ssid +
            "</option>";

        Serial.println(ssid);
    }

    if (tempNetworks.length() == 0)
    {
        tempNetworks =
            "<option value=\"\">No se encontraron redes</option>";
    }

    cachedNetworks = tempNetworks;
}

void WebPortal::begin()
{
    /*
     * Pagina principal
     */
    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println("Pagina principal solicitada");

            String html = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Ampercore Hub</title>

<style>

body{
    font-family: Arial, sans-serif;
    max-width: 600px;
    margin: auto;
    padding: 20px;
}

h1{
    color:#1976D2;
}

label{
    display:block;
    margin-top:15px;
}

select,input{
    width:100%;
    padding:10px;
    margin-top:5px;
    margin-bottom:15px;
    box-sizing:border-box;
}

button{
    width:100%;
    padding:12px;
    border:none;
    border-radius:5px;
    color:white;
    font-size:16px;
    cursor:pointer;
}

.save-btn{
    background:#1976D2;
}

.scan-btn{
    background:#43A047;
}

</style>
</head>

<body>

<h1>Ampercore Hub</h1>

<form action="/save" method="GET">

<label>Red WiFi</label>

<select name="ssid">
%NETWORKS%
</select>

<label>Contraseña</label>

<input
    type="password"
    name="password"
    required>

<label>Nombre del Hub</label>

<input
    type="text"
    name="hub"
    value="AmpercoreHub"
    required>

<button
    class="save-btn"
    type="submit">
    Guardar Configuración
</button>

</form>

<br>

<a href="/scan">
<button
    class="scan-btn"
    type="button">
    Actualizar Redes
</button>
</a>

</body>
</html>
)rawliteral";

            html.replace(
                "%NETWORKS%",
                cachedNetworks
            );

            request->send(
                200,
                "text/html",
                html
            );
        }
    );

    /*
     * Actualizar redes
     */
    server.on(
        "/scan",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println(
                "Escaneo manual solicitado"
            );

            scanNetworks();

            request->redirect("/");
        }
    );

    /*
     * Guardar configuracion
     */
    server.on(
        "/save",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String ssid = "";
            String password = "";
            String hub = "";

            if (request->hasParam("ssid"))
            {
                ssid =
                    request
                    ->getParam("ssid")
                    ->value();
            }

            if (request->hasParam("password"))
            {
                password =
                    request
                    ->getParam("password")
                    ->value();
            }

            if (request->hasParam("hub"))
            {
                hub =
                    request
                    ->getParam("hub")
                    ->value();
            }

            Serial.println();
            Serial.println(
                "===== CONFIG GUARDADA ====="
            );

            Serial.println(
                "SSID: " + ssid
            );

            Serial.println(
                "PASSWORD: " + password
            );

            Serial.println(
                "HUB: " + hub
            );

            configManager.saveWifi(
                ssid,
                password,
                hub
            );

            request->send(
                200,
                "text/html",
                R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Configuración Guardada</title>
</head>
<body>

<h1>Configuración Guardada</h1>

<p>
El ESP32 se reiniciará automáticamente.
</p>

</body>
</html>
)rawliteral"
            );

            delay(2000);

            ESP.restart();
        }
    );

    server.begin();

    Serial.println(
        "Portal web iniciado"
    );
}