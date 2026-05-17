# ChronoBloom ESP32-C3 — Quick Session Primer
**Last updated: 2026-05-17 | Firmware: v2.2.0 (post-modularization)**

---

## Project Identity
**Name**: ChronoBloom ESP32-C3 (formerly neopixelClock-esp32c3-v3 / Iris Clock)
**Repo**: `C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`
**Hardware**: XIAO ESP32-C3, 3 NeoPixel rings (60/24/12 LEDs), VEML7700 lux sensor
**Interface**: Claude Desktop (Windows), VSCode PlatformIO, USB + OTA flashing
**Visual**: Concentric LED rings creating flower-like light bloom pattern

---

## Critical Workflow Rules
1. **Local repo is #1 truth source** — always check local files first via filesystem MCP
2. **Read CLAUDE.md before implementing** — session anchor, defines Chat vs Code roles
3. **Claude Chat** = planning, design, documentation only (unless explicitly requested otherwise)
4. **Claude Code** = ALL implementation, file edits, builds, flashing
5. **Never implement from Chat** when Code is available
6. **Session closure is mandatory** — see docs/SESSION_CLOSURE.md

---

## Firmware File Structure
```
src/
  main.cpp       — Single monolith firmware (~3100 lines as of v2.2.0)
  web_html.h     — PROGMEM HTML/JS constants (extracted from main.cpp, Session 17)
tools/
  gen_symmap.py  — Regenerates docs/symmap.json + docs/FUNCTION_INVENTORY.md
```
> Run gen_symmap.py after any session that adds, removes, or renames functions in main.cpp or web_html.h.

---

## Current Feature State (v2.2.0)
- Analog clock — 3 rings show time via LED position + color
- Web UI — live SVG preview, per-ring controls, settings persist EEPROM v11
- VEML7700 auto-brightness — manual/auto/scheduled modes, gain hysteresis, brightness ramping
- 17 time-interval animations — quarter/half/hour escalating intensity; 8-palette system
- 6 dedicated focus reminder animations
- Animation controls — palette (8), speed (1-5), brightness (50-255), trail length (2-12)
- NTP sync, WiFi, mDNS hostname (auto-reconnect handler)
- OTA firmware updates — ArduinoOTA port 3232 + web UI /update page
- WiFi provisioning portal — captive portal on first boot; AP fallback at 192.168.4.1
- WiFi credential update — /wifi page, Preferences tier, no reflash required
- Focus Reminders — visual nudge at configurable interval/days/hours (v2.0)
- /previewAnimation endpoint — fire any animation without settings mutation
- /diag endpoint — 16-field JSON diagnostic (uptime, heap, NTP delta, lux, brightness, etc.)
- Demo Mode — DemoMode class, 6-step 93s sequence, /demo/start|stop|status|overlay endpoints
- Physical buttons — GPIO5 (UP), GPIO9 (DOWN), polled non-ISR, hold-to-repeat, factory reset on boot hold
- Software watchdog — 10s window, auto-reboot on loop stall

---

## Quick Pin Reference (8" variant)
```
GPIO10  NeoPixel rings data    300 Ohm resistor; sacrificial pixel at physical 0; RING_PIXEL_OFFSET=1
GPIO20  Center pixel           15" variant only; separate strip
GPIO5   Button UP              INPUT_PULLUP, polled, D3
GPIO9   Button DOWN            INPUT_PULLUP, polled, D9
GPIO6   I2C SDA                VEML7700 (green wire)
GPIO7   I2C SCL                VEML7700 (yellow wire)
```
LED counts (8" active): 60 outer + 24 middle + 12 inner + 1 center = 97 active + 1 sacrificial = 98 total physical

---

## Build / Flash Commands
```powershell
pio run -e esp32c3_v3_8inch                                                # Build
pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COM6  # USB flash
pio device monitor                                                          # Serial 115200
```
OTA preferred post-first-flash: http://esp32c3-v3-8inch.local/update

---

## Web UI Access
- 8" variant: `http://esp32c3-v3-8inch.local/`
- 15" variant: `http://esp32c3-v3-15inch.local/`
- WiFi credentials: set via /wifi page or platformio.ini build flags (WIFI_SSID / WIFI_PASSWORD)

---

## Code Navigation — Read Before Touching main.cpp
- **[FUNCTION_INVENTORY.md](FUNCTION_INVENTORY.md)** — 110 functions: purpose, line range, state reads/writes
- **[symmap.json](symmap.json)** — machine-readable `{ name, start_line, end_line }`, SHA256-pinned

> main.cpp is ~3100 lines. Never read it whole. Look up function in FUNCTION_INVENTORY.md, get line range from symmap.json, read only those lines.
> web_html.h contains no functions — it is PROGMEM data only. Read directly if editing HTML/JS/CSS.

---

## Documentation Map
| File | Purpose |
|---|---|
| WORKFLOW.md | Chat vs Code roles, source of truth hierarchy |
| SESSION_CLOSURE.md | Mandatory end-of-session checklist |
| SESSIONS.md | Session history, prompts, completed session narratives |
| CHANGELOG.md | Version history and file change log |
| REVIEW.md | Technical audit, known issues, debugging notes |
| README.md | Build instructions, features, quick start |
| HARDWARE.md | Pin maps, sensors, physical specs |
| FEATURES.md | Current/planned/rejected features, EEPROM schema |
| ANIMATIONS.md | Animation catalog and triggers |
| API.md | Web endpoints, settings structure |
| TROUBLESHOOTING.md | Common issues, diagnostic steps |
| ARCHITECTURE.md | Focus Reminder subsystem design |
| AUDIT_2026-05-13.md | Maturation audit findings |
| publish/ | Publishing prep: PUBLISH.md, DEMO_MODE.md |

---

## Session Anchor Phrase
**"Iris Clock workflow rules active"** — say at start of any new session to confirm context loaded.

---

## Session Closure Reminder
Every session ends with docs/SESSION_CLOSURE.md. No exceptions.
