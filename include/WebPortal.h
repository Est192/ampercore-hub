#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
extern String cachedNetworks;

class WebPortal
{
public:
    void begin();
};

extern String cachedNetworks;

void scanNetworks();