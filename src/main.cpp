// Logix RGB Plug: firmware for the RGB night-light smart plug (Tasmota template "RGB Light").
//
// Boot order matters: the plug and light are back in their saved state and the button is live before
// WiFi, MQTT or the web server are touched, and none of those ever block it.

#include <Arduino.h>

#include "api.h"
#include "config.h"
#include "light.h"
#include "log.h"
#include "mqtt.h"
#include "net.h"
#include "outlets.h"
#include "settings.h"
#include "storage.h"

void setup() {
  Serial.begin(115200);
  Settings::readTasmota();  // before LittleFS: Tasmota keeps its settings inside its area
  Storage::begin();         // formats LittleFS on the first boot after Tasmota
  Outlets::begin(Settings::tasmotaOutlets());
  Light::begin(Settings::tasmotaLight());
  logf("\n" LOGIX_MODEL_NAME " " FW_VERSION " (%s)", ESP.getResetReason().c_str());
  Settings::begin();
  logf("[sys] %s (%s)", Settings::name().c_str(), Settings::hostname().c_str());
  Net::begin();
  Api::begin();
  Mqtt::begin();
}

void loop() {
  Outlets::loop();
  Light::loop();
  Net::loop();
  Api::loop();
  Mqtt::loop();
}
