# Health Monitor Reference

## Overview

`HealthMonitor` evaluates heap, Wi-Fi, MQTT, main-loop and MQTT-loop activity, reconnect frequency, and per-device telemetry age. It samples heap at a 5-second interval and is called from the main firmware loop. API callbacks and health updates share state protected by a FreeRTOS mutex.

Health states are `OK`, `WARNING`, and `CRITICAL`. The aggregate state uses the highest severity across monitored dimensions. Health monitoring records and reports conditions; it does not restart the ESP32 or change application/device state.

## Thresholds

| Signal | Warning | Critical |
|---|---|---|
| Free heap | At or below 100 KiB | Below 50 KiB |
| Wi-Fi RSSI while connected | At or below -65 dBm | Below -80 dBm |
| Device telemetry age | More than 120 seconds | More than 300 seconds |
| No inbound MQTT messages | More than 300 seconds; raises warning severity | No critical threshold |
| Main firmware loop gap | More than 15 seconds | No critical threshold |
| MQTT loop age | More than 15 seconds | No critical threshold |
| Reconnect frequency | Three or more Wi-Fi/MQTT reconnects in a five-minute window | No critical threshold |
| Wi-Fi/MQTT disconnection | Warning state | No critical threshold |

An unreported device is timed from discovery until its first telemetry message. Thereafter, telemetry receipt time is used. Devices older than the warning/critical thresholds transition to `WARNING`/`OFFLINE` health states; recovery is recorded when fresh telemetry resumes.

## Tracked Events

The monitor can record state-change events such as low heap, weak signal, Wi-Fi/MQTT disconnect or reconnect, stale MQTT traffic, loop stalls, delayed/offline/recovered devices, and unexpected reset reasons (CPU panic, watchdog, or brownout). Events are emitted on state transitions rather than every sample. The newest 50 events are retained in a RAM ring; they are not persisted by `HealthMonitor`.

## API

- `GET /api/health` returns the current aggregate status, heap measurements, connectivity, device counts, RSSI, reconnect counts, and loop ages.
- `GET /api/health/events` returns the current event ring in chronological order.

`HealthMonitor::getMetrics()` exposes named metrics internally. `getAgentHealthReport()` generates an `overallStatus` and active `issues` JSON document, but the current `ApiServer` does not register a route for that report.

Health state and event history reset when the ESP32 restarts. Device tracking is updated by `DeviceManager`; the monitor does not independently discover MQTT devices.
