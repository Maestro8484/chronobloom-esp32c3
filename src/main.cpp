#include <Arduino.h>
#include <EEPROM.h>
#if !defined(ESP32)
#error "NeoPixelClock v3 requires an ESP32-family Arduino core."
#endif
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <esp_sntp.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"
#include <Preferences.h>
#if LUX_SENSOR_ENABLED
#include <Adafruit_VEML7700.h>
#endif

using ClockWebServer = WebServer;

// ===================== Build-time configuration =====================
#ifndef LED_DATA_PIN
#define LED_DATA_PIN 10
#endif

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

#ifndef CLOCK_PIXEL_COUNT
#define CLOCK_PIXEL_COUNT 98
#endif

#ifndef CENTER_PIXEL_ENABLED
#define CENTER_PIXEL_ENABLED 1
#endif

#ifndef CENTER_PIXEL_INDEX
#define CENTER_PIXEL_INDEX 1
#endif

#ifndef CENTER_PIXEL_SEPARATE_OUTPUT
#define CENTER_PIXEL_SEPARATE_OUTPUT 0
#endif

#ifndef CENTER_PIXEL_PIN
#define CENTER_PIXEL_PIN -1
#endif

#ifndef CENTER_PIXEL_STRIP_COUNT
#define CENTER_PIXEL_STRIP_COUNT 1
#endif

#ifndef RING_PIXEL_OFFSET
#define RING_PIXEL_OFFSET 2
#endif

#ifndef SACRIFICIAL_PIXEL_ENABLED
#define SACRIFICIAL_PIXEL_ENABLED 0
#endif

#ifndef SACRIFICIAL_PIXEL_INDEX
#define SACRIFICIAL_PIXEL_INDEX 0
#endif

#ifndef ENABLE_WIFI_UI
#define ENABLE_WIFI_UI 1
#endif

#ifndef ENABLE_NTP
#define ENABLE_NTP 1
#endif

#ifndef TEMP_SENSOR_ENABLED
#define TEMP_SENSOR_ENABLED 0
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "clock-ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "clock-password"
#endif

#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "esp32c3-ringclock"
#endif

#ifndef DEVICE_TITLE
#define DEVICE_TITLE "NeoPixelClock v3"
#endif

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 20000
#endif

#ifndef NTP_TIMEZONE_TZ
// Mountain time with US daylight saving rules.
#define NTP_TIMEZONE_TZ "MST7MDT,M3.2.0,M11.1.0"
#endif

#ifndef BUTTON_UP_PIN
#define BUTTON_UP_PIN 5
#endif

#ifndef BUTTON_DOWN_PIN
#define BUTTON_DOWN_PIN 9
#endif

// Shared logical ring order. RING_PIXEL_OFFSET selects where logical outer LED 0
// lands in the physical chain for each clock variant.
struct RingConfig {
  uint8_t count;
  uint16_t offset;
  bool clockwise;
};

constexpr RingConfig RING_OUTER_60 = {60, RING_PIXEL_OFFSET, true};
constexpr RingConfig RING_MIDDLE_24 = {24, RING_PIXEL_OFFSET + 60, true};
constexpr RingConfig RING_INNER_12 = {12, RING_PIXEL_OFFSET + 84, true};

#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT && (CENTER_PIXEL_PIN < 0)
#error "CENTER_PIXEL_PIN must be set when CENTER_PIXEL_SEPARATE_OUTPUT is enabled."
#endif

#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT && (CENTER_PIXEL_INDEX >= CENTER_PIXEL_STRIP_COUNT)
#error "CENTER_PIXEL_INDEX must be inside CENTER_PIXEL_STRIP_COUNT when CENTER_PIXEL_SEPARATE_OUTPUT is enabled."
#endif

#if CENTER_PIXEL_ENABLED && !CENTER_PIXEL_SEPARATE_OUTPUT && (CENTER_PIXEL_INDEX >= CLOCK_PIXEL_COUNT)
#error "CENTER_PIXEL_INDEX must be inside CLOCK_PIXEL_COUNT when CENTER_PIXEL_ENABLED is set."
#endif

#if SACRIFICIAL_PIXEL_ENABLED && (SACRIFICIAL_PIXEL_INDEX >= CLOCK_PIXEL_COUNT)
#error "SACRIFICIAL_PIXEL_INDEX must be inside CLOCK_PIXEL_COUNT when SACRIFICIAL_PIXEL_ENABLED is set."
#endif

#if (RING_PIXEL_OFFSET + 96) > CLOCK_PIXEL_COUNT
#error "RING_PIXEL_OFFSET leaves too few pixels for the 60+24+12 ring chain."
#endif

#if CENTER_PIXEL_ENABLED && !CENTER_PIXEL_SEPARATE_OUTPUT && (CENTER_PIXEL_INDEX >= RING_PIXEL_OFFSET) && (CENTER_PIXEL_INDEX < (RING_PIXEL_OFFSET + 96))
#error "CENTER_PIXEL_INDEX overlaps the ring pixel range."
#endif

#if SACRIFICIAL_PIXEL_ENABLED && (SACRIFICIAL_PIXEL_INDEX >= RING_PIXEL_OFFSET) && (SACRIFICIAL_PIXEL_INDEX < (RING_PIXEL_OFFSET + 96))
#error "SACRIFICIAL_PIXEL_INDEX overlaps the ring pixel range."
#endif

#if CENTER_PIXEL_ENABLED && !CENTER_PIXEL_SEPARATE_OUTPUT && SACRIFICIAL_PIXEL_ENABLED && (CENTER_PIXEL_INDEX == SACRIFICIAL_PIXEL_INDEX)
#error "CENTER_PIXEL_INDEX and SACRIFICIAL_PIXEL_INDEX must be different physical pixels."
#endif

static void configureWiFiHostname() {
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  if (!WiFi.setHostname(DEVICE_HOSTNAME)) {
    Serial.println("Wi-Fi hostname set failed");
  }
}

static void writeStatusLed(bool on) {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

// ===================== Ambient light sensor =====================
#if LUX_SENSOR_ENABLED
#include <Wire.h>
#endif

class LuxSensor {
 public:
  void begin() {
#if LUX_SENSOR_ENABLED
    if (!veml_.begin()) {
      Serial.println("VEML7700 not found on I2C");
      available_ = false;
      return;
    }
    veml_.setGain(VEML7700_GAIN_1);
    veml_.setIntegrationTime(VEML7700_IT_100MS);
    veml_.enable(true);
    available_ = true;
    Serial.println("VEML7700 initialized");
#endif
  }

  bool available() const { return available_; }

  float lux() {
#if LUX_SENSOR_ENABLED
    if (!available_) return -1.0f;
    uint32_t nowMs = millis();
    if (nowMs - lastReadMs_ < 120) return luxAvg_;
    lastReadMs_ = nowMs;

    // Wait 400ms after a gain change (2 full 100ms integration cycles + margin)
    // before trusting the reading. Short settling was the primary cause of
    // gain oscillation: reading was still invalid when next switch decision ran.
    if (nowMs - lastGainChangeMs_ < 400) return luxAvg_;

    float reading = veml_.readLux();
    if (reading < 0.0f) return luxAvg_;  // discard library error returns

    // 3-state gain machine with hysteresis bands to prevent oscillation.
    // Previous code had no hysteresis: GAIN_1_8 -> GAIN_2 direct jump
    // caused reading to spike -> GAIN_1_8 again -> flicker loop.
    uint8_t g = veml_.getGain();
    bool gainChanged = false;
    if (g == VEML7700_GAIN_2 && reading > 200.0f) {
      // High-sensitivity: exit when scene is bright enough (>200 hysteresis vs <50 entry)
      veml_.setGain(VEML7700_GAIN_1);
      gainChanged = true;
    } else if (g == VEML7700_GAIN_1 && reading > 0.0f && reading < 50.0f) {
      // Normal: enter high-sensitivity for dark scenes
      veml_.setGain(VEML7700_GAIN_2);
      gainChanged = true;
    } else if (g == VEML7700_GAIN_1 && reading > 900.0f) {
      // Normal: enter low-sensitivity for very bright light
      veml_.setGain(VEML7700_GAIN_1_8);
      gainChanged = true;
    } else if (g == VEML7700_GAIN_1_8 && reading < 300.0f) {
      // Low-sensitivity: return to normal (never jump directly to GAIN_2)
      veml_.setGain(VEML7700_GAIN_1);
      gainChanged = true;
    }
    if (gainChanged) {
      lastGainChangeMs_ = nowMs;
      return luxAvg_;
    }

    // Faster smoothing (alpha 0.15) so we track real changes while still
    // suppressing noise. Previous 0.05 was so slow that resets after a gain
    // switch left luxAvg_ stale for many seconds.
    luxAvg_ = luxAvg_ * 0.85f + reading * 0.15f;
    return luxAvg_;
#else
    return -1.0f;
#endif
  }

  // Returns the current ramped brightness (what LEDs actually show).
  // Use this for web UI display so it matches hardware state.
  uint8_t autoBrightness() {
    return autoBrightnessCached(millis());
  }

  // Returns the brightness target (pre-ramp) for diagnostic display.
  uint8_t autoBrightnessTarget() const {
    return (uint8_t)cachedBrightness_;
  }

  uint8_t autoBrightnessCached(uint32_t nowMs) {
#if LUX_SENSOR_ENABLED
    if (!available_) return 128;

    // Refresh the lux-derived target every 500ms
    if (nowMs - lastBrightnessMs_ >= 500) {
      float lx = lux();
      if (lx >= 0.0f) {
        float normalized = log10(max(0.1f, lx) + 1.0f) / log10(10001.0f);
        cachedBrightness_ = constrain(normalized * 240.0f + 15.0f, 15.0f, 255.0f);
      }
      lastBrightnessMs_ = nowMs;
    }

    // On first call, snap rampedBrightness_ to target to avoid a ramp from 128
    if (lastRampMs_ == 0) {
      rampedBrightness_ = cachedBrightness_;
      lastRampMs_ = nowMs;
      return (uint8_t)rampedBrightness_;
    }

    // Ramp toward target at ≤50 brightness units/sec.
    // This prevents the sudden strip-wide flash/dark when lux changes rapidly.
    // At 50/sec: full range (255→15) takes ~4.8 seconds — smooth but responsive.
    uint32_t dt = nowMs - lastRampMs_;
    if (dt > 200) dt = 200;  // cap dt so a stall doesn't cause a big jump
    lastRampMs_ = nowMs;
    float maxStep = dt * 0.050f;  // 50 units/sec → 0.050 per ms
    float diff = cachedBrightness_ - rampedBrightness_;
    if (diff > maxStep)       diff = maxStep;
    else if (diff < -maxStep) diff = -maxStep;
    rampedBrightness_ = constrain(rampedBrightness_ + diff, 15.0f, 255.0f);

    return (uint8_t)rampedBrightness_;
#else
    return 128;
#endif
  }

 private:
#if LUX_SENSOR_ENABLED
  Adafruit_VEML7700 veml_;
#endif
  bool available_ = false;
  float luxAvg_ = 100.0f;
  uint32_t lastGainChangeMs_ = 0;
  uint32_t lastReadMs_ = 0;
  float cachedBrightness_ = 128.0f;  // target (lux-derived, unsmoothed)
  float rampedBrightness_ = 128.0f;  // actual output (smoothed toward target)
  uint32_t lastBrightnessMs_ = 0;
  uint32_t lastRampMs_ = 0;
};

// ===================== Persistent settings =====================
struct ClockSettings {
  uint8_t magic;
  uint8_t version;
  uint8_t dayBrightness;
  uint8_t nightBrightness;
  uint8_t nightStartHour;
  uint8_t nightEndHour;
  uint8_t colorTheme;
  uint8_t secondTrail;
  uint8_t progressSeconds;
  uint8_t hourlyChime;
  uint8_t statusAnimations;
  uint8_t outerMarkerRed;
  uint8_t outerMarkerGreen;
  uint8_t outerMarkerBlue;
  uint8_t outerMarkerLevel;
  uint8_t outerFillerRed;
  uint8_t outerFillerGreen;
  uint8_t outerFillerBlue;
  uint8_t outerFillerLevel;
  uint8_t secondsRed;
  uint8_t secondsGreen;
  uint8_t secondsBlue;
  uint8_t secondsLevel;
  uint8_t minutesRed;
  uint8_t minutesGreen;
  uint8_t minutesBlue;
  uint8_t minutesLevel;
  uint8_t hoursRed;
  uint8_t hoursGreen;
  uint8_t hoursBlue;
  uint8_t hoursLevel;
  uint8_t centerRed;
  uint8_t centerGreen;
  uint8_t centerBlue;
  uint8_t centerLevel;
  // Auto-brightness
  uint8_t autoBrightnessMode;
  uint8_t minAutoBrightness;
  uint8_t maxAutoBrightness;
  // Time-interval animations
  uint8_t quarterAnimation;
  uint8_t halfHourAnimation;
  uint8_t hourAnimation;
  uint8_t intervalAnimationsEnabled;
  // FOCUS REMINDERS MVP (v8) - 16 bytes total (13 used + 3 reserved)
  uint8_t focusReminder_enabled;        // Master enable/disable
  uint8_t focusReminder_startHour;      // Window start (0-23)
  uint8_t focusReminder_endHour;        // Window end (0-23)
  uint16_t focusReminder_intervalMinutes; // Repeat interval in minutes (1-1440)
  uint8_t focusReminder_daysMask;       // Bitmask Sun=0, Sat=6
  uint8_t focusReminder_animation;      // Animation type (0-5)
  uint8_t focusReminder_durationSeconds; // Duration (1-60) - reserved for v2
  uint32_t focusReminder_lastFireMs;    // Last fire timestamp (millis)
  uint8_t outerRingOffset;              // 0-59: clockwise LED rotation applied to all rings at render time
  // Animation customization (added v11)
  uint8_t animationPalette;     // 0=Rainbow,1=Fire,2=Ocean,3=Forest,4=Candy,5=Neon,6=Monochrome,7=Clock
  uint8_t animationSpeed;       // 1-5 (1=slow/dreamy, 3=normal, 5=fast)
  uint8_t animationBrightness;  // 50-255 peak brightness during animations
  uint8_t trailLength;          // 2-12 LEDs (chase/sweep trail length)
  uint8_t reminderPalette;      // 0=Amber,1=Red,2=Magenta,3=Cyan (reminder animations only)
};

constexpr uint8_t SETTINGS_MAGIC = 0xC1;
constexpr uint8_t SETTINGS_VERSION = 11;
constexpr size_t EEPROM_BYTES = 256;

class SettingsStore {
 public:
  void begin() {
    EEPROM.begin(EEPROM_BYTES);
    EEPROM.get(0, settings_);
    if (!valid(settings_)) {
      settings_ = defaults();
      save();
    }
  }

  const ClockSettings &get() const { return settings_; }

  void update(const ClockSettings &settings) {
    settings_ = sanitize(settings);
    save();
  }

  void resetToDefaults() {
    uint8_t zero = 0;
    EEPROM.put(0, zero);  // invalidate magic so begin() resets on next boot
    EEPROM.commit();
    settings_ = defaults();
  }

 private:
  static ClockSettings defaults() {
    return {SETTINGS_MAGIC, SETTINGS_VERSION, 44, 5, 22, 7, 0, 0, 0, 1, 1,
            70,  150, 255, 220,   // outerMarker: bright blue-white
            0,   5,   200, 130,   // outerFiller: deep royal blue
            180, 220, 255, 255,   // seconds:     ice blue
            255, 80,  0,   245,   // minutes:     orange
            220, 0,   180, 255,   // hours:       hot pink/magenta
            255, 60,  0,   180,   // center:      warm orange-red
            1,   10,  255,        // autoBrightness: mode=auto, min=10, max=255
            3,   1,   4,   1,     // animations: shimmer, sweep, spiral, enabled
            0,   8,   22,   60,   0, 0, 60, 60,  // focusReminder: disabled, 08-22h, 60min, no days, quarter anim
            0,   // outerRingOffset: no rotation
            0, 3, 200, 4, 0};  // animPalette, animSpeed, animBrightness, trailLength, reminderPalette
  }

  static bool valid(const ClockSettings &settings) {
    return settings.magic == SETTINGS_MAGIC && settings.version == SETTINGS_VERSION &&
           settings.dayBrightness <= 255 && settings.nightBrightness <= 255 &&
           settings.nightStartHour < 24 && settings.nightEndHour < 24 &&
           settings.colorTheme <= 2 && settings.secondTrail <= 1 &&
           settings.progressSeconds <= 1 && settings.hourlyChime <= 1 &&
           settings.statusAnimations <= 1 && settings.autoBrightnessMode <= 2 &&
           settings.minAutoBrightness <= 255 && settings.maxAutoBrightness <= 255 &&
           settings.quarterAnimation <= 6 && settings.halfHourAnimation <= 7 &&
           settings.hourAnimation <= 10 && settings.intervalAnimationsEnabled <= 1 &&
           settings.focusReminder_enabled <= 1 &&
           settings.focusReminder_startHour < 24 &&
           settings.focusReminder_endHour < 24 &&
           settings.focusReminder_intervalMinutes >= 1 && settings.focusReminder_intervalMinutes <= 1440 &&
           settings.focusReminder_daysMask <= 127 &&
           settings.focusReminder_animation <= 11 &&
           settings.focusReminder_durationSeconds >= 1 && settings.focusReminder_durationSeconds <= 60 &&
           settings.outerRingOffset < 60 &&
           settings.animationPalette <= 7 &&
           settings.animationSpeed >= 1 && settings.animationSpeed <= 5 &&
           settings.animationBrightness >= 50 && settings.animationBrightness <= 255 &&
           settings.trailLength >= 2 && settings.trailLength <= 12 &&
           settings.reminderPalette <= 3;
  }

  static ClockSettings sanitize(ClockSettings settings) {
    settings.magic = SETTINGS_MAGIC;
    settings.version = SETTINGS_VERSION;
    settings.nightStartHour %= 24;
    settings.nightEndHour %= 24;
    if (settings.colorTheme > 2) settings.colorTheme = 0;
    settings.secondTrail = settings.secondTrail ? 1 : 0;
    settings.progressSeconds = settings.progressSeconds ? 1 : 0;
    settings.hourlyChime = settings.hourlyChime ? 1 : 0;
    settings.statusAnimations = settings.statusAnimations ? 1 : 0;
    if (settings.autoBrightnessMode > 2) settings.autoBrightnessMode = 1;
    if (settings.minAutoBrightness > settings.maxAutoBrightness) {
      settings.minAutoBrightness = 10;
      settings.maxAutoBrightness = 255;
    }
    if (settings.quarterAnimation > 6) settings.quarterAnimation = 0;
    if (settings.halfHourAnimation > 7) settings.halfHourAnimation = 0;
    if (settings.hourAnimation > 10) settings.hourAnimation = 0;
    settings.intervalAnimationsEnabled = settings.intervalAnimationsEnabled ? 1 : 0;
    settings.focusReminder_enabled = settings.focusReminder_enabled ? 1 : 0;
    if (settings.focusReminder_startHour >= 24) settings.focusReminder_startHour = 8;
    if (settings.focusReminder_endHour >= 24) settings.focusReminder_endHour = 22;
    if (settings.focusReminder_intervalMinutes < 1) settings.focusReminder_intervalMinutes = 60;
    if (settings.focusReminder_intervalMinutes > 1440) settings.focusReminder_intervalMinutes = 1440;
    settings.focusReminder_daysMask &= 127;  // Mask to 7 bits (Sun-Sat)
    if (settings.focusReminder_animation > 11) settings.focusReminder_animation = 0;
    if (settings.focusReminder_durationSeconds < 1) settings.focusReminder_durationSeconds = 60;
    if (settings.focusReminder_durationSeconds > 60) settings.focusReminder_durationSeconds = 60;
    if (settings.outerRingOffset >= 60) settings.outerRingOffset = 0;
    if (settings.animationPalette > 7) settings.animationPalette = 0;
    if (settings.animationSpeed < 1 || settings.animationSpeed > 5) settings.animationSpeed = 3;
    if (settings.animationBrightness < 50) settings.animationBrightness = 200;
    if (settings.trailLength < 2 || settings.trailLength > 12) settings.trailLength = 4;
    if (settings.reminderPalette > 3) settings.reminderPalette = 0;
    return settings;
  }

  void save() {
    EEPROM.put(0, settings_);
    EEPROM.commit();
  }

  ClockSettings settings_ = defaults();
};

// ===================== Time model =====================
struct ClockTime {
  uint8_t hour;   // 0-23
  uint8_t minute; // 0-59
  uint8_t second; // 0-59
};

class TimeModel {
 public:
  ClockTime get() {
    noInterrupts();
    ClockTime copy = time_;
    interrupts();
    return copy;
  }

  void set(uint8_t hour, uint8_t minute, uint8_t second) {
    if (hour > 23 || minute > 59 || second > 59) return;
    noInterrupts();
    time_.hour = hour;
    time_.minute = minute;
    time_.second = second;
    dirty_ = true;
    interrupts();
  }

  void tickOneSecond() {
    noInterrupts();
    incrementSecondNoLock();
    dirty_ = true;
    interrupts();
  }

  void addMinutes(int delta) {
    noInterrupts();
    int total = static_cast<int>(time_.hour) * 60 + time_.minute + delta;
    const int dayMinutes = 24 * 60;
    total %= dayMinutes;
    if (total < 0) total += dayMinutes;
    time_.hour = total / 60;
    time_.minute = total % 60;
    dirty_ = true;
    interrupts();
  }

  void addHours(int delta) {
    noInterrupts();
    int h = (static_cast<int>(time_.hour) + delta) % 24;
    if (h < 0) h += 24;
    time_.hour = static_cast<uint8_t>(h);
    dirty_ = true;
    interrupts();
  }

  void markDirty() {
    noInterrupts();
    dirty_ = true;
    interrupts();
  }

  bool consumeDirty() {
    noInterrupts();
    bool changed = dirty_;
    dirty_ = false;
    interrupts();
    return changed;
  }

 private:
  void incrementSecondNoLock() {
    time_.second++;
    if (time_.second > 59) {
      time_.second = 0;
      time_.minute++;
      if (time_.minute > 59) {
        time_.minute = 0;
        time_.hour = (time_.hour + 1) % 24;
      }
    }
  }

  ClockTime time_ = {12, 0, 0};
  volatile bool dirty_ = true;
};

// ===================== LED renderer =====================
enum AnimPhase : uint8_t {
  ANIM_IDLE,
  ANIM_Q1,   // Quarter: sparkle burst
  ANIM_Q2,   // Quarter: pulse markers
  ANIM_Q3,   // Quarter: ring shimmer
  ANIM_H1,   // Half: rainbow sweep
  ANIM_H2,   // Half: dual flash
  ANIM_H3,   // Half: tidal pulse
  ANIM_HR1,  // Hour: chime sweep
  ANIM_HR2,  // Hour: firework burst
  ANIM_HR3,  // Hour: zenith cascade
  ANIM_HR4,  // Hour: rainbow spiral
  ANIM_HR5,  // Hour: breathing mandala
  // Quarter additions
  ANIM_Q4,   // Quarter: laser ping
  ANIM_Q5,   // Quarter: DNA twist
  ANIM_Q6,   // Quarter: tick spark
  // Half-hour additions
  ANIM_H4,   // Half: comet chase
  ANIM_H5,   // Half: color explosion
  ANIM_H6,   // Half: Knight Rider bounce
  ANIM_H7,   // Half: strobe party
  // Hour additions
  ANIM_HR6,  // Hour: supernova
  ANIM_HR7,  // Hour: matrix rain
  ANIM_HR8,  // Hour: galaxy spin
  ANIM_HR9,  // Hour: color wipe
  ANIM_HR10, // Hour: thunderstorm
  // Reminder animations
  ANIM_REM1, // Reminder: amber pulse
  ANIM_REM2, // Reminder: attention ring
  ANIM_REM3, // Reminder: heartbeat
  ANIM_REM4, // Reminder: sunrise wake
  ANIM_REM5, // Reminder: campfire flicker
  ANIM_REM6  // Reminder: neon sign
};

enum StatusMode : uint8_t {
  STATUS_NONE,
  STATUS_WIFI_CONNECTING,
  STATUS_WIFI_OK,
  STATUS_WIFI_FAIL,
  STATUS_BUTTON,
  STATUS_TIME_SYNC,
  STATUS_SETTINGS_SAVED,
  STATUS_OTA_UPDATE,
  STATUS_OTA_SUCCESS,
  STATUS_OTA_FAILED
};

class ClockRenderer {
 public:
  ClockRenderer(Adafruit_NeoPixel &strip, SettingsStore &settings,
                Adafruit_NeoPixel *centerStrip = nullptr)
      : strip_(strip), settings_(settings), centerStrip_(centerStrip),
        lux_(nullptr) {}

  void begin() {
    strip_.begin();
    strip_.setBrightness(settings_.get().dayBrightness);
    strip_.clear();
    strip_.show();
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->begin();
      centerStrip_->setBrightness(settings_.get().dayBrightness);
      centerStrip_->clear();
      centerStrip_->show();
    }
#endif
  }

  void setLuxSensor(LuxSensor *lux) { lux_ = lux; }

  void setStatus(StatusMode mode, uint16_t durationMs) {
    if (!settings_.get().statusAnimations) return;
    statusMode_ = mode;
    statusUntilMs_ = millis() + durationMs;
    lastAnimationMs_ = 0;
  }

  void triggerQuarterAnimation(uint32_t now) {
    const uint8_t mode = settings_.get().quarterAnimation;
    if (mode == 0) return;
    animPhase_ = static_cast<AnimPhase>(ANIM_Q1 + mode - 1);
    animStartMs_ = now;
    animStep_ = 255;  // force first-frame update
    animHue_ = 0;
  }

  void triggerHalfHourAnimation(uint32_t now) {
    const uint8_t mode = settings_.get().halfHourAnimation;
    if (mode == 0) return;
    animPhase_ = static_cast<AnimPhase>(ANIM_H1 + mode - 1);
    animStartMs_ = now;
    animStep_ = 255;
    animHue_ = 0;
  }

  void triggerHourAnimation(uint32_t now) {
    const uint8_t mode = settings_.get().hourAnimation;
    if (mode == 0) return;
    animPhase_ = static_cast<AnimPhase>(ANIM_HR1 + mode - 1);
    animStartMs_ = now;
    animStep_ = 255;
    animHue_ = 0;
  }

  void triggerAnimDirect(AnimPhase phase, uint32_t now) {
    animPhase_   = phase;
    animStartMs_ = now;
    animStep_    = 255;
    animHue_     = 0;
  }

  void triggerReminderDirectAnimation(uint8_t mode, uint32_t now) {
    if (mode == 0) { triggerQuarterAnimation(now); return; }
    if (mode == 1) { triggerHalfHourAnimation(now); return; }
    if (mode == 2) { triggerHourAnimation(now); return; }
    if (mode == 3) { triggerQuarterAnimation(now); return; }
    if (mode == 4) { triggerHalfHourAnimation(now); return; }
    if (mode == 5) { triggerHourAnimation(now); return; }
    static const AnimPhase remPhases[] = {
      ANIM_REM1, ANIM_REM2, ANIM_REM3, ANIM_REM4, ANIM_REM5, ANIM_REM6
    };
    const uint8_t idx = mode - 6;
    if (idx >= 6) return;
    animPhase_   = remPhases[idx];
    animStartMs_ = now;
    animStep_    = 0;
    animHue_     = 0;
  }

  bool animating() const { return animPhase_ != ANIM_IDLE; }

  void renderAnimFrame(uint32_t now) {
    const ClockSettings &settings = settings_.get();
    const uint8_t br = effectiveBrightness(lastTime_, settings, now);
    strip_.setBrightness(br);
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->setBrightness(br);
      centerStrip_->clear();
    }
#endif
    strip_.clear();
    tickAnimation(now);
    setSacrificialPixelDark();
    logShow(now, "anim");
    strip_.show();
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) centerStrip_->show();
#endif
  }

  bool needsFullAnimationFrame(uint32_t now) const {
    return statusActive(now) || (settings_.get().hourlyChime && chimeActive(lastTime_));
  }

  bool needsCenterAnimationFrame() const {
    return centerIdleActive();
  }

  void renderCenterAnimationFrame(const ClockTime &time) {
    lastTime_ = time;
    const ClockSettings &settings = settings_.get();
    const uint32_t now = millis();
    const uint8_t brightness = effectiveBrightness(time, settings, now);
    strip_.setBrightness(brightness);
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->setBrightness(brightness);
      centerStrip_->clear();
    }
#endif
    renderCenterIdle(now);
    setSacrificialPixelDark();
    strip_.show();
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->show();
    }
#endif
  }

  void render(const ClockTime &time) {
    lastTime_ = time;
    const ClockSettings &settings = settings_.get();
    const uint32_t now = millis();
    const uint8_t br = effectiveBrightness(time, settings, now);
    strip_.setBrightness(br);
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->setBrightness(br);
      centerStrip_->clear();
    }
#endif
    strip_.clear();

    renderFace(settings);
    renderSeconds(time, settings);
    renderMinutes(time, settings);
    renderHours(time, settings);

    const bool hourlyChimeNow = settings.hourlyChime && chimeActive(time);
    if (hourlyChimeNow) {
      renderHourlyChime(now);
    }
    if (statusActive(now)) {
      renderStatus(now);
    } else if (!hourlyChimeNow) {
      renderCenterIdle(now);
    }

    setSacrificialPixelDark();
    logShow(now, "face");
    strip_.show();
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_) {
      centerStrip_->show();
    }
#endif
  }

 private:
  bool nightActive(const ClockTime &time, const ClockSettings &settings) const {
    if (settings.nightStartHour == settings.nightEndHour) return false;
    if (settings.nightStartHour < settings.nightEndHour) {
      return time.hour >= settings.nightStartHour && time.hour < settings.nightEndHour;
    }
    return time.hour >= settings.nightStartHour || time.hour < settings.nightEndHour;
  }

  uint8_t effectiveBrightness(const ClockTime &time, const ClockSettings &settings, uint32_t now) const {
    switch (settings.autoBrightnessMode) {
      case 0:
        return settings.dayBrightness;
      case 1:
        if (lux_) {
          uint8_t auto_val = lux_->autoBrightnessCached(now);
          return constrain(auto_val, settings.minAutoBrightness, settings.maxAutoBrightness);
        }
        return settings.dayBrightness;
      case 2:
        return nightActive(time, settings) ? settings.nightBrightness : settings.dayBrightness;
      default:
        return settings.dayBrightness;
    }
  }

  void renderFace(const ClockSettings &settings) {
    const uint32_t outerMarker = ringColor(settings.outerMarkerRed, settings.outerMarkerGreen,
                                           settings.outerMarkerBlue, settings.outerMarkerLevel);
    const uint32_t outerFiller = ringColor(settings.outerFillerRed, settings.outerFillerGreen,
                                           settings.outerFillerBlue, settings.outerFillerLevel);
    const uint32_t middleAmbient = scale(ringColor(settings.hoursRed, settings.hoursGreen,
                                                  settings.hoursBlue, 255), 22);
    const uint32_t innerAmbient = scale(ringColor(settings.centerRed, settings.centerGreen,
                                                 settings.centerBlue, 255), 24);
    for (uint8_t i = 0; i < RING_OUTER_60.count; ++i) {
      setRingPixel(RING_OUTER_60, i, (i % 5 == 0) ? outerMarker : outerFiller);
    }
    for (uint8_t i = 0; i < RING_MIDDLE_24.count; ++i) {
      setRingPixel(RING_MIDDLE_24, i, middleAmbient);
    }
    for (uint8_t i = 0; i < RING_INNER_12.count; ++i) {
      setRingPixel(RING_INNER_12, i, innerAmbient);
    }
    (void)settings.colorTheme;
  }

  void renderSeconds(const ClockTime &time, const ClockSettings &settings) {
    const uint32_t color = ringColor(settings.secondsRed, settings.secondsGreen, settings.secondsBlue,
                                     settings.secondsLevel);
    if (settings.progressSeconds) {
      for (uint8_t i = 0; i <= time.second; ++i) {
        setRingPixel(RING_OUTER_60, i, scale(color, 20));
      }
    }

    if (settings.secondTrail) {
      const uint8_t trail[] = {52, 28, 14, 7};
      for (uint8_t i = 0; i < sizeof(trail); ++i) {
        const uint8_t idx = (time.second + RING_OUTER_60.count - i - 1) % RING_OUTER_60.count;
        setRingPixel(RING_OUTER_60, idx, scale(color, trail[i]));
      }
    }

    setRingPixel(RING_OUTER_60, time.second, color);
  }

  void renderMinutes(const ClockTime &time, const ClockSettings &settings) {
    setRingPixel(RING_OUTER_60, time.minute,
                 ringColor(settings.minutesRed, settings.minutesGreen, settings.minutesBlue,
                           settings.minutesLevel));
  }

  void renderHours(const ClockTime &time, const ClockSettings &settings) {
    const uint8_t hour12 = time.hour % 12;
    const uint32_t color = ringColor(settings.hoursRed, settings.hoursGreen, settings.hoursBlue,
                                     settings.hoursLevel);

    // Middle ring (24 LED): 1→2→1 LED by thirds, base shifted +1 CW from hour position.
    // :00–:19 → 1 LED; :20–:39 → 2 LEDs (straddle); :40–:59 → 1 LED advanced.
    const uint8_t midBase = (hour12 * 2 + 1) % RING_MIDDLE_24.count;
    if (time.minute < 20) {
      setRingPixel(RING_MIDDLE_24, midBase, color);
    } else if (time.minute < 40) {
      setRingPixel(RING_MIDDLE_24, midBase, color);
      setRingPixel(RING_MIDDLE_24, (midBase + 1) % RING_MIDDLE_24.count, color);
    } else {
      setRingPixel(RING_MIDDLE_24, (midBase + 1) % RING_MIDDLE_24.count, color);
    }

    // Inner ring (12 LED): fixed 1 LED per hour. Middle ring carries sub-hour progression.
    setRingPixel(RING_INNER_12, hour12, color);
  }

  uint32_t secondColor(uint8_t intensity) {
    switch (settings_.get().colorTheme) {
      case 1:
        return strip_.Color(0, intensity, 65);
      case 2:
        return strip_.Color(intensity, 0, 90);
      default:
        return strip_.Color(0, 0, intensity);
    }
  }

  uint32_t ringColor(uint8_t r, uint8_t g, uint8_t b, uint8_t level) {
    return strip_.Color((static_cast<uint16_t>(r) * level) / 255,
                        (static_cast<uint16_t>(g) * level) / 255,
                        (static_cast<uint16_t>(b) * level) / 255);
  }

  void renderStatus(uint32_t now) {
    uint8_t head = (now / 90) % RING_INNER_12.count;
    uint32_t color = strip_.Color(40, 40, 40);
    switch (statusMode_) {
      case STATUS_WIFI_CONNECTING:
        color = strip_.Color(0, 0, 140);
        break;
      case STATUS_WIFI_OK:
        color = strip_.Color(0, 120, 20);
        break;
      case STATUS_WIFI_FAIL:
        color = strip_.Color(140, 0, 0);
        break;
      case STATUS_BUTTON:
        color = strip_.Color(120, 50, 0);
        break;
      case STATUS_TIME_SYNC:
        color = strip_.Color(0, 100, 90);
        break;
      case STATUS_SETTINGS_SAVED:
        color = strip_.Color(70, 0, 120);
        break;
      case STATUS_OTA_UPDATE:
        color = strip_.Color(0, 0, 200);  // Blue for OTA update in progress
        break;
      case STATUS_OTA_SUCCESS:
        color = strip_.Color(0, 200, 0);  // Green for OTA success
        break;
      case STATUS_OTA_FAILED:
        color = strip_.Color(200, 0, 0);  // Red for OTA failure
        break;
      default:
        break;
    }
    setRingPixel(RING_INNER_12, head, color);
    setRingPixel(RING_INNER_12, (head + RING_INNER_12.count - 1) % RING_INNER_12.count, dim(color, 3));
    setCenterPixel(pulse(color, now, 220, 6, 180));
  }

  void renderHourlyChime(uint32_t now) {
    uint8_t sweep = (now / 65) % RING_OUTER_60.count;
    setRingPixel(RING_OUTER_60, sweep, strip_.Color(80, 80, 80));
    setRingPixel(RING_MIDDLE_24, (sweep * RING_MIDDLE_24.count) / RING_OUTER_60.count, strip_.Color(120, 80, 0));
    setCenterPixel(pulse(strip_.Color(90, 65, 10), now, 180, 4, 130));
  }

  uint32_t paletteColor(uint8_t position, bool useReminderPalette = false) {
    const ClockSettings &s = settings_.get();
    const uint8_t br = s.animationBrightness;
    const uint8_t pal = useReminderPalette ? s.reminderPalette : s.animationPalette;
    if (useReminderPalette) {
      switch (pal) {
        case 0: {
          uint8_t g = (uint8_t)((uint16_t)position * br / 510 + br / 4);
          return strip_.Color(br, g, 0);
        }
        case 1:
          return strip_.Color(br, (uint8_t)(position / 8), 0);
        case 2:
          return strip_.Color(br, 0, (uint8_t)((uint16_t)position * br / 510));
        case 3:
          return strip_.Color((uint8_t)(position / 4), (uint8_t)(br * 3 / 4), (uint8_t)(br / 2));
        default:
          return strip_.Color(br, br / 4, 0);
      }
    }
    switch (pal) {
      case 0:
        return strip_.ColorHSV((uint16_t)((uint32_t)position * 65536 / 256), 255, br);
      case 1: {
        if (position < 85)  return strip_.Color((uint8_t)((uint32_t)position * 3 * br / 255), 0, 0);
        if (position < 170) return strip_.Color(br, (uint8_t)((uint32_t)(position - 85) * 3 * br / 510), 0);
        return strip_.Color(br, (uint8_t)((uint32_t)(position - 170) * 3 * br / 510 + br / 2), 0);
      }
      case 2: {
        if (position < 128) return strip_.Color(0, (uint8_t)((uint16_t)position * br / 128), br);
        return strip_.Color((uint8_t)((uint16_t)(position - 128) * br / 127),
                            (uint8_t)(br / 2 + (uint16_t)(position - 128) * br / 254), br);
      }
      case 3: {
        if (position < 128) return strip_.Color(0, (uint8_t)((uint16_t)position * br / 128), 0);
        return strip_.Color((uint8_t)((uint16_t)(position - 128) * br / 127),
                            br, (uint8_t)((uint16_t)(position - 128) * br / 127));
      }
      case 4: {
        uint16_t hue = (uint16_t)((uint32_t)position * 43690 / 256) + 54613;
        return strip_.ColorHSV(hue, 220, br);
      }
      case 5: {
        uint16_t hue = (uint16_t)((uint32_t)position * 32768 / 256) + 43690;
        return strip_.ColorHSV(hue, 255, br);
      }
      case 6: {
        uint8_t scaled = (uint8_t)((uint16_t)position * br / 255);
        return ringColor(s.hoursRed, s.hoursGreen, s.hoursBlue, scaled);
      }
      case 7: {
        if (position < 64)  return ringColor(s.outerMarkerRed, s.outerMarkerGreen, s.outerMarkerBlue, br);
        if (position < 128) return ringColor(s.minutesRed, s.minutesGreen, s.minutesBlue, br);
        if (position < 192) return ringColor(s.hoursRed, s.hoursGreen, s.hoursBlue, br);
        return ringColor(s.centerRed, s.centerGreen, s.centerBlue, br);
      }
      default:
        return strip_.ColorHSV((uint16_t)((uint32_t)position * 65536 / 256), 255, br);
    }
  }

  uint32_t scaledElapsed(uint32_t elapsed) {
    const uint8_t spd = settings_.get().animationSpeed;
    static const uint8_t num[] = {1, 3, 4, 6, 8};
    static const uint8_t den[] = {2, 4, 4, 4, 4};
    const uint8_t idx = (spd < 1) ? 0u : (spd > 5) ? 4u : (uint8_t)(spd - 1);
    return elapsed * num[idx] / den[idx];
  }

  void tickAnimation(uint32_t now) {
    if (animPhase_ == ANIM_IDLE) return;
    uint32_t elapsed = now - animStartMs_;
    const ClockSettings &settings = settings_.get();

    switch (animPhase_) {

      case ANIM_Q1: {
        if (elapsed >= 600) { animPhase_ = ANIM_IDLE; return; }
        setCenterPixel(strip_.Color(255, 255, 255));
        for (uint8_t i = 0; i < 60; i += 15)
          setRingPixel(RING_OUTER_60, i, strip_.Color(255, 255, 255));
        if (elapsed < 100) {
          strip_.setBrightness(255);
        } else if (elapsed < 500) {
          strip_.setBrightness((uint8_t)(255 - ((elapsed - 100) * 255) / 400));
        } else {
          strip_.setBrightness(0);
        }
        break;
      }

      case ANIM_Q2: {
        if (elapsed >= 2500) { animPhase_ = ANIM_IDLE; return; }
        uint32_t markerColor = ringColor(settings.outerMarkerRed, settings.outerMarkerGreen,
                                         settings.outerMarkerBlue, 255);
        bool bright = (elapsed % 1000) < 500;
        uint32_t c = bright ? markerColor : scale(markerColor, 128);
        for (uint8_t i = 0; i < 60; i += 5)
          setRingPixel(RING_OUTER_60, i, c);
        break;
      }

      case ANIM_Q3: {
        if (elapsed >= 2500) { animPhase_ = ANIM_IDLE; return; }
        renderFace(settings);
        strip_.setBrightness((elapsed % 600) < 400 ? 255 : settings.dayBrightness);
        break;
      }

      case ANIM_H1: {
        if (elapsed >= 5000) { animPhase_ = ANIM_IDLE; return; }
        if (elapsed < 2000) {
          uint8_t n = (uint8_t)(elapsed * RING_OUTER_60.count / 2000);
          for (uint8_t i = 0; i < n; i++)
            setRingPixel(RING_OUTER_60, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_OUTER_60.count)));
        } else {
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_OUTER_60.count)));
          if (elapsed < 4000) {
            uint8_t nm = (uint8_t)((elapsed - 2000) * RING_MIDDLE_24.count / 2000);
            uint8_t ni = (uint8_t)((elapsed - 2000) * RING_INNER_12.count / 2000);
            for (uint8_t i = 0; i < nm; i++)
              setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_MIDDLE_24.count + 21845)));
            for (uint8_t i = 0; i < ni; i++)
              setRingPixel(RING_INNER_12, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_INNER_12.count + 43690)));
          } else {
            for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
              setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_MIDDLE_24.count + 21845)));
            for (uint8_t i = 0; i < RING_INNER_12.count; i++)
              setRingPixel(RING_INNER_12, i, strip_.ColorHSV((uint16_t)(i * 65536UL / RING_INNER_12.count + 43690)));
          }
        }
        break;
      }

      case ANIM_H2: {
        if (elapsed >= 5000) { animPhase_ = ANIM_IDLE; return; }
        uint8_t flash = (uint8_t)(elapsed / 600);
        if (flash < 3) {
          bool swapped = (elapsed % 600) >= 300;
          for (uint8_t i = 0; i < 30; i++) {
            setRingPixel(RING_OUTER_60, i,      swapped ? strip_.Color(0, 255, 255) : strip_.Color(255, 0, 0));
            setRingPixel(RING_OUTER_60, i + 30, swapped ? strip_.Color(255, 0, 0)   : strip_.Color(0, 255, 255));
          }
        }
        break;
      }

      case ANIM_H3: {
        if (elapsed >= 5000) { animPhase_ = ANIM_IDLE; return; }
        uint8_t wave = (uint8_t)(elapsed / 1300);
        if (wave < 3) {
          uint32_t phase = elapsed % 1300;
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++) {
            uint8_t brightness = 64 + (uint8_t)((sinf((i + wave * 10) * 0.1f) + 1.0f) * 96.0f);
            setRingPixel(RING_OUTER_60, i, strip_.Color(0, brightness, brightness));
          }
          strip_.setBrightness(phase < 500 ? 64 : settings.dayBrightness);
        }
        break;
      }

      case ANIM_HR1: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        renderHourlyChime(now);
        break;
      }

      case ANIM_HR2: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        uint8_t newStep;
        if      (elapsed < 500)  newStep = 0;
        else if (elapsed < 600)  newStep = 1;
        else if (elapsed < 1000) newStep = 2;
        else if (elapsed < 1400) newStep = 3;
        else if (elapsed < 1800) newStep = 4;
        else                     newStep = 5;
        if (newStep != animStep_) animStep_ = newStep;

        if (animStep_ >= 1) setCenterPixel(strip_.Color(255, 255, 255));
        if (animStep_ >= 2) {
          randomSeed(0xABCD + 2 * 137);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)
            setRingPixel(RING_INNER_12, i, strip_.ColorHSV(random(65536)));
        }
        if (animStep_ >= 3) {
          randomSeed(0xABCD + 3 * 137);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV(random(65536)));
        }
        if (animStep_ >= 4) {
          randomSeed(0xABCD + 4 * 137);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, strip_.ColorHSV(random(65536)));
        }
        if (animStep_ >= 5) {
          uint8_t sparkStep = (uint8_t)((elapsed - 1800) / 150);
          randomSeed(0x1234 + sparkStep * 31);
          if (random(100) < 30)
            setRingPixel(RING_OUTER_60, (uint8_t)random(60), strip_.Color(255, 255, 255));
        }
        break;
      }

      case ANIM_HR3: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        uint32_t gold = strip_.Color(255, 200, 0);
        if (elapsed < 1200) {
          uint8_t n = (uint8_t)(elapsed * 30 / 1200);
          for (uint8_t i = 0; i < n; i++) {
            setRingPixel(RING_OUTER_60, i, gold);
            setRingPixel(RING_OUTER_60, 59 - i, gold);
          }
        } else if (elapsed < 1400) {
          for (uint8_t i = 0; i < 30; i++) {
            setRingPixel(RING_OUTER_60, i, gold);
            setRingPixel(RING_OUTER_60, 59 - i, gold);
          }
          setCenterPixel(strip_.Color(255, 255, 255));
        } else if (elapsed < 2600) {
          uint8_t n = (uint8_t)((elapsed - 1400) * 30 / 1200);
          for (uint8_t i = n; i < 30; i++) {
            setRingPixel(RING_OUTER_60, i, gold);
            setRingPixel(RING_OUTER_60, 59 - i, gold);
          }
          setCenterPixel(strip_.Color(255, 255, 255));
        }
        break;
      }

      case ANIM_HR4: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        uint8_t outerN, middleN, innerN;
        if (elapsed < 2400) {
          outerN = (uint8_t)(elapsed * 120 / 2400); middleN = 0; innerN = 0;
        } else if (elapsed < 4320) {
          outerN = 120; middleN = (uint8_t)((elapsed - 2400) * 48 / 1920); innerN = 0;
        } else if (elapsed < 5760) {
          outerN = 120; middleN = 48; innerN = (uint8_t)((elapsed - 4320) * 24 / 1440);
        } else {
          outerN = 120; middleN = 48; innerN = 24;
        }
        {
          uint16_t hue = 0;
          for (uint8_t p = 0; p < outerN; p++) {
            setRingPixel(RING_OUTER_60, p % RING_OUTER_60.count, strip_.ColorHSV(hue));
            hue += 1092;
          }
        }
        {
          uint16_t hue = 0;
          for (uint8_t p = 0; p < middleN; p++) {
            uint8_t idx = (uint8_t)((RING_MIDDLE_24.count * 2 - 1 - p) % RING_MIDDLE_24.count);
            setRingPixel(RING_MIDDLE_24, idx, strip_.ColorHSV(hue));
            hue += 2730;
          }
        }
        {
          uint16_t hue = 0;
          for (uint8_t p = 0; p < innerN; p++) {
            setRingPixel(RING_INNER_12, p % RING_INNER_12.count, strip_.ColorHSV(hue));
            hue += 5461;
          }
        }
        break;
      }

      case ANIM_HR5: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        uint8_t breathNum = (uint8_t)(elapsed / 1667);
        uint16_t hue = (uint16_t)(breathNum * 10923u);
        for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
          setRingPixel(RING_OUTER_60, i, strip_.ColorHSV((uint16_t)(hue + i * 1092u)));
        for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
          setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV((uint16_t)(hue + i * 2730u)));
        for (uint8_t i = 0; i < RING_INNER_12.count; i++)
          setRingPixel(RING_INNER_12, i, strip_.ColorHSV((uint16_t)(hue + i * 5461u)));
        {
          float breathPhase = (float)(elapsed % 1667) * 3.14159f / 1667.0f;
          strip_.setBrightness((uint8_t)(50 + fabsf(sinf(breathPhase)) * 205.0f));
        }
        break;
      }

      // ===== QUARTER ADDITIONS =====

      case ANIM_Q4: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 1400) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        const uint8_t tl = settings_.get().trailLength;
        uint32_t headColor = paletteColor(0);
        if (se < 600) {
          uint8_t pos = (uint8_t)(se * 60 / 600) % 60;
          setRingPixel(RING_OUTER_60, pos, headColor);
          for (uint8_t t = 1; t <= tl; t++) {
            uint8_t trailBr = (uint8_t)(255u * (tl - t + 1) / (tl + 1));
            setRingPixel(RING_OUTER_60, (pos + 60 - t) % 60, scale(headColor, trailBr));
          }
        } else if (se < 1000) {
          uint8_t fadeAmt = (uint8_t)(255u - (se - 600u) * 255u / 400u);
          for (uint8_t t = 0; t < tl; t++) {
            uint8_t tp = (uint8_t)((60 - t) % 60);
            uint8_t trailBr = (uint8_t)((uint32_t)fadeAmt * (tl - t) / tl);
            setRingPixel(RING_OUTER_60, tp, scale(headColor, trailBr));
          }
        }
        break;
      }

      case ANIM_Q5: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 2000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        const uint8_t tl = settings_.get().trailLength;
        if (se < 1500) {
          uint8_t pos  = (uint8_t)(se * 60 / 1500) % 60;
          uint8_t pos2 = (pos + 30) % 60;
          uint32_t cA = paletteColor(0);
          uint32_t cB = paletteColor(128);
          setRingPixel(RING_OUTER_60, pos, cA);
          setRingPixel(RING_OUTER_60, pos2, cB);
          for (uint8_t t = 1; t <= tl; t++) {
            uint8_t trailBr = (uint8_t)(255u * (tl - t + 1) / (tl + 1));
            setRingPixel(RING_OUTER_60, (pos  + 60 - t) % 60, scale(cA, trailBr));
            setRingPixel(RING_OUTER_60, (pos2 + 60 - t) % 60, scale(cB, trailBr));
          }
        }
        break;
      }

      case ANIM_Q6: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 800) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          uint8_t step = (uint8_t)(se / 200);
          if (step < 4) {
            const uint8_t positions[] = {0, 15, 30, 45};
            const uint8_t palPos[]    = {0, 85, 170, 255};
            setRingPixel(RING_OUTER_60, positions[step], paletteColor(palPos[step]));
          }
        }
        break;
      }

      // ===== HALF-HOUR ADDITIONS =====

      case ANIM_H4: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 4000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        const uint8_t tl = settings_.get().trailLength;
        uint32_t headColor = paletteColor(0);
        if (se < 3000) {
          uint8_t pos = (uint8_t)(se * 60 / 1500) % 60;
          setRingPixel(RING_OUTER_60, pos, headColor);
          for (uint8_t t = 1; t <= tl; t++) {
            uint8_t trailBr = (uint8_t)(255u * (tl - t + 1) / (tl + 1));
            setRingPixel(RING_OUTER_60, (pos + 60 - t) % 60, scale(headColor, trailBr));
          }
        } else if (se < 3300) {
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(i * 4)));
        }
        break;
      }

      case ANIM_H5: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 3500) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        if (se < 300) {
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(i * 4)));
        } else {
          uint32_t fp = se - 300;
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++) {
            uint8_t rate     = (uint8_t)((i * 7 + 13) % 8 + 2);
            uint32_t ft      = (uint32_t)rate * 300;
            uint8_t  pct     = (fp < ft) ? (uint8_t)(255u - fp * 255u / ft) : 0u;
            if (pct > 0)
              setRingPixel(RING_OUTER_60, i, scale(paletteColor((uint8_t)(i * 4)), pct));
          }
        }
        break;
      }

      case ANIM_H6: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 5000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          const uint8_t tl = settings_.get().trailLength;
          uint32_t headColor = paletteColor(0);
          uint32_t passTime = se % 800;
          uint32_t passNum  = se / 800;
          uint8_t pos = (passNum % 2 == 0)
              ? (uint8_t)(passTime * 59 / 800)
              : (uint8_t)(59 - passTime * 59 / 800);
          setRingPixel(RING_OUTER_60, pos, headColor);
          for (uint8_t t = 1; t <= tl; t++) {
            uint8_t trailBr = (uint8_t)(255u * (tl - t + 1) / (tl + 1));
            uint8_t tp = (passNum % 2 == 0)
                ? (uint8_t)((pos + 60 - t) % 60)
                : (uint8_t)((pos + t) % 60);
            setRingPixel(RING_OUTER_60, tp, scale(headColor, trailBr));
          }
        }
        break;
      }

      case ANIM_H7: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 3000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        if (se < 2000) {
          uint8_t fi = (uint8_t)((se / 125) % 4);
          const uint8_t palPos[] = {0, 85, 170, 255};
          uint32_t c = paletteColor(palPos[fi]);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, c);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, c);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, c);
        } else {
          uint8_t fadeAmt = (uint8_t)(255u - (se - 2000u) * 255u / 500u);
          uint32_t c = scale(paletteColor(0), fadeAmt);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, c);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, c);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, c);
        }
        break;
      }

      // ===== HOUR ADDITIONS =====

      case ANIM_HR6: {
        if (elapsed >= 8500) { animPhase_ = ANIM_IDLE; return; }
        const uint8_t br = settings_.get().animationBrightness;
        if (elapsed < 500) {
          uint8_t rb = (uint8_t)(elapsed * 255 / 500);
          uint32_t white = strip_.Color(rb, rb, rb);
          strip_.setBrightness(br);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, white);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, white);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, white);
        } else if (elapsed < 6000) {
          strip_.setBrightness(br);
          uint16_t hueOff = (uint16_t)((elapsed - 500) * 65536UL / 5500);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, strip_.ColorHSV((uint16_t)(hueOff + i * 65536UL / RING_OUTER_60.count), 255, br));
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV((uint16_t)(hueOff + 21845 + i * 65536UL / RING_MIDDLE_24.count), 255, br));
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)
            setRingPixel(RING_INNER_12, i, strip_.ColorHSV((uint16_t)(hueOff + 43690 + i * 65536UL / RING_INNER_12.count), 255, br));
        } else {
          uint8_t fadeAmt = (elapsed < 8000) ? (uint8_t)(255u - (elapsed - 6000u) * 255u / 2000u) : 0u;
          strip_.setBrightness(fadeAmt);
          uint16_t hueOff = (uint16_t)((6000 - 500) * 65536UL / 5500);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, strip_.ColorHSV((uint16_t)(hueOff + i * 65536UL / RING_OUTER_60.count), 255, br));
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, strip_.ColorHSV((uint16_t)(hueOff + 21845 + i * 65536UL / RING_MIDDLE_24.count), 255, br));
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)
            setRingPixel(RING_INNER_12, i, strip_.ColorHSV((uint16_t)(hueOff + 43690 + i * 65536UL / RING_INNER_12.count), 255, br));
        }
        break;
      }

      case ANIM_HR7: {
        if (elapsed >= 8000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          const uint8_t tl  = settings_.get().trailLength;
          const uint8_t gbr = settings_.get().animationBrightness;
          for (uint8_t d = 0; d < 8; d++) {
            uint32_t period  = (uint32_t)(d * 3 + 5) % 8 * 600 + 1800;
            uint8_t  offset  = (uint8_t)((d * 7 + 3) % 60);
            uint8_t  pos     = (uint8_t)((elapsed % period * 60 / period + offset) % 60);
            setRingPixel(RING_OUTER_60, pos, strip_.Color(0, gbr, 0));
            for (uint8_t t = 1; t <= tl; t++) {
              uint8_t trailBr = (uint8_t)((uint32_t)gbr * (tl - t + 1) / (tl + 1));
              setRingPixel(RING_OUTER_60, (pos + 60 - t) % 60, strip_.Color(0, trailBr, 0));
            }
          }
        }
        break;
      }

      case ANIM_HR8: {
        if (elapsed >= 10000) { animPhase_ = ANIM_IDLE; return; }
        {
          const uint8_t br = settings_.get().animationBrightness;
          strip_.setBrightness(elapsed < 9000 ? br : (uint8_t)((10000u - elapsed) * br / 1000u));
          uint8_t outerOff  = (uint8_t)(elapsed * 256 / 3000);
          uint8_t middleOff = (uint8_t)(86u - (uint8_t)(elapsed * 256 / 5000));
          uint8_t innerOff  = (uint8_t)(171u + (uint8_t)(elapsed * 256 / 7000));
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(outerOff + i * 256 / RING_OUTER_60.count)));
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, paletteColor((uint8_t)(middleOff + i * 256 / RING_MIDDLE_24.count)));
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)
            setRingPixel(RING_INNER_12, i, paletteColor((uint8_t)(innerOff + i * 256 / RING_INNER_12.count)));
        }
        break;
      }

      case ANIM_HR9: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 7000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        if (se < 3000) {
          uint8_t n = (uint8_t)(se * RING_OUTER_60.count / 3000);
          for (uint8_t i = 0; i < n; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(i * 4)));
        } else if (se < 4500) {
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(i * 4)));
          uint8_t n = (uint8_t)((se - 3000) * RING_MIDDLE_24.count / 1500);
          for (uint8_t i = 0; i < n; i++)
            setRingPixel(RING_MIDDLE_24, i, paletteColor((uint8_t)(i * 10)));
        } else {
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, paletteColor((uint8_t)(i * 4)));
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, paletteColor((uint8_t)(i * 10)));
          uint8_t n = (uint8_t)((se - 4500) * RING_INNER_12.count / 1000);
          if (n > RING_INNER_12.count) n = RING_INNER_12.count;
          for (uint8_t i = 0; i < n; i++)
            setRingPixel(RING_INNER_12, i, paletteColor((uint8_t)(i * 21)));
        }
        break;
      }

      case ANIM_HR10: {
        if (elapsed >= 10500) { animPhase_ = ANIM_IDLE; return; }
        {
          const uint8_t br = settings_.get().animationBrightness;
          uint32_t baseColor = strip_.Color((uint8_t)(20u * br / 255u), 0, (uint8_t)(60u * br / 255u));
          float breathPhase = (float)(elapsed % 2000) * 3.14159f / 2000.0f;
          uint8_t breathBr = (uint8_t)(br * (0.8f + 0.2f * sinf(breathPhase)));
          strip_.setBrightness(breathBr);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, baseColor);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, baseColor);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, baseColor);
          const uint16_t strikeTimes[] = {500, 2000, 3500, 5000, 7000};
          for (uint8_t s = 0; s < 5; s++) {
            if (elapsed >= strikeTimes[s] && elapsed < strikeTimes[s] + 80u) {
              uint8_t p1 = (uint8_t)((strikeTimes[s] * 13u + 7u) % 60u);
              uint8_t p2 = (uint8_t)((strikeTimes[s] * 23u + 31u) % 60u);
              uint8_t p3 = (uint8_t)((strikeTimes[s] * 37u + 53u) % 60u);
              setRingPixel(RING_OUTER_60, p1, strip_.Color(255, 255, 255));
              setRingPixel(RING_OUTER_60, p2, strip_.Color(255, 255, 255));
              setRingPixel(RING_OUTER_60, p3, strip_.Color(255, 255, 255));
              setCenterPixel(strip_.Color(255, 255, 255));
            }
          }
        }
        break;
      }

      // ===== REMINDER ANIMATIONS =====

      case ANIM_REM1: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 4000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          uint32_t c = paletteColor(0, true);
          const uint16_t starts[] = {0, 350, 700, 1050};
          for (uint8_t r = 0; r < 4; r++) {
            if (se < starts[r]) continue;
            uint32_t rElap = se - starts[r];
            if (rElap >= 1500) continue;
            uint32_t cp = rElap % 500;
            uint8_t pct;
            if (cp < 200)      pct = (uint8_t)(cp * 255u / 200u);
            else if (cp < 300) pct = 255;
            else               pct = (uint8_t)(255u - (cp - 300u) * 255u / 200u);
            uint32_t rc = scale(c, pct);
            switch (r) {
              case 0: for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, rc);  break;
              case 1: for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, rc); break;
              case 2: for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, rc);  break;
              case 3: setCenterPixel(rc); break;
            }
          }
        }
        break;
      }

      case ANIM_REM2: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 2000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          const ClockSettings &s2 = settings_.get();
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, ringColor(s2.outerMarkerRed, s2.outerMarkerGreen, s2.outerMarkerBlue, 38));
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, ringColor(s2.minutesRed, s2.minutesGreen, s2.minutesBlue, 38));
          uint32_t period = 667;
          uint8_t pos = (uint8_t)((se % period) * RING_INNER_12.count / period);
          uint32_t c = paletteColor(0, true);
          setRingPixel(RING_INNER_12, pos, c);
          for (uint8_t t = 1; t <= 2; t++) {
            uint8_t trailBr = (uint8_t)(255u * (2u - t + 1u) / 3u);
            setRingPixel(RING_INNER_12, (pos + RING_INNER_12.count - t) % RING_INNER_12.count, scale(c, trailBr));
          }
        }
        break;
      }

      case ANIM_REM3: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 5000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          uint32_t c = paletteColor(0, true);
          uint32_t cp = se % 1160;
          uint8_t pct;
          if      (cp < 80)  pct = (uint8_t)(cp * 255u / 80u);
          else if (cp < 160) pct = 255;
          else if (cp < 280) pct = (uint8_t)(255u - (cp - 160u) * 255u / 120u);
          else if (cp < 360) pct = (uint8_t)((cp - 280u) * 255u / 80u);
          else if (cp < 440) pct = 255;
          else if (cp < 640) pct = (uint8_t)(255u - (cp - 440u) * 255u / 200u);
          else               pct = 38;
          uint32_t rc = scale(c, pct);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, rc);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, rc);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, rc);
          setCenterPixel(rc);
        }
        break;
      }

      case ANIM_REM4: {
        const uint32_t se = scaledElapsed(elapsed);
        if (se >= 6000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        if (se < 1000) {
          uint8_t pct = (uint8_t)(se * 255u / 1000u);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)
            setRingPixel(RING_INNER_12, i, scale(strip_.Color(60, 0, 0), pct));
        } else if (se < 2500) {
          uint8_t ip = (uint8_t)((se - 1000u) * 32u / 1500u);
          uint32_t ic = paletteColor(ip, true);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++) setRingPixel(RING_INNER_12, i, ic);
          uint8_t mp = (uint8_t)((se - 1000u) * 128u / 1500u);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++)
            setRingPixel(RING_MIDDLE_24, i, scale(strip_.Color(60, 0, 0), mp));
        } else if (se < 4000) {
          for (uint8_t i = 0; i < RING_INNER_12.count; i++) setRingPixel(RING_INNER_12, i, paletteColor(32, true));
          uint8_t mp = (uint8_t)((se - 2500u) * 32u / 1500u);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, paletteColor(mp, true));
          uint8_t op = (uint8_t)((se - 2500u) * 128u / 1500u);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)
            setRingPixel(RING_OUTER_60, i, scale(strip_.Color(60, 0, 0), op));
        } else {
          uint8_t pos = (uint8_t)((se - 4000u) * 128u / 2000u + 64u);
          uint32_t c = paletteColor(pos, true);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++)  setRingPixel(RING_OUTER_60, i, c);
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) setRingPixel(RING_MIDDLE_24, i, c);
          for (uint8_t i = 0; i < RING_INNER_12.count; i++)  setRingPixel(RING_INNER_12, i, c);
        }
        break;
      }

      case ANIM_REM5: {
        if (elapsed >= 5000) { animPhase_ = ANIM_IDLE; return; }
        animStep_++;
        {
          float breathPhase = (float)(elapsed % 2000) * 3.14159f / 2000.0f;
          uint8_t breathBr = (uint8_t)(settings_.get().animationBrightness * (0.8f + 0.2f * sinf(breathPhase)));
          strip_.setBrightness(breathBr);
          uint8_t seed = animStep_;
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++) {
            uint8_t pn = (uint8_t)((i * 7 + seed * 3) % 160);
            setRingPixel(RING_OUTER_60, i, paletteColor(pn, true));
          }
          for (uint8_t i = 0; i < RING_MIDDLE_24.count; i++) {
            uint8_t pn = (uint8_t)((i * 11 + seed * 5) % 160);
            setRingPixel(RING_MIDDLE_24, i, paletteColor(pn, true));
          }
          for (uint8_t i = 0; i < RING_INNER_12.count; i++) {
            uint8_t pn = (uint8_t)((i * 17 + seed * 7) % 160);
            setRingPixel(RING_INNER_12, i, paletteColor(pn, true));
          }
          if (elapsed % 200 < 50) {
            uint8_t sp = (uint8_t)((seed * 47 + 13) % 60);
            setRingPixel(RING_OUTER_60, sp, strip_.Color(255, 220, 100));
          }
        }
        break;
      }

      case ANIM_REM6: {
        if (elapsed >= 4000) { animPhase_ = ANIM_IDLE; return; }
        strip_.setBrightness(settings_.get().animationBrightness);
        {
          uint32_t c = paletteColor(0, true);
          uint32_t dimOuter = scale(c, 30);
          for (uint8_t i = 0; i < RING_OUTER_60.count; i++) setRingPixel(RING_OUTER_60, i, dimOuter);
          bool glitch = (elapsed % 700) >= 550 && (elapsed % 700) < 670;
          if (!glitch) {
            bool on = (elapsed % 82) < 40;
            if (on) {
              for (uint8_t i = 0; i < RING_INNER_12.count; i++) setRingPixel(RING_INNER_12, i, c);
            }
          }
        }
        break;
      }

      default:
        animPhase_ = ANIM_IDLE;
        break;
    }
  }

  bool chimeActive(const ClockTime &time) const {
    return time.minute == 0 && time.second < 6;
  }

  bool statusActive(uint32_t now) const {
    return statusMode_ != STATUS_NONE && static_cast<int32_t>(statusUntilMs_ - now) > 0;
  }

  void logShow(uint32_t now, const char *src) {
    if (lastShowMs_ != 0) {
      uint32_t gap = now - lastShowMs_;
      if (gap < minFrameMs_) minFrameMs_ = gap;
      if (gap > maxFrameMs_) maxFrameMs_ = gap;
      if (gap < 15) {
        Serial.printf("[RENDER] RAPID %s gap=%lums\n", src, (unsigned long)gap);
      }
    }
    ++renderCount_;
    lastShowMs_ = now;
    if (now - lastDiagMs_ >= 30000) {
      if (lastDiagMs_ != 0) {
        uint32_t dt = now - lastDiagMs_;
        Serial.printf("[RENDER] src=%s fps=%.1f min=%lu max=%lu ms/frame\n",
                      src, renderCount_ * 1000.0f / dt,
                      minFrameMs_ == 0xFFFFFFFFu ? 0UL : (unsigned long)minFrameMs_,
                      (unsigned long)maxFrameMs_);
      }
      renderCount_ = 0;
      minFrameMs_ = 0xFFFFFFFFu;
      maxFrameMs_ = 0;
      lastDiagMs_ = now;
    }
  }

  uint32_t dim(uint32_t color, uint8_t divisor) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return strip_.Color(r / divisor, g / divisor, b / divisor);
  }

  uint32_t scale(uint32_t color, uint8_t amount) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return strip_.Color((static_cast<uint16_t>(r) * amount) / 255,
                        (static_cast<uint16_t>(g) * amount) / 255,
                        (static_cast<uint16_t>(b) * amount) / 255);
  }

  uint32_t pulse(uint32_t color, uint32_t now, uint16_t periodMs, uint8_t floor, uint8_t ceiling) {
    const uint16_t phase = now % periodMs;
    const uint16_t half = periodMs / 2;
    const uint16_t ramp = phase < half ? phase : periodMs - phase;
    const uint8_t span = ceiling - floor;
    const uint8_t amount = floor + (static_cast<uint32_t>(span) * ramp) / half;
    return scale(color, amount);
  }

  bool centerIdleActive() const {
    return CENTER_PIXEL_ENABLED != 0 && settings_.get().statusAnimations;
  }

  void renderCenterIdle(uint32_t now) {
    if (!centerIdleActive()) return;
    const ClockSettings &settings = settings_.get();
    uint32_t color = ringColor(settings.centerRed, settings.centerGreen, settings.centerBlue,
                               settings.centerLevel);
    setCenterPixel(pulse(color, now, 1800, 3, 75));
  }

  void setCenterPixel(uint32_t color) {
#if CENTER_PIXEL_ENABLED
#if CENTER_PIXEL_SEPARATE_OUTPUT
    if (centerStrip_ && CENTER_PIXEL_INDEX < CENTER_PIXEL_STRIP_COUNT) {
      centerStrip_->setPixelColor(CENTER_PIXEL_INDEX, color);
    }
#else
    if (CENTER_PIXEL_INDEX < CLOCK_PIXEL_COUNT) {
      strip_.setPixelColor(CENTER_PIXEL_INDEX, color);
    }
#endif
#else
    (void)color;
#endif
  }

  void setSacrificialPixelDark() {
#if SACRIFICIAL_PIXEL_ENABLED
    if (SACRIFICIAL_PIXEL_INDEX < CLOCK_PIXEL_COUNT) {
      strip_.setPixelColor(SACRIFICIAL_PIXEL_INDEX, 0);
    }
#endif
  }

  void setRingPixel(const RingConfig &ring, uint8_t logicalIndex, uint32_t color) {
    if (logicalIndex >= ring.count) return;
    uint8_t rot = settings_.get().outerRingOffset;
    uint8_t rotated = rot
        ? static_cast<uint8_t>((static_cast<uint32_t>(logicalIndex) + rot * ring.count / 60) % ring.count)
        : logicalIndex;
    uint8_t physical = ring.clockwise ? rotated : (ring.count - 1 - rotated);
    strip_.setPixelColor(ring.offset + physical, color);
  }

  Adafruit_NeoPixel &strip_;
  SettingsStore &settings_;
  Adafruit_NeoPixel *centerStrip_;
  LuxSensor *lux_;
  StatusMode statusMode_ = STATUS_NONE;
  uint32_t statusUntilMs_ = 0;
  uint32_t lastAnimationMs_ = 0;
  ClockTime lastTime_ = {12, 0, 0};
  AnimPhase animPhase_ = ANIM_IDLE;
  uint32_t animStartMs_ = 0;
  uint8_t animStep_ = 0;
  uint32_t animHue_ = 0;
  uint32_t lastShowMs_ = 0;
  uint32_t renderCount_ = 0;
  uint32_t minFrameMs_ = 0xFFFFFFFFu;
  uint32_t maxFrameMs_ = 0;
  uint32_t lastDiagMs_ = 0;
};

// ===================== Temperature placeholder =====================
class TemperatureInput {
 public:
  void begin() {}
  bool available() const { return TEMP_SENSOR_ENABLED != 0; }
  float celsius() const { return NAN; }
};

// ===================== Time sync =====================
class TimeSync {
 public:
  explicit TimeSync(TimeModel &model) : model_(model) {}

  void begin() {
#if ENABLE_NTP
    configTzTime(NTP_TIMEZONE_TZ, "pool.ntp.org", "time.nist.gov", "time.google.com");
    // Fire syncNow() from the Arduino loop task each time SNTP updates the system
    // clock, rather than polling blindly. This closes the race where the system
    // clock is set to UTC but localtime_r hasn't yet re-applied the TZ string.
    sntp_set_time_sync_notification_cb([](struct timeval *) {
      TimeSync::s_sntpPending_ = true;
    });
#endif
  }

  bool syncNow() {
#if ENABLE_NTP
    time_t rawUtc = time(nullptr);
    if (rawUtc < 1700000000) return false;
    struct tm localTime;
    if (!localtime_r(&rawUtc, &localTime)) return false;
    {
      ClockTime old = model_.get();
      int32_t oldSec = old.hour * 3600 + old.minute * 60 + old.second;
      int32_t newSec = localTime.tm_hour * 3600 + localTime.tm_min * 60 + localTime.tm_sec;
      lastDeltaSec_ = newSec - oldSec;
      if (lastDeltaSec_ > 43200)  lastDeltaSec_ -= 86400;
      if (lastDeltaSec_ < -43200) lastDeltaSec_ += 86400;
    }
    model_.set(localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    lastSyncMs_ = millis();
    synced_ = true;
    Serial.printf("[NTP] Time synced: %02d:%02d:%02d local  (UTC epoch %lu)\n",
                  localTime.tm_hour, localTime.tm_min, localTime.tm_sec,
                  (unsigned long)rawUtc);
    return true;
#else
    return false;
#endif
  }

  void loop() {
    if (WiFi.status() != WL_CONNECTED) return;
    const uint32_t now = millis();
    // Consume SNTP callback flag (set from SNTP task, read here in Arduino task).
    // On single-core ESP32-C3, volatile bool read/write is effectively atomic.
    const bool sntpFired = s_sntpPending_;
    if (sntpFired) s_sntpPending_ = false;
    if (sntpFired || !synced_ || now - lastSyncMs_ > syncIntervalMs_) {
      syncNow();
    }
  }

  bool synced() const { return synced_; }

  int32_t lastDeltaSec() const { return lastDeltaSec_; }

  static volatile bool s_sntpPending_;

 private:
  TimeModel &model_;
  bool synced_ = false;
  uint32_t lastSyncMs_ = 0;
  int32_t lastDeltaSec_ = 0;
  static constexpr uint32_t syncIntervalMs_ = 6UL * 60UL * 60UL * 1000UL;
};

volatile bool TimeSync::s_sntpPending_ = false;

// Forward declaration — defined in the globals section below
extern Adafruit_NeoPixel ledStrip;

// ===================== WiFi Setup (WiFiManager) =====================
bool setupWiFi() {
  Preferences prefs;
  prefs.begin("factory", false);
  const bool forcePortal = prefs.getBool("portal", false);
  if (forcePortal) prefs.putBool("portal", false);
  prefs.end();

  WiFi.setAutoReconnect(true);
  WiFiManager wm;
  wm.setConnectRetries(1);

  // Blue LEDs while portal is open — visible from across the room
  wm.setAPCallback([](WiFiManager *) {
    ledStrip.setBrightness(150);
    ledStrip.fill(ledStrip.Color(0, 60, 255));
    ledStrip.show();
    Serial.println("[WiFi] Portal open — connect to esp32c3-clock-setup");
  });

  if (forcePortal) {
    Serial.println("[WiFi] Factory-reset flag set — launching provisioning portal.");
    WiFi.disconnect(false, true);  // erase saved AP from NVS
    delay(200);
    { Preferences wprefs; wprefs.begin("wifi", false); wprefs.clear(); wprefs.end(); }
    wm.resetSettings();
    wm.setConfigPortalTimeout(0);  // stay open until user configures
    return wm.startConfigPortal("esp32c3-clock-setup", "");
  }

  // Priority 1: credentials saved via /wifi web page
  {
    Preferences wprefs;
    wprefs.begin("wifi", true);
    String savedSsid = wprefs.getString("ssid", "");
    String savedPass = wprefs.getString("pass", "");
    wprefs.end();
    if (savedSsid.length() > 0) {
      Serial.printf("[WiFi] Trying saved SSID: %s\n", savedSsid.c_str());
      WiFi.begin(savedSsid.c_str(), savedPass.c_str());
      const uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        Serial.print(".");
        delay(500);
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected with saved credentials");
        return true;
      }
      Serial.printf("[WiFi] Saved credentials failed, status=%d.\n", WiFi.status());
      WiFi.disconnect(false);
      delay(250);
    }
  }

  wm.setConfigPortalTimeout(120);  // 2-minute portal window for normal fallback

  if (strcmp(WIFI_SSID, "clock-ssid") != 0) {
    Serial.printf("[WiFi] Trying build-time SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connected with build-time credentials");
      return true;
    }

    Serial.printf("[WiFi] Build-time credentials failed, status=%d. Starting setup portal.\n",
                  WiFi.status());
    WiFi.disconnect(false);
    delay(250);
  }

  return wm.autoConnect("esp32c3-clock-setup", "");
}

// ===================== OTA Setup =====================
void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update starting...");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Update complete, rebooting...");
    delay(2000);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    esp_task_wdt_reset();
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 500) {
      Serial.printf("[OTA] Progress: %u/%u (%.1f%%)\n", progress, total,
                   (float)progress * 100.0 / total);
      lastLog = millis();
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error %u: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    else Serial.println("Unknown");
    ESP.restart();
  });

  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready. Upload: pio run -e esp32c3_v3_8inch -t upload --upload-port %s.local:3232\n", DEVICE_HOSTNAME);
}

// Incremented by ButtonInput on each consume. Readable from WebUi /diag handler.
static uint32_t g_buttonEventCount = 0;

// ===================== Web UI =====================
class WebUi {
 public:
  WebUi(TimeModel &model, SettingsStore &settings, ClockRenderer &renderer,
        TimeSync &timeSync, TemperatureInput &temperature)
      : model_(model),
        settings_(settings),
        renderer_(renderer),
        timeSync_(timeSync),
        temperature_(temperature),
        lux_(nullptr),
        server_(80) {}

  void setLuxSensor(LuxSensor *lux) { lux_ = lux; }

  void begin() {
#if ENABLE_WIFI_UI
    Serial.println("[WiFi] Starting WiFi provisioning...");
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    renderer_.setStatus(STATUS_WIFI_CONNECTING, WIFI_CONNECT_TIMEOUT_MS);

    if (!setupWiFi() || WiFi.status() != WL_CONNECTED) {
      // STA failed — broadcast own AP so device is always reachable
      WiFi.mode(WIFI_AP);
      WiFi.softAP(DEVICE_HOSTNAME);
      apMode_ = true;
      Serial.printf("[WiFi] AP mode active. SSID=%s  IP=192.168.4.1\n", DEVICE_HOSTNAME);
      Serial.println("[WiFi] Connect to that SSID then visit http://192.168.4.1/");
      renderer_.setStatus(STATUS_WIFI_FAIL, 3000);
    } else {
      // Reapply hostname after STA connection stabilises
      delay(2000);
      WiFi.setHostname(DEVICE_HOSTNAME);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);  // prevent modem-sleep RMT interference with WS2812B
      Serial.printf("[WiFi] Hostname: %s\n", WiFi.getHostname());
      Serial.printf("[WiFi] SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    }

    enabled_ = true;
    setupRoutes();
    server_.begin();

    if (!apMode_) {
      mdnsEnabled_ = MDNS.begin(DEVICE_HOSTNAME);
      if (mdnsEnabled_) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mDNS] Registered: %s.local\n", DEVICE_HOSTNAME);
      } else {
        Serial.println("[mDNS] Start failed");
      }

      // Re-register mDNS after WiFi reconnects
      WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (mdnsEnabled_) {
          MDNS.begin(DEVICE_HOSTNAME);
          Serial.printf("[mDNS] Re-registered after reconnect: %s.local\n", DEVICE_HOSTNAME);
        }
      }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

      setupOTA();

      timeSync_.begin();
      if (timeSync_.syncNow()) {
        renderer_.setStatus(STATUS_TIME_SYNC, 1500);
      } else {
        renderer_.setStatus(STATUS_WIFI_OK, 1500);
      }
    }
#endif
  }

  void loop() {
    if (!enabled_) return;
    server_.handleClient();
    ArduinoOTA.handle();

    if (!apMode_) {
      static uint32_t lastReconnectMs = 0;
      const uint32_t now = millis();
      if (WiFi.status() != WL_CONNECTED && now - lastReconnectMs >= 30000) {
        lastReconnectMs = now;
        Serial.println("[WiFi] Not connected — attempting reconnect...");
        WiFi.reconnect();
      }
    }
  }

  bool enabled() const { return enabled_; }
  bool apMode() const { return apMode_; }

 private:
  void setupRoutes() {
    server_.on("/", HTTP_GET, [&]() {
      static const char HTML_P1[] PROGMEM =
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>\n"
        "<title>ESP32 Ring Clock</title>\n"
        "<style>\n"
        ":root{color-scheme:dark;--bg:#090b10;--panel:#151922;--panel2:#10141c;--line:#2c3442;--text:#eef3fb;--muted:#92a0b5;--accent:#6bd7ff}\n"
        "*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 50% 18%,#17202f 0,#090b10 54%);color:var(--text);font-family:system-ui,Segoe UI,sans-serif}\n"
        "main{display:grid;grid-template-columns:minmax(280px,430px) minmax(310px,1fr);gap:18px;max-width:1120px;margin:0 auto;padding:18px}\n"
        ".stage,.panel{background:linear-gradient(180deg,var(--panel),var(--panel2));border:1px solid var(--line);border-radius:8px;box-shadow:0 18px 45px #0008}\n"
        ".stage{position:sticky;top:14px;align-self:start;padding:16px}.clock-wrap{display:grid;place-items:center;min-height:380px}\n"
        "h1{font-size:18px;margin:0 0 4px}.sub{color:var(--muted);font-size:13px;margin:0 0 12px}#now{font-size:42px;font-weight:750;letter-spacing:0;margin:8px 0 2px}.state{color:var(--muted);font-size:13px;line-height:1.4;min-height:20px}\n"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.panel{padding:14px;margin-bottom:12px}.panel h2{font-size:15px;margin:0 0 12px;color:#dbe7f7}\n"
        "label{display:block;color:var(--muted);font-size:12px;margin:10px 0 4px}input,select,button{font:inherit;border-radius:6px;border:1px solid #374253;background:#0c1017;color:var(--text);padding:8px;min-height:38px}\n"
        "input[type=number]{width:82px}input[type=color]{width:58px;padding:3px}.row{display:flex;gap:8px;align-items:end;flex-wrap:wrap}.ringrow{display:grid;grid-template-columns:82px 66px 1fr 54px;gap:8px;align-items:center;margin:8px 0}.ringrow span{color:#dbe7f7}\n"
        "button{cursor:pointer;background:#203146;border-color:#42546d}button.primary{background:#145875;border-color:#2d9ccb;color:white}.toggle{display:flex;gap:8px;flex-wrap:wrap}.toggle label{display:flex;gap:6px;align-items:center;margin:0;color:#dce6f5;background:#0c1017;border:1px solid #303846;border-radius:6px;padding:8px}\n"
        "svg{width:min(86vw,380px);height:auto;display:block}.led{opacity:.18;transition:fill .18s,opacity .18s,filter .18s}.on{opacity:1;filter:drop-shadow(0 0 5px currentColor)}.ghost{opacity:.4}.marker{opacity:.5}.center{filter:drop-shadow(0 0 12px currentColor)}\n"
        "@media(max-width:820px){main{grid-template-columns:1fr}.stage{position:static}.grid{grid-template-columns:1fr}.clock-wrap{min-height:300px}}\n"
        "</style></head>\n";
      static const char HTML_P2[] PROGMEM =
        "<body><main>\n"
        "<section class='stage'>\n"
        "<h1>ESP32 Ring Clock</h1><p class='sub'>Outer 60 LEDs are the clock face, minute hand, and second hand; middle/inner rings show hours.</p>\n"
        "<div class='clock-wrap'><svg id='clockSvg' viewBox='0 0 420 420' role='img' aria-label='NeoPixel clock preview'></svg></div>\n"
        "<div id='now'>--:--:--</div><div id='state' class='state'>Connecting...</div>\n"
        "</section>\n"
        "<section>\n"
        "<div class='grid'>\n"
        "<div class='panel'><h2>Time</h2>\n"
        "<form onsubmit='setTime();return false;' class='row'>\n"
        "<div><label>Hour</label><input id='h' type='number' min='0' max='23' placeholder='HH'></div>\n"
        "<div><label>Minute</label><input id='m' type='number' min='0' max='59' placeholder='MM'></div>\n"
        "<div><label>Second</label><input id='s' type='number' min='0' max='59' placeholder='SS'></div>\n"
        "<button class='primary' type='submit'>Set</button></form>\n"
        "<div class='row'><button onclick='post(\"/addMinute\")'>+1 min</button><button onclick='post(\"/subMinute\")'>-1 min</button><button onclick='syncBrowser()'>Browser sync</button><button onclick='post(\"/syncNtp\")'>NTP sync</button></div>\n"
        "</div>\n"
        "<div class='panel'><h2>Display</h2>\n"
        "<div class='row'><div><label>Day</label><input id='dayBrightness' type='number' min='0' max='255'></div><div><label>Night</label><input id='nightBrightness' type='number' min='0' max='255'></div><div><label>Night start</label><input id='nightStartHour' type='number' min='0' max='23'></div><div><label>Night end</label><input id='nightEndHour' type='number' min='0' max='23'></div></div>\n"
        "<label>Preview effect</label><select id='previewMode'><option value='live'>Live clock</option><option value='trail'>All-ring trails</option><option value='spark'>Hourly sparkle</option></select>\n"
        "</div></div>\n"
        "<div class='panel'><h2>Rings</h2>\n"
        "<div class='ringrow'><span>Outer marks</span><input id='outerMarkerColor' type='color'><input id='outerMarkerLevel' type='range' min='0' max='255'><output id='outerMarkerLevelOut'></output></div>\n"
        "<div class='ringrow'><span>Outer fill</span><input id='outerFillerColor' type='color'><input id='outerFillerLevel' type='range' min='0' max='255'><output id='outerFillerLevelOut'></output></div>\n"
        "<div class='ringrow'><span>Outer sec</span><input id='secondsColor' type='color'><input id='secondsLevel' type='range' min='0' max='255'><output id='secondsLevelOut'></output></div>\n"
        "<div class='ringrow'><span>Outer min</span><input id='minutesColor' type='color'><input id='minutesLevel' type='range' min='0' max='255'><output id='minutesLevelOut'></output></div>\n"
        "<div class='ringrow'><span>Hour rings</span><input id='hoursColor' type='color'><input id='hoursLevel' type='range' min='0' max='255'><output id='hoursLevelOut'></output></div>\n"
        "<div class='ringrow'><span>Center</span><input id='centerColor' type='color'><input id='centerLevel' type='range' min='0' max='255'><output id='centerLevelOut'></output></div>\n"
        "<div class='row'><div><label>Theme</label><select id='colorTheme'><option value='0'>Classic</option><option value='1'>Aqua</option><option value='2'>Magenta</option></select></div></div>\n"
        "<div class='toggle'><label><input id='secondTrail' type='checkbox'>Second trail</label><label><input id='progressSeconds' type='checkbox'>Progress ring</label><label><input id='hourlyChime' type='checkbox'>Hourly chime</label><label><input id='statusAnimations' type='checkbox'>Status</label></div>\n"
        "<div class='row'><div><label>Ring rotation offset (0-59 LEDs)</label><input id='outerRingOffset' type='number' min='0' max='59' style='width:70px'></div></div>\n"
        "<div class='row'><button class='primary' onclick='saveSettings()'>Save display</button></div>\n"
        "</div>\n"
        "<div class='panel'><h2>Auto-Brightness</h2>\n"
        "<select id='autoBrightnessMode'>\n"
        "  <option value='0'>Manual</option>\n"
        "  <option value='1'>Auto (light sensor)</option>\n"
        "  <option value='2'>Scheduled (day/night)</option>\n"
        "</select>\n"
        "<div id='autoPanel' style='display:none'>\n"
        "  <label>Current light: <span id='luxValue'>--</span> lux</label>\n"
        "  <div class='row'>\n"
        "    <div><label>Min bright</label><input id='minAutoBrightness' type='number' min='5' max='255'></div>\n"
        "    <div><label>Max bright</label><input id='maxAutoBrightness' type='number' min='5' max='255'></div>\n"
        "  </div>\n"
        "</div>\n"
        "<div class='row'><button class='primary' onclick='saveSettings()'>Save auto-brightness</button></div>\n"
        "</div>\n"
        "<div class='panel'><h2>Time Animations</h2>\n"
        "<label><input id='intervalAnimationsEnabled' type='checkbox'> Enable interval animations</label>\n"
        "<div class='row'>\n"
        "  <div><label>Quarter (:15/:30/:45)</label><select id='quarterAnimation'>\n"
        "    <option value='0'>Off</option>\n"
        "    <option value='1'>Sparkle burst</option>\n"
        "    <option value='2'>Quarter pulse</option>\n"
        "    <option value='3'>Ring shimmer</option>\n"
        "    <option value='4'>Laser ping</option>\n"
        "    <option value='5'>DNA twist</option>\n"
        "    <option value='6'>Tick spark</option>\n"
        "  </select><button onclick=\"previewAnim('quarter','quarterAnimation')\">&#9654; Preview</button></div>\n"
        "  <div><label>Half-hour (:30)</label><select id='halfHourAnimation'>\n"
        "    <option value='0'>Off</option>\n"
        "    <option value='1'>Rainbow sweep</option>\n"
        "    <option value='2'>Dual flash</option>\n"
        "    <option value='3'>Tidal pulse</option>\n"
        "    <option value='4'>Comet chase</option>\n"
        "    <option value='5'>Color explosion</option>\n"
        "    <option value='6'>Knight Rider</option>\n"
        "    <option value='7'>Strobe party</option>\n"
        "  </select><button onclick=\"previewAnim('halfhour','halfHourAnimation')\">&#9654; Preview</button></div>\n"
        "</div>\n"
        "<label>Top of hour (:00)</label><select id='hourAnimation'>\n"
        "  <option value='0'>Off</option>\n"
        "  <option value='1'>Current chime</option>\n"
        "  <option value='2'>Firework burst</option>\n"
        "  <option value='3'>Zenith cascade</option>\n"
        "  <option value='4'>Rainbow spiral</option>\n"
        "  <option value='5'>Breathing mandala</option>\n"
        "  <option value='6'>Supernova</option>\n"
        "  <option value='7'>Matrix rain</option>\n"
        "  <option value='8'>Galaxy spin</option>\n"
        "  <option value='9'>Color wipe</option>\n"
        "  <option value='10'>Thunderstorm</option>\n"
        "</select><button onclick=\"previewAnim('hour','hourAnimation')\">&#9654; Preview</button>\n"
        "<div class='row'><button class='primary' onclick='saveSettings()'>Save animations</button></div>\n"
        "</div>\n"
        "<div class='panel'><h2>&#127775; Animation Style</h2>\n"
        "<div class='row'>\n"
        "  <div><label>Color palette</label><select id='animationPalette'>\n"
        "    <option value='0'>Rainbow</option>\n"
        "    <option value='1'>Fire</option>\n"
        "    <option value='2'>Ocean</option>\n"
        "    <option value='3'>Forest</option>\n"
        "    <option value='4'>Candy</option>\n"
        "    <option value='5'>Neon</option>\n"
        "    <option value='6'>Monochrome (hour color)</option>\n"
        "    <option value='7'>Clock colors</option>\n"
        "  </select></div>\n"
        "  <div><label>Reminder palette</label><select id='reminderPalette'>\n"
        "    <option value='0'>Amber (warm)</option>\n"
        "    <option value='1'>Red (urgent)</option>\n"
        "    <option value='2'>Magenta (bold)</option>\n"
        "    <option value='3'>Cyan-warm (unusual)</option>\n"
        "  </select></div>\n"
        "</div>\n"
        "<div class='row'>\n"
        "  <div><label>Speed</label><select id='animationSpeed'>\n"
        "    <option value='1'>1 - Dreamy slow</option>\n"
        "    <option value='2'>2 - Relaxed</option>\n"
        "    <option value='3'>3 - Normal</option>\n"
        "    <option value='4'>4 - Energetic</option>\n"
        "    <option value='5'>5 - Hyperactive</option>\n"
        "  </select></div>\n"
        "  <div><label>Peak brightness</label>\n"
        "    <input id='animationBrightness' type='range' min='50' max='255'>\n"
        "    <output id='animationBrightnessOut'></output>\n"
        "  </div>\n"
        "  <div><label>Trail length</label>\n"
        "    <input id='trailLength' type='range' min='2' max='12'>\n"
        "    <output id='trailLengthOut'></output>\n"
        "  </div>\n"
        "</div>\n"
        "<div class='row'>\n"
        "  <div><label>Preview with</label><select id='stylePreviewType'>\n"
        "    <option value='quarter'>Quarter animation</option>\n"
        "    <option value='halfhour'>Half-hour animation</option>\n"
        "    <option value='hour'>Hour animation</option>\n"
        "    <option value='reminder'>Reminder animation</option>\n"
        "  </select></div>\n"
        "  <button onclick='previewStyleAnim()'>&#9654; Preview style</button>\n"
        "  <button class='primary' onclick='saveAnimStyle()'>Save style</button>\n"
        "</div>\n"
        "</div>\n"
        "<div class='panel'><h2>Focus Reminders (ADHD)</h2>\n"
        "<p class='sub' style='font-size:12px;color:#92a0b5'>Visual nudge system for hyperfocus interruption. Fires animations at set intervals on selected days/times.</p>\n"
        "<div class='toggle'><label><input id='focusReminder_enabled' type='checkbox'> Enable focus reminders</label></div>\n"
        "<div class='row'>\n"
        "  <div><label>Start hour</label><input id='focusReminder_startHour' type='number' min='0' max='23' placeholder='HH'></div>\n"
        "  <div><label>End hour</label><input id='focusReminder_endHour' type='number' min='0' max='23' placeholder='HH'></div>\n"
        "  <div><label>Interval (min)</label><input id='focusReminder_intervalMinutes' type='number' min='1' max='1440' placeholder='60'></div>\n"
        "</div>\n"
        "<label>Days of week</label>\n"
        "<div class='toggle' id='daysToggle' style='display:flex;gap:4px;flex-wrap:wrap'></div>\n"
        "<div><label>Animation</label><select id='focusReminder_animation'>\n"
        "  <option value='0'>Use quarter animation</option>\n"
        "  <option value='1'>Use half-hour animation</option>\n"
        "  <option value='2'>Use hour animation</option>\n"
        "  <option value='3'>Use quarter animation (alt)</option>\n"
        "  <option value='4'>Use half-hour animation (alt)</option>\n"
        "  <option value='5'>Use hour animation (alt)</option>\n"
        "  <option value='6'>Amber pulse (warm inward wave)</option>\n"
        "  <option value='7'>Attention ring (amber radar)</option>\n"
        "  <option value='8'>Heartbeat (double-beat pulse)</option>\n"
        "  <option value='9'>Sunrise wake (red to yellow)</option>\n"
        "  <option value='10'>Campfire flicker (warm sparks)</option>\n"
        "  <option value='11'>Neon sign (amber buzz)</option>\n"
        "</select><button onclick=\"previewAnim('reminder','focusReminder_animation')\">&#9654; Preview</button></div>\n"
        "<div class='row'><button class='primary' onclick='saveFocusReminder()'>Save reminder</button></div>\n"
        "</div>\n"
        "<div class='panel'><h2>Network</h2><div id='net' class='state'>--</div><div class='row'><button onclick='loadNet()'>Refresh network</button></div></div>\n"
        "<div class='panel'><h2>&#9881; Admin</h2><div class='row'>"
        "<a href='/update' style='display:inline-block;padding:10px 16px;background:#145875;border:1px solid #2d9ccb;color:white;border-radius:6px;text-decoration:none;font-size:14px'>Firmware Update</a>"
        "<a href='/wifi' style='display:inline-block;padding:10px 16px;background:#145875;border:1px solid #2d9ccb;color:white;border-radius:6px;text-decoration:none;font-size:14px'>WiFi Settings</a>"
        "</div></div>\n"
        "</section></main>\n";
      static const char HTML_P3[] PROGMEM =
        "<script>\n"
        "const counts={outer:60,middle:24,inner:12}, radii={outer:182,middle:134,inner:88};\n"
        "const leds={outer:[],middle:[],inner:[]}; let settings={}, current={hour:12,minute:0,second:0}, netTimer=0;\n"
        "function qs(id){return document.getElementById(id)} function pad(n){return String(n).padStart(2,'0')}\n"
        "function makeClock(){const svg=qs('clockSvg'); for(const ring of ['outer','middle','inner']){for(let i=0;i<counts[ring];i++){const a=(i/counts[ring])*Math.PI*2-Math.PI/2,x=210+Math.cos(a)*radii[ring],y=210+Math.sin(a)*radii[ring],c=document.createElementNS('http://www.w3.org/2000/svg','circle');c.setAttribute('cx',x);c.setAttribute('cy',y);c.setAttribute('r',ring==='outer'?4.4:ring==='middle'?5.8:7.2);c.classList.add('led');svg.appendChild(c);leds[ring].push(c)}} const center=document.createElementNS('http://www.w3.org/2000/svg','circle');center.id='centerLed';center.setAttribute('cx',210);center.setAttribute('cy',210);center.setAttribute('r',17);center.classList.add('led','center');svg.appendChild(center)}\n"
        "function setLed(el,color,level,cls='on'){el.style.color=color;el.setAttribute('fill',color);el.style.opacity=Math.max(.04,level/255);el.className.baseVal='led '+cls}\n"
        "function clearRing(r){for(const el of leds[r]){el.setAttribute('fill','#243044');el.style.opacity=.22;el.className.baseVal='led'}}\n"
        "function level(id){return Number(qs(id+'Level')?.value||180)} function color(id){return qs(id+'Color')?.value||'#ffffff'}\n"
        "function draw(){clearRing('middle');clearRing('inner');for(let i=0;i<60;i++){const mark=i%5===0;setLed(leds.outer[i],mark?color('outerMarker'):color('outerFiller'),mark?level('outerMarker'):level('outerFiller'),mark?'marker':'ghost')}let s=current.second,m=current.minute,h=current.hour%12,hoff=current.minute>=30?1:0,h24=(h*2+hoff)%24;const mode=qs('previewMode')?.value||'live',tick=Math.floor(Date.now()/90);if(settings.progressSeconds){for(let i=0;i<=s;i++)setLed(leds.outer[i],color('seconds'),38,'ghost')}if(settings.secondTrail||mode==='trail'){for(let i=1;i<7;i++)setLed(leds.outer[(s+60-i)%60],color('seconds'),Math.max(20,level('seconds')-(i*32)),'ghost')}if(mode==='trail'){for(let i=1;i<5;i++){setLed(leds.outer[(m+60-i)%60],color('minutes'),Math.max(25,level('minutes')-(i*42)),'ghost');setLed(leds.middle[(h24+24-i)%24],color('hours'),Math.max(25,level('hours')-(i*42)),'ghost');setLed(leds.inner[(h+12-i)%12],color('hours'),Math.max(25,level('hours')-(i*52)),'ghost')}}if(mode==='spark'){for(let i=0;i<10;i++){setLed(leds.outer[(tick+i*6)%60],i%2?color('hours'):color('minutes'),90+(i*10),'ghost')}}for(let i=0;i<24;i++)setLed(leds.middle[i],color('hours'),22,'marker');for(let i=0;i<12;i++)setLed(leds.inner[i],color('center'),24,'marker');setLed(leds.outer[s],color('seconds'),level('seconds'));setLed(leds.outer[m],color('minutes'),level('minutes'));setLed(leds.middle[h24],color('hours'),level('hours'));setLed(leds.inner[h],color('hours'),level('hours'));setLed(leds.inner[(h+hoff)%12],color('hours'),level('hours'));const pulse=45+Math.floor((Math.sin(Date.now()/450)+1)*85);setLed(qs('centerLed'),color('center'),Math.min(level('center'),pulse),'on')}\n"
        "async function refresh(){const r=await fetch('/time');const t=await r.json();current=t;const h12=t.hour%12||12;const ampm=t.hour<12?'AM':'PM';qs('now').textContent=`${pad(h12)}:${pad(t.minute)}:${pad(t.second)} ${ampm}`;qs('state').textContent=`IP ${t.ip||'-'} | Wi-Fi ${t.wifi?'on':'off'} | NTP ${t.ntpSynced?'synced':'waiting'}`;draw()}\n"
        "async function loadNet(){const r=await fetch('/net');const n=await r.json();qs('net').textContent=`${n.hostname} | ${n.ssid} | IP ${n.ip} | GW ${n.gateway} | RSSI ${n.rssi} dBm`}\n"
        "async function loadSettings(){const r=await fetch('/settings');settings=await r.json();for(const k of ['dayBrightness','nightBrightness','nightStartHour','nightEndHour','colorTheme','outerMarkerLevel','outerFillerLevel','secondsLevel','minutesLevel','hoursLevel','centerLevel'])qs(k).value=settings[k];for(const k of ['outerMarkerColor','outerFillerColor','secondsColor','minutesColor','hoursColor','centerColor'])qs(k).value=settings[k];for(const k of ['secondTrail','progressSeconds','hourlyChime','statusAnimations'])qs(k).checked=!!settings[k];qs('autoBrightnessMode').value=settings.autoBrightnessMode;qs('minAutoBrightness').value=settings.minAutoBrightness;qs('maxAutoBrightness').value=settings.maxAutoBrightness;qs('quarterAnimation').value=settings.quarterAnimation;qs('halfHourAnimation').value=settings.halfHourAnimation;qs('hourAnimation').value=settings.hourAnimation;qs('intervalAnimationsEnabled').checked=!!settings.intervalAnimationsEnabled;qs('focusReminder_enabled').checked=!!settings.focusReminder_enabled;qs('focusReminder_startHour').value=settings.focusReminder_startHour||8;qs('focusReminder_endHour').value=settings.focusReminder_endHour||22;qs('focusReminder_intervalMinutes').value=settings.focusReminder_intervalMinutes||60;qs('focusReminder_animation').value=settings.focusReminder_animation||0;const daysToggle=qs('daysToggle');daysToggle.innerHTML='';const daysNames=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];for(let i=0;i<7;i++){const label=document.createElement('label');const checkbox=document.createElement('input');checkbox.type='checkbox';checkbox.checked=!!(settings.focusReminder_daysMask&(1<<i));checkbox.id='focusReminder_day'+i;label.appendChild(checkbox);label.appendChild(document.createTextNode(daysNames[i]));daysToggle.appendChild(label)}qs('outerRingOffset').value=settings.outerRingOffset||0;qs('animationPalette').value=settings.animationPalette??0;qs('animationSpeed').value=settings.animationSpeed??3;qs('animationBrightness').value=settings.animationBrightness??200;qs('trailLength').value=settings.trailLength??4;qs('reminderPalette').value=settings.reminderPalette??0;qs('autoBrightnessMode').onchange=()=>{qs('autoPanel').style.display=Number(qs('autoBrightnessMode').value)===1?'block':'none'};qs('autoBrightnessMode').onchange();bindLive();draw();refreshLux();setInterval(refreshLux,2000)}\n"
        "async function refreshLux(){const r=await fetch('/lux');const data=await r.json();if(data.available){qs('luxValue').textContent=data.lux.toFixed(1)}}\n"
        "function bindLive(){for(const k of ['outerMarkerLevel','outerFillerLevel','secondsLevel','minutesLevel','hoursLevel','centerLevel']){const out=qs(k+'Out');const upd=()=>{out.value=qs(k).value;draw()};qs(k).oninput=upd;upd()}for(const k of ['outerMarkerColor','outerFillerColor','secondsColor','minutesColor','hoursColor','centerColor','previewMode'])qs(k).oninput=draw;for(const k of ['secondTrail','progressSeconds'])qs(k).oninput=()=>{settings[k]=qs(k).checked;draw()};for(const k of ['animationBrightness','trailLength']){const out=qs(k+'Out');const upd=()=>{out.value=qs(k).value};qs(k).oninput=upd;upd()}}\n"
        "async function post(url,body){await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});await refresh()}\n"
        "function setTime(){post('/set',`hour=${h.value}&minute=${m.value}&second=${s.value}`)}\n"
        "function syncBrowser(){const d=new Date();post('/syncBrowser',`hour=${d.getHours()}&minute=${d.getMinutes()}&second=${d.getSeconds()}`)}\n"
        "function saveSettings(){const p=new URLSearchParams();for(const k of ['dayBrightness','nightBrightness','nightStartHour','nightEndHour','colorTheme','outerMarkerLevel','outerFillerLevel','secondsLevel','minutesLevel','hoursLevel','centerLevel'])p.set(k,qs(k).value);for(const k of ['outerMarkerColor','outerFillerColor','secondsColor','minutesColor','hoursColor','centerColor'])p.set(k,qs(k).value);for(const k of ['secondTrail','progressSeconds','hourlyChime','statusAnimations'])p.set(k,qs(k).checked?1:0);p.set('autoBrightnessMode',qs('autoBrightnessMode').value);p.set('minAutoBrightness',qs('minAutoBrightness').value);p.set('maxAutoBrightness',qs('maxAutoBrightness').value);p.set('quarterAnimation',qs('quarterAnimation').value);p.set('halfHourAnimation',qs('halfHourAnimation').value);p.set('hourAnimation',qs('hourAnimation').value);p.set('intervalAnimationsEnabled',qs('intervalAnimationsEnabled').checked?1:0);p.set('outerRingOffset',qs('outerRingOffset').value);post('/settings',p.toString()).then(loadSettings)}\n"
        "function saveFocusReminder(){const p=new URLSearchParams();p.set('focusReminder_enabled',qs('focusReminder_enabled').checked?1:0);p.set('focusReminder_startHour',qs('focusReminder_startHour').value);p.set('focusReminder_endHour',qs('focusReminder_endHour').value);p.set('focusReminder_intervalMinutes',qs('focusReminder_intervalMinutes').value);p.set('focusReminder_animation',qs('focusReminder_animation').value);let daysMask=0;for(let i=0;i<7;i++){if(qs('focusReminder_day'+i).checked)daysMask|=(1<<i)}p.set('focusReminder_daysMask',daysMask);post('/settings',p.toString()).then(loadSettings)}\n"
        "function previewAnim(type,modeId){const mode=qs(modeId).value;fetch('/previewAnimation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'type='+type+'&mode='+mode})}\n"
        "function previewStyleAnim(){const t=qs('stylePreviewType').value;const modeMap={quarter:'quarterAnimation',halfhour:'halfHourAnimation',hour:'hourAnimation',reminder:'focusReminder_animation'};const mode=qs(modeMap[t]).value;fetch('/previewAnimation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'type='+t+'&mode='+mode})}\n"
        "function saveAnimStyle(){const p=new URLSearchParams();p.set('animationPalette',qs('animationPalette').value);p.set('animationSpeed',qs('animationSpeed').value);p.set('animationBrightness',qs('animationBrightness').value);p.set('trailLength',qs('trailLength').value);p.set('reminderPalette',qs('reminderPalette').value);post('/settings',p.toString()).then(loadSettings)}\n"
        "makeClock();loadSettings();refresh();loadNet();setInterval(refresh,1000);setInterval(draw,90);\n"
        "</script></body></html>";
      server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server_.send(200, "text/html", "");
      server_.sendContent_P(HTML_P1);
      server_.sendContent_P(HTML_P2);
      server_.sendContent_P(HTML_P3);
      server_.client().flush();
    });

    server_.on("/time", HTTP_GET, [&]() {
      ClockTime t = model_.get();
      char buf[128];
      snprintf(buf, sizeof(buf),
        "{\"hour\":%u,\"minute\":%u,\"second\":%u"
        ",\"ntpSynced\":%s,\"wifi\":%s,\"ip\":\"%s\"}",
        t.hour, t.minute, t.second,
        timeSync_.synced() ? "true" : "false",
        (WiFi.status() == WL_CONNECTED) ? "true" : "false",
        WiFi.localIP().toString().c_str());
      server_.send(200, "application/json", buf);
    });

    server_.on("/temperature", HTTP_GET, [&]() {
      if (!temperature_.available()) {
        server_.send(200, "application/json", "{\"available\":false}");
        return;
      }
      server_.send(200, "application/json", String("{\"available\":true,\"celsius\":") +
                                           temperature_.celsius() + "}");
    });

    server_.on("/lux", HTTP_GET, [&]() {
      if (!lux_ || !lux_->available()) {
        server_.send(200, "application/json", "{\"available\":false}");
        return;
      }
      String payload = String("{\"available\":true")
        + ",\"lux\":" + lux_->lux()
        + ",\"autoBrightness\":" + lux_->autoBrightness()
        + "}";
      server_.send(200, "application/json", payload);
    });

    server_.on("/net", HTTP_GET, [&]() {
      char buf[256];
      snprintf(buf, sizeof(buf),
        "{\"hostname\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\""
        ",\"gateway\":\"%s\",\"subnet\":\"%s\",\"dns\":\"%s\""
        ",\"rssi\":%d,\"status\":%d}",
        DEVICE_HOSTNAME,
        WiFi.SSID().c_str(),
        WiFi.localIP().toString().c_str(),
        WiFi.gatewayIP().toString().c_str(),
        WiFi.subnetMask().toString().c_str(),
        WiFi.dnsIP().toString().c_str(),
        (int)WiFi.RSSI(),
        (int)WiFi.status());
      server_.send(200, "application/json", buf);
    });

    server_.on("/diag", HTTP_GET, [&]() {
      uint32_t uptimeSec = millis() / 1000;
      esp_reset_reason_t rr = esp_reset_reason();
      const char *bootReason = "unknown";
      switch (rr) {
        case ESP_RST_POWERON:  bootReason = "power-on";  break;
        case ESP_RST_SW:       bootReason = "software";  break;
        case ESP_RST_PANIC:    bootReason = "panic";     break;
        case ESP_RST_INT_WDT:  bootReason = "int-wdt";  break;
        case ESP_RST_TASK_WDT: bootReason = "task-wdt"; break;
        case ESP_RST_WDT:      bootReason = "watchdog";  break;
        case ESP_RST_BROWNOUT: bootReason = "brownout";  break;
        case ESP_RST_DEEPSLEEP:bootReason = "deepsleep"; break;
        default: break;
      }
      char buf[300];
      snprintf(buf, sizeof(buf),
        "{\"uptime\":%u,\"firmware_version\":%u,\"fw\":\"%s\",\"boot_reason\":\"%s\""
        ",\"free_heap\":%u,\"wifi_ssid\":\"%s\",\"wifi_rssi\":%d"
        ",\"wifi_ip\":\"%s\",\"ntp_synced\":%s,\"ntp_last_delta\":%d"
        ",\"button_events\":%u}",
        (unsigned)uptimeSec, (unsigned)SETTINGS_VERSION, FIRMWARE_VERSION, bootReason,
        (unsigned)ESP.getFreeHeap(),
        WiFi.SSID().c_str(), (int)WiFi.RSSI(),
        WiFi.localIP().toString().c_str(),
        timeSync_.synced() ? "true" : "false",
        (int)timeSync_.lastDeltaSec(),
        (unsigned)g_buttonEventCount);
      server_.send(200, "application/json", buf);
    });

    server_.on("/settings", HTTP_GET, [&]() {
      server_.send(200, "application/json", settingsJson());
    });

    server_.on("/settings", HTTP_POST, [&]() {
      ClockSettings settings = settings_.get();
      if (server_.hasArg("dayBrightness")) settings.dayBrightness = clampByte(server_.arg("dayBrightness").toInt(), 0, 255);
      if (server_.hasArg("nightBrightness")) settings.nightBrightness = clampByte(server_.arg("nightBrightness").toInt(), 0, 255);
      if (server_.hasArg("nightStartHour")) settings.nightStartHour = clampByte(server_.arg("nightStartHour").toInt(), 0, 23);
      if (server_.hasArg("nightEndHour")) settings.nightEndHour = clampByte(server_.arg("nightEndHour").toInt(), 0, 23);
      if (server_.hasArg("colorTheme")) settings.colorTheme = clampByte(server_.arg("colorTheme").toInt(), 0, 2);
      if (server_.hasArg("secondTrail")) settings.secondTrail = server_.arg("secondTrail").toInt() ? 1 : 0;
      if (server_.hasArg("progressSeconds")) settings.progressSeconds = server_.arg("progressSeconds").toInt() ? 1 : 0;
      if (server_.hasArg("hourlyChime")) settings.hourlyChime = server_.arg("hourlyChime").toInt() ? 1 : 0;
      if (server_.hasArg("statusAnimations")) settings.statusAnimations = server_.arg("statusAnimations").toInt() ? 1 : 0;
      if (server_.hasArg("outerMarkerLevel")) settings.outerMarkerLevel = clampByte(server_.arg("outerMarkerLevel").toInt(), 0, 255);
      if (server_.hasArg("outerFillerLevel")) settings.outerFillerLevel = clampByte(server_.arg("outerFillerLevel").toInt(), 0, 255);
      if (server_.hasArg("secondsLevel")) settings.secondsLevel = clampByte(server_.arg("secondsLevel").toInt(), 0, 255);
      if (server_.hasArg("minutesLevel")) settings.minutesLevel = clampByte(server_.arg("minutesLevel").toInt(), 0, 255);
      if (server_.hasArg("hoursLevel")) settings.hoursLevel = clampByte(server_.arg("hoursLevel").toInt(), 0, 255);
      if (server_.hasArg("centerLevel")) settings.centerLevel = clampByte(server_.arg("centerLevel").toInt(), 0, 255);
      if (server_.hasArg("outerMarkerColor")) parseColor(server_.arg("outerMarkerColor"), settings.outerMarkerRed, settings.outerMarkerGreen, settings.outerMarkerBlue);
      if (server_.hasArg("outerFillerColor")) parseColor(server_.arg("outerFillerColor"), settings.outerFillerRed, settings.outerFillerGreen, settings.outerFillerBlue);
      if (server_.hasArg("secondsColor")) parseColor(server_.arg("secondsColor"), settings.secondsRed, settings.secondsGreen, settings.secondsBlue);
      if (server_.hasArg("minutesColor")) parseColor(server_.arg("minutesColor"), settings.minutesRed, settings.minutesGreen, settings.minutesBlue);
      if (server_.hasArg("hoursColor")) parseColor(server_.arg("hoursColor"), settings.hoursRed, settings.hoursGreen, settings.hoursBlue);
      if (server_.hasArg("centerColor")) parseColor(server_.arg("centerColor"), settings.centerRed, settings.centerGreen, settings.centerBlue);
      if (server_.hasArg("autoBrightnessMode")) settings.autoBrightnessMode = clampByte(server_.arg("autoBrightnessMode").toInt(), 0, 2);
      if (server_.hasArg("minAutoBrightness")) settings.minAutoBrightness = clampByte(server_.arg("minAutoBrightness").toInt(), 5, 255);
      if (server_.hasArg("maxAutoBrightness")) settings.maxAutoBrightness = clampByte(server_.arg("maxAutoBrightness").toInt(), 5, 255);
      if (server_.hasArg("quarterAnimation")) settings.quarterAnimation = clampByte(server_.arg("quarterAnimation").toInt(), 0, 6);
      if (server_.hasArg("halfHourAnimation")) settings.halfHourAnimation = clampByte(server_.arg("halfHourAnimation").toInt(), 0, 7);
      if (server_.hasArg("hourAnimation")) settings.hourAnimation = clampByte(server_.arg("hourAnimation").toInt(), 0, 10);
      if (server_.hasArg("intervalAnimationsEnabled")) settings.intervalAnimationsEnabled = server_.arg("intervalAnimationsEnabled").toInt() ? 1 : 0;
      if (server_.hasArg("outerRingOffset")) settings.outerRingOffset = clampByte(server_.arg("outerRingOffset").toInt(), 0, 59);
      if (server_.hasArg("focusReminder_enabled")) settings.focusReminder_enabled = server_.arg("focusReminder_enabled").toInt() ? 1 : 0;
      if (server_.hasArg("focusReminder_startHour")) settings.focusReminder_startHour = clampByte(server_.arg("focusReminder_startHour").toInt(), 0, 23);
      if (server_.hasArg("focusReminder_endHour")) settings.focusReminder_endHour = clampByte(server_.arg("focusReminder_endHour").toInt(), 0, 23);
      if (server_.hasArg("focusReminder_intervalMinutes")) settings.focusReminder_intervalMinutes = clampWord(server_.arg("focusReminder_intervalMinutes").toInt(), 1, 1440);
      if (server_.hasArg("focusReminder_daysMask")) settings.focusReminder_daysMask = clampByte(server_.arg("focusReminder_daysMask").toInt(), 0, 127);
      if (server_.hasArg("focusReminder_animation")) settings.focusReminder_animation = clampByte(server_.arg("focusReminder_animation").toInt(), 0, 11);
      if (server_.hasArg("focusReminder_durationSeconds")) settings.focusReminder_durationSeconds = clampByte(server_.arg("focusReminder_durationSeconds").toInt(), 1, 60);
      if (server_.hasArg("animationPalette"))    settings.animationPalette    = clampByte(server_.arg("animationPalette").toInt(), 0, 7);
      if (server_.hasArg("animationSpeed"))      settings.animationSpeed      = clampByte(server_.arg("animationSpeed").toInt(), 1, 5);
      if (server_.hasArg("animationBrightness")) settings.animationBrightness = clampByte(server_.arg("animationBrightness").toInt(), 50, 255);
      if (server_.hasArg("trailLength"))         settings.trailLength         = clampByte(server_.arg("trailLength").toInt(), 2, 12);
      if (server_.hasArg("reminderPalette"))     settings.reminderPalette     = clampByte(server_.arg("reminderPalette").toInt(), 0, 3);
      settings_.update(settings);
      renderer_.setStatus(STATUS_SETTINGS_SAVED, 1300);
      model_.markDirty();
      server_.send(200, "text/plain", "ok");
    });

    server_.on("/set", HTTP_POST, [&]() {
      if (!server_.hasArg("hour") || !server_.hasArg("minute") || !server_.hasArg("second")) {
        server_.send(400, "text/plain", "missing hour/minute/second");
        return;
      }
      model_.set(server_.arg("hour").toInt(), server_.arg("minute").toInt(), server_.arg("second").toInt());
      renderer_.setStatus(STATUS_TIME_SYNC, 1200);
      server_.send(200, "text/plain", "ok");
    });

    server_.on("/syncBrowser", HTTP_POST, [&]() {
      if (!server_.hasArg("hour") || !server_.hasArg("minute") || !server_.hasArg("second")) {
        server_.send(400, "text/plain", "missing hour/minute/second");
        return;
      }
      model_.set(server_.arg("hour").toInt(), server_.arg("minute").toInt(), server_.arg("second").toInt());
      renderer_.setStatus(STATUS_TIME_SYNC, 1500);
      server_.send(200, "text/plain", "ok");
    });

    server_.on("/syncNtp", HTTP_POST, [&]() {
      bool ok = timeSync_.syncNow();
      renderer_.setStatus(ok ? STATUS_TIME_SYNC : STATUS_WIFI_FAIL, 1500);
      server_.send(ok ? 200 : 503, "text/plain", ok ? "ok" : "ntp unavailable");
    });

    server_.on("/addMinute", HTTP_POST, [&]() {
      model_.addMinutes(1);
      renderer_.setStatus(STATUS_BUTTON, 700);
      server_.send(200, "text/plain", "ok");
    });

    server_.on("/subMinute", HTTP_POST, [&]() {
      model_.addMinutes(-1);
      renderer_.setStatus(STATUS_BUTTON, 700);
      server_.send(200, "text/plain", "ok");
    });

    server_.on("/previewAnimation", HTTP_POST, [&]() {
      const String type = server_.arg("type");
      const int    mode = server_.arg("mode").toInt();
      const uint32_t now = millis();
      static const AnimPhase qPhases[]  = {ANIM_Q1,  ANIM_Q2,  ANIM_Q3,  ANIM_Q4,  ANIM_Q5,  ANIM_Q6};
      static const AnimPhase hhPhases[] = {ANIM_H1,  ANIM_H2,  ANIM_H3,  ANIM_H4,  ANIM_H5,  ANIM_H6,  ANIM_H7};
      static const AnimPhase hrPhases[] = {ANIM_HR1, ANIM_HR2, ANIM_HR3, ANIM_HR4, ANIM_HR5,
                                           ANIM_HR6, ANIM_HR7, ANIM_HR8, ANIM_HR9, ANIM_HR10};
      if (type == "quarter") {
        if (mode < 1 || mode > 6) { server_.send(400, "text/plain", "mode 1-6"); return; }
        renderer_.triggerAnimDirect(qPhases[mode - 1], now);
      } else if (type == "halfhour") {
        if (mode < 1 || mode > 7) { server_.send(400, "text/plain", "mode 1-7"); return; }
        renderer_.triggerAnimDirect(hhPhases[mode - 1], now);
      } else if (type == "hour") {
        if (mode < 1 || mode > 10) { server_.send(400, "text/plain", "mode 1-10"); return; }
        renderer_.triggerAnimDirect(hrPhases[mode - 1], now);
      } else if (type == "reminder") {
        if (mode < 0 || mode > 11) { server_.send(400, "text/plain", "mode 0-11"); return; }
        renderer_.triggerReminderDirectAnimation((uint8_t)mode, now);
      } else {
        server_.send(400, "text/plain", "type: quarter|halfhour|hour|reminder");
        return;
      }
      server_.send(200, "text/plain", "ok");
    });

    // ===== WiFi Settings Page (GET /wifi) =====
    server_.on("/wifi", HTTP_GET, [&]() {
      Preferences wprefs;
      wprefs.begin("wifi", true);
      String savedSsid = wprefs.getString("ssid", "");
      wprefs.end();
      const char *dispSsid = savedSsid.length() > 0 ? savedSsid.c_str() : "(none saved)";
      char wifiSt[64];
      snprintf(wifiSt, sizeof(wifiSt), "%s", wifiStatusText(WiFi.status()));
      if (WiFi.status() == WL_CONNECTED) {
        snprintf(wifiSt, sizeof(wifiSt), "%s (%s)", wifiStatusText(WiFi.status()), WiFi.SSID().c_str());
      }
      static const char WIFI_P1[] PROGMEM =
        "<!doctype html><html><head>\n"
        "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>\n"
        "<title>WiFi Settings</title>\n"
        "<style>\n"
        ":root{color-scheme:dark;--bg:#090b10;--panel:#151922;--panel2:#10141c;--line:#2c3442;--text:#eef3fb;--muted:#92a0b5;--accent:#6bd7ff}\n"
        "*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 50% 18%,#17202f 0,#090b10 54%);color:var(--text);font-family:system-ui,Segoe UI,sans-serif}\n"
        "main{max-width:480px;margin:40px auto;padding:20px}\n"
        ".panel{background:linear-gradient(180deg,var(--panel),var(--panel2));border:1px solid var(--line);border-radius:8px;padding:24px;box-shadow:0 18px 45px #0008}\n"
        "h1{font-size:18px;margin:0 0 16px}\n"
        ".info{background:#0c1017;border:1px solid var(--line);border-radius:6px;padding:12px;margin-bottom:20px;font-size:13px;color:var(--muted);line-height:1.8}\n"
        "label{display:block;color:var(--muted);font-size:12px;margin:12px 0 4px}\n"
        "input{width:100%;font:inherit;border-radius:6px;border:1px solid #374253;background:#0c1017;color:var(--text);padding:8px 10px;min-height:38px}\n"
        ".row{display:flex;gap:10px;margin-top:20px}\n"
        "button{font:inherit;border-radius:6px;cursor:pointer;padding:10px 20px;min-height:40px;border:1px solid #42546d;background:#203146;color:var(--text)}\n"
        "button.primary{background:#145875;border-color:#2d9ccb;color:white}\n"
        "#status{margin-top:14px;min-height:18px;font-size:13px;color:var(--accent)}\n"
        "a{color:var(--accent);text-decoration:none;font-size:13px}a:hover{text-decoration:underline}\n"
        "</style></head><body><main>\n"
        "<div class='panel'>\n"
        "<h1>&#128246; WiFi Settings</h1>\n"
        "<div class='info'><strong>Saved SSID:</strong> ";
      static const char WIFI_MID[] PROGMEM =
        "<br><strong>Status:</strong> ";
      static const char WIFI_P2[] PROGMEM =
        "</div>\n"
        "<form onsubmit='save();return false;'>\n"
        "<label>SSID</label><input id='ssid' type='text' maxlength='32' placeholder='Network name' required>\n"
        "<label>Password</label><input id='pass' type='password' maxlength='64' placeholder='Leave blank for open network'>\n"
        "<div class='row'><button type='submit' class='primary'>Save &amp; Connect</button><button type='button' onclick='history.back()'>Cancel</button></div>\n"
        "</form>\n"
        "<div id='status'></div>\n"
        "<p style='margin-top:20px'><a href='/'>&#8592; Back to clock</a></p>\n"
        "</div></main>\n"
        "<script>\n"
        "async function save(){\n"
        "  const ssid=document.getElementById('ssid').value.trim();\n"
        "  const pass=document.getElementById('pass').value;\n"
        "  if(!ssid){alert('SSID required');return}\n"
        "  document.getElementById('status').textContent='Saving...';\n"
        "  try{\n"
        "    const r=await fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)});\n"
        "    document.getElementById('status').textContent=await r.text();\n"
        "  }catch(e){document.getElementById('status').textContent='Saved. Device reconnecting \xe2\x80\x94 you may need to rejoin the network.';}\n"
        "}\n"
        "</script></body></html>";
      server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server_.send(200, "text/html", "");
      server_.sendContent_P(WIFI_P1);
      server_.sendContent(dispSsid);
      server_.sendContent_P(WIFI_MID);
      server_.sendContent(wifiSt);
      server_.sendContent_P(WIFI_P2);
      server_.client().flush();
    });

    // ===== WiFi Save Handler (POST /wifi) =====
    server_.on("/wifi", HTTP_POST, [&]() {
      String ssid = server_.arg("ssid");
      String pass = server_.arg("pass");
      ssid.trim();
      if (ssid.length() < 1 || ssid.length() > 32) {
        server_.send(400, "text/plain", "SSID must be 1-32 characters");
        return;
      }
      Preferences wprefs;
      wprefs.begin("wifi", false);
      wprefs.putString("ssid", ssid);
      wprefs.putString("pass", pass);
      wprefs.end();
      Serial.printf("[WiFi] Credentials saved via web UI: SSID=%s\n", ssid.c_str());
      server_.send(200, "text/plain", "Saved. Reconnecting to " + ssid + "...");
      delay(300);
      WiFi.disconnect(false);
      delay(100);
      WiFi.begin(ssid.c_str(), pass.c_str());
    });

    // ===== Firmware Update Web Page (GET /update) =====
    server_.on("/update", HTTP_GET, [&]() {
      static const char UPDATE_P1[] PROGMEM =
        "<!doctype html><html><head>\n"
        "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>\n"
        "<title>Firmware Update - ESP32 Ring Clock</title>\n"
        "<style>\n"
        ":root{color-scheme:dark;--bg:#090b10;--panel:#151922;--panel2:#10141c;--line:#2c3442;--text:#eef3fb;--muted:#92a0b5;--accent:#6bd7ff}\n"
        "*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 50% 18%,#17202f 0,#090b10 54%);color:var(--text);font-family:system-ui,Segoe UI,sans-serif}\n"
        "main{max-width:600px;margin:40px auto;padding:20px}\n"
        ".panel{background:linear-gradient(180deg,var(--panel),var(--panel2));border:1px solid var(--line);border-radius:8px;box-shadow:0 18px 45px #0008;padding:24px;margin-bottom:16px}\n"
        "h1{margin:0 0 8px;font-size:24px}p{color:var(--muted);margin:0 0 16px}\n"
        "input[type=file]{display:block;margin:16px 0;padding:8px;background:#0c1017;border:1px solid #374253;border-radius:6px;color:var(--text)}\n"
        "button{background:#145875;border:1px solid #2d9ccb;color:white;padding:12px 24px;border-radius:6px;font-size:16px;cursor:pointer;margin:8px 8px 8px 0}\n"
        "button:hover{background:#1a6a8f}button.danger{background:#8b2e2e;border-color:#c04040}\n"
        ".progress{width:100%;height:24px;background:#0c1017;border:1px solid #374253;border-radius:4px;overflow:hidden;margin:16px 0}\n"
        ".progress-bar{height:100%;background:#6bd7ff;width:0%;transition:width 0.3s;display:flex;align-items:center;justify-content:center;font-size:12px;color:#090b10}\n"
        ".status{margin:16px 0;padding:12px;border-radius:6px;display:none}\n"
        ".status.success{background:#0a3a2a;border:1px solid #10a060;color:#90ff90}\n"
        ".status.error{background:#3a0a0a;border:1px solid #a01010;color:#ff9090}\n"
        ".status.info{background:#0a1a3a;border:1px solid #1060a0;color:#90d0ff}\n"
        "#updateForm{margin:16px 0}\n"
        ".info-box{background:#0c1017;border-left:3px solid #6bd7ff;padding:12px;margin:16px 0;border-radius:4px}\n"
        "a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}\n"
        "</style></head>\n"
        "<body><main>\n"
        "<div class='panel'>\n"
        "<h1>&#128295; Firmware Update</h1>\n"
        "<p>Upload a new .bin firmware file to update the clock.</p>\n"
        "<div class='info-box'>\n"
        "<strong>Current Firmware:</strong> Latest<br>\n"
        "<strong>Flash Size:</strong> ~700 KB<br>\n"
        "<strong>Device:</strong> XIAO ESP32-C3\n"
        "</div>\n"
        "<div id='updateForm'>\n"
        "<input type='file' id='binFile' accept='.bin' required>\n"
        "<button onclick='uploadFirmware()'>Upload &amp; Update</button>\n"
        "<button onclick='history.back()'>Cancel</button>\n"
        "</div>\n"
        "<div class='progress' id='progress' style='display:none'>\n"
        "<div class='progress-bar' id='progressBar'>0%</div>\n"
        "</div>\n"
        "<div class='status' id='status'></div>\n"
        "</main>\n";
      static const char UPDATE_P2[] PROGMEM =
        "<script>\n"
        "function uploadFirmware(){\n"
        "  const file=document.getElementById('binFile').files[0];\n"
        "  if(!file){alert('Please select a .bin file');return}\n"
        "  if(!file.name.endsWith('.bin')){alert('File must be .bin format');return}\n"
        "  if(file.size>800000){alert('File too large (max ~700KB)');return}\n"
        "  const xhr=new XMLHttpRequest();\n"
        "  xhr.upload.onprogress=(e)=>{\n"
        "    if(e.lengthComputable){\n"
        "      const pct=Math.round(e.loaded/e.total*100);\n"
        "      document.getElementById('progressBar').style.width=pct+'%';\n"
        "      document.getElementById('progressBar').textContent=pct+'%';\n"
        "    }\n"
        "  };\n"
        "  xhr.onload=()=>{\n"
        "    const msg=document.getElementById('status');\n"
        "    if(xhr.status===200){\n"
        "      msg.innerHTML='&#10003; Upload successful! Device will reboot with new firmware...';\n"
        "      msg.className='status success';\n"
        "      document.getElementById('updateForm').style.display='none';\n"
        "      setTimeout(()=>{window.location.href='/'},5000);\n"
        "    }else{\n"
        "      msg.innerHTML='&#10007; Update failed: '+xhr.responseText;\n"
        "      msg.className='status error';\n"
        "    }\n"
        "    msg.style.display='block';\n"
        "  };\n"
        "  xhr.onerror=()=>{\n"
        "    const msg=document.getElementById('status');\n"
        "    msg.innerHTML='&#10007; Upload error (connection lost?)';\n"
        "    msg.className='status error';\n"
        "    msg.style.display='block';\n"
        "  };\n"
        "  document.getElementById('progress').style.display='block';\n"
        "  const fd=new FormData();\n"
        "  fd.append('firmware',file,file.name);\n"
        "  xhr.open('POST','/update');\n"
        "  xhr.send(fd);\n"
        "}\n"
        "</script></body></html>";
      server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server_.send(200, "text/html", "");
      server_.sendContent_P(UPDATE_P1);
      server_.sendContent_P(UPDATE_P2);
      server_.client().flush();
    });

    // ===== Firmware Upload Handler (POST /update) =====
    server_.on("/update", HTTP_POST, [&]() {
      // Handler completion - called after all chunks received
      if (Update.isRunning()) {
        if (Update.end(true)) {
          renderer_.setStatus(STATUS_OTA_SUCCESS, 1500);
          server_.send(200, "text/plain", "Update successful, rebooting...");
          delay(1000);
          ESP.restart();
        } else {
          renderer_.setStatus(STATUS_OTA_FAILED, 2000);
          server_.send(500, "text/plain", String("Update failed: ") + Update.getError());
        }
      } else {
        server_.send(400, "text/plain", "No update in progress");
      }
    }, [&]() {
      // Handler upload chunks
      HTTPUpload &upload = server_.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[FW] Update starting: %s, size: %u\n", upload.filename.c_str(), upload.totalSize);
        if (upload.totalSize > 0 && upload.totalSize > (ESP.getFreeSketchSpace() - 0x1000)) {
          Serial.println("[FW] Not enough free space");
          server_.send(413, "text/plain", "File too large");
          return;
        }
        renderer_.setStatus(STATUS_OTA_UPDATE, 60000);  // 60s timeout
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          Serial.printf("[FW] Update.begin() failed, error: %d\n", Update.getError());
          server_.send(500, "text/plain", "Update.begin failed");
          return;
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        esp_task_wdt_reset();
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Serial.printf("[FW] Write failed, error: %d\n", Update.getError());
          server_.send(500, "text/plain", "Update.write failed");
          Update.abort();
          return;
        }
        Serial.printf("[FW] Written: %u bytes (%.1f%%)\n", upload.totalSize - upload.currentSize,
                     (float)(upload.totalSize - upload.currentSize) * 100 / upload.totalSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        Serial.println("[FW] Upload complete, finalizing...");
      }
    });
  }

  String settingsJson() {
    const ClockSettings &s = settings_.get();
    char oc[8], fc[8], sc[8], mc[8], hc[8], cc[8];
    snprintf(oc, sizeof(oc), "#%02X%02X%02X", s.outerMarkerRed, s.outerMarkerGreen, s.outerMarkerBlue);
    snprintf(fc, sizeof(fc), "#%02X%02X%02X", s.outerFillerRed, s.outerFillerGreen, s.outerFillerBlue);
    snprintf(sc, sizeof(sc), "#%02X%02X%02X", s.secondsRed, s.secondsGreen, s.secondsBlue);
    snprintf(mc, sizeof(mc), "#%02X%02X%02X", s.minutesRed, s.minutesGreen, s.minutesBlue);
    snprintf(hc, sizeof(hc), "#%02X%02X%02X", s.hoursRed, s.hoursGreen, s.hoursBlue);
    snprintf(cc, sizeof(cc), "#%02X%02X%02X", s.centerRed, s.centerGreen, s.centerBlue);
    char buf[1100];
    snprintf(buf, sizeof(buf),
      "{\"dayBrightness\":%u,\"nightBrightness\":%u"
      ",\"nightStartHour\":%u,\"nightEndHour\":%u"
      ",\"colorTheme\":%u,\"secondTrail\":%u,\"progressSeconds\":%u"
      ",\"hourlyChime\":%u,\"statusAnimations\":%u"
      ",\"outerMarkerColor\":\"%s\",\"outerMarkerLevel\":%u"
      ",\"outerFillerColor\":\"%s\",\"outerFillerLevel\":%u"
      ",\"secondsColor\":\"%s\",\"secondsLevel\":%u"
      ",\"minutesColor\":\"%s\",\"minutesLevel\":%u"
      ",\"hoursColor\":\"%s\",\"hoursLevel\":%u"
      ",\"centerColor\":\"%s\",\"centerLevel\":%u"
      ",\"autoBrightnessMode\":%u,\"minAutoBrightness\":%u,\"maxAutoBrightness\":%u"
      ",\"quarterAnimation\":%u,\"halfHourAnimation\":%u,\"hourAnimation\":%u"
      ",\"intervalAnimationsEnabled\":%u"
      ",\"focusReminder_enabled\":%u,\"focusReminder_startHour\":%u"
      ",\"focusReminder_endHour\":%u,\"focusReminder_intervalMinutes\":%u"
      ",\"focusReminder_daysMask\":%u,\"focusReminder_animation\":%u"
      ",\"focusReminder_durationSeconds\":%u,\"outerRingOffset\":%u"
      ",\"animationPalette\":%u,\"animationSpeed\":%u"
      ",\"animationBrightness\":%u,\"trailLength\":%u"
      ",\"reminderPalette\":%u}",
      s.dayBrightness, s.nightBrightness,
      s.nightStartHour, s.nightEndHour,
      s.colorTheme, s.secondTrail, s.progressSeconds,
      s.hourlyChime, s.statusAnimations,
      oc, s.outerMarkerLevel,
      fc, s.outerFillerLevel,
      sc, s.secondsLevel,
      mc, s.minutesLevel,
      hc, s.hoursLevel,
      cc, s.centerLevel,
      s.autoBrightnessMode, s.minAutoBrightness, s.maxAutoBrightness,
      s.quarterAnimation, s.halfHourAnimation, s.hourAnimation,
      s.intervalAnimationsEnabled,
      s.focusReminder_enabled, s.focusReminder_startHour,
      s.focusReminder_endHour, s.focusReminder_intervalMinutes,
      s.focusReminder_daysMask, s.focusReminder_animation,
      s.focusReminder_durationSeconds, s.outerRingOffset,
      s.animationPalette, s.animationSpeed,
      s.animationBrightness, s.trailLength,
      s.reminderPalette);
    return String(buf);
  }

  static String boolJson(bool value) {
    return value ? "true" : "false";
  }

  static uint8_t clampByte(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return static_cast<uint8_t>(value);
  }

  static uint16_t clampWord(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return static_cast<uint16_t>(value);
  }

  static String colorHex(uint8_t r, uint8_t g, uint8_t b) {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", r, g, b);
    return String(buffer);
  }

  static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }

  static void parseColor(const String &value, uint8_t &r, uint8_t &g, uint8_t &b) {
    uint8_t start = value.startsWith("#") ? 1 : 0;
    if (value.length() - start != 6) return;
    int digits[6];
    for (uint8_t i = 0; i < 6; ++i) {
      digits[i] = hexNibble(value[start + i]);
      if (digits[i] < 0) return;
    }
    r = static_cast<uint8_t>((digits[0] << 4) | digits[1]);
    g = static_cast<uint8_t>((digits[2] << 4) | digits[3]);
    b = static_cast<uint8_t>((digits[4] << 4) | digits[5]);
  }

  static const char *wifiStatusText(wl_status_t status) {
    switch (status) {
      case WL_IDLE_STATUS:
        return "idle";
      case WL_NO_SSID_AVAIL:
        return "ssid not found";
      case WL_SCAN_COMPLETED:
        return "scan completed";
      case WL_CONNECTED:
        return "connected";
      case WL_CONNECT_FAILED:
        return "connect failed";
      case WL_CONNECTION_LOST:
        return "connection lost";
      case WL_DISCONNECTED:
        return "disconnected";
      default:
        return "unknown";
    }
  }

  String htmlPage() { return ""; }

  TimeModel &model_;
  SettingsStore &settings_;
  ClockRenderer &renderer_;
  TimeSync &timeSync_;
  TemperatureInput &temperature_;
  LuxSensor *lux_;
  ClockWebServer server_;
  bool enabled_ = false;
  bool mdnsEnabled_ = false;
  bool apMode_ = false;
};

// ===================== Focus Reminder Scheduler =====================
class FocusReminderScheduler {
 public:
  explicit FocusReminderScheduler(TimeModel &model, ClockRenderer &renderer, SettingsStore &settings)
      : model_(model), renderer_(renderer), settings_(settings) {}

  void checkAndFire(uint32_t now) {
    const ClockSettings &s = settings_.get();
    if (!s.focusReminder_enabled) return;

    ClockTime t = model_.get();
    uint8_t dow = getDayOfWeek();

    // Conditions check: day matches
    if (!(s.focusReminder_daysMask & (1 << dow))) return;

    // Time window check
    bool inWindow = false;
    if (s.focusReminder_startHour < s.focusReminder_endHour) {
      // Normal window (e.g., 08:00-22:00)
      inWindow = (t.hour >= s.focusReminder_startHour && t.hour < s.focusReminder_endHour);
    } else if (s.focusReminder_startHour > s.focusReminder_endHour) {
      // Wrapped window (e.g., 22:00-08:00, crosses midnight)
      inWindow = (t.hour >= s.focusReminder_startHour || t.hour < s.focusReminder_endHour);
    }
    if (!inWindow) return;

    // Interval check: enough time since last fire?
    uint32_t intervalMs = static_cast<uint32_t>(s.focusReminder_intervalMinutes) * 60000UL;
    if (now - s.focusReminder_lastFireMs < intervalMs) return;

    // Fire animation (reuse existing renderer methods)
    triggerReminderAnimation(s.focusReminder_animation, now);

    // Update last fire time in settings
    ClockSettings updated = s;
    updated.focusReminder_lastFireMs = now;
    settings_.update(updated);

    Serial.printf("[FocusReminder] Fired at %02d:%02d (interval=%d min, dow=%d)\n",
                  t.hour, t.minute, s.focusReminder_intervalMinutes, dow);
  }

 private:
  uint8_t getDayOfWeek() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return static_cast<uint8_t>(t.tm_wday);  // 0=Sun … 6=Sat
  }

  void triggerReminderAnimation(uint8_t mode, uint32_t now) {
    renderer_.triggerReminderDirectAnimation(mode, now);
  }

  TimeModel &model_;
  ClockRenderer &renderer_;
  SettingsStore &settings_;
};

class ButtonInput {
 public:
  enum class HoldPhase : uint8_t { IDLE, REPEAT_MIN, REPEAT_HOUR };

  void begin() {
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  }

  void poll(uint32_t now) {
    bool upNow   = (digitalRead(BUTTON_UP_PIN)   == LOW);
    bool downNow = (digitalRead(BUTTON_DOWN_PIN) == LOW);

    // --- UP button ---
    if (upNow && !lastUp_ && now - lastUpMs_ > kDebounceMs) {
      // Fresh press: debounce passed, leading edge
      upPressed_     = true;
      upPressedAt_   = now;
      upLastRepeat_  = now;
      upPhase_       = HoldPhase::IDLE;
      lastUpMs_      = now;
    } else if (upNow && lastUp_) {
      // Held: advance hold phase and fire repeats
      uint32_t held = now - upPressedAt_;
      if (held >= kHoldHourMs) {
        upPhase_ = HoldPhase::REPEAT_HOUR;
      } else if (held >= kHoldRepeatMs) {
        if (upPhase_ == HoldPhase::IDLE) upPhase_ = HoldPhase::REPEAT_MIN;
      }
      if (upPhase_ != HoldPhase::IDLE && now - upLastRepeat_ >= kRepeatIntervalMs) {
        upRepeat_     = true;
        upLastRepeat_ = now;
      }
    } else if (!upNow) {
      // Released
      upPhase_ = HoldPhase::IDLE;
    }

    // --- DOWN button ---
    if (downNow && !lastDown_ && now - lastDownMs_ > kDebounceMs) {
      downPressed_     = true;
      downPressedAt_   = now;
      downLastRepeat_  = now;
      downPhase_       = HoldPhase::IDLE;
      lastDownMs_      = now;
    } else if (downNow && lastDown_) {
      uint32_t held = now - downPressedAt_;
      if (held >= kHoldHourMs) {
        downPhase_ = HoldPhase::REPEAT_HOUR;
      } else if (held >= kHoldRepeatMs) {
        if (downPhase_ == HoldPhase::IDLE) downPhase_ = HoldPhase::REPEAT_MIN;
      }
      if (downPhase_ != HoldPhase::IDLE && now - downLastRepeat_ >= kRepeatIntervalMs) {
        downRepeat_     = true;
        downLastRepeat_ = now;
      }
    } else if (!downNow) {
      downPhase_ = HoldPhase::IDLE;
    }

    lastUp_   = upNow;
    lastDown_ = downNow;
  }

  // Returns non-zero delta if a press or repeat fired, 0 otherwise.
  // Positive = UP direction, negative = DOWN direction.
  int consumeUp() {
    if (upPressed_) { upPressed_ = false; ++g_buttonEventCount; return 1; }
    if (upRepeat_)  { upRepeat_  = false; ++g_buttonEventCount;
                      return (upPhase_ == HoldPhase::REPEAT_HOUR) ? 60 : 1; }
    return 0;
  }
  int consumeDown() {
    if (downPressed_) { downPressed_ = false; ++g_buttonEventCount; return -1; }
    if (downRepeat_)  { downRepeat_  = false; ++g_buttonEventCount;
                        return (downPhase_ == HoldPhase::REPEAT_HOUR) ? -60 : -1; }
    return 0;
  }

  // Legacy single-press API kept for any callers that only care about edge.
  bool consumeUpPress()   { return consumeUp()   != 0; }
  bool consumeDownPress() { return consumeDown() != 0; }

 private:
  static constexpr uint32_t kDebounceMs      =  50;
  static constexpr uint32_t kHoldRepeatMs    = 500;
  static constexpr uint32_t kRepeatIntervalMs= 150;
  static constexpr uint32_t kHoldHourMs      = 2000;

  bool upPressed_   = false;
  bool downPressed_ = false;
  bool upRepeat_    = false;
  bool downRepeat_  = false;
  bool lastUp_      = false;
  bool lastDown_    = false;

  uint32_t lastUpMs_      = 0;
  uint32_t lastDownMs_    = 0;
  uint32_t upPressedAt_   = 0;
  uint32_t downPressedAt_ = 0;
  uint32_t upLastRepeat_  = 0;
  uint32_t downLastRepeat_= 0;

  HoldPhase upPhase_   = HoldPhase::IDLE;
  HoldPhase downPhase_ = HoldPhase::IDLE;
};

// ===================== Globals =====================
Adafruit_NeoPixel ledStrip(CLOCK_PIXEL_COUNT, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
Adafruit_NeoPixel centerLedStrip(CENTER_PIXEL_STRIP_COUNT, CENTER_PIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif
SettingsStore settingsStore;
TimeModel timeModel;
#if CENTER_PIXEL_ENABLED && CENTER_PIXEL_SEPARATE_OUTPUT
ClockRenderer renderer(ledStrip, settingsStore, &centerLedStrip);
#else
ClockRenderer renderer(ledStrip, settingsStore);
#endif
TemperatureInput temperature;
LuxSensor luxSensor;
TimeSync timeSync(timeModel);
WebUi webUi(timeModel, settingsStore, renderer, timeSync, temperature);
FocusReminderScheduler reminderScheduler(timeModel, renderer, settingsStore);
ButtonInput buttons;

uint32_t lastTickMs = 0;
uint32_t lastAnimationRenderMs = 0;
uint32_t lastStatusLogMs = 0;
static uint8_t lastMinute = 255;

static void logRuntimeStatus(uint32_t now) {
  if (now - lastStatusLogMs < 10000) return;
  lastStatusLogMs = now;

  ClockTime t = timeModel.get();
  uint32_t uptimeSec = now / 1000;
  const ClockSettings &s = settingsStore.get();

  Serial.printf("\n[%02d:%02d:%02d] uptime=%02lu:%02lu:%02lu  heap=%lu B free\n",
                t.hour, t.minute, t.second,
                uptimeSec / 3600, (uptimeSec % 3600) / 60, uptimeSec % 60,
                static_cast<unsigned long>(ESP.getFreeHeap()));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("  WiFi  : %s  IP=%s  RSSI=%d dBm\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
  } else {
    Serial.printf("  WiFi  : OFFLINE (status=%d)\n", WiFi.status());
  }

#if LUX_SENSOR_ENABLED
  if (luxSensor.available()) {
    float lx = luxSensor.lux();
    uint8_t brRamped = luxSensor.autoBrightnessCached(now);
    uint8_t brTarget = luxSensor.autoBrightnessTarget();
    uint8_t brEffective = constrain((int)brRamped, s.minAutoBrightness, s.maxAutoBrightness);
    Serial.printf("  Lux   : %.1f lux  br=%d->%d(eff=%d)  mode=%d  min=%d  max=%d\n",
                  lx, brTarget, brRamped, brEffective,
                  s.autoBrightnessMode, s.minAutoBrightness, s.maxAutoBrightness);
  } else {
    Serial.println("  Lux   : VEML7700 not available");
  }
#endif

  Serial.printf("  LEDs  : count=%d  ringOffset(hw)=%d  rotOffset(sw)=%d  center=%d  sac=%s\n",
                CLOCK_PIXEL_COUNT, RING_PIXEL_OFFSET, s.outerRingOffset,
                CENTER_PIXEL_INDEX, SACRIFICIAL_PIXEL_ENABLED ? "yes" : "no");
  Serial.printf("  NTP   : %s\n", timeSync.synced() ? "synced" : "waiting");
}

void setup() {
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
#endif
  writeStatusLed(false);

  Serial.begin(115200);
  const uint32_t serialStartMs = millis();
  while (!Serial && millis() - serialStartMs < 2000) {
    delay(10);
  }
  Serial.println();
  Serial.println("NeoPixelClock v3 booting...");
  {
    esp_reset_reason_t rr = esp_reset_reason();
    const char *rrStr = "unknown";
    switch (rr) {
      case ESP_RST_POWERON:  rrStr = "power-on";         break;
      case ESP_RST_SW:       rrStr = "software reset";   break;
      case ESP_RST_PANIC:    rrStr = "panic/exception";  break;
      case ESP_RST_INT_WDT:  rrStr = "interrupt WDT";   break;
      case ESP_RST_TASK_WDT: rrStr = "task WDT";        break;
      case ESP_RST_WDT:      rrStr = "watchdog";        break;
      case ESP_RST_BROWNOUT: rrStr = "brownout";        break;
      case ESP_RST_DEEPSLEEP:rrStr = "deep-sleep wake"; break;
      default: break;
    }
    Serial.printf("Reset reason: %s (%d)\n", rrStr, (int)rr);
  }
  Serial.print("Build target: ");
  Serial.println(DEVICE_TITLE);
  Serial.print("LED count: ");
  Serial.println(CLOCK_PIXEL_COUNT);
  Serial.print("Ring pixel offset: ");
  Serial.println(RING_PIXEL_OFFSET);
  Serial.print("Center pixel: ");
  if (CENTER_PIXEL_ENABLED) {
#if CENTER_PIXEL_SEPARATE_OUTPUT
    Serial.print("separate output GPIO ");
    Serial.print(CENTER_PIXEL_PIN);
    Serial.print(", pixel index ");
    Serial.println(CENTER_PIXEL_INDEX);
#else
    Serial.print("enabled at physical index ");
    Serial.println(CENTER_PIXEL_INDEX);
#endif
  } else {
    Serial.println("disabled");
  }
  Serial.print("Sacrificial pixel: ");
  if (SACRIFICIAL_PIXEL_ENABLED) {
    Serial.print("enabled at physical index ");
    Serial.println(SACRIFICIAL_PIXEL_INDEX);
  } else {
    Serial.println("disabled");
  }
#if LUX_SENSOR_ENABLED
  Wire.begin(LUX_SENSOR_I2C_SDA, LUX_SENSOR_I2C_SCL);
#endif
  buttons.begin();
  settingsStore.begin();
  temperature.begin();
  luxSensor.begin();
  renderer.setLuxSensor(&luxSensor);
  renderer.begin();

  // Factory reset: hold UP (GPIO5) at power-on to enter prompt, then hold DOWN (GPIO9)
  // for 3 continuous seconds to confirm.  GPIO9 is the XIAO BOOT pin — it must NOT be
  // held at the reset instant or the chip enters download mode.  Requiring UP-only at
  // power-on avoids this; DOWN can safely be added once firmware is running.
  if (digitalRead(BUTTON_UP_PIN) == LOW) {
    Serial.println("[FactoryReset] UP held at boot — add DOWN within 5s and hold both 3s to reset.");
    ledStrip.setBrightness(200);
    ledStrip.fill(ledStrip.Color(255, 0, 0));
    ledStrip.show();

    bool confirmed = false;
    const uint32_t windowStart = millis();
    uint32_t bothHeldSince = 0;
    bool bothHeld = false;

    while (millis() - windowStart < 5000) {
      const bool upLow  = (digitalRead(BUTTON_UP_PIN)   == LOW);
      const bool downLow = (digitalRead(BUTTON_DOWN_PIN) == LOW);

      if (!upLow) break;  // UP released: cancel immediately

      if (upLow && downLow) {
        if (!bothHeld) { bothHeld = true; bothHeldSince = millis(); }
        if (millis() - bothHeldSince >= 3000) { confirmed = true; break; }
      } else {
        bothHeld = false;  // DOWN released: reset hold timer
      }
      delay(50);
    }

    if (confirmed) {
      Serial.println("[FactoryReset] Confirmed — clearing settings, forcing WiFi portal, rebooting.");
      settingsStore.resetToDefaults();
      Preferences prefs;
      prefs.begin("factory", false);
      prefs.putBool("portal", true);
      prefs.end();
      for (int i = 0; i < 2; i++) {
        ledStrip.fill(ledStrip.Color(255, 255, 255));
        ledStrip.show();
        delay(400);
        ledStrip.clear();
        ledStrip.show();
        delay(200);
      }
      ESP.restart();
    } else {
      Serial.println("[FactoryReset] Cancelled — resuming normal boot.");
      ledStrip.setBrightness(settingsStore.get().dayBrightness);
      ledStrip.clear();
      ledStrip.show();
    }
  }

  webUi.setLuxSensor(&luxSensor);
  webUi.begin();
  if (webUi.enabled()) {
    writeStatusLed(true);
    Serial.print("Web UI available at IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Web UI hostname: http://");
    Serial.print(DEVICE_HOSTNAME);
    Serial.println(".local/");
  } else {
    Serial.println("Web UI disabled (Wi-Fi not connected). Clock still runs offline.");
  }

  lastTickMs = millis();

  // Configure task watchdog: 10-second window, reboot on timeout.
  esp_task_wdt_init(10, true);  // 10s timeout, panic (reboot) on trigger
  esp_task_wdt_add(NULL);       // subscribe the Arduino loop task
}

void loop() {
  esp_task_wdt_reset();
  const uint32_t now = millis();
  buttons.poll(now);

  while (now - lastTickMs >= 1000) {
    timeModel.tickOneSecond();
    lastTickMs += 1000;
  }

  timeSync.loop();
  webUi.loop();
  logRuntimeStatus(now);

  // Check and fire focus reminders
  reminderScheduler.checkAndFire(now);

  // Trigger interval animations (quarter, half-hour, hourly)
  const ClockTime time = timeModel.get();
  const ClockSettings &settings = settingsStore.get();

  if (settings.intervalAnimationsEnabled && !renderer.animating()) {
    if (time.minute != lastMinute && time.second == 0) {
      lastMinute = time.minute;
      if (time.minute == 0) {
        renderer.triggerHourAnimation(now);
      } else if (time.minute == 30) {
        renderer.triggerHalfHourAnimation(now);
      } else if (time.minute % 15 == 0) {
        renderer.triggerQuarterAnimation(now);
      }
    }
  }

  {
    int upDelta = buttons.consumeUp();
    if (upDelta != 0) { timeModel.addMinutes(upDelta); renderer.setStatus(STATUS_BUTTON, 700); }
  }
  {
    int downDelta = buttons.consumeDown();
    if (downDelta != 0) { timeModel.addMinutes(downDelta); renderer.setStatus(STATUS_BUTTON, 700); }
  }

  if (renderer.animating()) {
    renderer.renderAnimFrame(now);
    if (!renderer.animating()) {
      timeModel.markDirty();
    }
  } else if (timeModel.consumeDirty()) {
    renderer.render(timeModel.get());
    lastAnimationRenderMs = now;
  } else if (renderer.needsFullAnimationFrame(now) && now - lastAnimationRenderMs >= 80) {
    renderer.render(timeModel.get());
    lastAnimationRenderMs = now;
  } else if (renderer.needsCenterAnimationFrame() && now - lastAnimationRenderMs >= 80) {
    // Full render instead of center-only: renderCenterAnimationFrame() called
    // setBrightness() every 80ms which re-scales the whole pixel buffer via
    // lossy integer division. Over ~12 frames/sec, ring pixels drifted ~7 units
    // darker than their correct value. render() on tick snapped them back —
    // creating a 1Hz brightness pulse on the outer ring. Inner ring ambient
    // pixels (scale 22-24/255) were too dim to show the drift, making them
    // appear "not in sync" with the outer ring. Full render here is safe:
    // strip_.clear() only zeroes RAM — it never calls show() — so no blank frame.
    renderer.render(timeModel.get());
    lastAnimationRenderMs = now;
  }
}

