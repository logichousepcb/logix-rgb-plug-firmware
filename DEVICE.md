# RGB Night-Light Smart Plug — xTool M1

Device survey taken **2026-11-01** from the running Tasmota firmware at `192.168.22.79`,
using the Tasmota HTTP command API (`/cm?cmnd=...`) and the config dump (`/dl`). Nothing on
the device was changed.

## 1. Summary

| Item | Value |
|---|---|
| Product | Plug-in smart plug with an **RGB night light**, 1 switched outlet, **no energy monitoring**. Tasmota template name `RGB Light`; the exact make/model isn't recorded and no public template matches the pin map (see §12) |
| Location | Drives the **xTool M1** laser engraver |
| MCU | **ESP8266EX** with external flash, running at 80 MHz |
| Flash | **1 MB** (chip ID `0x1440A1`), flash mode DOUT (3), 40 MHz |
| Energy meter IC | None (no HLW/BL/CSE pins in the template, no ENERGY section in `Status 10`) |
| Outlets / relays | 1 relay: Relay1 = GPIO15 (Tasmota `POWER1`) |
| Light | RGB, 3 PWM channels: red GPIO0, green GPIO2, blue GPIO5 (Tasmota `POWER2`) |
| Buttons | 1 GPIO button: Button1 = GPIO13, active low, external pull-up |
| LEDs | No separate status or link LED |
| Current firmware | Tasmota **12.0.2** (build 2022-06-20), core 2.7.4.9, SDK 2.2.2-dev |

Electrically this is the Geekbes YM-WS-1 (Logix Plug) with the status LED replaced by an RGB
light. The relay and the button are on the same pins.

## 2. Network identity

| Item | Value |
|---|---|
| IP address | 192.168.22.79 / 255.255.255.0, via DHCP (`IPAddress1 0.0.0.0`), `WifiConfig 2` = WifiManager |
| Gateway / DNS | 192.168.22.1 / 192.168.22.1 (DNS2 0.0.0.0) |
| MAC | `B4:E6:2D:0A:F8:87` |
| Hostname | `tasmota-0AF887-6279` (Tasmota default, pattern `%s-%04d` from the default topic) |
| Wi-Fi SSID | `ESP` (SSID2 empty), channel 11, 802.11n, BSSID `06:EC:DA:14:77:72` |
| Signal | **−79 dBm** (RSSI 42%), Wi-Fi TX power 17.0 dBm. Weak: see §9 |
| Web server | Enabled, **Admin mode**. The HTTP API answers with no credentials, so there is no web password in effect |
| HTTP API | Enabled |

## 3. GPIO map (from the active template)

Template: `{"NAME":"RGB Light","GPIO":[416,0,417,0,0,418,0,0,0,64,0,224,0,0],"FLAG":0,"BASE":18}`

The ESP8266 template array is in the order GPIO0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16, A0.

| GPIO | Tasmota code | Function | Notes |
|---|---|---|---|
| GPIO0 | 416 | PWM 1 | Light, **red**, active high. Boot strap pin, must be high at boot |
| GPIO1 | 0 | None | UART TX |
| GPIO2 | 417 | PWM 2 | Light, **green**, active high. Boot strap pin, must be high at boot |
| GPIO3 | 0 | None | UART RX |
| GPIO4 | 0 | None | — |
| GPIO5 | 418 | PWM 3 | Light, **blue**, active high |
| GPIO9 | 0 | None | **Do not use:** wired to the external flash on an ESP8266EX |
| GPIO10 | 0 | None | **Do not use:** wired to the external flash on an ESP8266EX |
| GPIO12 | 0 | None | — (the Geekbes has its LED here; this plug has none) |
| GPIO13 | 64 | Button_n 1 | Push button, active low, **no internal pull-up** in Tasmota (`_n`), so the board has an external one |
| GPIO14 | 0 | None | — |
| GPIO15 | 224 | Relay 1 | Outlet relay, active high. Boot strap pin, must be low at boot (fine for a relay driver) |
| GPIO16 | 0 | None | — |
| A0 | 0 | None | Not used |

The red/green/blue order is Tasmota's channel order (`SetOption37 0`, no channel remap): PWM1 is
channel 1 = red. Which pin is which colour still needs a look on site (§12).

## 4. Button

| Setting | Value | Meaning |
|---|---|---|
| ButtonDebounce | 50 ms | — |
| ButtonTopic | 0 | Button acts on the local power devices |
| SetOption1 | OFF | Multi-press and hold actions are allowed |
| SetOption11 | OFF | Single and double press are not swapped |
| SetOption13 | OFF | Multi-press detection is on |
| SetOption32 | 40 | Hold time 4 s |
| SetOption73 | OFF | Button is not decoupled from the power devices |
| LedState | 1 | No LED pin, so it has no effect |

With two power devices on one button, Tasmota's multi-press maps **single press → `POWER1`
(relay)** and **double press → `POWER2` (light)**. The replacement firmware does the same.

## 5. Light

| Setting | Value | Meaning |
|---|---|---|
| Light power | `POWER2` OFF at survey | The light is Tasmota's second power device |
| Color | `1A1A1A` = white at dimmer 10 (`HSBColor 0,0,10`, channels 10/10/10 %) | — |
| Saved color / dimmer | `light_color` 255,255,255 (full scale), `light_dimmer` 10 | See §10 |
| Scheme / Fade / Speed | 0 / OFF / 1 | No effects or fades in use |
| LedTable | ON | Gamma correction on |
| PWM | Frequency 880 Hz, range 1023, `DimmerRange` 0–100 | — |
| RGBWWTable | 255,255,255,255,255 | No channel scaling |
| SetOption15 / 16 / 20 / 68 | ON / OFF / OFF / OFF | Normal RGB light, `POWER2` switches it, channels not split into separate PWM lights |
| WakeupDuration | 60 | Not used |

## 6. Behaviour and configuration

| Setting | Value | Meaning |
|---|---|---|
| PowerOnState | 3 | Restore the last saved relay **and light** state after a power cycle |
| SaveState / SaveData | 1 / 1 | Power state and settings are saved to flash |
| SetOption0 | ON | Save power state |
| Interlock | OFF | — |
| PulseTime | all 0 | No auto-off |
| SetOption65 | OFF | Fast power-cycle factory reset **enabled** in Tasmota |
| SetOption26 | OFF | — |
| StateText1 / 2 | OFF / ON | — |
| Sleep | 50 ms (dynamic) | — |
| TelePeriod | 300 s | — |
| Timers | All 16 disabled, master OFF | — |
| Rules 1/2/3 | Empty, all OFF | — |
| Friendly names | `xTool M1` / `xTool M1` (relay / light) | Device name `xTool M1` |
| State at survey | Relay OFF, light OFF | — |
| Boot count / saves | 155 boots / 683 flash saves | Since 2022-08-09 |
| Uptime at survey | 29 min | Restart reason: Power On |
| Free memory | Heap ≈ 22–23 KB; program 619 KB, free sketch space 384 KB | — |
| Serial | 115200 8N1, SerialLog 2, WebLog 2 | — |

The stored `OtaUrl` is `http://ota.tasmota.com/tasmota/release-9.1.0/tasmota-lite.bin.gz`,
as on the Geekbes: first flashed with 9.1.0 lite, later upgraded to 12.0.2 by file upload.

## 7. MQTT

| Item | Value |
|---|---|
| Broker | `192.168.1.130:1883` (different subnet from the device) |
| User | `esp` (password not printed) |
| Client ID | `DVES_0AF887` (mask `DVES_%06X`) |
| Topic | `tasmota_0AF887` (Tasmota default `tasmota_%06X`), group topic `tasmotas` |
| FullTopic | `%prefix%/%topic%/` |
| Connection | Connected (MqttCount 1) |
| Home Assistant discovery | Off (SetOption19 OFF) |
| Retain flags | All off |

## 8. Time and location

| Item | Value |
|---|---|
| NTP | `pool.ntp.org`, `nl.pool.ntp.org`, `0.nl.pool.ntp.org` (Tasmota defaults) |
| Timezone | `+01:00` with EU DST rules (**Tasmota defaults**) |
| Lat / Long | 48.858360, 2.294442 (**Tasmota defaults: Eiffel Tower**) |

All out-of-box defaults. The replacement firmware uses US Eastern, like the other Logix devices.

## 9. Flash and OTA constraints for the replacement firmware

- **1 MB total flash**, same layout as the other plugs. Tasmota reports **384 KB** free for an
  upload, so the first install must be the **`.bin.gz`** (Logix RGB Plug 1.0.0: 307 KB; the
  plain `.bin` is 434 KB and too big).
- ESP8266EX with external flash: GPIO9/10 stay untouched.
- GPIO0 and GPIO2 carry the light. They are boot strap pins (must be high at boot); Tasmota
  drives them the same way, and the firmware only starts PWM after boot.
- **Weak Wi-Fi (−79 dBm).** If the plug can't hold the connection after the switch, it falls
  back to its setup AP after a minute and keeps retrying the saved network. The relay and
  button keep working throughout.
- **Moving off Tasmota:** Firmware Upgrade → Upgrade by file upload → `firmware.bin.gz`. A
  failed install means opening the plug for serial flashing (TX/RX/GND/3V3 + GPIO0 to GND).
- Keep a working rollback path. Never ship an image that cannot accept the next OTA.

## 10. Tasmota settings sector (for the first-boot import)

- Tasmota **12.0.2**, `SaveAddress` = **`F6000`**, `CR 368/699`, `CfgHolder 4617`.
- 12.0.2 rotates its settings through the eight sectors `0xF4000`–`0xFB000`; the current copy
  is the one with the highest `save_flag`. `0xF6000` lies inside the firmware's LittleFS
  (`0xEB000`–`0xFB000`), so the "scan all eight sectors before mounting LittleFS" import
  applies unchanged. Note this plug's current copy is at `F6000`, not `FA000` as on the Geekbes.
- The config dump `/dl` was decoded once in a scratch folder (passwords never printed) to
  confirm the layout. It matched the Logix Plug's: `save_flag` 683 (= `SaveCount`), version
  `0x0C000200`, power 0 at `0x2E8`, MQTT port 1883 at `0xEFC`, SSID `ESP` at text 4, MQTT
  host/user at texts 12/14, friendly name 1 `xTool M1` at text 44 (and name 2 at text 45).
- **Light fields** (new for this device), confirmed in the dump:

  | Offset | Field | Value on this plug |
  |---|---|---|
  | `0x2E8` bit 1 | saved `POWER2` (light) | 0 (off) |
  | `0x498`–`0x49C` | `light_color[5]`, color at full scale | 255,255,255,255,255 |
  | `0x49D` | `light_correction` (LedTable) | 1 |
  | `0x49E` | `light_dimmer`, 0–100 | 10 |
  | `0x4A1`–`0x4A3` | `light_fade` / `light_speed` / `light_scheme` | 0 / 1 / 0 |

  The firmware imports on/off, the color (first three bytes) and the dimmer (scaled to
  brightness 0–255, so 10 → 26).

## 11. Raw data

`Status 0` from the survey (trimmed of `SetOption`/`Features` bitmaps):

```json
{"Status":{"Module":0,"DeviceName":"xTool M1","FriendlyName":["xTool M1","xTool M1"],"Topic":"tasmota_0AF887","ButtonTopic":"0","Power":0,"PowerOnState":3,"LedState":1,"LedMask":"FFFF","SaveData":1,"SaveState":1,"SwitchTopic":"0","SwitchMode":[0,0,0,0,0,0,0,0]},
 "StatusPRM":{"Baudrate":115200,"SerialConfig":"8N1","GroupTopic":"tasmotas","OtaUrl":"http://ota.tasmota.com/tasmota/release-9.1.0/tasmota-lite.bin.gz","RestartReason":"Power On","BootCount":155,"BCResetTime":"2022-08-09T09:50:53","SaveCount":683,"SaveAddress":"F6000"},
 "StatusFWR":{"Version":"12.0.2(tasmota)","BuildDateTime":"2022-06-20T12:41:03","Boot":4,"Core":"2_7_4_9","SDK":"2.2.2-dev(38a443e)","CpuFrequency":80,"Hardware":"ESP8266EX","CR":"368/699"},
 "StatusMEM":{"ProgramSize":619,"Free":384,"Heap":22,"ProgramFlashSize":1024,"FlashSize":1024,"FlashChipId":"1440A1","FlashFrequency":40,"FlashMode":3},
 "StatusNET":{"Hostname":"tasmota-0AF887-6279","IPAddress":"192.168.22.79","Gateway":"192.168.22.1","Subnetmask":"255.255.255.0","DNSServer1":"192.168.22.1","DNSServer2":"0.0.0.0","Mac":"B4:E6:2D:0A:F8:87","Webserver":2,"HTTP_API":1,"WifiConfig":2,"WifiPower":17.0},
 "StatusMQT":{"MqttHost":"192.168.1.130","MqttPort":1883,"MqttClientMask":"DVES_%06X","MqttClient":"DVES_0AF887","MqttUser":"esp","MqttCount":1,"MAX_PACKET_SIZE":1200,"KEEPALIVE":30,"SOCKET_TIMEOUT":4},
 "StatusSTS":{"POWER1":"OFF","POWER2":"OFF","Dimmer":10,"Color":"1A1A1A","HSBColor":"0,0,10","Channel":[10,10,10],"Scheme":0,"Fade":"OFF","Speed":1,"LedTable":"ON","Wifi":{"AP":1,"SSId":"ESP","BSSId":"06:EC:DA:14:77:72","Channel":11,"Mode":"11n","RSSI":42,"Signal":-79}}}
```

## 12. Open items

- **Make/model:** not known. The template matches no public entry; a look at the label on
  the plug would settle it.
- **Light colours on site:** confirm GPIO0 = red, GPIO2 = green, GPIO5 = blue (set
  `#ff0000`, `#00ff00`, `#0000ff` and look).

### Resolved

- **Updated to the 1.0.0 release build on 2026-11-01** (`logix-rgb-plug-1.0.0.bin`, SHA-256
  `82694007…7dac85`) over the plug's own `/update`. It was back in about 8 s with the relay
  on and the light (`#8ff0a4`, brightness 119) restored, and MQTT connected. Two
  `presses=double` requests, as LogixDIY sends them, toggled the light off and on and left
  the relay alone.

- **Button confirmed 2026-11-01** under Logix RGB Plug: single presses switch the relay,
  double presses the light. 9 physical presses gave 7 actions (5 single, 2 double), with no
  double counts. The web page's light controls (color, brightness, Pulse) also worked.

- **Installed Logix RGB Plug 1.0.0 on 2026-11-01** over Tasmota's upload form
  (`firmware.bin.gz`, SHA-256 `799b6ea6…cba5b5`). The first boot imported the Tasmota 12.0.2
  settings from **`0xFB000`**: Tasmota writes a fresh copy there on the restart after an
  upload, which has the highest `save_flag`, so the survey's `F6000` was no longer the current
  copy. WiFi `ESP`, MQTT `192.168.1.130:1883`, the name `xTool M1`, relay off and light off
  (white, brightness 26) all came across. It rejoined at 192.168.22.79 as `xtool-m1`, on a
  closer AP this time (−64 dBm, channel 1), and connected to MQTT. Free space for the next
  update is 504 KB, so plain `firmware.bin` uploads work from now on.
