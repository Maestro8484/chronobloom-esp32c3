# ChronoBloom ESP32-C3 — Changelog

All notable changes to this project are documented here.
Format: **[vX.X.X] — YYYY-MM-DD**

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
