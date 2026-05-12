# ChronoBloom ESP32-C3 — Claude Code Session Prompts

Maturation track session prompts in execution order.
Paste each into Claude Code using your autotext primer (replace TASK and FUNCTIONS/HARDWARE FOCUS fields).

---

## Status

| # | Session | Task | Status |
|---|---|---|---|
| 1 | Docs write | Write maturation content to REVIEW.md and FEATURES.md | Done |
| 1b | Symmap regenerate | Regenerate symmap.json and FUNCTION_INVENTORY.md | Pending |
| 2 | OTA baseline + Tasks 2 and 3 | Verify WiFi.setAutoReconnect and ArduinoOTA.onError. USB flash. OTA test. | Pending |
| 3 | Task 1 | Implement /diag endpoint | Pending |
| 4 | Task 4 | Software watchdog in loop() | Pending |
| 5 | Task 6 | Re-add physical buttons GPIO8/GPIO9 polled | Pending |
| 6 | Task 5 | Button-hold factory reset on boot | Pending |
| 7 | Docs close | Update maturation checklist to completed | Pending |

---

## Session 1b — Regenerate Symmap

TASK: Regenerate docs/symmap.json and docs/FUNCTION_INVENTORY.md. symmap.json was last modified 2026-05-09, src/main.cpp was last modified 2026-05-10 -- symmap is stale by at least two changelog entries (2.0.3, 2.0.4). Use whatever generation method was used originally. If no script exists, grep src/main.cpp for all function signatures and rebuild from scratch. Commit when done.

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 2 — OTA Baseline + Tasks 2 and 3

TASK: OTA maturation Tasks 2 and 3. Read the Maturation Goal section of REVIEW.md for full task specs before touching main.cpp. Use FUNCTION_INVENTORY.md and symmap.json to locate setupWiFi() and the ArduinoOTA setup block -- verify symmap timestamps match main.cpp before trusting line ranges, grep if stale. Check for WiFi.setAutoReconnect(true) and ArduinoOTA.onError() handler. Report findings before changing anything. Add only what is missing. Build and flash via USB so OTA test can be run manually.

FUNCTIONS/HARDWARE FOCUS: setupWiFi(), ArduinoOTA setup block

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 4 — Task 4: Software Watchdog

TASK: OTA maturation Task 4. Add software watchdog feed in loop() with 10-second window. Read the Maturation Goal section of REVIEW.md for task spec. Use FUNCTION_INVENTORY.md and symmap.json to locate loop() and setup(). Verify hardware watchdog is not being suppressed before adding feed. Build, flash via OTA, confirm device recovers from simulated hang.

FUNCTIONS/HARDWARE FOCUS: loop(), setup()

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 5 — Task 6: Buttons Re-Added

TASK: OTA maturation Task 6. Re-add physical buttons using polled reads only -- no ISRs. Read the Maturation Goal section of REVIEW.md and REVIEW.md Section 1 for the polling implementation template before touching main.cpp. Read platformio.ini and HARDWARE.md to confirm GPIO8 and GPIO9 have no conflicts with existing pin assignments before writing any code. Report pin availability findings before implementing. Physical wiring by user required before OTA flash.

FUNCTIONS/HARDWARE FOCUS: ButtonInput class (removed 2.0.4), loop(), platformio.ini

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 6 — Task 5: Button-Hold Factory Reset

TASK: OTA maturation Task 5. Add button-hold factory reset on boot. Read the Maturation Goal section of REVIEW.md for full spec before touching main.cpp. Requires Task 6 (buttons on GPIO8/GPIO9) to already be wired and confirmed working. Use FUNCTION_INVENTORY.md and symmap.json to locate setup(), SettingsStore, and ButtonInput. Build, flash via OTA, test by holding buttons at power-on.

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.

---

## Session 7 — Close Maturation Checklist

TASK: Update maturation checklist in REVIEW.md and FEATURES.md. Mark all completed tasks with completion dates from CHANGELOG. Commit.

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
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (preferred — upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx

Read minimum files the task requires. State what you read and why.
