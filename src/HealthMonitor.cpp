#include "HealthMonitor.h"

#include <WiFi.h>
#include <esp_system.h>

#include "DiagnosticsManager.h"
#include "MQTTManager.h"

extern DiagnosticsManager diagnosticsManager;

namespace
{
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

HealthStatus worstStatus(HealthStatus left, HealthStatus right)
{
    return static_cast<uint8_t>(left) >= static_cast<uint8_t>(right)
        ? left
        : right;
}

const char* rebootReasonDescription(esp_reset_reason_t reason)
{
    switch(reason)
    {
        case ESP_RST_PANIC:
            return "CPU panic";
        case ESP_RST_INT_WDT:
            return "interrupt watchdog";
        case ESP_RST_TASK_WDT:
            return "task watchdog";
        case ESP_RST_WDT:
            return "watchdog";
        case ESP_RST_BROWNOUT:
            return "brownout";
        default:
            return nullptr;
    }
}
}

void HealthMonitor::begin(
    DiagnosticsManager& diagnosticsManagerRef,
    MQTTManager& mqttManagerRef
)
{
    diagnostics = &diagnosticsManagerRef;
    mqttManager = &mqttManagerRef;

    mutex = xSemaphoreCreateMutex();
    if(mutex == nullptr)
    {
        diagnostics->critical("SYSTEM", "HealthMonitor no pudo crear su mutex");
        return;
    }

    const unsigned long now = millis();
    lastSampleAt = now - SAMPLE_INTERVAL_MS;
    lastLoopAt = now;
    lastMqttMessageAt = now;
    lastMqttLoopAt = now;
    reconnectWindowStartedAt = now;
    currentHeap = ESP.getFreeHeap();
    minimumHeap = ESP.getMinFreeHeap();
    averageHeap = currentHeap;
    heapSampleTotal = currentHeap;
    heapSampleCount = 1;
    currentRssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    previousWifiConnected = WiFi.isConnected();
    previousMqttConnected = mqttManager->isConnected();
    hasSeenWifiConnection = previousWifiConnected;
    hasSeenMqttConnection = previousMqttConnected;
    initialized = true;

    diagnostics->info("SYSTEM", "HealthMonitor iniciado");

    const char* unexpectedReason = rebootReasonDescription(esp_reset_reason());
    if(unexpectedReason != nullptr)
    {
        takeMutex();
        addEventLocked(
            "UNEXPECTED_REBOOT",
            String("Reinicio inesperado detectado: ") + unexpectedReason,
            HEALTH_WARNING
        );
        giveMutex();
    }

    loop();
}

void HealthMonitor::takeMutex()
{
    if(mutex != nullptr)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void HealthMonitor::giveMutex()
{
    if(mutex != nullptr)
    {
        xSemaphoreGive(mutex);
    }
}

void HealthMonitor::addEventLocked(
    const String& category,
    const String& message,
    HealthStatus status
)
{
    const size_t index = (eventStart + eventCount) % MAX_HEALTH_EVENTS;
    events[index].category = category;
    events[index].message = message;
    events[index].timestamp = millis();

    if(eventCount < MAX_HEALTH_EVENTS)
    {
        ++eventCount;
    }
    else
    {
        eventStart = (eventStart + 1) % MAX_HEALTH_EVENTS;
    }

    if(diagnostics == nullptr)
    {
        return;
    }

    switch(status)
    {
        case HEALTH_CRITICAL:
            diagnostics->critical("SYSTEM", category + ": " + message);
            break;
        case HEALTH_WARNING:
            diagnostics->warning("SYSTEM", category + ": " + message);
            break;
        case HEALTH_OK:
            diagnostics->info("SYSTEM", category + ": " + message);
            break;
    }
}

void HealthMonitor::sampleHeapLocked()
{
    currentHeap = ESP.getFreeHeap();
    minimumHeap = ESP.getMinFreeHeap();
    heapSampleTotal += currentHeap;
    ++heapSampleCount;
    averageHeap = heapSampleCount == 0
        ? currentHeap
        : static_cast<uint32_t>(heapSampleTotal / heapSampleCount);
}

void HealthMonitor::recordReconnectLocked(bool mqtt, unsigned long now)
{
    if(now - reconnectWindowStartedAt >= RECONNECT_WINDOW_MS)
    {
        reconnectWindowStartedAt = now;
        reconnectsInWindow = 0;
        highReconnectRateActive = false;
    }

    ++reconnectsInWindow;
    if(mqtt)
    {
        ++mqttReconnectCount;
    }
    else
    {
        ++wifiReconnectCount;
    }

    if(reconnectsInWindow >= 3 && !highReconnectRateActive)
    {
        addEventLocked(
            "HIGH_RECONNECT_RATE",
            "Tres o mas reconexiones de red en una ventana de cinco minutos",
            HEALTH_WARNING
        );
        highReconnectRateActive = true;
    }
}

void HealthMonitor::noteMqttMessageReceived()
{
    takeMutex();
    lastMqttMessageAt = millis();
    mqttTrafficStale = false;
    giveMutex();
}

void HealthMonitor::noteMqttLoop()
{
    takeMutex();
    lastMqttLoopAt = millis();
    giveMutex();
}

void HealthMonitor::noteDeviceDiscovered(const String& id)
{
    takeMutex();

    for(DeviceHealth& device : deviceHealth)
    {
        if(device.id == id)
        {
            giveMutex();
            return;
        }
    }

    DeviceHealth device;
    device.id = id;
    device.discoveredAt = millis();
    device.lastSeen = device.discoveredAt;
    device.status = HEALTH_OK;
    deviceHealth.push_back(device);

    addEventLocked("DEVICE_DISCOVERED", "Dispositivo detectado: " + id, HEALTH_OK);
    giveMutex();
}

void HealthMonitor::noteTelemetryReceived(const String& id, unsigned long timestamp)
{
    takeMutex();

    for(DeviceHealth& device : deviceHealth)
    {
        if(device.id == id)
        {
            device.lastSeen = timestamp;
            device.hasTelemetry = true;
            giveMutex();
            return;
        }
    }

    DeviceHealth device;
    device.id = id;
    device.discoveredAt = timestamp;
    device.lastSeen = timestamp;
    device.hasTelemetry = true;
    deviceHealth.push_back(device);

    giveMutex();
}

void HealthMonitor::noteDeviceRemoved(const String& id)
{
    takeMutex();

    for(size_t i = 0; i < deviceHealth.size(); ++i)
    {
        if(deviceHealth[i].id == id)
        {
            deviceHealth.erase(deviceHealth.begin() + i);
            addEventLocked("DEVICE_REMOVED", "Dispositivo eliminado: " + id, HEALTH_OK);
            break;
        }
    }

    giveMutex();
}

/*
 * Telemetry.timestamp is the authoritative last-seen time. A device that has
 * never reported telemetry uses its discovery time as the initial reference,
 * so a silent device is still detected rather than remaining ONLINE forever.
 */
HealthStatus HealthMonitor::evaluateDeviceLocked(DeviceHealth& device, unsigned long now)
{
    const unsigned long referenceTime = device.hasTelemetry
        ? device.lastSeen
        : device.discoveredAt;
    const unsigned long age = now - referenceTime;

    HealthStatus status = HEALTH_OK;
    if(age > STALE_TELEMETRY_CRITICAL_MS)
    {
        status = HEALTH_CRITICAL;
    }
    else if(age > STALE_TELEMETRY_WARNING_MS)
    {
        status = HEALTH_WARNING;
    }

    if(status != device.status)
    {
        if(status == HEALTH_WARNING)
        {
            addEventLocked(
                "TELEMETRY_DELAYED",
                "Telemetria retrasada para " + device.id + " (" + String(age / 1000) + " s)",
                HEALTH_WARNING
            );
        }
        else if(status == HEALTH_CRITICAL)
        {
            addEventLocked(
                "DEVICE_OFFLINE",
                "Dispositivo sin telemetria por " + String(age / 1000) + " s: " + device.id,
                HEALTH_CRITICAL
            );
        }
        else
        {
            addEventLocked(
                "DEVICE_RECOVERED",
                "Telemetria recuperada para " + device.id,
                HEALTH_OK
            );
        }

        device.status = status;
    }

    return status;
}

size_t HealthMonitor::countOfflineDevicesLocked() const
{
    size_t count = 0;
    for(const DeviceHealth& device : deviceHealth)
    {
        if(device.status == HEALTH_CRITICAL)
        {
            ++count;
        }
    }
    return count;
}

size_t HealthMonitor::countWarningDevicesLocked() const
{
    size_t count = 0;
    for(const DeviceHealth& device : deviceHealth)
    {
        if(device.status == HEALTH_WARNING)
        {
            ++count;
        }
    }
    return count;
}

/*
 * Each sampling pass evaluates independent dimensions, then combines them
 * using the worst severity. Recovery and anomaly events are edge-triggered to
 * prevent a sustained fault from filling the event ring every five seconds.
 */
void HealthMonitor::evaluateLocked(unsigned long now)
{
    const bool wifiConnected = WiFi.isConnected();
    const bool mqttConnected = mqttManager != nullptr && mqttManager->isConnected();

    if(now - lastSampleAt >= SAMPLE_INTERVAL_MS)
    {
        lastSampleAt = now;
        sampleHeapLocked();
    }

    heapStatus = currentHeap < 50U * 1024U
        ? HEALTH_CRITICAL
        : currentHeap <= 100U * 1024U ? HEALTH_WARNING : HEALTH_OK;

    if(heapStatus != previousHeapStatus)
    {
        if(heapStatus != HEALTH_OK)
        {
            addEventLocked(
                "LOW_HEAP",
                "Memoria libre: " + String(currentHeap) + " bytes",
                heapStatus
            );
        }
        else
        {
            addEventLocked("HEAP_RECOVERED", "Memoria libre recuperada", HEALTH_OK);
        }
        previousHeapStatus = heapStatus;
    }

    if(wifiConnected)
    {
        wifiStatus = HEALTH_OK;
        currentRssi = WiFi.RSSI();
        rssiStatus = currentRssi < -80
            ? HEALTH_CRITICAL
            : currentRssi <= -65 ? HEALTH_WARNING : HEALTH_OK;

        if(rssiStatus != previousRssiStatus)
        {
            if(rssiStatus != HEALTH_OK)
            {
                addEventLocked(
                    "LOW_SIGNAL",
                    "RSSI WiFi: " + String(currentRssi) + " dBm",
                    rssiStatus
                );
            }
            else
            {
                addEventLocked("SIGNAL_RECOVERED", "Nivel de señal WiFi recuperado", HEALTH_OK);
            }
            previousRssiStatus = rssiStatus;
        }

        if(!previousWifiConnected && hasSeenWifiConnection)
        {
            recordReconnectLocked(false, now);
            addEventLocked("WIFI_RECONNECTED", "WiFi reconectado", HEALTH_OK);
        }

        hasSeenWifiConnection = true;
    }
    else
    {
        wifiStatus = HEALTH_WARNING;
        rssiStatus = HEALTH_WARNING;
        if(previousWifiConnected)
        {
            addEventLocked("WIFI_DISCONNECTED", "Conexion WiFi perdida", HEALTH_WARNING);
        }
    }
    previousWifiConnected = wifiConnected;

    if(mqttConnected)
    {
        mqttStatus = HEALTH_OK;
        if(!previousMqttConnected && hasSeenMqttConnection)
        {
            recordReconnectLocked(true, now);
            addEventLocked("MQTT_RECONNECTED", "MQTT reconectado", HEALTH_OK);
        }
        hasSeenMqttConnection = true;
    }
    else
    {
        mqttStatus = HEALTH_WARNING;
        if(previousMqttConnected)
        {
            addEventLocked("MQTT_DISCONNECTED", "Conexion MQTT perdida", HEALTH_WARNING);
        }
    }
    previousMqttConnected = mqttConnected;

    const unsigned long mqttLoopGap = now - lastMqttLoopAt;
    if(mqttLoopGap > LOOP_STALL_WARNING_MS)
    {
        mqttLoopStatus = HEALTH_WARNING;
        if(!mqttLoopStallActive)
        {
            addEventLocked(
                "MQTT_LOOP_STALLED",
                "Ciclo MQTT sin actividad durante " + String(mqttLoopGap / 1000) + " s",
                HEALTH_WARNING
            );
            mqttLoopStallActive = true;
        }
    }
    else
    {
        mqttLoopStatus = HEALTH_OK;
        if(mqttLoopStallActive)
        {
            addEventLocked("MQTT_LOOP_RECOVERED", "Ciclo MQTT recuperado", HEALTH_OK);
            mqttLoopStallActive = false;
        }
    }

    const unsigned long mqttIdle = now - lastMqttMessageAt;
    if(mqttIdle > MQTT_STALE_WARNING_MS)
    {
        mqttStatus = worstStatus(mqttStatus, HEALTH_WARNING);
        if(!mqttTrafficStale)
        {
            addEventLocked(
                "MQTT_NO_TRAFFIC",
                "No se reciben mensajes MQTT desde hace " + String(mqttIdle / 1000) + " s",
                HEALTH_WARNING
            );
            mqttTrafficStale = true;
        }
    }
    else
    {
        mqttTrafficStale = false;
    }

    for(DeviceHealth& device : deviceHealth)
    {
        evaluateDeviceLocked(device, now);
    }

    if(lastLoopAt != 0)
    {
        lastLoopGap = now - lastLoopAt;
        if(lastLoopGap > LOOP_STALL_WARNING_MS)
        {
            loopStatus = HEALTH_WARNING;
            if(!loopStallActive)
            {
                ++loopStallCount;
                addEventLocked(
                    "LOOP_STALLED",
                    "Ciclo principal bloqueado por " + String(lastLoopGap / 1000) + " s",
                    HEALTH_WARNING
                );
                loopStallActive = true;
            }
        }
        else
        {
            loopStatus = HEALTH_OK;
            if(loopStallActive)
            {
                addEventLocked("LOOP_RECOVERED", "Ciclo principal recuperado", HEALTH_OK);
                loopStallActive = false;
            }
        }
    }

    overallStatus = evaluateSystemLocked(now);
}

HealthStatus HealthMonitor::evaluateSystemLocked(unsigned long now)
{
    (void)now;
    HealthStatus status = HEALTH_OK;
    status = worstStatus(status, heapStatus);
    status = worstStatus(status, wifiStatus);
    status = worstStatus(status, rssiStatus);
    status = worstStatus(status, mqttStatus);
    status = worstStatus(status, loopStatus);
    status = worstStatus(status, mqttLoopStatus);

    for(const DeviceHealth& device : deviceHealth)
    {
        status = worstStatus(status, device.status);
    }

    return status;
}

void HealthMonitor::loop()
{
    if(!initialized || mutex == nullptr)
    {
        return;
    }

    const unsigned long now = millis();
    takeMutex();
    evaluateLocked(now);
    lastLoopAt = now;
    giveMutex();
}

HealthStatus HealthMonitor::evaluateSystem()
{
    if(!initialized || mutex == nullptr)
    {
        return HEALTH_WARNING;
    }

    takeMutex();
    evaluateLocked(millis());
    const HealthStatus result = overallStatus;
    giveMutex();
    return result;
}

String HealthMonitor::getStatusName(HealthStatus status) const
{
    switch(status)
    {
        case HEALTH_OK:
            return "OK";
        case HEALTH_WARNING:
            return "WARNING";
        case HEALTH_CRITICAL:
            return "CRITICAL";
        default:
            return "WARNING";
    }
}

String HealthMonitor::getDeviceState(HealthStatus status) const
{
    if(status == HEALTH_CRITICAL)
    {
        return "OFFLINE";
    }
    return status == HEALTH_WARNING ? "WARNING" : "ONLINE";
}

std::vector<HealthMetric> HealthMonitor::getMetrics()
{
    evaluateSystem();

    takeMutex();
    std::vector<HealthMetric> metrics;
    metrics.reserve(11 + deviceHealth.size());

    metrics.push_back({"heap", String(currentHeap), heapStatus});
    metrics.push_back({"minimumHeap", String(minimumHeap), heapStatus});
    metrics.push_back({"averageHeap", String(averageHeap), heapStatus});
    metrics.push_back({"wifi", WiFi.isConnected() ? "connected" : "disconnected", wifiStatus});
    metrics.push_back({"rssi", WiFi.isConnected() ? String(currentRssi) : "unavailable", rssiStatus});
    metrics.push_back({"mqtt", mqttManager != nullptr && mqttManager->isConnected() ? "connected" : "disconnected", mqttStatus});
    metrics.push_back({"deviceCount", String(deviceHealth.size()), HEALTH_OK});
    metrics.push_back({"offlineDevices", String(countOfflineDevicesLocked()), countOfflineDevicesLocked() > 0 ? HEALTH_CRITICAL : HEALTH_OK});
    metrics.push_back({"warningDevices", String(countWarningDevicesLocked()), countWarningDevicesLocked() > 0 ? HEALTH_WARNING : HEALTH_OK});
    metrics.push_back({"loopGapMs", String(lastLoopGap), loopStatus});
    metrics.push_back({"mqttLoopAgeMs", String(millis() - lastMqttLoopAt), mqttLoopStatus});
    for(const DeviceHealth& device : deviceHealth)
    {
        metrics.push_back({
            "device:" + device.id,
            getDeviceState(device.status),
            device.status
        });
    }
    giveMutex();
    return metrics;
}

String HealthMonitor::getHealthJson()
{
    evaluateSystem();
    takeMutex();

    String json;
    json.reserve(240);
    json = "{\"status\":";
    appendJsonString(json, getStatusName(overallStatus));
    json += ",\"heap\":";
    json += String(currentHeap);
    json += ",\"minHeap\":";
    json += String(minimumHeap);
    json += ",\"averageHeap\":";
    json += String(averageHeap);
    json += ",\"wifiConnected\":";
    json += WiFi.isConnected() ? "true" : "false";
    json += ",\"mqttConnected\":";
    json += mqttManager != nullptr && mqttManager->isConnected() ? "true" : "false";
    json += ",\"deviceCount\":";
    json += String(deviceHealth.size());
    json += ",\"offlineDevices\":";
    json += String(countOfflineDevicesLocked());
    json += ",\"warningDevices\":";
    json += String(countWarningDevicesLocked());
    json += ",\"rssi\":";
    if(WiFi.isConnected())
    {
        json += String(currentRssi);
    }
    else
    {
        json += "null";
    }
    json += ",\"wifiReconnects\":";
    json += String(wifiReconnectCount);
    json += ",\"mqttReconnects\":";
    json += String(mqttReconnectCount);
    json += ",\"loopGapMs\":";
    json += String(lastLoopGap);
    json += ",\"mqttLoopAgeMs\":";
    json += String(millis() - lastMqttLoopAt);
    json += '}';

    giveMutex();
    return json;
}

String HealthMonitor::getHealthEventsJson()
{
    takeMutex();

    String json;
    json.reserve(24 + eventCount * 96);
    json = "{\"count\":";
    json += String(eventCount);
    json += ",\"events\":[";

    for(size_t i = 0; i < eventCount; ++i)
    {
        if(i > 0)
        {
            json += ',';
        }

        const HealthEvent& event = events[(eventStart + i) % MAX_HEALTH_EVENTS];
        json += "{\"category\":";
        appendJsonString(json, event.category);
        json += ",\"message\":";
        appendJsonString(json, event.message);
        json += ",\"timestamp\":";
        json += String(event.timestamp);
        json += '}';
    }

    json += "]}";
    giveMutex();
    return json;
}

void HealthMonitor::appendActiveIssue(
    String& json,
    bool& hasIssue,
    const String& type,
    HealthStatus severity,
    const String& message
) const
{
    if(severity == HEALTH_OK)
    {
        return;
    }

    if(hasIssue)
    {
        json += ',';
    }
    hasIssue = true;

    json += "{\"type\":";
    appendJsonString(json, type);
    json += ",\"severity\":";
    appendJsonString(json, getStatusName(severity));
    json += ",\"message\":";
    appendJsonString(json, message);
    json += '}';
}

String HealthMonitor::getAgentHealthReport()
{
    evaluateSystem();
    takeMutex();

    String json;
    json.reserve(512);
    json = "{\"overallStatus\":";
    appendJsonString(json, getStatusName(overallStatus));
    json += ",\"issues\":[";
    bool hasIssue = false;

    if(heapStatus != HEALTH_OK)
    {
        appendActiveIssue(
            json,
            hasIssue,
            "LOW_HEAP",
            heapStatus,
            "Available heap is " + String(currentHeap) + " bytes"
        );
    }

    if(!WiFi.isConnected())
    {
        appendActiveIssue(
            json,
            hasIssue,
            "WIFI_DISCONNECTED",
            HEALTH_WARNING,
            "WiFi is disconnected"
        );
    }
    else if(rssiStatus != HEALTH_OK)
    {
        appendActiveIssue(
            json,
            hasIssue,
            "LOW_SIGNAL",
            rssiStatus,
            "WiFi signal strength is " + String(currentRssi) + " dBm"
        );
    }

    if(mqttManager == nullptr || !mqttManager->isConnected())
    {
        appendActiveIssue(
            json,
            hasIssue,
            "MQTT_DISCONNECTED",
            HEALTH_WARNING,
            "MQTT broker connection is unavailable"
        );
    }
    else if(mqttTrafficStale)
    {
        appendActiveIssue(
            json,
            hasIssue,
            "MQTT_NO_TRAFFIC",
            HEALTH_WARNING,
            "No inbound MQTT messages have been received for more than 5 minutes"
        );
    }

    for(const DeviceHealth& device : deviceHealth)
    {
        if(device.status != HEALTH_OK)
        {
            const unsigned long age = millis() - (device.hasTelemetry ? device.lastSeen : device.discoveredAt);
            appendActiveIssue(
                json,
                hasIssue,
                device.status == HEALTH_CRITICAL ? "DEVICE_OFFLINE" : "TELEMETRY_DELAYED",
                device.status,
                device.id + " has not reported telemetry for " + String(age / 1000) + " seconds"
            );
        }
    }

    if(loopStatus != HEALTH_OK)
    {
        appendActiveIssue(
            json,
            hasIssue,
            "LOOP_STALLED",
            loopStatus,
            "Main loop interval exceeded the 15 second warning threshold"
        );
    }

    if(mqttLoopStatus != HEALTH_OK)
    {
        appendActiveIssue(
            json,
            hasIssue,
            "MQTT_LOOP_STALLED",
            mqttLoopStatus,
            "MQTT service loop has not run within the 15 second threshold"
        );
    }

    json += "]}";
    giveMutex();
    return json;
}
