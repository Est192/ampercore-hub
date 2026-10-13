#pragma once

#include <Arduino.h>
#include <Preferences.h>

class ConfigManager
{
private:
    Preferences preferences;

public:
    void begin();

    void saveWifi(
        const String& ssid,
        const String& password,
        const String& hubName
    );

    String getSSID();

    String getPassword();

    String getHubName();

    bool isConfigured();

    void clear();
};