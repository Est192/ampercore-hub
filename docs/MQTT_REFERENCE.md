# MQTT Reference

## Connection

The current firmware has a fixed broker configuration in `src/MQTTManager.cpp`:

| Setting | Value |
|---|---|
| Host | `broker.hivemq.com` |
| Port | `1883` |
| Transport | Plain TCP through `WiFiClient` |
| Client ID | `AMPERCORE-HUB-` followed by a hexadecimal value derived from `ESP.getEfuseMac()` |
| Authentication | None configured |
| TLS | None configured |

The hub resolves the broker host, tests TCP reachability, and then connects with PubSubClient. A connection subscribes to `ampercore/discovery` and `ampercore/device/#`. Failed connections are retried by the MQTT service loop after a 15-second interval, provided Wi-Fi station mode is connected.

## Topics and Payloads

| Direction | Topic | Payload/behavior |
|---|---|---|
| Device to hub | `ampercore/discovery` | JSON object with non-empty string fields `id`, `name`, and `type`. |
| Device to hub | `ampercore/device/{id}/telemetry` | JSON fields `voltage`, `current`, `power`, and `energy`. |
| Hub to device | `ampercore/device/{id}/command` | JSON object with an `action` of `turn_on`, `turn_off`, or `reboot`, sent by REST command routes. |
| Hub to broker | `ampercore/hub/heartbeat` | JSON with `heap` (free heap bytes) and `uptime` (seconds); attempted every 30 seconds. |

Discovery example:

```json
{"id":"device-01","name":"Example device","type":"switch"}
```

Telemetry example:

```json
{"voltage":120.0,"current":1.25,"power":150.0,"energy":0.42}
```

Command example:

```json
{"action":"turn_on"}
```

The examples show the fields consumed or produced by the source; they do not imply a particular device model, unit convention, or hardware.

## Processing Notes

- Discovery requires valid JSON and non-empty `id`, `name`, and `type`. The hub registers new devices with state `online`; duplicate IDs are ignored by `DeviceManager`.
- Telemetry is associated with the device ID extracted from its topic. Malformed JSON or an invalid topic is logged. Missing numeric telemetry fields default to `0.0`.
- Telemetry timestamps are assigned from `millis()` when received; payload timestamps are not read.
- A telemetry message for an ID that is not registered is rejected by `DeviceManager`; the hub does not implicitly create the device from telemetry alone.
- The hub processes incoming discovery and telemetry. It does not implement a command acknowledgement or state-confirmation subscription in the current code.
- The heartbeat is only published when the MQTT client is connected. A disconnected publish attempt is logged as skipped.

## Security and Deployment Notes

The broker host, port, topic namespace, and client-ID scheme are compiled into the firmware. There is no portal setting for them. The current connection uses a public broker endpoint without TLS or credentials; do not treat these topics as a private or authenticated control channel.
