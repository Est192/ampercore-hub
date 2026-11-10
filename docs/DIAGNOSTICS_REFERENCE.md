# Diagnostics Reference

## DiagnosticsManager

Firmware modules write diagnostics through `DiagnosticsManager` with one of four levels:

- `INFO`
- `WARNING`
- `ERROR`
- `CRITICAL`

Entries include a level, module, message, and millisecond uptime timestamp. Messages are sent to the serial console, retained in a 200-entry RAM ring, and appended to LittleFS when it is available. The manager uses mutexes for the log ring and filesystem operations.

## Storage

At startup the manager calls `LittleFS.begin(true)` and creates `/logs` if needed. The `true` argument enables the filesystem library's format-on-mount-failure behavior, so a failed mount may erase prior filesystem contents. When filesystem access succeeds, each log entry is appended to:

- `/logs/system.log`: timestamp, level, module, and message on separate lines.
- `/logs/agent.log`: one JSON object per line with numeric `timestamp`, `level`, `module`, and `message`.

When system time is valid, the system-log timestamp is formatted as UTC. Otherwise it falls back to `uptime-ms={milliseconds}`. The JSONL timestamp remains the uptime millisecond value.

The files are append-only in the current implementation; no rotation, retention limit, or download endpoint is implemented. If LittleFS initialization fails, logging continues to Serial and RAM. A filesystem write failure is reflected in the error counter, with one console warning to avoid repeatedly reporting the same storage fault.

## Counters and API

The manager counts MQTT messages received/sent, MQTT reconnects, Wi-Fi reconnects, registered devices, warnings, and errors. The counters and RAM log ring are runtime state.

- `GET /api/diagnostics` returns uptime, current/minimum heap, Wi-Fi/MQTT connection flags, registered device count, message/reconnect counters, errors, and warnings.
- `GET /api/logs` returns `count` and the current RAM ring as `logs`, with each entry containing `timestamp`, `level`, `module`, and `message`.

`/api/logs` does not read back the LittleFS files. Persistent files are not included in its response.

## HealthMonitor Relationship

`HealthMonitor` records its state transitions and anomaly events through `DiagnosticsManager`, so health findings also appear in the diagnostic streams. The health event ring is separate from the diagnostics ring and is exposed by `GET /api/health/events`.
