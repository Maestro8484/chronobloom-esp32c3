# Function Inventory — src/main.cpp

**Source SHA256:** `52d8af3a811cebaa0bc073832f71de9c4910d47fe27a22d490ac1fcd3d5c498b`  
**Status:** Draft — items marked [REVIEW] need confirmation.

---

## Free Functions

---

### `configureWiFiHostname`
**Lines:** 147–152

Sets `INADDR_NONE` for all address fields then calls `WiFi.setHostname()` with `DEVICE_HOSTNAME`.  
Called once during WiFi bringup; hostname must be applied before the stack negotiates DHCP.

- **Reads:** `DEVICE_HOSTNAME` (build-time define)
- **Writes:** WiFi driver hostname slot
- **Build-env effect:** `DEVICE_HOSTNAME` is `esp32c3-v3-8inch` for the 8inch env and `esp32c3-v3-15inch` for the 15inch env (set in each env's `build_flags` in `platformio.ini`).
- **WebUI effect:** None directly; determines the mDNS hostname visible in the browser URL bar (e.g. `http://esp32c3-v3-8inch.local/`).
- **Dependencies:** Called inside `WebUi::begin`; the ESP WiFi stack requires hostname to be set before DHCP negotiation.

---

### `writeStatusLed`
**Lines:** 154–160

Drives a GPIO status LED high or low. Compiled to a no-op when `STATUS_LED_PIN < 0`.

- **Reads:** `STATUS_LED_PIN` (build-time define)
- **Writes:** GPIO `STATUS_LED_PIN`
- **Build-env effect:** `STATUS_LED_PIN=-1` is defined in `[common]` in `platformio.ini` — compiled to a no-op on **both** variants. Neither XIAO ESP32-C3 build uses a hardware status LED.
- **Dependencies:** Called from `setup()` after WiFi connects.

---

### `setupWiFi`
**Lines:** 1251–1279

Attempts to connect using build-time `WIFI_SSID`/`WIFI_PASSWORD` first; falls back to WiFiManager captive portal (`esp32c3-clock-setup`, open password) if credentials are the placeholder values or if the direct connect times out.

- **Reads:** `WIFI_SSID`, `WIFI_PASSWORD`, `WIFI_CONNECT_TIMEOUT_MS` (all build-time defines)
- **Writes:** WiFi driver state
- **Build-env effect:** Both envs supply real SSID/password via `platformio.ini` `-D` flags; placeholder guard (`strcmp` check) prevents attempting the captive portal when credentials are real.
- **Dependencies:** Called by `WebUi::begin`. Returns `bool` success; caller promotes device to AP mode on failure.

---

### `setupOTA`
**Lines:** 1282–1316

Registers ArduinoOTA handlers (start / end / progress / error) and calls `ArduinoOTA.begin()`. OTA password is hardcoded `"iris_ota_2026"`. Logs the exact `pio run ... --upload-port` command for convenience.

- **Reads:** `DEVICE_HOSTNAME` (build-time define)
- **Writes:** ArduinoOTA global state; Serial log
- **Build-env effect:** Hostname in the log message reflects the active env.
- **Dependencies:** Called inside `WebUi::begin` only when STA mode succeeds (not in AP mode).

---

### `logRuntimeStatus`
**Lines:** 2012–2052

Prints a status block to Serial every 10 seconds: current time, uptime, heap free, WiFi SSID/IP/RSSI, lux + brightness figures (if `LUX_SENSOR_ENABLED`), LED config, and NTP sync status.

- **Reads:** `timeModel`, `settingsStore`, `luxSensor`, `timeSync`, `WiFi`, `lastStatusLogMs` (global)
- **Writes:** `lastStatusLogMs`; Serial output only
- **Build-env effect:** Lux block is compiled out when `LUX_SENSOR_ENABLED=0`.
- **Dependencies:** Called every `loop()` tick; reads globals directly.

---

### `setup`
**Lines:** 2054–2132

Arduino entry point. Initialises Serial, logs reset reason and build identity, starts I²C (if `LUX_SENSOR_ENABLED`), then calls `begin()` on every subsystem in dependency order: `settingsStore → temperature → luxSensor → renderer → webUi`. Sets `lastTickMs` for the 1 Hz ticker.

- **Reads:** `ESP_RST_*` reset reason; all build-time defines for diagnostic prints
- **Writes:** `lastTickMs`; configures GPIO for `STATUS_LED_PIN` if ≥ 0
- **Build-env effect:** I²C `Wire.begin(LUX_SENSOR_I2C_SDA, LUX_SENSOR_I2C_SCL)` only when `LUX_SENSOR_ENABLED=1`. Center pixel separate-output strip is only instantiated for the 8inch env.
- **Dependencies:** All global objects must be constructed before `setup()` runs (they are — all globals are static storage).

---

### `loop`
**Lines:** 2134–2189

Arduino main loop. Responsibilities in order each iteration:

1. **1 Hz ticker** — calls `timeModel.tickOneSecond()` for every 1000 ms elapsed since `lastTickMs` (drift-free, accumulates rather than resets to `now`).
2. **NTP sync** — `timeSync.loop()` checks the SNTP callback flag and periodic 6-hour re-sync.
3. **Web server** — `webUi.loop()` dispatches HTTP requests and runs `ArduinoOTA.handle()`.
4. **Status logging** — `logRuntimeStatus()` every 10 s.
5. **Focus reminder** — `reminderScheduler.checkAndFire()`.
6. **Interval animation triggers** — checks for minute transitions at :00/:15/:30/:45 when `intervalAnimationsEnabled`.
7. **Render decision** (priority order):
   - If animation is active → `renderAnimFrame()`, then `markDirty()` on completion.
   - Else if time model dirty → `render()`.
   - Else if status/chime active and ≥80 ms since last render → `render()`.
   - Else if center animation needed and ≥80 ms → `render()` (full render to avoid brightness desync — see comment in source).

- **Reads:** `lastTickMs`, `lastAnimationRenderMs`, `lastMinute` (all globals); `timeModel`, `renderer`, `webUi`, `timeSync`, `settingsStore`
- **Writes:** `lastTickMs`, `lastAnimationRenderMs`, `lastMinute`
- **Build-env effect:** None directly; render path adapts via renderer's `CENTER_PIXEL_*` compile guards.

---

## Class: `LuxSensor`

Wraps VEML7700 ambient light sensor with a 3-state gain state machine, EMA smoothing, and a 50-units/sec brightness ramp. Compiled to stubs when `LUX_SENSOR_ENABLED=0`.

---

### `LuxSensor::begin`
**Lines:** 169–182

Calls `veml_.begin()`, sets initial gain to `VEML7700_GAIN_1` and integration time to 100 ms, marks sensor available. Prints result to Serial.

- **Reads:** `LUX_SENSOR_ENABLED` (compile-time)
- **Writes:** `available_`, VEML7700 hardware registers
- **Build-env effect:** Entire body compiled out when `LUX_SENSOR_ENABLED=0`; `available_` stays `false`.

---

### `LuxSensor::lux`
**Lines:** 186–236

Returns a smoothed lux reading. Key behaviors:
- Rate-limited to one hardware read per 120 ms.
- 400 ms settling guard after any gain change (prevents oscillation that plagued earlier firmware).
- 3-state gain machine: GAIN_2 (dark) → GAIN_1 (normal) → GAIN_1_8 (bright) with asymmetric hysteresis bands (50/200 and 300/900 lux thresholds).
- EMA alpha 0.15 (was 0.05; raised to track scene changes faster post gain-switch).

- **Reads:** `available_`, `lastReadMs_`, `lastGainChangeMs_`, `luxAvg_`; VEML7700 hardware
- **Writes:** `lastReadMs_`, `lastGainChangeMs_`, `luxAvg_`; VEML7700 gain register

---

### `LuxSensor::autoBrightness`
**Lines:** 240–242

Thin wrapper — returns `autoBrightnessCached(millis())`.

---

### `LuxSensor::autoBrightnessTarget`
**Lines:** 245–247

Returns `cachedBrightness_` (the lux-derived target before ramping). Used by `logRuntimeStatus` for diagnostics.

---

### `LuxSensor::autoBrightnessCached`
**Lines:** 249–286

Converts lux to LED brightness (log10 curve, 15–255 range) every 500 ms, then applies a 50 units/sec ramp toward the target. On first call, snaps directly to target (no initial ramp-from-128). `dt` is capped at 200 ms to absorb stalls without a jump.

- **Reads:** `available_`, `luxAvg_`, `cachedBrightness_`, `rampedBrightness_`, `lastBrightnessMs_`, `lastRampMs_`
- **Writes:** `cachedBrightness_`, `rampedBrightness_`, `lastBrightnessMs_`, `lastRampMs_`
- **WebUI effect:** `minAutoBrightness` / `maxAutoBrightness` are applied by the caller (`effectiveBrightness`), not here.

---

## Class: `SettingsStore`

Owns the 256-byte EEPROM region. Validates magic+version on load; writes defaults on mismatch. All mutations go through `sanitize()` before saving.

---

### `SettingsStore::begin`
**Lines:** 366–373

Opens EEPROM, reads settings, replaces with defaults if invalid.

- **Reads:** EEPROM byte 0…sizeof(ClockSettings)
- **Writes:** `settings_`; EEPROM (only on invalid load)

---

### `SettingsStore::update`
**Lines:** 377–380

Sanitizes and saves the given settings struct.

- **Reads:** incoming `ClockSettings`
- **Writes:** `settings_`; EEPROM via `save()`

---

### `SettingsStore::defaults`
**Lines:** 383–395

Returns a hardcoded `ClockSettings` with `SETTINGS_MAGIC = 0xC1`, `SETTINGS_VERSION = 10`, and the default palette (blue-white markers, royal-blue filler, ice-blue seconds, orange minutes, hot-pink hours, warm-orange center).

---

### `SettingsStore::valid`
**Lines:** 397–415

Validates magic, version, and all field ranges. Returns `false` on any out-of-range value, triggering a full defaults reset.

- **Build-env effect:** None — validation ranges are independent of hardware variant.

---

### `SettingsStore::sanitize`
**Lines:** 417–447

Clamps all fields to valid ranges; never rejects a POST silently. Notable: `minAutoBrightness > maxAutoBrightness` resets both to (10, 255).

---

## Class: `TimeModel`

Thread-safe (interrupt-safe) time store. Uses `noInterrupts()`/`interrupts()` guards on every read and write. All call sites are currently in `loop()` (no ISR usage), but the guards are correct defensive practice: on single-core ESP32-C3 they cost ~2 cycles and ensure correctness if `tickOneSecond` is ever promoted to a hardware timer ISR.

---

### `TimeModel::get`
**Lines:** 466–471

Returns a copy of `time_` under interrupt lock.

---

### `TimeModel::set`
**Lines:** 473–481

Validates h/m/s ranges then sets all three fields and marks dirty.

- **Writes:** `time_`, `dirty_`

---

### `TimeModel::tickOneSecond`
**Lines:** 483–488

Increments the clock by one second (delegates to `incrementSecondNoLock`), marks dirty.

- **Writes:** `time_`, `dirty_`

---

### `TimeModel::addMinutes` / `TimeModel::addHours`
**Lines:** 490–510

Adjusts hour or minute with day-wrap arithmetic. Used by `/addMinute` and `/subMinute` WebUI endpoints.

- **Writes:** `time_`, `dirty_`

---

### `TimeModel::consumeDirty`
**Lines:** 517–524

Atomically reads and clears `dirty_`. The render loop uses this to decide whether to redraw.

---

### `TimeModel::incrementSecondNoLock`
**Lines:** 527–536

Private helper — cascades second→minute→hour with wrap-around. Must be called under interrupt lock.

---

## Class: `ClockRenderer`

Owns all LED output logic. Holds a reference to `ledStrip` (and optionally `centerStrip_` for separate-output center pixel). Operates in two modes: normal clock render (`render()`) and animation render (`renderAnimFrame()`).

---

### `ClockRenderer::begin`
**Lines:** 579–592

Calls `strip_.begin()`, sets initial brightness from `dayBrightness`, clears and shows. Initialises `centerStrip_` identically if `CENTER_PIXEL_SEPARATE_OUTPUT` is active.

- **Build-env effect:** `CENTER_PIXEL_SEPARATE_OUTPUT` is only set in specific configs; 8inch uses inline center pixel.

---

### `ClockRenderer::setStatus`
**Lines:** 596–601

Arms a timed status display. Ignored if `statusAnimations` is disabled. `durationMs` is added to `millis()` to set `statusUntilMs_`.

- **Reads:** `settings_.statusAnimations`
- **Writes:** `statusMode_`, `statusUntilMs_`, `lastAnimationMs_`
- **WebUI effect:** Controlled by the "Status" checkbox in the Display panel.

---

### `ClockRenderer::triggerQuarterAnimation` / `triggerHalfHourAnimation` / `triggerHourAnimation`
**Lines:** 603–628

Set `animPhase_` to the configured animation variant (0 = off), reset `animStartMs_` and `animStep_`. No-op when the setting is 0.

- **Reads:** `settings_.quarterAnimation` / `halfHourAnimation` / `hourAnimation`
- **Writes:** `animPhase_`, `animStartMs_`, `animStep_`, `animHue_`
- **WebUI effect:** Controlled by the Time Animations panel selectors.

---

### `ClockRenderer::render`
**Lines:** 681–717

Full clock face render. Sequence: set brightness → clear → `renderFace` → `renderSeconds` → `renderMinutes` → `renderHours` → conditional chime or status or center-idle → `setSacrificialPixelDark` → `show()`.

- **Reads:** `settings_`, `lux_`, `lastTime_` (updated at entry), all `ClockTime` fields
- **Writes:** `lastTime_`, `ledStrip` pixel buffer, hardware SPI/PWM via `show()`
- **Dependencies:** All private render helpers; `effectiveBrightness`

---

### `ClockRenderer::renderAnimFrame`
**Lines:** 632–649

Render path used during animations. Calls `tickAnimation()` instead of the clock face helpers. Always calls `setSacrificialPixelDark()` before `show()`.

- **Reads:** `animPhase_`, `lastTime_`, `settings_`, `lux_`
- **Writes:** pixel buffer; calls `tickAnimation()`

---

### `ClockRenderer::needsFullAnimationFrame`
**Lines:** 651–653

Returns true if a status animation is active or the hourly chime is showing. Used by `loop()` to schedule re-renders at 12.5 Hz even when time hasn't ticked.

---

### `ClockRenderer::needsCenterAnimationFrame`
**Lines:** 655–657

Returns true if `CENTER_PIXEL_ENABLED` and `statusAnimations` are both on. Triggers the 80 ms center-pulse re-render path in `loop()`.

---

### `ClockRenderer::effectiveBrightness`
**Lines:** 728–743

Returns the LED strip brightness to apply, based on `autoBrightnessMode`:
- 0 = manual (`dayBrightness`)
- 1 = lux sensor (`autoBrightnessCached`, clamped to min/max)
- 2 = scheduled (`nightBrightness` or `dayBrightness` depending on hour)

- **Reads:** `settings_.autoBrightnessMode`, `settings_.dayBrightness`, `settings_.nightBrightness`, `settings_.minAutoBrightness`, `settings_.maxAutoBrightness`, `lux_`; `nightActive()` for mode 2
- **WebUI effect:** Controlled by the Auto-Brightness panel.

---

### `ClockRenderer::renderFace`
**Lines:** 745–764

Draws the static clock face: outer ring with 5-LED markers every 5 positions, middle ring dimly lit in hours color (scale 22/255), inner ring dimly in center color (scale 24/255). `colorTheme` field is read but unused (no-op, left as dead code stub [REVIEW: intentional placeholder?]).

- **Reads:** `settings_.outerMarkerRed/Green/Blue/Level`, `outerFillerRed/…`, `hoursRed/…`, `centerRed/…`, `colorTheme`
- **Writes:** `ledStrip` pixels via `setRingPixel`
- **Note:** `colorTheme` is accessed then immediately discarded via `(void)settings.colorTheme`. The private `secondColor()` helper (line 805) also references `colorTheme` but is never called — `renderSeconds` uses `ringColor()` directly. Both are dead code, left as stubs for a future theme-wide color override feature.

---

### `ClockRenderer::renderSeconds`
**Lines:** 766–784

Places the second hand on the outer ring. If `progressSeconds`: fills LEDs 0…second at 20/255 intensity. If `secondTrail`: paints 4 trailing pixels at decreasing intensities (52, 28, 14, 7 out of 255). Always places the full-intensity second dot last.

- **Reads:** `time.second`, `settings_.progressSeconds`, `settings_.secondTrail`, `settings_.secondsRed/…/Level`
- **WebUI effect:** "Second trail" and "Progress ring" checkboxes.

---

### `ClockRenderer::renderMinutes`
**Lines:** 786–790

Places one full-intensity pixel on the outer ring at `time.minute`.

- **Reads:** `time.minute`, `settings_.minutesRed/…/Level`

---

### `ClockRenderer::renderHours`
**Lines:** 792–803

Places the hour hand on middle and inner rings. Uses 12-hour format. If minute ≥ 30, `hourOffset = 1` advances the position by one LED to show half-hour progression. Middle ring uses index `(hour12 * 2 + offset) % 24`; inner ring lights both `hour12` and `(hour12 + offset) % 12` (so two pixels when mid-hour).

- **Reads:** `time.hour`, `time.minute`, `settings_.hoursRed/…/Level`

---

### `ClockRenderer::renderStatus`
**Lines:** 822–859

Draws a two-pixel rotating dot on the inner ring in a color keyed to `statusMode_` (blue=connecting, green=OK, red=fail, etc.) plus a pulsing center pixel.

- **Reads:** `statusMode_`, `lux_` indirectly via brightness
- **WebUI effect:** Status colors are fixed in code; not user-configurable.

---

### `ClockRenderer::renderHourlyChime`
**Lines:** 861–866

Draws a sweeping white dot on outer ring and proportional amber dot on middle ring, plus a pulsing gold center. Sweep period = 65 ms per position.

- **Reads:** `now` (millis)
- **Dependencies:** Called by both `render()` (when `hourlyChime` active) and `tickAnimation()` for `ANIM_HR1`.

---

### `ClockRenderer::tickAnimation`
**Lines:** 868–1087

State machine dispatching on `animPhase_`. Eleven animation cases:

| Phase | Name | Duration | Description |
|---|---|---|---|
| ANIM_Q1 | Sparkle burst | 600 ms | White flash on 15-pixel markers, rapid fade |
| ANIM_Q2 | Quarter pulse | 2500 ms | Outer 5-position markers pulse between full/half brightness (1 Hz) |
| ANIM_Q3 | Ring shimmer | 2500 ms | Full face re-draw with brightness strobe |
| ANIM_H1 | Rainbow sweep | 5000 ms | HSV rainbow fills outer→middle→inner in sequence |
| ANIM_H2 | Dual flash | 5000 ms | Outer ring alternates cyan/red halves at 300 ms |
| ANIM_H3 | Tidal pulse | 5000 ms | Sine-wave brightness on cyan outer ring, 3 waves |
| ANIM_HR1 | Chime sweep | 10000 ms | Reuses `renderHourlyChime()` for 10 s |
| ANIM_HR2 | Firework burst | 10000 ms | Step-gated ring fill from center outward, random HSV; trailing spark |
| ANIM_HR3 | Zenith cascade | 10000 ms | Gold fills outer ring from both ends to center, then fades symmetrically |
| ANIM_HR4 | Rainbow spiral | 10000 ms | Outer CW rainbow fills, then middle CCW, then inner CW |
| ANIM_HR5 | Breathing mandala | 10000 ms | Hue-shifted HSV all rings, sin-wave brightness, 6 breath cycles |

- **Reads:** `animPhase_`, `animStartMs_`, `animStep_`, `animHue_`, `settings_`
- **Writes:** `animPhase_` (sets ANIM_IDLE on timeout), `animStep_`, pixel buffer

---

### `ClockRenderer::setRingPixel`
**Lines:** 1158–1166

Maps a logical ring index to a physical strip index, applying `outerRingOffset` rotation and clockwise/counter-clockwise direction from `RingConfig`. The rotation math scales `outerRingOffset` (0–59 outer-ring steps) to each ring's pixel count proportionally.

- **Reads:** `settings_.outerRingOffset`, `ring.count`, `ring.offset`, `ring.clockwise`
- **WebUI effect:** "Ring rotation offset" slider. Applies uniformly to all three rings.

---

### `ClockRenderer::setCenterPixel`
**Lines:** 1134–1148

Writes to `centerStrip_` when `CENTER_PIXEL_SEPARATE_OUTPUT=1`, otherwise writes to the main `strip_` at `CENTER_PIXEL_INDEX`.

- **Build-env effect:** 8inch env uses inline center pixel at index 1; separate-output path unused in standard builds.

---

### `ClockRenderer::setSacrificialPixelDark`
**Lines:** 1150–1156

Forces pixel `SACRIFICIAL_PIXEL_INDEX` to 0 (black) every frame. Compiled out when `SACRIFICIAL_PIXEL_ENABLED=0`. The sacrificial pixel is a first-in-chain LED used to buffer signal integrity; it must stay dark.

- **Build-env effect:** 8inch env enables `SACRIFICIAL_PIXEL_ENABLED=1` at index 0.

---

### `ClockRenderer::dim` / `scale` / `pulse`
**Lines:** 1097–1119

Color math utilities:
- `dim(color, divisor)` — divides each channel by `divisor`.
- `scale(color, amount)` — multiplies each channel by `amount/255`.
- `pulse(color, now, periodMs, floor, ceiling)` — triangle-wave modulation between `floor` and `ceiling` intensity.

---

### `ClockRenderer::nightActive`
**Lines:** 720–726

Returns true if current hour is in the night window. Handles wrap-around (e.g., 22:00–07:00). Returns false if start == end (disabled).

- **Reads:** `time.hour`, `settings_.nightStartHour`, `settings_.nightEndHour`

---

### `ClockRenderer::chimeActive`
**Lines:** 1089–1091

Returns true when `minute == 0 && second < 6`. Limits chime display to 6 seconds past the hour.

---

### `ClockRenderer::statusActive`
**Lines:** 1093–1095

Returns true while `statusUntilMs_` is in the future.

---

### `ClockRenderer::centerIdleActive`
**Lines:** 1122–1124

Returns true when `CENTER_PIXEL_ENABLED` and `statusAnimations` setting is on.

---

### `ClockRenderer::renderCenterIdle`
**Lines:** 1126–1132

Applies a slow 1800 ms period pulse (3→75 brightness range) on the center pixel using the center color setting.

- **WebUI effect:** Enabled by the "Status" checkbox; color from the Center row in the Rings panel.

---

## Class: `TemperatureInput`

Stub class — `TEMP_SENSOR_ENABLED` is always 0 in current builds. `available()` returns false; `celsius()` returns `NAN`. The WebUI `/temperature` endpoint exists but returns `{"available":false}`.

---

## Class: `TimeSync`

---

### `TimeSync::begin`
**Lines:** 1195–1205

Calls `configTzTime()` with `NTP_TIMEZONE_TZ` and three NTP servers. Registers an SNTP notification callback that sets the static `s_sntpPending_` flag (volatile bool, safe on single-core ESP32-C3).

- **Build-env effect:** `NTP_TIMEZONE_TZ="MST7MDT,M3.2.0,M11.1.0"` (Mountain time with US DST rules) is set in `[common]` — identical for both envs. No per-env override exists in `platformio.ini`.

---

### `TimeSync::syncNow`
**Lines:** 1207–1223

Reads system UTC epoch, rejects values before epoch 1700000000 (avoids applying unsynced time). Calls `localtime_r` to convert to local time using the TZ string, then sets `timeModel`. Logs sync result.

- **Reads:** `time(nullptr)` (POSIX system clock)
- **Writes:** `TimeModel` (via `model_.set()`), `lastSyncMs_`, `synced_`

---

### `TimeSync::loop`
**Lines:** 1225–1234

Consumes `s_sntpPending_` flag and calls `syncNow()` on: SNTP callback, first sync, or 6-hour periodic re-sync. No-op if WiFi is down.

- **Reads:** `s_sntpPending_` (volatile static), `synced_`, `lastSyncMs_`
- **Writes:** `s_sntpPending_`; delegates to `syncNow()`

---

## Class: `WebUi`

Owns `ClockWebServer` on port 80. Manages WiFi connect, mDNS, OTA setup, and all HTTP routes.

---

### `WebUi::begin`
**Lines:** 1333–1390

Full bringup sequence (guarded by `ENABLE_WIFI_UI`):
1. Sets renderer status to `STATUS_WIFI_CONNECTING`.
2. Calls `setupWiFi()`; on failure switches to AP mode (`DEVICE_HOSTNAME`, open).
3. On STA success: re-applies hostname, registers mDNS, attaches WiFi reconnect event for mDNS re-registration, calls `setupOTA()`, calls `timeSync_.begin()`, fires initial NTP sync.
4. Always calls `setupRoutes()` and `server_.begin()`.

- **Reads:** `DEVICE_HOSTNAME`, `WIFI_CONNECT_TIMEOUT_MS`
- **Writes:** `enabled_`, `apMode_`, `mdnsEnabled_`
- **Dependencies:** `setupWiFi`, `setupOTA`, `TimeSync::begin`, `TimeSync::syncNow`

---

### `WebUi::loop`
**Lines:** 1392–1396

Calls `server_.handleClient()` and `ArduinoOTA.handle()`. No-op when `!enabled_`.

---

### `WebUi::setupRoutes`
**Lines:** 1401–1668

Registers all HTTP routes:

| Method | Path | Action |
|---|---|---|
| GET | `/` | Serve main HTML page |
| GET | `/time` | JSON: hour/minute/second/ntpSynced/wifi/ip |
| GET | `/temperature` | JSON: available/celsius |
| GET | `/lux` | JSON: available/lux/autoBrightness |
| GET | `/net` | JSON: hostname/ssid/ip/gateway/subnet/dns/rssi/status |
| GET | `/settings` | JSON: all ClockSettings fields |
| POST | `/settings` | Update settings from form params; triggers STATUS_SETTINGS_SAVED |
| POST | `/set` | Set time directly (h/m/s params) |
| POST | `/syncBrowser` | Same as `/set` with STATUS_TIME_SYNC for 1500 ms |
| POST | `/syncNtp` | Force NTP sync immediately |
| POST | `/addMinute` | Increment time by 1 minute |
| POST | `/subMinute` | Decrement time by 1 minute |
| GET | `/update` | Serve firmware update HTML page |
| POST | `/update` | Receive firmware .bin, flash via `Update` API, restart |

The POST `/update` handler checks free sketch space before beginning; flashes in chunks; triggers OTA status LEDs on success/failure.

- **Dependencies:** All model/settings/renderer methods; `parseColor`, `clampByte`, `clampWord`

---

### `WebUi::settingsJson`
**Lines:** 1670–1708

Serializes all `ClockSettings` fields to a JSON string. Colors are formatted as `#RRGGBB` hex via `colorHex()`.

---

### `WebUi::htmlPage`
**Lines:** 1773–1906

Returns the full single-page app as a string literal. The UI is a two-column dark-themed grid: left column has an SVG clock preview (built in JS), right column has panels for Time, Display, Rings, Auto-Brightness, Time Animations, Focus Reminders, Network, and Admin (firmware update link). All settings round-trip via `/settings` GET+POST. JS re-renders the SVG at 90 ms intervals; time display refreshes at 1 s.

---

### `WebUi::parseColor`
**Lines:** 1739–1750

Parses a `#RRGGBB` hex string into R, G, B bytes. Silently ignores strings with wrong length or invalid hex digits (leaves r/g/b unchanged).

---

### `WebUi::clampByte` / `clampWord`
**Lines:** 1714–1724

Integer clamp helpers that return `uint8_t` / `uint16_t`. Used to sanitize all POST parameters before they reach `ClockSettings`.

---

## Class: `FocusReminderScheduler`

---

### `FocusReminderScheduler::checkAndFire`
**Lines:** 1926–1961

Evaluates whether a focus reminder should fire on each `loop()` call. Gate conditions (all must pass):
1. `focusReminder_enabled` is set.
2. Current day-of-week bit is set in `focusReminder_daysMask`.
3. Current hour is within the start/end window (handles midnight-wrap).
4. `millis() - focusReminder_lastFireMs >= intervalMs`.

On fire: calls `triggerReminderAnimation()`, updates `focusReminder_lastFireMs` in EEPROM via `settings_.update()`, logs to Serial.

- **Reads:** `settings_`, `timeModel`, `time(nullptr)` (via `getDayOfWeek`)
- **Writes:** `settings_.focusReminder_lastFireMs` (persisted to EEPROM); triggers animation via renderer
- **WebUI effect:** All fields controlled by the Focus Reminders panel.

---

### `FocusReminderScheduler::getDayOfWeek`
**Lines:** 1964–1969

Returns `tm_wday` (0=Sun … 6=Sat) from POSIX `localtime_r`. **Latent bug:** unlike `TimeSync::syncNow`, this function has no guard for `time(nullptr) < 1700000000`. On a cold boot before NTP syncs, `localtime_r` returns a near-epoch date (Jan 1970) whose `tm_wday` is almost certainly wrong. The interval gate in `checkAndFire` prevents rapid re-fires, but the first reminder after boot can fire on the wrong day if `focusReminder_lastFireMs` is 0 and the day-mask happens to allow the epoch weekday (Thursday = 1970-01-01).

---

### `FocusReminderScheduler::triggerReminderAnimation`
**Lines:** 1971–1982

Maps `focusReminder_animation` (0–5) to the three animation tier methods. Values 0/3 → quarter, 1/4 → half-hour, 2/5 → hour. Cases 3–5 are intentional placeholder slots: the WebUI labels them `"(dup)"` explicitly and the source comment says "Duplicate for safety". They are reserved for distinct v2 animation types that haven't been implemented yet.

---

## Key Data Structures

### `ClockSettings` (struct, lines 302–357)
Stored at EEPROM offset 0. 57+ bytes. Magic `0xC1`, version `10`. Adding fields requires bumping `SETTINGS_VERSION` to force a defaults reset on existing devices.

### `RingConfig` (struct, lines 104–108)
Immutable compile-time descriptors for each LED ring. Three constants: `RING_OUTER_60`, `RING_MIDDLE_24`, `RING_INNER_12`. `offset` is the physical chain index of logical position 0; `clockwise` controls direction; `count` is the ring size.

### `ClockTime` (struct, lines 457–461)
Plain POD: hour (0–23), minute (0–59), second (0–59). Copied by value across thread boundaries.

### `AnimPhase` / `StatusMode` (enums, lines 543–569)
`AnimPhase` drives `tickAnimation()`; `StatusMode` drives `renderStatus()`. Both are `uint8_t` for EEPROM/struct packing compatibility.
