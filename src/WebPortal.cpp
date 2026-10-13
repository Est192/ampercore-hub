#include "WebPortal.h"
#include "ConfigManager.h"

#include <WiFi.h>

extern ConfigManager configManager;

AsyncWebServer server(80);

String cachedNetworks = "";

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
            "<option>No se encontraron redes</option>";
    }

    cachedNetworks = tempNetworks;
}

void WebPortal::begin()
{
    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            Serial.println(
                "Pagina principal solicitada"
            );

            String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>Ampercore Hub</title>

<style>

body{
    font-family:Arial,sans-serif;
    max-width:700px;
    margin:auto;
    padding:20px;
}

h1{
    color:#1976D2;
}

.card{
    border:1px solid #ddd;
    padding:15px;
    border-radius:10px;
    margin-bottom:20px;
}

label{
    display:block;
    margin-top:10px;
}

select,input{
    width:100%;
    padding:10px;
    box-sizing:border-box;
    margin-top:5px;
    margin-bottom:10px;
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
    background:#388E3C;
}

.api-btn{
    background:#F57C00;
}

</style>

</head>

<body>

<h1>Ampercore Hub</h1>

<div class="card">

<h2>Estado del Hub</h2>

<p>
<strong>WiFi conectado:</strong>
WIFI_NAME
</p>

<p>
<strong>IP:</strong>
HUB_IP
</p>

<p>
<strong>Nombre:</strong>
HUB_NAME
</p>

</div>

<div class="card">

<h2>Configuración WiFi</h2>

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
    value="HUB_NAME"
    required>

<button
    class="save-btn"
    type="submit">
    Guardar Configuración
</button>

</form>

</div>

<div class="card">

<a href="/scan">
<button
    class="scan-btn"
    type="button">
    Actualizar Redes
</button>
</a>

</div>

<div class="card">

<h2>API REST</h2>

<ul>
<li>/api/status</li>
<li>/api/config</li>
<li>/api/reboot</li>
</ul>

</div>

</body>
</html>
)rawliteral";

            html.replace(
                "%NETWORKS%",
                cachedNetworks
            );

            html.replace(
                "HUB_NAME",
                configManager.getHubName()
            );

            html.replace(
                "WIFI_NAME",
                WiFi.SSID()
            );

            html.replace(
                "HUB_IP",
                WiFi.localIP().toString()
            );

            request->send(
                200,
                "text/html",
                html
            );
        });

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
        });

    server.on(
        "/save",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String ssid = "";
            String password = "";
            String hub = "";

            if(request->hasParam("ssid"))
                ssid =
                    request->getParam(
                        "ssid"
                    )->value();

            if(request->hasParam("password"))
                password =
                    request->getParam(
                        "password"
                    )->value();

            if(request->hasParam("hub"))
                hub =
                    request->getParam(
                        "hub"
                    )->value();

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
</head>

<body>

<h1>Configuración Guardada</h1>

<p>
Reiniciando dispositivo...
</p>

</body>
</html>
)rawliteral"
            );

            delay(2000);

            ESP.restart();
        });

    server.begin();

    Serial.println(
        "Portal web iniciado"
    );
}