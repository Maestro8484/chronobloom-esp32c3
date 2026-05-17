# ChronoBloom ESP32-C3 — Claude Code Session Prompts

Maturation track session prompts in execution order.
Paste each into Claude Code using your autotext primer (replace TASK and FUNCTIONS/HARDWARE FOCUS fields).

---

## Status

| # | Session | Task | Status |
|---|---|---|---|
| 1 | Docs write | Write maturation content to REVIEW.md and FEATURES.md | Done |
| 1b | Symmap regenerate | Regenerate symmap.json and FUNCTION_INVENTORY.md | Done (2026-05-13) |
| 2 | OTA baseline + Tasks 2 and 3 | Verify WiFi.setAutoReconnect and ArduinoOTA.onError. USB flash. OTA test. | Done (2026-05-11) |
| 3 | Task 1 | Implement /diag endpoint | Done (2026-05-13) |
| 4 | Task 4 | Software watchdog in loop() | Done (2026-05-11) |
| 5 | Task 6 | Re-add physical buttons GPIO9/GPIO5 polled | Done (2026-05-13) |
| 6 | Task 5 | Button-hold factory reset on boot | Done (2026-05-13, v2.0.8) |
| 7 | Task 7 | Button hold-to-repeat + GPIO swap (UP=5, DOWN=9) | Done (2026-05-13, v2.0.7) |
| 8 | Docs close | Update maturation checklist to completed | Done (2026-05-13) |
| 9 | WiFi web UI | /wifi page, Preferences creds tier, reconnect poll | Done (2026-05-13, v2.0.9) |
| 10 | Docs audit fix | Fix 5 stale documentation entries from maturation audit | Done (2026-05-13) |
| 11 | WebUI crash fix + heap hardening | PROGMEM chunked HTML, snprintf JSON — eliminate 15inch OOM crash | Done (2026-05-13, v2.1.0) |
| 12 | Animation expansion | 17 new animations, 8-palette system, speed/brightness/trail controls, preview buttons, 12h UI | Done (2026-05-15, v2.1.0) |
| 13 | Publishing prep | LICENSE/NOTICE files, docs/publish/PUBLISH.md, docs/publish/DEMO_MODE.md spec | Done (2026-05-16) |
| 14 | Demo Mode firmware | DemoMode class, LuxSensor override, /demo endpoints, WebUI controls | Done (2026-05-16, v2.2.0) |
| 15 | Symmap regeneration | Fix gen_symmap.py parser bugs, regenerate inventory, update HARDWARE.md | Done (2026-05-16) |
| 16 | v2.1.1 firmware fixes | Ring rotation rounding, brightness floor, EEPROM wear fix, /diag expansion | Done (2026-05-16) |
| 17 | web_html.h modularization | Extract PROGMEM HTML/JS constants from main.cpp into src/web_html.h; update #include | Done (2026-05-16) |
| 18 | Docs: session closure system | PRIMER.md refresh, PRIMER-extended.md refresh, SESSION_CLOSURE.md created, SESSIONS.md updated | Done (2026-05-17) |
| 19 | Ring dimming + reset button | renderFace ambient fix, /diag extension, /settings/reset endpoint, web UI reset button | Done (2026-05-17, v2.2.1) |
| 20 | renderFace ambient fix + complexity audit | Flatten ambient scale to 50/50; write code complexity handoff for Claude Chat | Done (2026-05-17, v2.2.2) |
| 21 | outerRingBrightness + theme export/import | New EEPROM field for outer ring brightness, new color defaults, client-side theme JSON export/import | Done (2026-05-17, v2.3.0) |
| 22 | Animation brightness + preview fixes | Fix 11 legacy animations ignoring animationBrightness; fix Animation Style preview saving/dispatching | Done (2026-05-17, v2.3.1) |
| 23 | Preview/palette/speed fix (v2.3.2) | Fix preview not applying unsaved sliders, suppress settings-saved flash, add palette+speed to animH1/animHr4/animQ3 | Done (2026-05-17, v2.3.2) |

---

## Completed Sessions

### Session 23 — Preview/Palette/Speed Fix (Done 2026-05-17, v2.3.2)
- **Root issue 1 — `previewAnim()` ignored unsaved style sliders**: all three regular Preview buttons (Q/H/Hr/Reminder) called `previewAnim(type, modeId)` which POSTed only `type+mode` to `/previewAnimation` — never saved current `animationBrightness`, `animationPalette`, `animationSpeed`, `trailLength`, or `reminderPalette` first. If the user dragged the brightness slider to 50 but hadn't saved, the preview played at the last-saved value. Fixed: `previewAnim` is now `async`, silently POSTs all five style fields to `/settings?silent=1` before POSTing to `/previewAnimation`. Returns early if mode=0.
- **Root issue 2 — `previewStyleAnim()` caused a 1.3 s purple status flash**: the `/settings` POST inside it lacked `silent=1`, so `renderer_.setStatus(STATUS_SETTINGS_SAVED, 1300)` fired. This set a 1.3 s spinning-dot overlay which ran visually before the preview animation on the status render path. Fixed with `silent=1` param addition.
- **Root issue 3 — `previewStyleAnim()` previewed hardcoded demos, not the selected slot**: the `stylePreviewType` picker listed 11 hard-coded `type:mode` demo combos. When user's recurring slot was e.g. Q3 at mode 3, the style preview always showed Q4–Hr9. Now: `stylePreviewType` has 4 plain slot options (quarter/halfhour/hour/reminder); `previewStyleAnim()` reads the matching Q/H/Hr/Reminder dropdown value and fires that mode — same animation the device will show at the real interval.
- **`/settings` silent param** (`src/main.cpp` `WebUi::setupRoutes` ~line 2449): added `if (!server_.hasArg("silent"))` guard around `setStatus(STATUS_SETTINGS_SAVED, 1300)`. Settings update and EEPROM dirty-mark still happen; only the status animation is suppressed.
- **`animQ3` speed** (`src/main.cpp` line 1121): `elapsed = now - animStartMs_` → `scaledElapsed(now - animStartMs_)`. Shimmer flash period and total 2500 ms window now scale with Animation Speed (spd=1: 2× slower; spd=5: 2× faster).
- **`animH1` palette + speed** (`src/main.cpp` lines 1128–1153): all `strip_.ColorHSV(i * 65536UL / count + offset)` replaced with `paletteColor((uint8_t)(i * 256 / count + offset))` across outer (offset 0), middle (offset +85), inner (offset +170) rings. `scaledElapsed` applied. Palette 0 (HSV rainbow) is visually identical to pre-patch.
- **`animHr4` palette + speed** (`src/main.cpp` lines 1255–1283): three inner loops using incrementing `uint16_t hue + strip_.ColorHSV(hue)` replaced with `paletteColor((uint8_t)(p * 256 / maxP))` per ring. Three `uint16_t hue = 0` locals removed. `scaledElapsed` applied. Function shrank 8 lines (37 → 29); shifted all subsequent function line numbers.
- **Symmap regenerated**: 139 functions (count unchanged), line numbers from animHr5 onward shifted −8.
- **Build**: `esp32c3_v3_8inch` SUCCESS, RAM 11.1%, Flash 57.4%.

### Session 22 — Animation Brightness + Preview Fixes (Done 2026-05-17, v2.3.1)
- **Root issue 1 — legacy animations bypassed `animationBrightness`**: `animQ1`–`animQ3`, `animH1`–`animH3`, `animHr1`–`animHr5` (11 functions) all called `strip_.setBrightness(255)` or used `ColorHSV` with no brightness arg. The newer animations (Q4+, H4+, Hr6+) already used `setBrightness(animationBrightness)` correctly. Fixed each legacy function: Q1 scales fade formula to `animationBrightness`; Q2 passes `animationBrightness` to `ringColor()`; Q3 replaces `255` flash with `animationBrightness`; H1/H2 add `setBrightness(animationBrightness)` at entry; H3 replaces `64`/`dayBrightness` pair with `animationBrightness/4`/`animationBrightness`; Hr1–Hr4 add `setBrightness(animationBrightness)` at entry; Hr5 breathing formula `50 + sinf()*205` replaced with `br*(0.2+0.8*sinf())`.
- **Root issue 2 — `previewStyleAnim()` did not save settings before firing**: function was synchronous, posted to `/previewAnimation` immediately. The animation ran with the previously-saved style values, so any changes to palette/brightness/speed in the UI had no effect on preview. Fixed by making function `async` and `await`-ing a `/settings` POST before triggering the animation.
- **Root issue 3 — "Preview with" picker coupled to assigned animation slots**: old dropdown listed types (quarter/halfhour/hour); JS then read the assigned mode for that type. If mode was 0 (Off) the server rejected silently; if mode 1 (animQ1, 600ms) the user got a 600ms white flash. Replaced the entire `stylePreviewType` select with a direct `type:mode` picker listing 11 palette-capable animations by name and duration (Q4–Q6, H4–H7, Hr7, Hr9, Rem1–2). `previewStyleAnim()` now splits the `"type:mode"` value directly, no per-type dropdown lookup.
- **Symmap regenerated**: 139 functions (unchanged count), line numbers shifted due to `setBrightness` insertions in 9 functions.
- **Build**: `esp32c3_v3_8inch` SUCCESS, RAM 11.1%, Flash 57.4%.
- **Commit**: see git log.

### Session 21 — outerRingBrightness + Theme Export/Import (Done 2026-05-17, v2.3.0)
- **`ClockSettings` struct** (`src/main.cpp` line 390): added `uint8_t outerRingBrightness` after `reminderPalette`; EEPROM size grows by 1 byte.
- **`SETTINGS_VERSION`** bumped 11 → 12; EEPROM auto-resets on first boot. New color defaults applied in `defaults()`: outerMarker 110/185/255 L210, outerFiller 0/8/200 L145, seconds 100/255/180 L230, minutes 255/100/0 L220, outerRingBrightness=90.
- **`SettingsStore::valid()`** (`src/main.cpp`): added `&& settings.outerRingBrightness <= 100` to boolean chain.
- **`SettingsStore::sanitize()`** (`src/main.cpp`): added clamp `if > 100 → 90` after reminderPalette clamp.
- **`renderFace()`** (`src/main.cpp` lines 849–871): added `orbScale` integer computation and `outerMarkerScaled`/`outerFillerScaled`; pixel loop now uses scaled colors instead of raw.
- **`WebUi::settingsJson()`** (`src/main.cpp`): added `outerRingBrightness` to JSON; buf 1100→1150.
- **`WebUi::setupRoutes()` POST /settings** (`src/main.cpp`): added `outerRingBrightness` clampByte(0,100) handler.
- **Rings panel** (`src/web_html.h`): added `outerRingBrightness` number input (0–100) after centerLevel row.
- **Color Theme panel** (`src/web_html.h`): new panel with Export/Import buttons and `themeStatus` feedback div; inserted before Network panel.
- **`loadSettings()` JS** (`src/web_html.h`): added `outerRingBrightness` value assignment.
- **`saveSettings()` JS** (`src/web_html.h`): added `outerRingBrightness` to numeric POST fields array.
- **`exportTheme()` JS** (`src/web_html.h`): serializes 6 colors + 6 levels + outerRingBrightness to `chronobloom-theme.json`; blob download.
- **`importTheme(file)` JS** (`src/web_html.h`): reads JSON, applies fields to UI, calls draw+saveSettings.
- **Build**: both envs clean. RAM 11.1%, Flash 57.1%. Flashed 8inch via USB COM6.
- **Symmap**: regenerated — 139 functions, all line numbers shifted ~10–14 lines due to struct expansion.

### Session 20 — renderFace Ambient Fix + Complexity Audit (Done 2026-05-17, v2.2.2)
- **Root cause identified**: v2.2.1 renderFace fix used `hoursLevel/6` and `centerLevel/6` as ambient scales, but those two settings have different defaults (255 vs 180), producing unequal ambient brightness per ring despite identical divisors — same class of error as the original magic numbers it replaced
- **Fix**: `renderFace()` lines 840–843 — replaced both expressions with flat constant `50` (~20% of full). Two lines of code instead of four. No coupling to per-ring color settings.
- **Handoff document**: `docs/chat-handoff-code-complexity-audit.md` — seven audit targets identified for Claude Chat review: magic numbers, parallel structures, unintended dual-role settings, auto-brightness curve constants, confirmed JS/firmware mismatch bug in `draw()`, trail brightness array, setRingPixel rotation formula opacity
- **Confirmed bug flagged (not fixed this session)**: `draw()` JS in `web_html.h` still renders ambient at `22`/`24` — does not match firmware `50`/`50`; web UI preview is visually wrong
- **Build**: 8" env clean. RAM 11.1%, Flash 56.9%. Not yet flashed (device offline).
- **platformio.ini**: FIRMWARE_VERSION 2.2.1 → 2.2.2

### Session 19 — Ring Dimming Fix + Reset Button (Done 2026-05-17, v2.2.1)
- **Investigation**: diagnosed outer ring appearing much brighter than middle/inner rings. Root cause confirmed in `renderFace()` lines 840–843: ambient fill hardcoded at 22/255 (~8.6%) for middle ring and 24/255 (~9.4%) for inner ring, vs outer ring at 51–86%. Single `setBrightness()` scalar applies uniformly — no per-ring compensation exists.
- **Patch B — `renderFace()` ambient scale fix** (`src/main.cpp` lines 840–845): replaced hardcoded `22` and `24` with `max(1, hoursLevel/6)` and `max(1, centerLevel/6)`. With defaults, ambient lifts to ~42 (~16.5%) and ~30 (~11.8%). No new settings — user tunes via existing `hoursLevel`/`centerLevel` sliders.
- **Patch A — `/diag` extension** (`src/main.cpp` ~2337): added seven new fields (`effective_brightness`, `outer_marker_level`, `outer_filler_level`, `hours_level`, `center_level`, `middle_ambient_scale`, `inner_ambient_scale`). Buffer bumped 1024→1536 bytes. `effectiveBrightness()` is private to `ClockRenderer` so `br_effective` computed inline for mode 1; modes 0/2 fall back to `dayBrightness`.
- **`POST /settings/reset` endpoint** (`src/main.cpp` ~2430): wraps `SettingsStore::resetToDefaults()`, triggers `STATUS_SETTINGS_SAVED` LED flash, marks time dirty. Inserted between `/settings` POST handler and `/set` handler.
- **"Reset all to defaults" button** (`src/web_html.h`): red button (`color:#c0392b`) added alongside "Save display" in Display panel. `resetToDefaults()` JS function POSTs to `/settings/reset`, guarded by `confirm()`, reloads UI via `loadSettings()`.
- **Investigation plan written**: `docs/ring-dimming-patch-plan.md` — full root cause analysis, per-ring brightness comparison table, patch options B1/B2 with rollback steps.
- **Build**: 8" env clean, RAM 11.1%, Flash 56.9%. Firmware not flashed (device offline during session).
- **`platformio.ini`**: FIRMWARE_VERSION bumped 2.2.0 → 2.2.1.

### Session 18 — Docs: Session Closure System (Done 2026-05-17)
- **Root cause addressed**: Session 17 (web_html.h modularization) was undocumented — no SESSIONS.md entry, no CHANGELOG.md entry, no symmap regen note
- **SESSION_CLOSURE.md created**: mandatory end-of-session checklist covering SESSIONS.md update, CHANGELOG.md update, gen_symmap.py run, PRIMER-extended.md staleness check, and commit
- **PRIMER.md updated**: added session closure reminder block and closure warning to the paste-in primer template
- **PRIMER-extended.md fully refreshed**: updated feature state to v2.2.0, corrected pin reference (GPIO5/GPIO9 buttons, GPIO3/4 removed), corrected file structure (web_html.h, gen_symmap.py), updated function count and line estimate, updated build commands, updated all doc links
- **SESSIONS.md updated**: Session 17 row added, Session 18 row added, this narrative block added
- No firmware changes. No symmap regen required.

### Session 17 — web_html.h Modularization (Done 2026-05-16)
- All PROGMEM HTML/JS string constants extracted from WebUi::setupRoutes in main.cpp into src/web_html.h (~36KB, created 2026-05-16 21:08)
- main.cpp gained `#include "web_html.h"` at line 17
- setupRoutes now references INDEX_P1/P2/P3 and other PROGMEM symbols from the header rather than defining them inline
- symmap.json SHA256 is stale post-modularization (line numbers shifted); regen required before next firmware session
- No EEPROM changes, no SETTINGS_VERSION bump, no new functions added
- Session was not closed at time of completion — no SESSIONS.md entry, no CHANGELOG.md entry written then. Retroactively documented in Session 18.

### Session 16 — Firmware Fixes (Done 2026-05-16, v2.1.1)
- **setRingPixel rounding**: Added `+30` bias to rotation expression; middle/inner rings now track outerRingOffset proportionally instead of quantizing to 2.5/5 LED steps
- **Brightness floor**: `sanitize()` floors `dayBrightness < 5 → 44`, `nightBrightness < 1 → 5`; prevents zero-brightness UI lockout
- **FocusReminder EEPROM thrash**: Moved `lastFireMs` to private `uint32_t lastFireMs_ = 0` RAM member; eliminates ~20k EEPROM writes/year at 15-min interval
- **DemoMode::statusJson**: Migrated from `String` concatenation to `snprintf` into 256-byte stack buffer; output byte-identical
- **`/diag` expanded**: Replaced minimal v2.0.6 handler; now returns `uptime_sec`, `firmware_version`, `settings_version`, `time`, `ntp_synced`, `ntp_last_delta_sec`, `wifi_status/ssid/rssi/ip`, `lux`, `brightness_target`, `brightness_ramped`, `button_event_count`, `free_heap`, `clock_pixel_count`, `ring_pixel_offset`, `outer_ring_offset`, `sacrificial_enabled`
- Build: both envs clean. 8": RAM 11.1%, Flash 56.8%. 15": RAM 11.1%, Flash 56.8%
- Commit: `aac6e1c`

### Session 14 — Demo Mode Firmware (Done 2026-05-16, v2.2.0)
- DemoMode class implemented: array-driven step table with millis() timing, non-blocking
- LuxSensor extended: `setLuxOverride(float)` / `clearLuxOverride()` methods to bypass hardware reads
- 6-step demo sequence (~93s total): idle clock → chimes → palette cycling → focus reminder → brightness ramp → end card
- Step struct: `{ uint32_t duration_ms, const char* subtitle }`
- Web endpoints added:
  - `POST /demo/start` — start demo playback
  - `POST /demo/stop` — stop demo, clear status
  - `GET /demo/status` — JSON with `{ active, step, subtitle, elapsed_ms, step_duration_ms }`
  - `GET /demo/overlay` — Full-screen 1920x1080 OBS-ready HTML overlay with fade transitions
- Web UI controls: Start/Stop buttons, live status display (Step N/6, subtitle, progress %), demoStatus div
- JavaScript: `startDemo()`, `stopDemo()`, `updateDemoStatus()` with 500ms polling
- Buttons ignored during demo; web endpoints continue normal operation
- No EEPROM changes, no SETTINGS_VERSION bump, no build flag changes
- Build result: Both envs clean, RAM 11.1%, Flash 56.8%

### Session 15 — Symmap Regeneration + HARDWARE.md Update (Done 2026-05-16)
- **gen_symmap.py parser fixes**:
  - Improved CLASS_OPEN_RE regex to handle base class inheritance (e.g., `class Foo : public Bar, public Baz`)
  - Added hard-reset guard: clear class_stack when brace_depth returns to 0 (prevents misattribution after large function bodies)
  - Added SKIP_BODY_FUNCTIONS to skip braces inside setupRoutes (~1220 lines with embedded HTML/JS, +3 unclosed braces)
  - Pre-pass 2: marks lines inside SKIP_BODY_FUNCTIONS as skip_lines; main loop doesn't count their braces
- **Regenerated symmap.json and FUNCTION_INVENTORY.md**:
  - 110 functions found (was 102 with bugs)
  - All class methods now correctly qualified: `TimeSync::loop`, `DemoMode::loop`, `WebUi::loop` (appears once, not twice)
  - Global functions correctly unqualified: `setup`, `loop`, `logRuntimeStatus`, `setupDemoModeRoutes`
  - DemoMode::* methods all present (was missing entirely before)
- **HARDWARE.md updated for 8" variant**:
  - Corrected LED count: 98 LEDs (97 active + 1 sacrificial)
  - Sacrificial pixel at physical index 0 (level-shifts 3.3V to 5V)
  - Ring offset = 1 (logical LED 0 → physical index 1)
  - Center pixel at physical index 97 (last in chain)
  - Button pins: GPIO5 (UP), GPIO9 (DOWN) with polled, non-ISR implementation
  - Added factory reset sequence and hold-to-repeat notes
- **WORKFLOW.md updated**: Added gen_symmap reminder to "Token Reduction" section
- No firmware changes. Docs-only session. All files verified before commit.

### Session 13 — Publishing Prep (Done 2026-05-16)
- LICENSE (Apache 2.0) and LICENSE-HARDWARE (CC BY 4.0) created in repo root
- NOTICE file created with attribution chain (Steve Manley → Mike van der Sluis → Maestro)
- docs/publish/PUBLISH.md created: platform targets, task tracker, STL notes, 15" experimental declaration
- docs/publish/DEMO_MODE.md created: full Demo Mode spec — DemoMode class, step table, /demo/status JSON, /demo/overlay HTML overlay, LuxSensor override API, OBS setup, 93s sequence
- No firmware changes. SETTINGS_VERSION unchanged. Version remains v2.1.0.

### Session 12 — Animation Expansion (Done 2026-05-15, v2.1.0)
- Added `animationPalette` (8 palettes), `animationSpeed` (1-5), `animationBrightness` (50-255), `trailLength` (2-12), `reminderPalette` (4 warm palettes) to `ClockSettings`
- SETTINGS_VERSION bumped 10→11; all clocks reset defaults on first boot
- New `paletteColor(position, useReminderPalette)` and `scaledElapsed(elapsed)` helpers in `ClockRenderer`
- 3 new quarter animations: ANIM_Q4 (laser ping), ANIM_Q5 (DNA twist), ANIM_Q6 (tick spark)
- 4 new half-hour animations: ANIM_H4 (comet chase), ANIM_H5 (color explosion), ANIM_H6 (Knight Rider), ANIM_H7 (strobe party)
- 5 new hour animations: ANIM_HR6 (supernova), ANIM_HR7 (matrix rain), ANIM_HR8 (galaxy spin), ANIM_HR9 (color wipe), ANIM_HR10 (thunderstorm)
- 6 dedicated reminder animations: ANIM_REM1-REM6 (amber pulse, attention ring, heartbeat, sunrise wake, campfire flicker, neon sign)
- `triggerReminderDirectAnimation()` + `triggerAnimDirect()` added to `ClockRenderer`
- `FocusReminderScheduler::triggerReminderAnimation()` simplified to single-line delegate
- `/previewAnimation` POST endpoint fires any animation by type/mode without settings mutation
- Web UI: preview buttons on all animation selectors; Animation Style panel; 12-hour AM/PM display
- `FIRMWARE_VERSION` define added to `platformio.ini`; `/diag` gains `fw` field
- symmap regenerated: 102 functions (was 93)
- Build: both envs clean, RAM 11.1%, Flash 56.3%

### Session 11 — WebUI Crash Fix + Heap Hardening (Done 2026-05-13, v2.1.0)
- Root cause: `htmlPage()` allocated ~6KB String on heap; on 15inch (larger pixel buffer) this triggered OOM/watchdog reset when root page loaded.
- Fix 1: `htmlPage()` replaced with three `PROGMEM const char[]` chunks (`HTML_P1/P2/P3`) streamed via `setContentLength(CONTENT_LENGTH_UNKNOWN)` + `sendContent_P()`. Route handler returns immediately after flush.
- Fix 2: `/wifi` GET — PROGMEM chunks + two small `sendContent()` calls for dynamic SSID/status injection. Eliminated `.replace()` on heap String.
- Fix 3: `/update` GET — two PROGMEM chunks; fully static page.
- Fix 4: `settingsJson()` — snprintf into `char buf[900]` with inline hex color formatting. Zero heap allocs except final `String(buf)`.
- Fix 5: `/time`, `/net`, `/diag` JSON handlers — snprintf into stack `char buf[]`; payload passed directly to `server_.send()`.
- Build result: RAM 11.1% (36KB/320KB), Flash 55.4%. Clean compile, zero warnings.

### Session 10 — Docs Audit Fix (Done 2026-05-13)
- CHANGELOG [2.0.8]: corrected false entry claiming RING_PIXEL_OFFSET changed 1→2; offset confirmed 1 (offset 0 blocked by compile-time guard when SACRIFICIAL_PIXEL_ENABLED=1)
- FEATURES.md Settings Structure: bumped version 8→10; corrected focus reminder field names (focusEnabled → focusReminder_enabled, etc.) and types to match main.cpp; added outerRingOffset field; extended version history with v9/v10 entries
- FEATURES.md: removed stale "Diagnostic endpoint" Medium Priority planned entry (/diag shipped v2.0.6)
- README.md: replaced espota/192.168.1.110 OTA upload command with esptool/COM6 (matches platformio.ini since v2.0.8)
- ARCHITECTURE.md: focusReminder field names already matched main.cpp — no edit required



### Session 2 — OTA Baseline + Tasks 2 and 3 (Done 2026-05-11, v2.0.5)
- WiFi.setAutoReconnect(true) confirmed present in setupWiFi()
- ArduinoOTA.onError() handler added, calls ESP.restart()
- OTA test run: mid-transfer WiFi block left device hung in OTA wait state -- onError did NOT fire (silent TCP stall bypasses library error detection). Confirmed Task 4 (watchdog) is critical fix.
- OTA password removed. espota consolidated as default upload protocol for 8inch env.

### Session 4 — Software Watchdog (Done 2026-05-11, v2.0.5)
- esp_task_wdt_init(10, true) + esp_task_wdt_add(NULL) in setup()
- esp_task_wdt_reset() fed at top of loop(), in ArduinoOTA.onProgress, and in UPLOAD_FILE_WRITE handler
- Web UI /update OTA fixed: FormData streaming multipart (was raw XHR, OOM at ~40%)
- UPDATE_SIZE_UNKNOWN fix: skip too-large check when upload.totalSize == 0

### Session 7 — Hold-to-Repeat + GPIO Swap (Done 2026-05-13, v2.0.7)
- `ButtonInput` extended with hold-to-repeat state machine
- `HoldPhase` enum: IDLE / REPEAT_MIN / REPEAT_HOUR per button
- Hold >500ms: fires +1/-1 min every 150ms; hold >2000ms: fires +60/-60 min per fire
- `consumeUp()`/`consumeDown()` return int delta; `loop()` passes directly to `addMinutes()`
- GPIO swap: UP=GPIO5(D3), DOWN=GPIO9(D9) — both `platformio.ini` and `main.cpp` fallback defines
- Verified: short press, 1s hold, 3s hold, release all behave per spec

### Session 5 — Buttons Re-Added (Done 2026-05-13, v2.0.x)
- ButtonInput class re-added using polled reads -- no ISRs
- GPIO9 (D9) = UP, GPIO5 (D3) = DOWN
- GPIO8 ruled out: GPIO8=0 + GPIO9=0 simultaneously is invalid boot-strapping state
- 50ms debounce, poll() called from loop()
- Physical wiring required by user before OTA flash

### Session 1b + 3 — Symmap Regen + /diag Endpoint (Done 2026-05-13, v2.0.6)
- tools/gen_symmap.py created as canonical regeneration script
- symmap.json and FUNCTION_INVENTORY.md rebuilt: 93 functions (87 prior + ButtonInput x4 + 2 new constructors)
- TimeSync gains lastDeltaSec_ (delta computed in syncNow before model.set)
- g_buttonEventCount global incremented by ButtonInput::consumeUpPress/consumeDownPress
- GET /diag returns 10-field JSON: uptime, firmware_version, boot_reason, free_heap, wifi_ssid, wifi_rssi, wifi_ip, ntp_synced, ntp_last_delta, button_events
- Verified live: curl http://esp32c3-v3-8inch.local/diag returned all 10 fields post-OTA; boot_reason=software (OTA triggers SW reset)

---

## Session 1b — Regenerate Symmap

TASK: Regenerate docs/symmap.json and docs/FUNCTION_INVENTORY.md. symmap.json was last modified 2026-05-09, src/main.cpp has been modified through v2.0.5 -- symmap is stale by at least changelog entries 2.0.3, 2.0.4, 2.0.5. Use whatever generation method was used originally. If no script exists, grep src/main.cpp for all function signatures and rebuild from scratch. Commit when done.

FUNCTIONS/HARDWARE FOCUS: docs/symmap.json, docs/FUNCTION_INVENTORY.md, src/main.cpp

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 3 — Task 1: /diag Endpoint

TASK: OTA maturation Task 1. Implement /diag HTTP GET endpoint. Read the Maturation Goal section of REVIEW.md for the full field spec before touching main.cpp. Use FUNCTION_INVENTORY.md and symmap.json to locate the web server handler registration block. Minimal surface change -- new endpoint only. Build, flash via OTA, verify /diag returns expected JSON.

FUNCTIONS/HARDWARE FOCUS: web server handler registration block, loop()

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 6 — Task 5: Button-Hold Factory Reset

TASK: OTA maturation Task 5. Add button-hold factory reset on boot. Read the Maturation Goal section of REVIEW.md for full spec before touching main.cpp. Requires Task 6 (buttons on GPIO9/GPIO5) to already be wired and confirmed working -- verify this first, abort if not. Use FUNCTION_INVENTORY.md and symmap.json to locate setup(), SettingsStore, and ButtonInput. Build, flash via OTA, test by holding both buttons at power-on for 3 seconds.

FUNCTIONS/HARDWARE FOCUS: setup(), SettingsStore, ButtonInput

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 7 — Task 8: Button Hold-to-Repeat + No-WiFi Time Adjustment

TASK: Extend ButtonInput with hold-to-repeat so the physical buttons are a usable no-WiFi time adjustment tool. Read REVIEW.md Section 1 for the polling implementation template before touching main.cpp. Read FUNCTION_INVENTORY.md and symmap.json to locate ButtonInput, loop(), and the addMinutes() call sites. Verify current button behavior before changing anything.

Design spec (Occam minimal -- no modes, no LED feedback changes):
- Short press: existing behavior (+1/-1 minute via timeModel.addMinutes())
- Hold >500ms: begin auto-repeat at 150ms intervals (+1/-1 minute per fire)
- Hold >2000ms: switch repeat unit to 60 minutes per fire (hour jump)
- Release: stop repeat, reset hold state
- Implementation: add pressedAt (uint32_t) and lastRepeatAt (uint32_t) per button to ButtonInput. poll() tracks hold duration and fires repeat events. No ISRs. No mode state machine.

Requires Session 5 (polled ButtonInput on GPIO9/GPIO5) to be complete and wired.

Build, flash via OTA. Test: hold UP, confirm minutes increment continuously, then confirm hour jump engages after 2 seconds.

FUNCTIONS/HARDWARE FOCUS: ButtonInput, loop(), timeModel.addMinutes()

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 8 — Close Maturation Checklist

TASK: Update maturation checklist in REVIEW.md and FEATURES.md. Mark all completed tasks with completion dates from CHANGELOG. Add Task 8 (button hold-to-repeat) to checklist. Commit.

FUNCTIONS/HARDWARE FOCUS: None -- docs only.

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.
