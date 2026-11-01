#include "outlets.h"

#include <Ticker.h>

#include "config.h"
#include "light.h"
#include "log.h"
#include "storage.h"

namespace {

const char *const KEY = "plug";

const uint32_t STATE_MAGIC = 0x4C504C53;  // "LPLS"
const char *const STATE_PATH = "/state.bin";

struct __attribute__((packed)) StoredState {
  uint32_t magic;
  uint8_t mask;  // bit 0 = relay
  uint8_t reserved[3];
  uint32_t checksum;
};

// Timer callbacks only run while loop() yields, so these never change mid-update; volatile
// because they are shared between the two contexts.
volatile uint8_t g_mask = 0;
volatile uint32_t g_changes = 0;
uint32_t g_handledChanges = 0;
uint8_t g_savedMask = 0;

// Actions waiting for MQTT; a few are kept in case several land between two loop() runs.
constexpr uint8_t EVENT_QUEUE = 4;

struct Button {
  volatile bool pressed;
  volatile uint32_t pressCount;
  volatile uint32_t actionCount;
  volatile uint32_t lastActionAt;
  volatile Outlets::Action lastAction;
  volatile uint32_t eventsTaken;
  volatile Outlets::Action events[EVENT_QUEUE];  // events[n % EVENT_QUEUE] = action number n
};
Button g_button = {};

Ticker g_scan;
bool g_candidate = false;  // pressed?
uint8_t g_stable = 0;
uint8_t g_presses = 0;     // presses in the current multi-press window
uint32_t g_firstPressAt = 0;

void apply(bool on) {
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  uint8_t mask = on ? 1 : 0;
  if (mask != g_mask) {
    g_mask = mask;
    g_changes = g_changes + 1;
  }
}

void act(Outlets::Action action) {
  g_button.events[g_button.actionCount % EVENT_QUEUE] = action;
  g_button.actionCount = g_button.actionCount + 1;
  g_button.lastActionAt = millis();
  g_button.lastAction = action;
  if (action == Outlets::Action::Double) {
    Light::toggle();
  } else {
    apply(!g_mask);
  }
}

// Every BUTTON_SCAN_MS. The input must hold the same level for BUTTON_STABLE_SAMPLES scans
// before it counts. Presses are collected for BUTTON_MULTI_MS from the first one: one press is
// a single, two or more a double.
void scanTick() {
  bool down = digitalRead(PIN_BUTTON) == LOW;
  if (down == g_candidate) {
    if (g_stable < 255) g_stable++;
  } else {
    g_candidate = down;
    g_stable = 1;
  }
  if (g_stable == BUTTON_STABLE_SAMPLES) {
    g_button.pressed = down;
    if (down) {
      g_button.pressCount = g_button.pressCount + 1;
      if (g_presses == 0) g_firstPressAt = millis();
      if (g_presses < 255) g_presses++;
    }
  }
  if (g_presses && (g_presses >= 2 || (uint32_t)(millis() - g_firstPressAt) >= BUTTON_MULTI_MS)) {
    act(g_presses >= 2 ? Outlets::Action::Double : Outlets::Action::Single);
    g_presses = 0;
  }
}

}  // namespace

namespace Outlets {

const char *actionName(Action action) {
  switch (action) {
    case Action::Single: return "single";
    case Action::Double: return "double";
    default: return "none";
  }
}

Action parseAction(const String &value) {
  String v = value;
  v.trim();
  v.toLowerCase();
  if (v.isEmpty() || v == "single") return Action::Single;
  if (v == "double") return Action::Double;
  return Action::None;
}

void begin(int initialMask) {
  StoredState s;
  bool stored = Storage::load(STATE_PATH, s, STATE_MAGIC);
  uint8_t mask = (stored ? s.mask : initialMask > 0 ? initialMask : 0) & 1;
  // Level first, then output, so the relay never glitches on.
  digitalWrite(PIN_RELAY, mask ? HIGH : LOW);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  g_mask = mask;
  g_savedMask = stored ? mask : 0;
  g_handledChanges = g_changes;
  if (!stored && mask) g_changes = g_changes + 1;  // loop() saves the inherited state
  logf("[plug] restored %s", mask ? "on" : "off");
  g_scan.attach_ms(BUTTON_SCAN_MS, scanTick);
}

void loop() {
  uint32_t changes = g_changes;
  if (changes == g_handledChanges) return;
  g_handledChanges = changes;
  uint8_t mask = g_mask;
  logf("[plug] %s", mask ? "on" : "off");
  if (mask == g_savedMask) return;
  StoredState s = {};
  s.mask = mask;
  if (Storage::save(STATE_PATH, s, STATE_MAGIC)) {
    g_savedMask = mask;
  } else {
    logf("[plug] failed to save state");
  }
}

bool isOn(uint8_t index) { return index < COUNT && g_mask; }
bool anyOn() { return g_mask != 0; }

void set(uint8_t index, bool on) {
  if (index < COUNT) apply(on);
}

void toggle(uint8_t index) {
  if (index < COUNT) apply(!isOn(index));
}

uint32_t changeCount() { return g_changes; }

const char *key(uint8_t index) { return index < COUNT ? KEY : "all"; }

int parse(const String &value) {
  String v = value;
  v.trim();
  v.toLowerCase();
  if (v.isEmpty() || v == "1" || v == KEY) return 0;  // the only outlet is also the default
  if (v == "all" || v == "0") return COUNT;
  return -1;
}

bool buttonPressed(uint8_t index) { return index < COUNT && g_button.pressed; }
uint32_t buttonPressCount(uint8_t index) { return index < COUNT ? g_button.pressCount : 0; }
uint32_t buttonActionCount(uint8_t index) { return index < COUNT ? g_button.actionCount : 0; }
Action buttonLastAction(uint8_t index) { return index < COUNT ? g_button.lastAction : Action::None; }

int32_t buttonLastActionMsAgo(uint8_t index) {
  if (index >= COUNT || g_button.actionCount == 0) return -1;
  return (int32_t)(millis() - g_button.lastActionAt);
}

void pressButton(uint8_t index, Action action) {
  if (index < COUNT && action != Action::None) act(action);
}

Action takeButtonEvent(uint8_t index) {
  if (index >= COUNT) return Action::None;
  // Too far behind: skip to the oldest action still in the queue.
  if (g_button.actionCount - g_button.eventsTaken > EVENT_QUEUE) g_button.eventsTaken = g_button.actionCount - EVENT_QUEUE;
  if (g_button.eventsTaken == g_button.actionCount) return Action::None;
  Action a = g_button.events[g_button.eventsTaken % EVENT_QUEUE];
  g_button.eventsTaken = g_button.eventsTaken + 1;
  return a;
}

}  // namespace Outlets
