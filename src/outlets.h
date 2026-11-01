#pragma once

// The plug's one outlet (relay) and its button.
//
// The button is scanned from a timer, not from loop(), so it keeps working while the network
// code is busy (WiFi connecting, MQTT broker unreachable, HTTP request). A single press toggles
// the relay, a double press toggles the light. The relay state is written to flash on every
// change and restored first thing at boot.
//
// The API keeps the Logix Outlet shape (arrays of outlets and buttons) with one entry each,
// so the same integration code handles both devices.

#include <Arduino.h>

namespace Outlets {

constexpr uint8_t COUNT = 1;  // index 0 = the plug

enum class Action : uint8_t { None, Single, Double };
const char *actionName(Action action);
// "single" or "double" (default "single" when empty); Action::None if not valid.
Action parseAction(const String &value);

// Restores the saved state (or `initialMask` if nothing is saved yet and it is >= 0, e.g. the
// state Tasmota left) and starts the button scan. Call first in setup().
void begin(int initialMask = -1);
void loop();   // saves state changes to flash

bool isOn(uint8_t index);
bool anyOn();
void set(uint8_t index, bool on);
void toggle(uint8_t index);
// Bumped on every outlet change, physical or remote; compare to detect changes.
uint32_t changeCount();

// "plug"
const char *key(uint8_t index);
// "", "1", "plug" or "all" (returns COUNT for all); -1 if not valid.
int parse(const String &value);

// ---- button (index as for outlets) ----
bool buttonPressed(uint8_t index);
uint32_t buttonPressCount(uint8_t index);
// Completed actions since boot (physical and simulated), the last one, and ms since it (-1 = none).
uint32_t buttonActionCount(uint8_t index);
Action buttonLastAction(uint8_t index);
int32_t buttonLastActionMsAgo(uint8_t index);
// Simulate an action exactly like the physical button: single toggles the outlet, double the light.
void pressButton(uint8_t index, Action action = Action::Single);
// The next action not yet taken (Action::None if none); used to publish one MQTT event per action.
Action takeButtonEvent(uint8_t index);

}  // namespace Outlets
