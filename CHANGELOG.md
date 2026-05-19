# ChronoBloom ESP32-C3 — Changelog

All notable changes to this project are documented here.
Format: **[vX.X.X] — YYYY-MM-DD**

---

## [v2.4.2] — 2026-05-18

### Added
- **Per-ring face brightness controls** (`middleFaceScale`, `innerFaceScale`): Two new `ClockSettings` fields (0–255) replacing the hardcoded `50` ambient scale in `renderFace()` for the middle (24 LED) and inner (12 LED) rings. Defaults: 55 each.
- **Contrast preset dropdown** (web UI Rings panel): Select from Current defaults / Soft contrast / Clear contrast / High contrast / Balanced glow. Selecting a preset immediately sets `outerRingBrightness`, `middleFaceScale`, and `innerFaceScale` and POSTs to persist. Dropdown reverts to "Custom" if any of the three linked fields is manually edited.
- `SETTINGS_VERSION` bumped 12 → 13 (stale EEPROM resets to new defaults on first boot).

### Files changed
- `src/main.cpp` — `ClockSettings` struct, `SETTINGS_VERSION`, `defaults()`, `valid()`, `renderFace()`, `settingsJson()`, `setupRoutes()` POST handler
- `src/web_html.h` — Rings panel HTML, `loadSettings`, `bindLive`, `saveSettings`, new `applyContrastPreset`/`updateContrastPreset` functions
- `docs/FUNCTION_INVENTORY.md`, `docs/symmap.json` — regenerated (line numbers shifted by struct/version changes)
- `platformio.ini` — `FIRMWARE_VERSION` 2.4.1 → 2.4.2

---

## [v2.3.6] — 2026-05-17

### Fixed
- **`renderAnimFrame` uint32 underflow — preview always 1-frame flicker** (`src/main.cpp` `loop()`): `loop()` captures `now = millis()` before `webUi.loop()` runs. The `/previewAnimation` HTTP handler (inside `webUi.loop()`) calls `triggerAnimDirect(..., millis())`, setting `animStartMs_` to a timestamp newer than `now`. `renderer.renderAnimFrame(now)` then computed `elapsed = now - animStartMs_` → uint32 underflow → ~4.3 billion → `elapsed >= 5000` immediately true → animation terminated on frame 1. Every web preview fired exactly one frame (a flicker) and ended. Scheduled animations at `:00/:15/:30/:45` were unaffected because their triggers pass the same stale `now`. Fix: `renderAnimFrame(now)` → `renderAnimFrame(millis())`. This bug has been present since v2.1.0 (Session 12).

### Files changed
- `src/main.cpp` — `loop()` line 3233
- `platformio.ini` — `FIRMWARE_VERSION` 2.3.5 → 2.3.6

---

## [v2.3.5] — 2026-05-17

### Added
- **`/diag` — `anim_phase`**: Current animation phase as a short string ("idle", "Q1"–"Q6", "H1"–"H7", "Hr1"–"Hr10", "Rem1"–"Rem6"). Shows what is actually rendering at poll time.
- **`/diag` — `last_anim_source`**: Source of the last animation trigger: "quarter", "halfhour", "hour", "reminder", or "preview". Persists after the animation ends.
- **`/diag` — `last_anim_mode`**: Mode number of the last animation trigger (matches the UI dropdown value; for "preview" source, carries the AnimPhase ordinal).
- **`/diag` — `settings_save_count`**: Count of `SettingsStore::update()` calls since boot. A bump here proves a `/settings` POST was received and applied.

### Files changed
- `src/main.cpp` — `SettingsStore::update`, `resetToDefaults`, `saveCount` (new); `ClockRenderer` member vars, `animPhaseName` (new), `lastAnimSource` (new), `lastAnimMode` (new), all five trigger functions, `/diag` handler
- `platformio.ini` — `FIRMWARE_VERSION` 2.3.4 → 2.3.5
- `docs/symmap.json` — regenerated (139 → 143 functions)
- `docs/FUNCTION_INVENTORY.md` — regenerated

---

## [v2.3.4] — 2026-05-17

### Fixed
- **Browser cache — `Cache-Control` header** (`src/main.cpp`, `WebUi::setupRoutes`): The `/` GET handler had no cache headers; browsers cached the HTML+JS page indefinitely. All JS changes in v2.3.1–v2.3.3 were silently ignored because browsers served the old cached page. Added `server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate")` before the chunked send. Browsers must now reload the page from the device on every visit. **After flashing: hard-refresh the browser (Ctrl+Shift+R) or open in private/incognito to immediately discard the cached page.**

### Files changed
- `src/main.cpp` — `WebUi::setupRoutes` `/` GET handler (line 2308)
- `platformio.ini` — `FIRMWARE_VERSION` 2.3.3 → 2.3.4

---

## [v2.3.3] — 2026-05-17

### Fixed
- **`animQ3` palette response** (`src/main.cpp`): replaced `renderFace(settings)` call with palette-colored ring fills using `paletteColor()`. Previously the animation drew the clock face in configured clock colors regardless of the Animation Palette setting. Now outer ring (RING_OUTER_60) and middle ring (RING_MIDDLE_24) fill with palette-sampled colors in a bright/dim cycle (400ms bright, 200ms dim at 64/255 scale). `strip_.setBrightness(animationBrightness)` applied. Function grew from 7 to 14 lines.
- **`saveAnimStyle()` JS purple flash** (`src/web_html.h`): converted from synchronous `post()` to `async fetch` with `silent=1` in POST body. The Save Style button no longer triggers the 1.3 s `STATUS_SETTINGS_SAVED` purple spinner on the LED ring.

### Files changed
- `src/main.cpp` — `animQ3` (lines 1120–1133)
- `src/web_html.h` — `saveAnimStyle` (line 236)
- `platformio.ini` — `FIRMWARE_VERSION` 2.3.2 → 2.3.3
- `docs/symmap.json` — regenerated (animQ3 +7 lines; all subsequent functions shifted)
- `docs/FUNCTION_INVENTORY.md` — regenerated

---

## [v2.3.2] — 2026-05-17

### Fixed
- **`previewAnim(type, modeId)` JS** (`src/web_html.h`): was synchronous and only POST'd `/previewAnimation` with the mode value — never applied current (unsaved) style slider values. Preview therefore ran with whatever was last saved. Now `async`; silently POSTs all five style fields to `/settings?silent=1` before triggering preview. Affects all four regular Preview buttons (Quarter, Half-hour, Hour, Reminder).
- **`previewStyleAnim()` JS** (`src/web_html.h`): saved style to `/settings` without `silent=1`, triggering a 1.3 s purple `STATUS_SETTINGS_SAVED` spinner before the preview played. Also previewed hardcoded demo animations instead of the user's actual selected slot. Now saves silently, reads `stylePreviewType` slot selector, looks up the current mode from the matching animation dropdown, and calls `/previewAnimation` with that slot+mode. If mode=0 (Off) returns silently with no flash.
- **`stylePreviewType` select** (`src/web_html.h`): was 11 hard-coded `type:mode` demo options. Replaced with 4 slot options: "Quarter (current selection)", "Half-hour (current selection)", "Hour (current selection)", "Reminder (current selection)".
- **`/settings` POST `silent` param** (`src/main.cpp`, `WebUi::setupRoutes`): added `if (!server_.hasArg("silent"))` guard around `renderer_.setStatus(STATUS_SETTINGS_SAVED, 1300)`. Settings still update and mark EEPROM dirty; only the status animation is suppressed.
- **`animQ3` speed** (`src/main.cpp`): `elapsed = now - animStartMs_` → `scaledElapsed(now - animStartMs_)`. Shimmer flash rate and total duration now track the Animation Speed slider.
- **`animH1` palette + speed** (`src/main.cpp`): `strip_.ColorHSV(i * 65536UL / count + offset)` on all three rings replaced with `paletteColor((uint8_t)(i * 256 / count + offset))`. `scaledElapsed` applied. Now responds to Palette and Speed sliders.
- **`animHr4` palette + speed** (`src/main.cpp`): three inner loops using `strip_.ColorHSV(hue)` replaced with `paletteColor((uint8_t)(p * 256 / maxP))`. `scaledElapsed` applied. Removed three unused `uint16_t hue` locals (-8 lines total). Now responds to Palette and Speed sliders.

### Files changed
- `src/main.cpp` — WebUi::setupRoutes (silent param), animQ3, animH1, animHr4
- `src/web_html.h` — previewAnim, previewStyleAnim, stylePreviewType select
- `platformio.ini` — FIRMWARE_VERSION 2.3.1 → 2.3.2
- `docs/symmap.json` — regenerated (animHr4 -8 lines; animHr5 through loop shifted)
- `docs/FUNCTION_INVENTORY.md` — regenerated

### Notes
- EEPROM compatible with v2.3.1 (no struct changes, SETTINGS_VERSION unchanged).
- Build: 8" env clean. RAM 11.1%, Flash 57.4%.
- Palette 0 (Rainbow/HSV) on animH1/animHr4 is visually equivalent to pre-patch behavior; all other palettes now apply.

---

## [v2.3.1] — 2026-05-17

### Fixed
- **`animQ1`** (`src/main.cpp`): `strip_.setBrightness(255)` hardcode replaced — peak and fade now scale to `settings.animationBrightness`.
- **`animQ2`** (`src/main.cpp`): `ringColor()` level argument was hardcoded `255`; now uses `settings.animationBrightness`.
- **`animQ3`** (`src/main.cpp`): flash brightness of `255` replaced with `settings.animationBrightness`; dim state preserved at `dayBrightness`.
- **`animH1`** (`src/main.cpp`): `ColorHSV` calls had no brightness arg (defaulted to 255); `strip_.setBrightness(animationBrightness)` added at entry.
- **`animH2`** (`src/main.cpp`): hardcoded full-saturation colors at 255; `strip_.setBrightness(animationBrightness)` added at entry.
- **`animH3`** (`src/main.cpp`): `setBrightness` called with `64` (dim) and `dayBrightness` (bright); both replaced with `animationBrightness/4` and `animationBrightness`.
- **`animHr1`–`animHr4`** (`src/main.cpp`): no `setBrightness` call; `strip_.setBrightness(animationBrightness)` added at entry in each.
- **`animHr5`** (`src/main.cpp`): breathing formula `50 + sinf() * 205` replaced with `br * (0.2 + 0.8 * sinf())` so peak equals `animationBrightness`.
- **`previewStyleAnim()` JS** (`src/web_html.h`): function was synchronous — triggered `/previewAnimation` before style settings were saved, so palette/brightness/speed changes had no effect on the preview. Now `async`; `await`s a `/settings` POST first.
- **"Preview with" → "Preview animation" dropdown** (`src/web_html.h`): old picker mapped to animation *types* (quarter/halfhour/hour), then read the currently-assigned mode for that type. If assigned mode was 0 (Off) the server rejected silently; if mode 1 (animQ1, 600ms) the user saw a brief flash. Replaced with a direct `type:mode` picker listing 11 palette-capable animations (Q4–Q6, H4–H7, Hr7, Hr9, Rem1–2) so preview always shows a meaningful palette demonstration independent of saved slot assignments.

### Files changed
- `src/main.cpp` — animQ1, animQ2, animQ3, animH1, animH2, animH3, animHr1, animHr2, animHr3, animHr4, animHr5
- `src/web_html.h` — previewStyleAnim() JS, stylePreviewType select options
- `platformio.ini` — FIRMWARE_VERSION 2.3.0 → 2.3.1

---

## [v2.3.0] — 2026-05-17

### Added
- **`ClockSettings.outerRingBrightness`** (new EEPROM field, uint8_t, 0–100%): per-ring brightness multiplier for the outer 60-LED ring, applied as integer scale before the pixel loop.
- **`renderFace()` outer ring scaling** (`src/main.cpp` lines 849–871): `orbScale` computed as `outerRingBrightness * 255 / 100`; `outerMarkerScaled` and `outerFillerScaled` replace raw `outerMarker`/`outerFiller` in pixel loop.
- **`SettingsStore::defaults()` new defaults** (`src/main.cpp`): `outerRingBrightness=90`; updated colors: outerMarker R/G/B 110/185/255 L210, outerFiller 0/8/200 L145, seconds 100/255/180 L230, minutes 255/100/0 L220.
- **Theme export/import panel** (`src/web_html.h` INDEX_P2): new "Color Theme" panel with Export and Import (file picker) buttons; `themeStatus` div for feedback.
- **Outer ring brightness input** (`src/web_html.h` INDEX_P2 Rings panel): number input `id='outerRingBrightness'` min=0 max=100, inserted after centerLevel ringrow.
- **`exportTheme()` JS** (`src/web_html.h` INDEX_P3): serializes all 6 ring colors, 6 levels, and `outerRingBrightness` to `chronobloom-theme.json`; triggers browser download.
- **`importTheme(file)` JS** (`src/web_html.h` INDEX_P3): reads JSON file, applies colors/levels/brightness to UI, calls `draw()` and `saveSettings()`.

### Changed
- **`SETTINGS_VERSION`** bumped 11 → 12. EEPROM auto-resets on first boot after flash.
- **`SettingsStore::valid()`**: added `&& settings.outerRingBrightness <= 100`.
- **`SettingsStore::sanitize()`**: added `if (settings.outerRingBrightness > 100) settings.outerRingBrightness = 90`.
- **`WebUi::settingsJson()`**: added `outerRingBrightness` to JSON format string and args; buf 1100→1150.
- **`WebUi::setupRoutes()` POST /settings**: added `outerRingBrightness` clampByte handler (0–100).
- **`loadSettings()` JS**: sets `outerRingBrightness` input from settings JSON.
- **`saveSettings()` JS**: includes `outerRingBrightness` in numeric POST fields array.
- **`platformio.ini`**: `FIRMWARE_VERSION` 2.2.2 → 2.3.0.

### Files changed
- `src/main.cpp` — struct, SETTINGS_VERSION, defaults, valid, sanitize, renderFace, settingsJson, setupRoutes
- `src/web_html.h` — outer ring brightness input, Color Theme panel, loadSettings, saveSettings, exportTheme, importTheme
- `platformio.ini` — FIRMWARE_VERSION

### Notes
- EEPROM incompatible with v2.2.x — auto-resets to new defaults on first boot.
- Build: 8" env clean. RAM 11.1%, Flash 57.1%.
- Theme JSON is client-side only; no new firmware endpoints required.

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
