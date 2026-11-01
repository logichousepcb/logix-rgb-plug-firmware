#include "settings.h"

#include <ESP8266WiFi.h>

#include <memory>

#include "config.h"
#include "log.h"
#include "storage.h"

namespace {

const uint32_t SETTINGS_MAGIC = 0x4C4F5531;  // "LOU1"
const char *const SETTINGS_PATH = "/settings.bin";

Settings::Data g_s = {};

template <size_t N>
void setField(char (&field)[N], const String &value) {
  memset(field, 0, N);
  strncpy(field, value.c_str(), N - 1);
}

template <size_t N>
void terminate(char (&field)[N]) {
  field[N - 1] = '\0';
}

// ---- Tasmota import ----
// Tasmota on a 1 MB ESP8266 keeps its Settings struct in one of the sectors 0xF4-0xFB. It
// rotates through all eight, and the copy with the highest save_flag is current (DEVICE.md
// §10). Those sectors overlap our LittleFS, so they are read into RAM before it is mounted
// (and formatted on the first boot). Offsets confirmed on a config dump of this plug's
// Tasmota 12.0.2 (light fields included), the Logix Plug's 12.0.2 and the WO1 outlets' 11.1.0
// and 12.0.2.
const uint32_t TASMOTA_FIRST_SECTOR = 0xF4, TASMOTA_LAST_SECTOR = 0xFB;
const uint32_t FS_FIRST_SECTOR = 0xEB, FS_END_SECTOR = 0xFB;  // eagle.flash.1m64.ld
const size_t TASMOTA_SIZE = 4096;
const size_t T_SAVE_FLAG = 0x004, T_VERSION = 0x008;
const size_t T_TEXT_POOL = 0x017, T_TEXT_POOL_LEN = 699;
const size_t T_POWER = 0x2E8;         // uint32, saved power states, bit 0 = relay, bit 1 = light
const size_t T_LIGHT_COLOR = 0x498;   // uint8[5], color at full scale (R, G, B used)
const size_t T_LIGHT_DIMMER = 0x49E;  // uint8, 0-100
const size_t T_MQTT_PORT = 0xEFC;     // uint16
// Text pool order (SettingsTextIndex). The first T_TEXTS must be there; the friendly name
// further on is optional.
enum { T_SSID1 = 4, T_PWD1 = 6, T_MQTT_HOST = 12, T_MQTT_USER = 14, T_MQTT_PWD = 15, T_TEXTS = 16 };
enum { T_FRIENDLY1 = 44 };
const char *const TASMOTA_DEFAULT_NAME = "Tasmota";

struct TasmotaImport {
  uint32_t saveFlag;  // 0 = nothing found
  uint32_t sector;
  uint32_t version;
  char ssid[33], pass[65], mqttHost[65], mqttUser[65], mqttPass[65];
  char name[33];  // "" = none worth keeping
  uint16_t mqttPort;
  uint8_t power;
  Light::State light;
};
TasmotaImport g_tasmota = {};

uint32_t u32(const uint8_t *b, size_t off) { uint32_t v; memcpy(&v, b + off, 4); return v; }
uint16_t u16(const uint8_t *b, size_t off) { uint16_t v; memcpy(&v, b + off, 2); return v; }

template <size_t N>
void copyText(char (&field)[N], const char *value) {
  memset(field, 0, N);
  strncpy(field, value, N - 1);
}

// Fills `out` from one sector; false if it isn't a usable Tasmota settings copy.
bool parseTasmota(const uint8_t *b, TasmotaImport &out) {
  uint32_t version = u32(b, T_VERSION);
  uint8_t major = version >> 24;
  if (u16(b, 2) != TASMOTA_SIZE || major < 9 || major > 14) return false;

  const char *texts[T_TEXTS];
  const char *p = reinterpret_cast<const char *>(b + T_TEXT_POOL);
  const char *end = p + T_TEXT_POOL_LEN;
  for (int i = 0; i < T_TEXTS; i++) {
    const char *nul = static_cast<const char *>(memchr(p, '\0', end - p));
    if (!nul) return false;
    texts[i] = p;
    p = nul + 1;
  }
  if (strncmp(texts[0], "http", 4) != 0) return false;  // OTA URL: sanity check of the layout
  size_t ssidLen = strlen(texts[T_SSID1]);
  if (ssidLen == 0 || ssidLen > 32 || strlen(texts[T_PWD1]) > 64) return false;

  out = {};
  out.saveFlag = u32(b, T_SAVE_FLAG) | 1;
  out.version = version;
  copyText(out.ssid, texts[T_SSID1]);
  copyText(out.pass, texts[T_PWD1]);
  copyText(out.mqttHost, texts[T_MQTT_HOST]);
  copyText(out.mqttUser, texts[T_MQTT_USER]);
  copyText(out.mqttPass, texts[T_MQTT_PWD]);
  uint16_t port = u16(b, T_MQTT_PORT);
  out.mqttPort = port ? port : 1883;
  uint32_t power = u32(b, T_POWER);
  out.power = power & 0x01;
  uint8_t dimmer = b[T_LIGHT_DIMMER] > 100 ? 100 : b[T_LIGHT_DIMMER];
  out.light = {(power & 0x02) != 0, b[T_LIGHT_COLOR], b[T_LIGHT_COLOR + 1], b[T_LIGHT_COLOR + 2],
               (uint8_t)((dimmer * 255 + 50) / 100), Light::EFFECT_NONE};

  for (int i = T_TEXTS; i <= T_FRIENDLY1; i++) {
    const char *nul = static_cast<const char *>(memchr(p, '\0', end - p));
    if (!nul) return true;  // older layout: no name
    if (i == T_FRIENDLY1 && strcmp(p, TASMOTA_DEFAULT_NAME) != 0 && Settings::hostnameFor(p).length())
      copyText(out.name, p);
    p = nul + 1;
  }
  return true;
}

void applyTasmota() {
  const TasmotaImport &t = g_tasmota;
  setField(g_s.ssid, t.ssid);
  setField(g_s.pass, t.pass);
  setField(g_s.mqttHost, t.mqttHost);
  setField(g_s.mqttUser, t.mqttUser);
  setField(g_s.mqttPass, t.mqttPass);
  g_s.mqttPort = t.mqttPort;
  logf("[tasmota] imported %u.%u.%u settings from 0x%05lX: WiFi \"%s\", MQTT %s:%u", t.version >> 24,
       (t.version >> 16) & 0xFF, (t.version >> 8) & 0xFF, (unsigned long)t.sector * SPI_FLASH_SEC_SIZE, g_s.ssid,
       g_s.mqttHost[0] ? g_s.mqttHost : "(off)", g_s.mqttPort);
  if (t.name[0] && Settings::setName(t.name).isEmpty()) logf("[tasmota] imported name \"%s\"", g_s.name);
}

}  // namespace

namespace Settings {

void begin() {
  bool loaded = Storage::load(SETTINGS_PATH, g_s, SETTINGS_MAGIC);
  if (!loaded) {
    g_s = {};
    g_s.mqttPort = 1883;
  }
  terminate(g_s.name);
  terminate(g_s.ssid);
  terminate(g_s.pass);
  terminate(g_s.admin);
  terminate(g_s.mqttHost);
  terminate(g_s.mqttUser);
  terminate(g_s.mqttPass);
  terminate(g_s.mqttTopic);
  if (!g_s.name[0] || hostnameFor(g_s.name).isEmpty()) setField(g_s.name, defaultName());

  if (!g_s.tasmotaImported) {
    if (g_tasmota.saveFlag) applyTasmota();
    g_s.tasmotaImported = 1;
    // Tasmota's copy outside LittleFS holds the WiFi and MQTT passwords; don't leave it.
    if (save() && g_tasmota.saveFlag && !ESP.flashEraseSector(TASMOTA_LAST_SECTOR))
      logf("[tasmota] could not erase the old settings sector");
  } else if (!loaded) {
    save();
  }
}

void readTasmota() {
  std::unique_ptr<uint32_t[]> buf(new (std::nothrow) uint32_t[TASMOTA_SIZE / 4]);
  if (!buf) return;
  const uint8_t *b = reinterpret_cast<const uint8_t *>(buf.get());
  TasmotaImport candidate;
  bool inFs[TASMOTA_LAST_SECTOR + 1] = {};
  for (uint32_t sector = TASMOTA_FIRST_SECTOR; sector <= TASMOTA_LAST_SECTOR; sector++) {
    if (!ESP.flashRead(sector * SPI_FLASH_SEC_SIZE, buf.get(), TASMOTA_SIZE)) continue;
    if (!parseTasmota(b, candidate)) continue;
    candidate.sector = sector;
    inFs[sector] = sector >= FS_FIRST_SECTOR && sector < FS_END_SECTOR;
    if (candidate.saveFlag > g_tasmota.saveFlag) g_tasmota = candidate;
  }
  // Copies inside the LittleFS area must go before it is mounted: erasing them later would
  // erase blocks the file system may be using. They are in RAM now.
  for (uint32_t sector = TASMOTA_FIRST_SECTOR; sector < FS_END_SECTOR; sector++) {
    if (inFs[sector]) ESP.flashEraseSector(sector);
  }
}

int tasmotaOutlets() { return g_tasmota.saveFlag ? g_tasmota.power : -1; }

const Light::State *tasmotaLight() { return g_tasmota.saveFlag ? &g_tasmota.light : nullptr; }

Data &get() { return g_s; }

bool save() {
  if (Storage::save(SETTINGS_PATH, g_s, SETTINGS_MAGIC)) return true;
  logf("[settings] failed to save");
  return false;
}

String deviceId() {
  String id = WiFi.macAddress();
  id.replace(":", "");
  id.toLowerCase();
  return id;
}

String defaultName() {
  String suffix = deviceId().substring(8);
  suffix.toUpperCase();
  return String(LOGIX_MODEL_NAME " ") + suffix;
}

String name() { return g_s.name; }

String outletName(uint8_t) { return name(); }

String hostnameFor(const String &n) {
  String h;
  bool dash = false;
  for (size_t i = 0; i < n.length() && h.length() < 32; i++) {
    char c = tolower(n[i]);
    if (isalnum(c)) {
      if (dash && h.length()) {
        if (h.length() >= 31) break;
        h += '-';
      }
      h += c;
      dash = false;
    } else {
      dash = true;
    }
  }
  return h;
}

String hostname() { return hostnameFor(g_s.name); }

String setName(const String &value) {
  String n = value;
  n.trim();
  if (n.isEmpty() || n.length() > 32) return "Name must be 1-32 characters.";
  for (size_t i = 0; i < n.length(); i++) {
    if ((uint8_t)n[i] < 0x20 || n[i] == '"' || n[i] == '\\') return "Name can't contain quotes, backslashes or control characters.";
  }
  if (hostnameFor(n).isEmpty()) return "Name needs at least one letter or digit.";
  setField(g_s.name, n);
  return save() ? "" : "Failed to save the name to flash.";
}

}  // namespace Settings
