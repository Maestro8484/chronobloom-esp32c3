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

---

## Completed Sessions

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
