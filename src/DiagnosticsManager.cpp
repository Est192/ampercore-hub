#include "DiagnosticsManager.h"

#include <LittleFS.h>
#include <time.h>

namespace
{
const char* levelName(LogLevel level)
{
    switch(level)
    {
        case LOG_INFO:
            return "INFO";
        case LOG_WARNING:
            return "WARNING";
        case LOG_ERROR:
            return "ERROR";
        case LOG_CRITICAL:
            return "CRITICAL";
        default:
            return "INFO";
    }
}

void appendJsonString(String& output, const String& value)
{
    output += '"';

    for(size_t i = 0; i < value.length(); ++i)
    {
        const char character = value[i];

        switch(character)
        {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if(static_cast<uint8_t>(character) < 0x20)
                {
                    output += ' ';
                }
                else
                {
                    output += character;
                }
                break;
        }
    }

    output += '"';
}

String fileTimestamp(unsigned long timestamp)
{
    const time_t currentTime = time(nullptr);

    if(currentTime > 1600000000)
    {
        struct tm timeInfo;
        if(gmtime_r(&currentTime, &timeInfo) != nullptr)
        {
            char formatted[24];
            if(strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%S", &timeInfo) > 0)
            {
                return String(formatted);
            }
        }
    }

    return "uptime-ms=" + String(timestamp);
}
}

void DiagnosticsManager::begin()
{
    logMutex = xSemaphoreCreateMutex();
    storageMutex = xSemaphoreCreateMutex();

    if(logMutex == nullptr || storageMutex == nullptr)
    {
        error("SYSTEM", "No se pudieron crear los mutex de DiagnosticsManager");
    }

    filesystemReady = LittleFS.begin(true);
    if(filesystemReady && !LittleFS.exists("/logs"))
    {
        filesystemReady = LittleFS.mkdir("/logs");
    }

    if(!filesystemReady)
    {
        error("SYSTEM", "LittleFS no disponible; se conserva logging en consola y RAM");
    }

    info("SYSTEM", "DiagnosticsManager iniciado");
}

void DiagnosticsManager::takeLogMutex()
{
    if(logMutex != nullptr)
    {
        xSemaphoreTake(logMutex, portMAX_DELAY);
    }
}

void DiagnosticsManager::giveLogMutex()
{
    if(logMutex != nullptr)
    {
        xSemaphoreGive(logMutex);
    }
}

void DiagnosticsManager::takeStorageMutex()
{
    if(storageMutex != nullptr)
    {
        xSemaphoreTake(storageMutex, portMAX_DELAY);
    }
}

void DiagnosticsManager::giveStorageMutex()
{
    if(storageMutex != nullptr)
    {
        xSemaphoreGive(storageMutex);
    }
}

void DiagnosticsManager::addLog(
    LogLevel level,
    const String& module,
    const String& message
)
{
    LogEntry entry;
    entry.level = levelName(level);
    entry.module = module;
    entry.message = message;
    entry.timestamp = millis();

    takeLogMutex();
    const size_t index = (logStart + logCount) % MAX_LOG_ENTRIES;
    logs[index] = entry;

    if(logCount < MAX_LOG_ENTRIES)
    {
        ++logCount;
    }
    else
    {
        logStart = (logStart + 1) % MAX_LOG_ENTRIES;
    }

    if(level == LOG_ERROR || level == LOG_CRITICAL)
    {
        ++errorCount;
    }
    else if(level == LOG_WARNING)
    {
        ++warningCount;
    }

    giveLogMutex();

    Serial.printf("[%s] %s\n", entry.level.c_str(), entry.message.c_str());

    if(filesystemReady && !persistLog(entry))
    {
        takeLogMutex();
        ++errorCount;
        const bool reportFailure = !storageFailureLogged;
        storageFailureLogged = true;
        giveLogMutex();

        if(reportFailure)
        {
            Serial.println("[ERROR] SYSTEM No se pudo persistir un evento en LittleFS");
        }
    }
}

bool DiagnosticsManager::persistLog(const LogEntry& entry)
{
    takeStorageMutex();

    const String timestamp = fileTimestamp(entry.timestamp);
    File systemFile = LittleFS.open("/logs/system.log", FILE_APPEND);
    File agentFile = LittleFS.open("/logs/agent.log", FILE_APPEND);

    const bool filesOpened = systemFile && agentFile;
    bool writeSucceeded = filesOpened;

    if(filesOpened)
    {
        systemFile.print(timestamp);
        systemFile.print('\n');
        systemFile.print(entry.level);
        systemFile.print('\n');
        systemFile.print(entry.module);
        systemFile.print('\n');
        systemFile.print(entry.message);
        systemFile.print('\n');

        agentFile.print("{\"timestamp\":");
        agentFile.print(entry.timestamp);
        agentFile.print(",\"level\":");
        String encoded;
        appendJsonString(encoded, entry.level);
        agentFile.print(encoded);
        agentFile.print(",\"module\":");
        encoded = "";
        appendJsonString(encoded, entry.module);
        agentFile.print(encoded);
        agentFile.print(",\"message\":");
        encoded = "";
        appendJsonString(encoded, entry.message);
        agentFile.print(encoded);
        agentFile.print("}\n");

        writeSucceeded = !systemFile.getWriteError() && !agentFile.getWriteError();
    }

    if(systemFile)
    {
        systemFile.close();
    }
    if(agentFile)
    {
        agentFile.close();
    }

    giveStorageMutex();
    return writeSucceeded;
}

void DiagnosticsManager::info(const String& module, const String& message)
{
    addLog(LOG_INFO, module, message);
}

void DiagnosticsManager::warning(const String& module, const String& message)
{
    addLog(LOG_WARNING, module, message);
}

void DiagnosticsManager::error(const String& module, const String& message)
{
    addLog(LOG_ERROR, module, message);
}

void DiagnosticsManager::critical(const String& module, const String& message)
{
    addLog(LOG_CRITICAL, module, message);
}

void DiagnosticsManager::info(const String& message)
{
    info("SYSTEM", message);
}

void DiagnosticsManager::warning(const String& message)
{
    warning("SYSTEM", message);
}

void DiagnosticsManager::error(const String& message)
{
    error("SYSTEM", message);
}

void DiagnosticsManager::critical(const String& message)
{
    critical("SYSTEM", message);
}

void DiagnosticsManager::incrementMqttMessagesReceived()
{
    takeLogMutex();
    ++mqttMessagesReceived;
    giveLogMutex();
}

void DiagnosticsManager::incrementMqttMessagesSent()
{
    takeLogMutex();
    ++mqttMessagesSent;
    giveLogMutex();
}

void DiagnosticsManager::incrementMqttReconnects()
{
    takeLogMutex();
    ++mqttReconnects;
    giveLogMutex();
}

void DiagnosticsManager::incrementWifiReconnects()
{
    takeLogMutex();
    ++wifiReconnects;
    giveLogMutex();
}

void DiagnosticsManager::setRegisteredDevices(uint32_t count)
{
    takeLogMutex();
    registeredDevices = count;
    giveLogMutex();
}

String DiagnosticsManager::getLogsJson()
{
    takeLogMutex();

    String json;
    json.reserve(32 + logCount * 96);
    json = "{\"count\":";
    json += String(logCount);
    json += ",\"logs\":[";

    for(size_t i = 0; i < logCount; ++i)
    {
        const LogEntry& entry = logs[(logStart + i) % MAX_LOG_ENTRIES];
        if(i > 0)
        {
            json += ',';
        }

        json += "{\"timestamp\":";
        json += String(entry.timestamp);
        json += ",\"level\":";
        appendJsonString(json, entry.level);
        json += ",\"module\":";
        appendJsonString(json, entry.module);
        json += ",\"message\":";
        appendJsonString(json, entry.message);
        json += '}';
    }

    json += "]}";
    giveLogMutex();
    return json;
}

String DiagnosticsManager::getDiagnosticsJson(bool mqttConnected, bool wifiConnected)
{
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t minimumHeap = ESP.getMinFreeHeap();
    const uint32_t uptime = millis() / 1000;

    takeLogMutex();
    const uint32_t received = mqttMessagesReceived;
    const uint32_t sent = mqttMessagesSent;
    const uint32_t reconnects = mqttReconnects;
    const uint32_t wifiReconnections = wifiReconnects;
    const uint32_t devices = registeredDevices;
    const uint32_t errors = errorCount;
    const uint32_t warnings = warningCount;
    giveLogMutex();

    String json;
    json.reserve(320);
    json = "{\"uptime\":";
    json += String(uptime);
    json += ",\"freeHeap\":";
    json += String(freeHeap);
    json += ",\"minimumHeap\":";
    json += String(minimumHeap);
    json += ",\"mqttConnected\":";
    json += mqttConnected ? "true" : "false";
    json += ",\"wifiConnected\":";
    json += wifiConnected ? "true" : "false";
    json += ",\"registeredDevices\":";
    json += String(devices);
    json += ",\"mqttMessagesReceived\":";
    json += String(received);
    json += ",\"mqttMessagesSent\":";
    json += String(sent);
    json += ",\"mqttReconnects\":";
    json += String(reconnects);
    json += ",\"wifiReconnects\":";
    json += String(wifiReconnections);
    json += ",\"errors\":";
    json += String(errors);
    json += ",\"warnings\":";
    json += String(warnings);
    json += '}';

    return json;
}