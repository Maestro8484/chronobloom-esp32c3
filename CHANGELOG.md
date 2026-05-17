# ChronoBloom ESP32-C3 — Changelog

All notable changes to this project are documented here.
Format: **[vX.X.X] — YYYY-MM-DD**

---

## [v2.2.3] — 2026-05-17

### Fixed
- **`web_html.h` `draw()` JS preview ambient mismatch**: middle and inner ring ambient levels were `22` and `24` — corrected to `50`/`50` to match firmware `renderFace()` introduced in v2.2.2.

### Changed (no behavior change)
- **`autoBrightnessCached()`**: extracted `LUX_CEILING`, `BRIGHTNESS_MIN`, `BRIGHTNESS_RANGE` constants; added explanatory comment block. Formula is identical.
- **`lux()` gain state machine**: added threshold summary comment (`GAIN_2->GAIN_1: >200`, `GAIN_1->GAIN_2: <50`, `GAIN_1->GAIN_1_8: >900`, `GAIN_1_8->GAIN_1: <300`, settle lockout 400ms).
- **`renderSeconds()` trail array**: added geometric-decay rationale comment.
- **`setRingPixel()` rotation formula**: added `+30` rounding-bias comment.
- **`renderCenterIdle()`**: extracted `CENTER_PULSE_PERIOD_MS`, `CENTER_PULSE_FLOOR`, `CENTER_PULSE_CEILING` constants.

### Files changed
- `src/web_html.h` — `draw()` ambient setLed levels 22→50, 24→50
- `src/main.cpp` — `autoBrightnessCached`, `lux`, `renderSeconds`, `setRingPixel`, `renderCenterIdle`

### Notes
- No EEPROM change. No `SETTINGS_VERSION` bump. No firmware behavior change.
- Build: 8" env clean. RAM 11.1%, Flash 56.9%.

---

## [v2.2.2] — 2026-05-17

### Fixed
- **`ClockRenderer::renderFace` ambient scale simplified**: replaced `hoursLevel/6` and `centerLevel/6` expressions (introduced in v2.2.1) with a flat `50/255` (~20%) for both middle and inner rings. The ratio formulas introduced an unintended dependency — `hoursLevel` defaults to 255 and `centerLevel` defaults to 180, so the same divisor produced different ambient levels per ring. A single constant is correct: both rings should glow at the same ambient intensity fraction regardless of their indicator color settings.

### Files changed
- `src/main.cpp` — `renderFace` lines 840–843 (4 lines → 2 lines)
- `platformio.ini` — FIRMWARE_VERSION 2.2.1 → 2.2.2

### Notes
- No EEPROM change. No `SETTINGS_VERSION` bump.
- Known follow-up: `draw()` JS preview in `web_html.h` still uses old ambient values `22`/`24` — does not match firmware `50`/`50`. Flagged in `docs/chat-handoff-code-complexity-audit.md` for next session.
- Build: 8" env clean. RAM 11.1%, Flash 56.9%.

---

## [v2.2.1] — 2026-05-17

### Fixed
- **`ClockRenderer::renderFace` ambient scale (lines 840–845)**: middle and inner ring ambient fill was hardcoded at 22/255 (~8.6%) and 24/255 (~9.4%), making those rings appear 6–10× dimmer than the outer ring. Scale factors are now `hoursLevel/6` and `centerLevel/6` respectively — with defaults 255/6≈42 and 180/6=30, lifting ambient fill to ~16% and ~12%. User can tune via existing `hoursLevel`/`centerLevel` sliders.

### Added
- **`POST /settings/reset` endpoint** (`WebUi::setupRoutes`): resets all settings to firmware defaults, triggers LED confirmation flash. Wraps existing `SettingsStore::resetToDefaults()`.
- **"Reset all to defaults" button** (`web_html.h`): red-styled button alongside "Save display" in Display panel. Calls `resetToDefaults()` JS with `confirm()` guard; reloads all form fields via `loadSettings()`.
- **`/diag` response extended** (`WebUi::setupRoutes`): seven new fields — `effective_brightness`, `outer_marker_level`, `outer_filler_level`, `hours_level`, `center_level`, `middle_ambient_scale`, `inner_ambient_scale`. Buffer 1024→1536 bytes.

### Files changed
- `src/main.cpp` — `renderFace` (lines 840–845), `/diag` handler (~2337), new `/settings/reset` handler (~2430)
- `src/web_html.h` — `resetToDefaults()` JS function, "Reset all to defaults" button
- `platformio.ini` — FIRMWARE_VERSION 2.2.0 → 2.2.1

### Notes
- No EEPROM layout change. No `SETTINGS_VERSION` bump.
- Build: 8" env clean. RAM 11.1%, Flash 56.9%.
- Firmware built but not yet flashed (device offline during session).

---

## [v2.2.0] — 2026-05-16

### Changed (refactor — no behavior change)
- **WebUI HTML/JS extracted to PROGMEM headers**
  - New file: `src/web_html.h`
  - `WebUi::setupRoutes` reduced from 742 lines to 313 lines (~429 lines removed)
  - Constants: `INDEX_P1/P2/P3`, `WIFI_P1/MID/P2`, `UPDATE_P1/P2`, `OVERLAY_HTML`
  - All string sends use `sendContent_P` PROGMEM-aware variants (unchanged from before)
- **`ClockRenderer::tickAnimation` decomposed into per-case methods**
  - `tickAnimation` reduced from 650 lines to 35 lines (dispatcher only)
  - 30 new private methods: `animQ1–Q6`, `animH1–H7`, `animHr1–Hr10`, `animRem1–Rem6`
  - One method per case; no logic merged or split further

### Notes
- Refactor only — runtime behavior is byte-identical
- `symmap.json` regenerated; 30 new `ClockRenderer::anim*` symbols added (139 total)
- No `SETTINGS_VERSION` change
- No EEPROM impact
- `src/main.cpp` reduced from 3578 to 3167 lines

---

## [v2.1.1] — 2026-05-16

### Fixed
- **`setRingPixel` rotation rounding** — middle and inner rings now follow the outer ring proportionally when `outerRingOffset` changes. Previous integer-division math quantized middle ring in steps of 2.5 LEDs and inner ring in steps of 5 LEDs, so most single-step offset changes produced no visible movement on those rings.
- **Brightness floor in `sanitize()`** — `dayBrightness < 5` now floors to 44, `nightBrightness < 1` floors to 5. Prevents WebUI lockout from accidental zero-brightness input.

### Changed
- **`FocusReminderScheduler::lastFireMs` moved to RAM** — no longer persisted to EEPROM on every reminder fire. Eliminates ~20k EEPROM writes/year for typical reminder configurations. `focusReminder_lastFireMs` field retained in `ClockSettings` as reserved (no SETTINGS_VERSION bump).
- **`DemoMode::statusJson` migrated to `snprintf`** — reduces heap fragmentation in the WebUI demo-mode hot path.

### Added
- **`/diag` endpoint** — JSON diagnostics: uptime, firmware/settings versions, time, NTP sync state, WiFi status/SSID/RSSI/IP, lux, brightness chain (target/ramped), button event counter, free heap, LED pixel config. Replaces and expands the simpler v2.0.6 `/diag`.

### Files changed
- `platformio.ini` — FIRMWARE_VERSION 2.1.0 → 2.1.1
- `src/main.cpp` — `setRingPixel`, `SettingsStore::sanitize`, `FocusReminderScheduler::checkAndFire`, `DemoMode::statusJson`, `/diag` handler in `WebUi::setupRoutes`
- `CHANGELOG.md` — this entry

---

## [v2.2.0] — 2026-05-16 (in development)

### Demo Mode
- Added `DemoMode` class: non-blocking state machine for video recording sequences
- 6-step demo sequence: idle clock, chime animations, palette cycling, focus reminder, auto-brightness ramp, end card
- Total runtime: ~93 seconds
- `LuxSensor` extended with `setLuxOverride(float)` / `clearLuxOverride()` for simulating darkness in auto-brightness demo
- Web endpoints: `POST /demo/start`, `POST /demo/stop`, `GET /demo/status` (JSON), `GET /demo/overlay` (OBS-ready HTML)
- Web UI controls: Start/Stop buttons, live status display with step counter and progress bar
- `/demo/status` JSON: `{ active, step, subtitle, elapsed_ms, step_duration_ms }`
- `/demo/overlay` HTML: Full-screen 1920x1080, 48px white text on semi-transparent black pill, 300ms fade-in/out, polls status every 500ms
- Buttons ignored during demo; normal web endpoints continue to function
- No EEPROM changes, no SETTINGS_VERSION bump, no build flag changes
- Build: both envs clean, RAM 11.1%, Flash 56.8%

### Documentation Fixes
- Fixed `gen_symmap.py` parser to correctly handle class inheritance, hard-reset brace stack, and skip large functions with embedded HTML/JS
- Regenerated `symmap.json` and `FUNCTION_INVENTORY.md`: 110 functions (was 102), all class methods now correctly qualified
- Updated `HARDWARE.md` for 8" variant: 98 LEDs (97 active + 1 sacrificial at index 0), ring offset 1, center at index 97, button pins GPIO5/GPIO9
- Added gen_symmap reminder to `WORKFLOW.md` Token Reduction section

---

## [v2.1.0] — 2026-05-15

### Animation Expansion
- Added `animationPalette` (0-7), `animationSpeed` (1-5), `animationBrightness` (50-255), `trailLength` (2-12), `reminderPalette` (0-3)
- SETTINGS_VERSION bumped 10→11
- New palette system: 8 global palettes (Rainbow, Fire, Ocean, Forest, Candy, Neon, Monochrome, Clock) + 4 warm reminder palettes (Amber, Red, Magenta, Cyan-warm)
- 17 new animations:
  - Quarter-hour: ANIM_Q4 (laser ping), ANIM_Q5 (DNA twist), ANIM_Q6 (tick spark)
  - Half-hour: ANIM_H4 (comet chase), ANIM_H5 (color explosion), ANIM_H6 (Knight Rider), ANIM_H7 (strobe party)
  - Top-of-hour: ANIM_HR6 (supernova), ANIM_HR7 (matrix rain), ANIM_HR8 (galaxy spin), ANIM_HR9 (color wipe), ANIM_HR10 (thunderstorm)
  - Dedicated reminders: ANIM_REM1-REM6 (amber pulse, attention ring, heartbeat, sunrise wake, campfire flicker, neon sign)
- Web UI: preview buttons on all animation selectors, Animation Style panel, 12-hour AM/PM display
- `POST /previewAnimation?type=quarter|halfhour|hour|reminder&mode=N` endpoint for live preview
- New helper functions: `ClockRenderer::paletteColor()`, `scaledElapsed()`
- Build result: RAM 11.1%, Flash 56.3%

---

## [v2.0.9] — 2026-05-13

### WiFi Web UI
- New `/wifi` page for WiFi management (SSID display, signal strength, reconnect control)
- Credentials tier system (three tiers for different access levels)
- mDNS reconnect logic: hostname automatically re-advertised after WiFi reconnection

---

## [v2.0.8] — 2026-05-13

### Button Factory Reset
- Hold both buttons on boot for 3 seconds to factory reset EEPROM to defaults
- Visual feedback: center pixel pulses orange during reset

---

## [v2.0.7] — 2026-05-13

### Button Hold-to-Repeat
- GPIO swap: UP=GPIO5 (D3), DOWN=GPIO9 (D9)
- Hold >500ms: auto-repeat +1/-1 minute every 150ms
- Hold >2000ms: auto-repeat +60/-60 minute every fire
- Release: repeat stops immediately

---

## [v2.0.6] — 2026-05-13

### Physical Buttons Re-Added
- ButtonInput class re-implemented using polled reads (no ISRs)
- GPIO9 (D9) = UP, GPIO5 (D3) = DOWN
- 50ms debounce
- `/diag` endpoint added: diagnostic information including firmware version, boot reason, free heap

---

## [v2.0.5] — 2026-05-11

### OTA and Watchdog Hardening
- WiFi.setAutoReconnect(true) confirmed in setupWiFi()
- ArduinoOTA.onError() handler: calls ESP.restart() on OTA failure
- Software watchdog: esp_task_wdt_init(10, true) in setup()
- WDT fed: top of loop(), ArduinoOTA.onProgress, UPLOAD_FILE_WRITE handler
- Web UI /update: FormData streaming multipart (replaced raw XHR)
- UPDATE_SIZE_UNKNOWN fix: skip size check when upload.totalSize == 0

---

## [v2.0.4] — (date unknown)

### Physical Button Removal
- GPIO3 (JTAG TCK) and GPIO4 (JTAG TDI) buttons removed
- Reason: spurious ISR fires with USB connected
- Buttons re-added in v2.0.6 on safe pins (GPIO9, GPIO5)

---

## [v2.0.0+] — Earlier Releases

See `docs/HISTORY.md` for lineage and earlier maturation track sessions.
