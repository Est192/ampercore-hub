#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * Observability architecture:
 * application modules write structured events here; this manager fans each
 * event out to the console, the fixed-size RAM ring, and LittleFS JSONL/text
 * files. API consumers use the JSON exporters without owning log state.
 */
enum LogLevel : uint8_t
{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
};

struct LogEntry
{
    String level;
    String module;
    String message;
    unsigned long timestamp;
};

class DiagnosticsManager
{
public:
    void begin();

    void info(const String& module, const String& message);
    void warning(const String& module, const String& message);
    void error(const String& module, const String& message);
    void critical(const String& module, const String& message);

    void info(const String& message);
    void warning(const String& message);
    void error(const String& message);
    void critical(const String& message);

    void incrementMqttMessagesReceived();
    void incrementMqttMessagesSent();
    void incrementMqttReconnects();
    void incrementWifiReconnects();
    void setRegisteredDevices(uint32_t count);

    String getLogsJson();
    String getDiagnosticsJson(bool mqttConnected, bool wifiConnected);

private:
    static constexpr size_t MAX_LOG_ENTRIES = 200;

    void addLog(LogLevel level, const String& module, const String& message);
    bool persistLog(const LogEntry& entry);
    void takeLogMutex();
    void giveLogMutex();
    void takeStorageMutex();
    void giveStorageMutex();

    LogEntry logs[MAX_LOG_ENTRIES];
    size_t logStart = 0;
    size_t logCount = 0;

    uint32_t mqttMessagesReceived = 0;
    uint32_t mqttMessagesSent = 0;
    uint32_t mqttReconnects = 0;
    uint32_t wifiReconnects = 0;
    uint32_t registeredDevices = 0;
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;

    SemaphoreHandle_t logMutex = nullptr;
    SemaphoreHandle_t storageMutex = nullptr;
    bool filesystemReady = false;
    bool storageFailureLogged = false;
};