#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

class DeviceManager;
class DiagnosticsManager;
class MQTTManager;

enum HealthStatus
{
    HEALTH_OK,
    HEALTH_WARNING,
    HEALTH_CRITICAL
};

struct HealthMetric
{
    String name;
    String value;
    HealthStatus status;
};

struct HealthEvent
{
    String category;
    String message;
    unsigned long timestamp;
};

/*
 * HealthMonitor owns the firmware health lifecycle:
 * 1. begin() establishes baselines and inspects the last ESP reset reason.
 * 2. loop() samples heap and link state, derives health, and emits events only
 *    when a condition changes (or a new time threshold is crossed).
 * 3. MQTTManager reports inbound packet activity and its service-loop activity.
 * 4. DeviceManager reports discovery, telemetry timestamps, and removals, so
 *    device health is calculated without exposing its internal container.
 * 5. The REST API, dashboard, and analysis agents consume the same evaluated
 *    state through JSON exporters.
 *
 * A fixed event ring retains the newest 50 events. Per-device health tracking
 * and report generation are guarded because API callbacks execute outside the
 * Arduino loop task. This layer reports and records anomalies only; it never
 * restarts the ESP32 or mutates device/application state.
 */
class HealthMonitor
{
public:
    void begin(
        DiagnosticsManager& diagnostics,
        MQTTManager& mqtt,
        DeviceManager& devices
    );

    void loop();

    void noteMqttMessageReceived();
    void noteMqttLoop();
    void noteDeviceDiscovered(const String& id);
    void noteDeviceRemoved(const String& id);

    HealthStatus evaluateSystem();
    std::vector<HealthMetric> getMetrics();
    String getHealthJson();
    String getSystemReportJson(const String& firmware);
    String getHealthEventsJson();
    String getAgentHealthReport();

private:
    struct DeviceHealth
    {
        String id;
        unsigned long discoveredAt = 0;
        HealthStatus status = HEALTH_OK;
    };

    static constexpr size_t MAX_HEALTH_EVENTS = 50;
    static constexpr unsigned long SAMPLE_INTERVAL_MS = 5000;
    static constexpr unsigned long STALE_TELEMETRY_WARNING_MS = 120000;
    static constexpr unsigned long STALE_TELEMETRY_CRITICAL_MS = 300000;
    static constexpr unsigned long MQTT_STALE_WARNING_MS = 300000;
    static constexpr unsigned long LOOP_STALL_WARNING_MS = 15000;
    static constexpr unsigned long RECONNECT_WINDOW_MS = 300000;

    void takeMutex();
    void giveMutex();
    void addEventLocked(const String& category, const String& message, HealthStatus status);
    void recordReconnectLocked(bool mqtt, unsigned long now);
    void evaluateLocked(unsigned long now);
    HealthStatus evaluateSystemLocked(unsigned long now);
    HealthStatus evaluateDeviceLocked(
        DeviceHealth& device,
        unsigned long lastSeen,
        bool hasTelemetry,
        unsigned long now
    );
    void sampleHeapLocked();
    size_t countOfflineDevicesLocked() const;
    size_t countWarningDevicesLocked() const;
    String getDeviceState(HealthStatus status) const;
    String getStatusName(HealthStatus status) const;
    void appendActiveIssue(
        String& json,
        bool& hasIssue,
        const String& type,
        HealthStatus severity,
        const String& message
    ) const;

    DiagnosticsManager* diagnostics = nullptr;
    MQTTManager* mqttManager = nullptr;
    DeviceManager* deviceManager = nullptr;
    SemaphoreHandle_t mutex = nullptr;

    HealthEvent events[MAX_HEALTH_EVENTS];
    size_t eventStart = 0;
    size_t eventCount = 0;
    std::vector<DeviceHealth> deviceHealth;

    unsigned long lastSampleAt = 0;
    unsigned long lastLoopAt = 0;
    unsigned long lastMqttMessageAt = 0;
    unsigned long lastMqttLoopAt = 0;

    uint32_t currentHeap = 0;
    uint32_t minimumHeap = 0;
    uint32_t averageHeap = 0;
    uint64_t heapAccumulator = 0;
    uint64_t heapSamples = 0;
    uint32_t loopStallCount = 0;
    uint32_t wifiReconnectCount = 0;
    uint32_t mqttReconnectCount = 0;
    uint32_t reconnectsInWindow = 0;
    unsigned long reconnectWindowStartedAt = 0;
    unsigned long lastLoopGap = 0;
    int currentRssi = 0;

    HealthStatus heapStatus = HEALTH_OK;
    HealthStatus rssiStatus = HEALTH_OK;
    HealthStatus mqttStatus = HEALTH_WARNING;
    HealthStatus wifiStatus = HEALTH_WARNING;
    HealthStatus loopStatus = HEALTH_OK;
    HealthStatus mqttLoopStatus = HEALTH_OK;
    HealthStatus previousHeapStatus = HEALTH_OK;
    HealthStatus previousRssiStatus = HEALTH_OK;

    bool previousWifiConnected = false;
    bool previousMqttConnected = false;
    bool hasSeenWifiConnection = false;
    bool hasSeenMqttConnection = false;
    bool mqttTrafficStale = false;
    bool loopStallActive = false;
    bool mqttLoopStallActive = false;
    bool highReconnectRateActive = false;
    bool initialized = false;
    HealthStatus overallStatus = HEALTH_WARNING;
};
