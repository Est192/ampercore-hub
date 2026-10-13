#include "WebPortal.h"

#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

void WebPortal::begin()
{
    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Ampercore Hub</title>
</head>
<body>

<h1>Ampercore Hub</h1>

<form action="/save">

SSID:<br>
<input type="text" name="ssid"><br><br>

Password:<br>
<input type="password" name="password"><br><br>

Hub Name:<br>
<input type="text" name="hub"><br><br>

<button type="submit">
Guardar
</button>

</form>

</body>
</html>
)rawliteral";

            request->send(
                200,
                "text/html",
                html
            );
        }
    );

    server.begin();
}