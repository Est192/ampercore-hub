#pragma once

#include <Arduino.h>

class WebPortal
{
public:
    void begin();
};

extern String cachedNetworks;

void scanNetworks();