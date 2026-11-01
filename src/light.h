#pragma once

// The plug's RGB light (three PWM channels), independent of the relay.
//
// Like the button, the light is driven from a timer, so fades and effects run smoothly while
// the network code is busy. Its state (on/off, color, brightness, effect) survives power cuts;
// it is saved a moment after the last change, so dragging a slider doesn't wear the flash.
//
// The API matches the Logix Vent's status light: brightness 0-255, color "#rrggbb", effects
// "None", "Pulse" and "Fast Pulse", optional transition time.

#include <Arduino.h>

namespace Light {

enum Effect : uint8_t { EFFECT_NONE, EFFECT_PULSE, EFFECT_FAST_PULSE, EFFECT_COUNT };

struct State {
  bool on;
  uint8_t r, g, b;     // color at full brightness, never black
  uint8_t brightness;  // 1-255
  uint8_t effect;      // Effect
};

// Restores the saved state, or `initial` (e.g. the state Tasmota left) if nothing is saved yet
// and it is given. Starts the PWM timer. Call in setup() right after Outlets::begin().
void begin(const State *initial = nullptr);
void loop();  // saves state changes to flash

State get();
// transitionMs < 0: the default fade.
void set(const State &state, int32_t transitionMs = -1);
void toggle();
// Bumped on every change; compare to detect changes.
uint32_t changeCount();

const char *effectName(uint8_t effect);
// "None", "Pulse", "Fast Pulse" (any case, also "none"/"off", "pulse", "fast_pulse"); -1 if not valid.
int effectIndex(const String &name);
// "#rrggbb"
String hex(const State &state);
// Parses "#rrggbb" or "rrggbb"; false if invalid.
bool parseHex(const String &value, uint8_t &r, uint8_t &g, uint8_t &b);

}  // namespace Light
