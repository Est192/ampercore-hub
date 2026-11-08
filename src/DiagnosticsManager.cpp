#include "DiagnosticsManager.h"

#include <LittleFS.h>
#include <time.h>

namespace
{
constexpr size_t MAX_PERSISTED_LOG_BYTES = 64 * 1024;

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

    filesystemReady = LittleFS.begin(false);
    if(filesystemReady && !LittleFS.exists("/logs"))
    {
        filesystemReady = LittleFS.mkdir("/logs");
    }

    if(filesystemReady &&
        !LittleFS.exists("/logs/agent.jsonl") &&
        LittleFS.exists("/logs/agent.log"))
    {
        filesystemReady = LittleFS.rename(
            "/logs/agent.log",
            "/logs/agent.jsonl"
        );
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

bool DiagnosticsManager::rotateFileIfNeeded(const char* path, size_t incomingBytes)
{
    File currentFile = LittleFS.open(path, FILE_READ);
    size_t currentSize = 0;
    if(currentFile)
    {
        currentSize = currentFile.size();
        currentFile.close();
    }

    if(currentSize == 0 || currentSize + incomingBytes <= MAX_PERSISTED_LOG_BYTES)
    {
        return true;
    }

    const String backupPath = String(path) + ".1";
    if(LittleFS.exists(backupPath) && !LittleFS.remove(backupPath))
    {
        return false;
    }

    if(!LittleFS.rename(path, backupPath))
    {
        return false;
    }

    return true;
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
    String humanMessage = entry.message;
    humanMessage.replace('\r', ' ');
    humanMessage.replace('\n', ' ');

    String systemLine;
    systemLine.reserve(timestamp.length() + entry.level.length() + entry.module.length() + humanMessage.length() + 8);
    systemLine = timestamp + "\n" + entry.level + "\n" + entry.module + "\n" + humanMessage + "\n";

    String agentLine;
    agentLine.reserve(entry.level.length() + entry.module.length() + entry.message.length() + 64);
    agentLine = "{\"timestamp\":";
    agentLine += String(entry.timestamp);
    agentLine += ",\"level\":";
    appendJsonString(agentLine, entry.level);
    agentLine += ",\"module\":";
    appendJsonString(agentLine, entry.module);
    agentLine += ",\"message\":";
    appendJsonString(agentLine, entry.message);
    agentLine += "}\n";

    bool filesOpened =
        rotateFileIfNeeded("/logs/system.log", systemLine.length()) &&
        rotateFileIfNeeded("/logs/agent.jsonl", agentLine.length());
    File systemFile;
    File agentFile;

    if(filesOpened)
    {
        systemFile = LittleFS.open("/logs/system.log", FILE_APPEND);
        agentFile = LittleFS.open("/logs/agent.jsonl", FILE_APPEND);
        filesOpened = systemFile && agentFile;
    }

    bool writeSucceeded = filesOpened;

    if(filesOpened)
    {
        systemFile.print(systemLine);
        agentFile.print(agentLine);

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

void DiagnosticsManager::setSettings(const DiagnosticsSettings& newSettings)
{
    takeLogMutex();
    settings = newSettings;
    giveLogMutex();
}

DiagnosticsSettings DiagnosticsManager::getSettings()
{
    takeLogMutex();
    const DiagnosticsSettings currentSettings = settings;
    giveLogMutex();
    return currentSettings;
}

bool DiagnosticsManager::shouldLogHeartbeat()
{
    return getSettings().logHeartbeat;
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

String DiagnosticsManager::getLogsJson(
    size_t limit,
    const String& level,
    unsigned long since
)
{
    takeLogMutex();

    if(limit == 0 || limit > MAX_LOG_ENTRIES)
    {
        limit = MAX_LOG_ENTRIES;
    }

    size_t selected[MAX_LOG_ENTRIES];
    size_t selectedCount = 0;

    for(size_t offset = logCount; offset > 0 && selectedCount < limit; --offset)
    {
        const size_t index = (logStart + offset - 1) % MAX_LOG_ENTRIES;
        const LogEntry& entry = logs[index];

        if(since != 0 && entry.timestamp < since)
        {
            continue;
        }

        if(!level.isEmpty() && !entry.level.equalsIgnoreCase(level))
        {
            continue;
        }

        selected[selectedCount++] = index;
    }

    String json;
    json.reserve(32 + selectedCount * 96);
    json = "{\"count\":";
    json += String(selectedCount);
    json += ",\"logs\":[";

    for(size_t i = selectedCount; i > 0; --i)
    {
        const LogEntry& entry = logs[selected[i - 1]];
        if(i != selectedCount)
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

String DiagnosticsManager::getDiagnosticsJson(
    bool mqttConnected,
    bool wifiConnected,
    uint32_t registeredDevices
)
{
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t minimumHeap = ESP.getMinFreeHeap();
    const uint32_t uptime = millis() / 1000;

    takeLogMutex();
    const uint32_t received = mqttMessagesReceived;
    const uint32_t sent = mqttMessagesSent;
    const uint32_t reconnects = mqttReconnects;
    const uint32_t wifiReconnections = wifiReconnects;
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
    json += String(registeredDevices);
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