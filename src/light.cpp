#include "light.h"

#include <Ticker.h>
#include <math.h>

#include "config.h"
#include "log.h"
#include "storage.h"

namespace {

const uint32_t LIGHT_MAGIC = 0x4C524C31;  // "LRL1"
const char *const LIGHT_PATH = "/light.bin";

const uint8_t PINS[3] = {PIN_RED, PIN_GREEN, PIN_BLUE};
const char *const EFFECT_NAMES[Light::EFFECT_COUNT] = {"None", "Pulse", "Fast Pulse"};
const uint32_t PULSE_PERIOD_MS[Light::EFFECT_COUNT] = {0, 2000, 600};
const float PULSE_MIN = 0.1f;  // pulses dip to this fraction of the brightness
const float GAMMA = 2.2f;      // like Tasmota's LedTable ON

struct __attribute__((packed)) StoredLight {
  uint32_t magic;
  uint8_t on, r, g, b, brightness, effect;
  uint8_t reserved[2];
  uint32_t checksum;
};

// Shared between loop() and the timer callback, which only runs while loop() yields.
Light::State g_state = {false, 255, 255, 255, 255, Light::EFFECT_NONE};
volatile uint32_t g_changes = 0;
uint32_t g_handledChanges = 0;
uint32_t g_changedAt = 0;
bool g_savePending = false;

// Linear channel levels 0..1 (before gamma): fading from g_from to g_to.
float g_from[3] = {}, g_to[3] = {}, g_now[3] = {};
uint32_t g_fadeStart = 0, g_fadeMs = 0;
uint16_t g_duty[3] = {0xFFFF, 0xFFFF, 0xFFFF};

Ticker g_tick;

void target(float out[3]) {
  const Light::State &s = g_state;
  float k = s.on ? s.brightness / 255.0f : 0.0f;
  out[0] = s.r / 255.0f * k;
  out[1] = s.g / 255.0f * k;
  out[2] = s.b / 255.0f * k;
}

void retarget(uint32_t ms) {
  for (int i = 0; i < 3; i++) g_from[i] = g_now[i];
  target(g_to);
  g_fadeStart = millis();
  g_fadeMs = ms;
}

uint16_t duty(float level) {
  if (level <= 0.0f) return 0;
  uint16_t d = (uint16_t)lroundf(powf(level, GAMMA) * LIGHT_PWM_RANGE);
  return d ? d : 1;  // the faintest setting still glows
}

void tick() {
  uint32_t elapsed = millis() - g_fadeStart;
  float t = g_fadeMs == 0 || elapsed >= g_fadeMs ? 1.0f : (float)elapsed / g_fadeMs;
  float k = 1.0f;
  uint32_t period = g_state.effect < Light::EFFECT_COUNT ? PULSE_PERIOD_MS[g_state.effect] : 0;
  if (g_state.on && period) {
    // Triangle wave from full brightness down to PULSE_MIN and back.
    float phase = (float)(millis() % period) / period;
    k = 1.0f - (1.0f - PULSE_MIN) * (1.0f - fabsf(2.0f * phase - 1.0f));
  }
  for (int i = 0; i < 3; i++) {
    g_now[i] = g_from[i] + (g_to[i] - g_from[i]) * t;
    uint16_t d = duty(g_now[i] * k);
    if (d != g_duty[i]) {
      g_duty[i] = d;
      analogWrite(PINS[i], d);
    }
  }
}

void sanitize(Light::State &s) {
  if (!s.r && !s.g && !s.b) s.r = s.g = s.b = 255;
  if (!s.brightness) s.brightness = 1;
  if (s.effect >= Light::EFFECT_COUNT) s.effect = Light::EFFECT_NONE;
}

}  // namespace

namespace Light {

void begin(const State *initial) {
  StoredLight s;
  if (Storage::load(LIGHT_PATH, s, LIGHT_MAGIC)) {
    g_state = {s.on != 0, s.r, s.g, s.b, s.brightness, s.effect};
  } else if (initial) {
    g_state = *initial;
    g_savePending = true;  // loop() saves the inherited state
  }
  sanitize(g_state);
  analogWriteRange(LIGHT_PWM_RANGE);
  analogWriteFreq(LIGHT_PWM_FREQ);
  for (uint8_t pin : PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  retarget(LIGHT_DEFAULT_TRANSITION_MS);  // fade up from dark after power-up
  logf("[light] restored %s %s, brightness %u", g_state.on ? "on" : "off", hex(g_state).c_str(), g_state.brightness);
  g_tick.attach_ms(LIGHT_TICK_MS, tick);
}

void loop() {
  uint32_t changes = g_changes;
  if (changes != g_handledChanges) {
    g_handledChanges = changes;
    g_changedAt = millis();
    g_savePending = true;
  }
  if (!g_savePending || (uint32_t)(millis() - g_changedAt) < LIGHT_SAVE_DELAY_MS) return;
  g_savePending = false;
  StoredLight s = {};
  s.on = g_state.on;
  s.r = g_state.r;
  s.g = g_state.g;
  s.b = g_state.b;
  s.brightness = g_state.brightness;
  s.effect = g_state.effect;
  if (!Storage::save(LIGHT_PATH, s, LIGHT_MAGIC)) logf("[light] failed to save state");
}

State get() { return g_state; }

void set(const State &state, int32_t transitionMs) {
  State s = state;
  sanitize(s);
  if (memcmp(&s, &g_state, sizeof(s)) == 0) return;
  g_state = s;
  g_changes = g_changes + 1;
  retarget(transitionMs < 0 ? LIGHT_DEFAULT_TRANSITION_MS : (uint32_t)transitionMs);
}

void toggle() {
  State s = g_state;
  s.on = !s.on;
  set(s);
}

uint32_t changeCount() { return g_changes; }

const char *effectName(uint8_t effect) { return effect < EFFECT_COUNT ? EFFECT_NAMES[effect] : EFFECT_NAMES[0]; }

int effectIndex(const String &name) {
  String n = name;
  n.trim();
  n.toLowerCase();
  n.replace("_", " ");
  if (n == "off") return EFFECT_NONE;
  for (int i = 0; i < EFFECT_COUNT; i++) {
    String e = EFFECT_NAMES[i];
    e.toLowerCase();
    if (n == e) return i;
  }
  return -1;
}

String hex(const State &s) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", s.r, s.g, s.b);
  return buf;
}

bool parseHex(const String &value, uint8_t &r, uint8_t &g, uint8_t &b) {
  String v = value;
  v.trim();
  if (v.startsWith("#")) v.remove(0, 1);
  if (v.length() != 6) return false;
  for (char c : v) {
    if (!isxdigit(c)) return false;
  }
  uint32_t n = strtoul(v.c_str(), nullptr, 16);
  r = n >> 16;
  g = n >> 8;
  b = n;
  return true;
}

}  // namespace Light
