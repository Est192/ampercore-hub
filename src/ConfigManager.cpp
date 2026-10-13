#include "ConfigManager.h"

void ConfigManager::begin()
{
    preferences.begin(
        "ampercore",
        false
    );
}

void ConfigManager::saveWifi(
    const String& ssid,
    const String& password,
    const String& hubName
)
{
    preferences.putString(
        "ssid",
        ssid
    );

    preferences.putString(
        "pass",
        password
    );

    preferences.putString(
        "hub",
        hubName
    );
}

String ConfigManager::getSSID()
{
    return preferences.getString(
        "ssid",
        ""
    );
}

String ConfigManager::getPassword()
{
    return preferences.getString(
        "pass",
        ""
    );
}

String ConfigManager::getHubName()
{
    return preferences.getString(
        "hub",
        "AmpercoreHub"
    );
}

bool ConfigManager::isConfigured()
{
    return getSSID().length() > 0;
}

void ConfigManager::clear()
{
    preferences.clear();
}