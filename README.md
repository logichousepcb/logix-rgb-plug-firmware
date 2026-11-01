<h1 align="center">Logix RGB Plug Firmware</h1>

<p align="center">
  Replacement firmware for an ESP8266 smart plug with an RGB night light (Tasmota template "RGB Light", 1 MB).<br>
  One switched outlet and one RGB light, controlled from its own web page, an HTTP API or MQTT.<br>
  Installs over the Tasmota web OTA form.
</p>

## Features

- **Works on its own:** the button works at all times, including with no WiFi, no MQTT, and while the network code is busy, because it is scanned from a timer and not from the main loop. A single press switches the plug and a double press switches the light, as on Tasmota.
- **Survives power cuts:** the plug's state is saved to flash the moment it changes. The light's state (on/off, color, brightness, effect) is saved two seconds after the last change. Both are restored at power-up, before WiFi starts.
- **RGB light:** color, brightness, smooth fades (0.5 s by default, adjustable per command) and two effects, `Pulse` and `Fast Pulse`. The light API is the same as the Logix Vent's status light. It is gamma corrected, like Tasmota's `LedTable ON`.
- **Name:** one device name (default `Logix RGB Plug XXXX`), which is also the outlet's name. The hostname follows the name (e.g. `xtool-m1.local`).
- **Settings page:** device name and admin password, WiFi (with scan), MQTT broker, firmware update.
- **Setup access point built in:** when there is no saved network, or the network is gone for a minute, an open AP `Logix-RGB-Plug-XXXX` comes up at `192.168.4.1` with a captive portal. The saved network keeps being retried, and the AP goes away once it is back.
- **Comes back from Tasmota with its settings:** on the first boot it imports Tasmota's WiFi network, MQTT broker, friendly name, relay state and light (on/off, color, dimmer), using the Tasmota 12.0.2 layout. It then erases Tasmota's settings.
- **LogixDIY ready:** mDNS `_logix._tcp`, a public `GET /api/info` and retained MQTT discovery on `logix/discovery/<id>`, the same contract as the Logix Plug. The outlet and button have the Logix Plug's shape. The light has the Logix Vent's `statusLight` shape.

See [API.md](API.md) for the HTTP and MQTT API and [DEVICE.md](DEVICE.md) for the hardware survey.

## Hardware

| GPIO | Function |
|---|---|
| 15 | Relay (active high) |
| 13 | Button (active low) |
| 0 | Light, red (PWM, active high) |
| 2 | Light, green (PWM, active high) |
| 5 | Light, blue (PWM, active high) |

There is no energy meter and no status LED. The pin map is hard-coded in [src/config.h](src/config.h).

## Building

[PlatformIO](https://platformio.org/):

```sh
pio run
```

This produces `.pio/build/logixrgbplug/firmware.bin` and `firmware.bin.gz`.

## Installing over Tasmota

Tasmota only has about 384 KB free for an upload, so **use `firmware.bin.gz`** (about 310 KB). The full `.bin` (about 435 KB) is too big for this step.

1. Open the Tasmota web page → **Firmware Upgrade** → **Upgrade by file upload**.
2. Choose `firmware.bin.gz` and click **Start upgrade**.
3. The device restarts into Logix RGB Plug. On this first boot it imports the WiFi network, MQTT broker and name from Tasmota and rejoins the network. Look for it at the same IP address (if DHCP hands it out again) or at `<name>.local`, e.g. `xtool-m1.local`.
4. If it can't join the network, connect to the WiFi network `Logix-RGB-Plug-XXXX` and set it up at `http://192.168.4.1`.

From the command line:

```sh
curl -s http://<tasmota-ip>/up > /dev/null
curl -F "u2=@.pio/build/logixrgbplug/firmware.bin.gz" http://<tasmota-ip>/u2
```

The plug and light keep the state Tasmota last saved. From then on they keep their state through power cuts and updates.

## Updating

Settings → **Firmware** accepts `firmware.bin` or `firmware.bin.gz`. Or:

```sh
curl -F "firmware=@.pio/build/logixrgbplug/firmware.bin.gz" http://<device>/update
```

### Going back to Tasmota

The full Tasmota image is larger than the free update space. Upload `tasmota-minimal.bin.gz` first, then the full Tasmota from its own upgrade page.

## License

[MIT](LICENSE)
