# API Reference

## General

The firmware registers an asynchronous HTTP server on port 80. The API has no authentication and uses plain HTTP. Keep it on a trusted network; the reboot/reset and device-command routes change device state.

All API responses use `application/json`, except the local HTML portal routes. The API constructs most response JSON in firmware.

Several response handlers concatenate string fields directly rather than applying a JSON string encoder. Values such as SSIDs and device metadata containing quotes or control characters may therefore produce invalid JSON.

## Read Endpoints

| Method and path | Parameters | Successful response |
|---|---|---|
| `GET /api/status` | None | `status` (`"online"`), `ip`, and `ssid`. |
| `GET /api/config` | None | `hubName`, saved `ssid`, and current local `ip`. Does not return the saved password. |
| `GET /api/networks` | None | `count` and `networks` array of scanned SSID strings. |
| `GET /api/device` | None | `hubId`, `chip` (`"ESP32"`), `cores`, `flashMB`, current free `heap`, and uptime in seconds. `cores` and `flashMB` are currently fixed response values. |
| `GET /api/mqtt` | None | `connected`, `broker`, and `port`. |
| `GET /api/devices` | None | `count` and `devices` array. Each list item contains `id`, `name`, `type`, and `state`. |
| `GET /api/deviceinfo?id={id}` | Required query parameter `id` | Device `id`, `name`, `type`, `state`, `voltage`, `current`, `power`, and `energy`. |
| `GET /api/diagnostics` | None | Uptime, heap, Wi-Fi/MQTT connection state, registered-device and MQTT/Wi-Fi reconnect/message counters, errors, and warnings. |
| `GET /api/logs` | None | `count` and current RAM `logs` array; each entry has `timestamp`, `level`, `module`, and `message`. |
| `GET /api/health` | None | Current health summary described below. |
| `GET /api/health/events` | None | `count` and health `events` array with `category`, `message`, and millisecond `timestamp`. |

### Health response

`GET /api/health` returns:

```json
{
  "status": "OK",
  "heap": 0,
  "minHeap": 0,
  "averageHeap": 0,
  "wifiConnected": true,
  "mqttConnected": true,
  "deviceCount": 0,
  "offlineDevices": 0,
  "warningDevices": 0,
  "rssi": -50,
  "wifiReconnects": 0,
  "mqttReconnects": 0,
  "loopGapMs": 5000,
  "mqttLoopAgeMs": 5000
}
```

The values above illustrate field names and types, not a measured device response. `rssi` is `null` when Wi-Fi is not connected. `status` is `OK`, `WARNING`, or `CRITICAL`.

## Device Commands

| Method and path | Parameters | Successful response |
|---|---|---|
| `POST /api/device/on` | `id` in query parameters or form/body parameters | `{"result":"ok"}` after publishing `{"action":"turn_on"}`. |
| `POST /api/device/off` | `id` in query parameters or form/body parameters | `{"result":"ok"}` after publishing `{"action":"turn_off"}`. |
| `POST /api/device/reboot` | `id` in query parameters or form/body parameters | `{"result":"ok"}` after publishing `{"action":"reboot"}`. |

The command is sent to `ampercore/device/{id}/command`. `id` can be a query parameter or a body parameter supported by `ESPAsyncWebServer`; the handler does not parse a raw JSON request body. These routes require the device to exist in the current in-memory registry and MQTT publishing to succeed. They do not restart the hub; the action is addressed to the target device.

## Hub Operations

| Method and path | Effect | Successful response |
|---|---|---|
| `POST /api/reboot` | Sends a response and restarts the ESP32. | `{"result":"rebooting"}` |
| `POST /api/reset` | Clears saved Preferences, sends a response, and restarts the ESP32. | `{"result":"resetting"}` |

## Error Responses

| Status | Condition | Example body |
|---|---|---|
| `400` | Required or empty command `id`; missing `id` on device info | `{"error":"missing id"}` or `{"error":"empty id"}` |
| `404` | Requested device does not exist | `{"error":"device not found"}` |
| `503` | Command cannot be published, including when MQTT is disconnected | `{"error":"command publish failed"}` |

## Local Web Portal Routes

These are HTML portal routes, not API JSON endpoints:

- `GET /` renders the setup/status page.
- `GET /scan` requests a Wi-Fi scan and returns a page that redirects to `/`.
- `GET /save?ssid=...&password=...&hub=...` saves Wi-Fi credentials and hub name, then restarts the ESP32. Because this uses GET over HTTP, credentials may appear in URLs and client/browser history.
