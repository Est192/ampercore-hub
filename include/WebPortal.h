#pragma once

#include <Arduino.h>
#include <vector>
#include <ESPAsyncWebServer.h>

class WebPortal
{
public:
    void begin();
};

extern AsyncWebServer server;

extern String cachedNetworks;
extern std::vector<String> scannedNetworks;

void scanNetworks();