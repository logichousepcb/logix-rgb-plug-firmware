#pragma once
#include <Arduino.h>

#define FW_VERSION "1.0.0"
// Logix device contract (LogixDIY Home Assistant integration)
#define LOGIX_MANUFACTURER "Logix"
#define LOGIX_MODEL "logixrgbplug"
#define LOGIX_MODEL_NAME "Logix RGB Plug"
#define LOGIX_AP_PREFIX "Logix-RGB-Plug-"
#define LOGIX_API_VERSION 1

// ---- RGB smart plug pin map (hard-coded, see DEVICE.md §3) ----
constexpr uint8_t PIN_RED = 0;      // light PWM, active high (boot strap: high at boot)
constexpr uint8_t PIN_GREEN = 2;    // light PWM, active high (boot strap: high at boot)
constexpr uint8_t PIN_BLUE = 5;     // light PWM, active high
constexpr uint8_t PIN_BUTTON = 13;  // push button, active low (external pull-up; internal one on too)
constexpr uint8_t PIN_RELAY = 15;   // relay, active high (boot strap: low at boot)

// ---- Button ----
constexpr uint32_t BUTTON_SCAN_MS = 25;
constexpr uint8_t BUTTON_STABLE_SAMPLES = 3;  // 75 ms debounce
// A second press within this time of the first makes a double press. Single press = relay,
// double press = light, like Tasmota with two power devices on one button.
constexpr uint32_t BUTTON_MULTI_MS = 400;

// ---- Light ----
constexpr uint32_t LIGHT_PWM_FREQ = 1000;  // Hz
constexpr uint16_t LIGHT_PWM_RANGE = 1023;
constexpr uint32_t LIGHT_TICK_MS = 20;
constexpr uint32_t LIGHT_DEFAULT_TRANSITION_MS = 500;
constexpr uint32_t LIGHT_SAVE_DELAY_MS = 2000;  // a slider sends many changes; save once it settles

// ---- Time: US Eastern (EST/EDT) ----
#define TZ_POSIX "EST5EDT,M3.2.0,M11.1.0"
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// ---- Network ----
constexpr uint32_t WIFI_AP_FALLBACK_MS = 60000;   // no WiFi this long: bring up the setup AP
constexpr uint32_t WIFI_AP_RETRY_MS = 60000;      // while in AP mode, retry the saved network
constexpr uint32_t WIFI_AP_ATTEMPT_MS = 20000;    // ... for this long each time
constexpr uint32_t MQTT_RETRY_MS = 10000;
