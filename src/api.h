#pragma once

// Web server on port 80: the web page, the HTTP API (see API.md) and firmware upload.

#include <Arduino.h>

namespace Api {

void begin();
void loop();

// GET /api/info: identity and capabilities; also the MQTT discovery payload.
String infoJson();
// GET /api/controller; live adds the fields that change every request (uptime, ms ago).
String controllerJson(bool live);
// Shared by HTTP and MQTT. which: 1, plug or all. state: on, off or toggle.
// source (e.g. "http 192.168.22.10", "mqtt") goes to the log.
bool setOutlet(const String &which, const String &state, String &message, const String &source);

// A light change from HTTP or MQTT; "" = not given. Giving only color, brightness or effect
// turns the light on.
struct LightRequest {
  String state;       // on, off or toggle
  String brightness;  // 0-255 (0 = off)
  String hex;         // #rrggbb
  String effect;      // None, Pulse, Fast Pulse
  String transition;  // seconds, 0-60
};
bool setLight(const LightRequest &req, String &message, const String &source);
// {"state":"on","brightness":26,"hex":"#ffffff","effect":"None"}; with effects, also the list.
String lightJson(bool effects);

void scheduleRestart(uint32_t delayMs);

}  // namespace Api
