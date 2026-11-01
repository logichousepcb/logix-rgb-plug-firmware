# Logix RGB Plug HTTP and MQTT API

All endpoints are on port 80 of the device (e.g. `http://192.168.22.79` or `http://<hostname>.local`).

- **Requests:** `POST` parameters are sent form-encoded (`application/x-www-form-urlencoded`) or as a query string.
- **Responses:** JSON.
- **Authentication:** none by default. Once an admin password is set, every endpoint except `GET /api/info` requires HTTP Basic auth with user `admin` (for curl: `-u admin:<password>`).

Actions return `{"ok":true,"message":"..."}` with HTTP 200. Errors return `{"ok":false,"message":"..."}` with HTTP 400. Actions that restart the device also return `"restart":true`.

**The plug** has one outlet, number 1 with key `plug`. Anywhere an outlet is given, `1`, `plug` or `all` is accepted. The outlet carries the device name (e.g. `xTool M1`). The outlet and button have the same shape as the Logix Plug's (arrays of outlets and buttons, one entry each).

**The light** is a separate RGB light with the same API as the Logix Vent's status light.

**The button:** a single press toggles the plug, a double press (two presses within 0.4 s) toggles the light.

# Identifying the device

The device follows the same Logix contract as the Logix Plug, Logix Outlet, Logix 3 and Logix Vent, so the LogixDIY integration can discover it.

## mDNS / zeroconf

The device advertises `_logix._tcp.local.` (and `_http._tcp`) on port 80, with these TXT records:

| Key | Example | Meaning |
|---|---|---|
| `model` | `logixrgbplug` | machine-readable model; stays the same across firmware versions |
| `id` | `b4e62d0af887` | lowercase MAC without separators; stable unique ID |
| `fw` | `1.0.0` | firmware version |
| `api` | `1` | API contract version; increases only on breaking changes |
| `name` | `xTool M1` | device name |
| `manufacturer` | `Logix` | |

## GET /api/info

Never requires a password.

```json
{
  "manufacturer": "Logix", "model": "logixrgbplug", "name": "xTool M1",
  "id": "b4e62d0af887", "mac": "B4:E6:2D:0A:F8:87", "hostname": "xtool-m1", "ip": "192.168.22.79",
  "fw": "1.0.0", "api": 1, "authRequired": false,
  "capabilities": {
    "outlets": 1, "buttons": 1,
    "outletKeys": ["plug"],
    "outletNames": ["xTool M1"],
    "actions": ["single", "double"],
    "statusLight": {"type": "rgb", "effects": ["None", "Pulse", "Fast Pulse"]},
    "energy": false, "energyMeters": 0
  },
  "mqtt": {"enabled": true, "connected": true, "topic": "logixrgbplug/b4e62d0af887"}
}
```

## MQTT discovery

When MQTT is enabled, the same JSON is published, retained, to `logix/discovery/<id>`.

# The plug

## Read everything

`GET /api/controller`

```json
{
  "uptimeMs": 81234,
  "name": "xTool M1",
  "outlets": [
    {"outlet": 1, "key": "plug", "name": "xTool M1", "state": "on"}
  ],
  "buttons": [
    {"button": 1, "key": "plug", "pressed": false, "pressCount": 5, "lastAction": "double", "actionCount": 4, "lastActionMsAgo": 5120}
  ],
  "light": {"state": "on", "brightness": 26, "hex": "#ffffff", "effect": "None"}
}
```

- `buttons[0].pressCount`: physical presses since boot (a double press counts 2).
- `buttons[0].actionCount`: physical and simulated actions since boot (a double press counts 1). When polling, a change means a new action; `lastAction` says which (`single`, `double`, or `none` before the first).
- `light.hex`: the color at full brightness; `brightness` (1-255) scales it.

## Switch the plug

`POST /api/outlet`

| Parameter | Value |
|---|---|
| `outlet` | `1`, `plug` or `all` (default: the plug) |
| `state` | `on`, `off` or `toggle` (default `toggle`) |

```sh
curl -X POST -d "outlet=plug&state=on" http://192.168.22.79/api/outlet
# {"ok":true,"message":"Plug on."}
```

The new state is saved to flash immediately and restored after a power cut.

## Simulate a button press

`POST /api/controller/press` with `button=1|plug` and `action=single|double` (default `single`). This behaves exactly like the physical button: `single` toggles the plug, `double` toggles the light, and both count as an action.

# The light

## Read

`GET /api/light`

```json
{"state": "on", "brightness": 26, "hex": "#ffffff", "effect": "None", "effects": ["None", "Pulse", "Fast Pulse"]}
```

## Change

`POST /api/light`

| Parameter | Value |
|---|---|
| `state` | `on`, `off` or `toggle` |
| `brightness` | 1-255 (`0` turns the light off) |
| `hex` (or `color`) | `#rrggbb`, not black |
| `effect` | `None`, `Pulse` or `Fast Pulse` |
| `transition` | fade time in seconds, 0-60 (default 0.5) |

Give any combination. Giving a color, brightness or effect without `state` also turns the light on.

```sh
curl -X POST -d "hex=%23ff8000&brightness=128" http://192.168.22.79/api/light
# {"ok":true,"message":"Light on."}
curl -X POST -d "state=off&transition=3" http://192.168.22.79/api/light
```

The light state is saved to flash 2 s after the last change and restored after a power cut.

# Device

| Endpoint | Purpose |
|---|---|
| `GET /api/info` | identity and capabilities (no password required) |
| `GET /api/status` | firmware, network, uptime, heap, raw button state, light summary |
| `POST /api/device` | `name` (1-32 chars; restarts if the hostname changes), `admin` (4-64 chars), `adminClear=1` |
| `GET /api/scan` | WiFi scan; returns `{"scanning":true}` until the results are ready; `?fresh=1` starts a new scan |
| `POST /api/wifi` | `ssid`, `pass` (blank for an open network); saves and restarts |
| `POST /api/forget` | erase the saved network and restart into the setup AP |
| `GET /api/mqtt` | MQTT settings and connection status (the password is never returned) |
| `POST /api/mqtt` | `host` (blank turns MQTT off), `port`, `user`, `pass` (blank keeps the saved one), `passClear=1`, `topic` (blank = default) |
| `GET /api/log` | recent log lines (plain text) |
| `POST /api/reboot` | restart |
| `POST /update` | firmware upload, multipart field `firmware`; `.bin` or `.bin.gz` |

# MQTT

Set the broker on the Settings page. The device connects with its hostname as the client ID and retries every 10 s. Every topic sits under the base topic `T`. The default is `logixrgbplug/<id>`. All messages are QoS 0.

## Published by the device

| Topic | Retained | Payload |
|---|---|---|
| `T/status` | yes | `online`, or `offline` (last will) |
| `T/state` | yes | same JSON as `GET /api/controller`, without `uptimeMs` and `lastActionMsAgo`. Sent on every change and at least every 60 s |
| `T/button/1/action` | no | `single` or `double`: one message per button action, physical or simulated |
| `T/result` | no | reply to each command: `{"command":"outlet","ok":true,"message":"Plug on."}` |

## Commands

| Topic | Payload | HTTP equivalent |
|---|---|---|
| `T/outlet/N/set` | `ON`, `OFF` or `TOGGLE` (N = `1`, `plug`, `all`) | `POST /api/outlet` |
| `T/set` | JSON: `{"plug":"on"}`, `{"outlet1":false}` or `{"all":"toggle"}` | `POST /api/outlet` |
| `T/light/set` | `ON`, `OFF`, `TOGGLE`, or JSON: `{"state":"on","brightness":128,"hex":"#ff0000","effect":"Pulse","transition":1}` (`"color":{"r":255,"g":0,"b":0}` also works) | `POST /api/light` |
| `T/press` | `1`, or `{"button":1,"action":"double"}` | `POST /api/controller/press` |
| `T/get` | anything | republishes `T/state` |

```sh
mosquitto_pub -h <broker> -t logixrgbplug/b4e62d0af887/outlet/plug/set -m ON
mosquitto_pub -h <broker> -t logixrgbplug/b4e62d0af887/light/set -m '{"hex":"#0000ff","effect":"Pulse"}'
mosquitto_sub -h <broker> -t 'logixrgbplug/b4e62d0af887/#' -v
```
