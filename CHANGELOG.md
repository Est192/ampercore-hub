# Changelog

Changes below are summarized from the repository's tagged history and current source. Earlier minor releases are grouped under their `v0.1` and `v0.2` series.

## v0.3.0-alpha

### DiagnosticsManager

- Added severity levels, module-tagged diagnostic entries, serial output, a 200-entry RAM ring, and LittleFS persistence to `/logs/system.log` and `/logs/agent.log`.
- Added MQTT, Wi-Fi, device, warning, and error counters and a JSON diagnostics export.

### HealthMonitor

- Added periodic health evaluation for free heap, Wi-Fi signal/connectivity, MQTT connectivity and inbound activity, main/MQTT loop activity, and device telemetry age.
- Added warning/critical states, reconnect tracking, reset-reason reporting, and a 50-entry event ring.
- Added JSON health and event exports. The monitor reports conditions; it does not restart the device.

### REST API

- Added diagnostics, log, health, and health-event endpoints.
- Added device command endpoints and HTTP actions for reboot and clearing saved configuration.

### Mobile Integration

- No mobile application or mobile-specific transport is included in this repository. The HTTP API is available to clients, but an app integration is not implemented.

## v0.2

### MQTT foundation (`v0.2.0.1`)

- Added MQTT client connection, reconnection, publish/subscribe handling, and hub MQTT integration.
- Added the periodic hub heartbeat on `ampercore/hub/heartbeat`.

### Device discovery (`v0.2.0.2`)

- Added device discovery messages on `ampercore/discovery` and runtime registration of discovered devices.

### Device telemetry (`v0.2.0.3`)

- Added telemetry parsing and updates from `ampercore/device/{id}/telemetry`.

## v0.1

### Initial setup (`v0.1.0`)

- Added the initial ESP32 PlatformIO firmware project.

### Wi-Fi setup and persistence (`v0.1.2`–`v0.1.3`)

- Added setup access-point and web portal flows, Wi-Fi scanning, saved network credentials, and station-mode auto-connection.

### Dashboard and REST API (`v0.1.4`)

- Added the local web dashboard and initial REST endpoints.

### Device management API (`v0.1.5`)

- Added device management endpoints and the initial in-memory device registry.
