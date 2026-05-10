# Codex Development Instructions

**This file acts as a documentation router. For quick session start, see [docs/PRIMER.md](docs/PRIMER.md).**

---

## Documentation Index

### Quick Start
- **[docs/PRIMER.md](docs/PRIMER.md)** — Quick session primer (read this first, 2-min read time)

### Code Navigation (use these before reading main.cpp)
- **[docs/FUNCTION_INVENTORY.md](docs/FUNCTION_INVENTORY.md)** — Every function: purpose, line range, state reads/writes, build-env and WebUI effects
- **[docs/symmap.json](docs/symmap.json)** — Machine-readable symbol map: `{ name, start_line, end_line }` for all 85 functions; SHA256-pinned to source

**Token-reduction rule**: `src/main.cpp` is ~2200 lines (~34k tokens). Never read it whole. Use `FUNCTION_INVENTORY.md` to find the relevant function, get its line range from `symmap.json`, then `Read` with `offset`/`limit` targeting only those lines.

### Reference Documentation
- **[WORKFLOW.md](WORKFLOW.md)** — Development workflow rules (Codex Chat vs Code, source of truth hierarchy)
- **[README.md](README.md)** — Build instructions, features, quick start
- **[REVIEW.md](REVIEW.md)** — Technical audit, known issues, debugging notes

### Detailed Documentation (docs/)
- **[docs/HARDWARE.md](docs/HARDWARE.md)** — Pin maps, sensors, physical construction, power specs
- **[docs/FEATURES.md](docs/FEATURES.md)** — Current/planned/rejected features, settings structure
- **[docs/ANIMATIONS.md](docs/ANIMATIONS.md)** — Animation catalog, triggers, implementation guide
- **[docs/HISTORY.md](docs/HISTORY.md)** — Project lineage (Steve Manley → Mike van der Sluis → Maestro)
- **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** — Common issues, diagnostic steps, solutions
- **[docs/API.md](docs/API.md)** — Web endpoints, settings structure, client examples

---

## Critical Workflow Rules

1. **Local repo is #1 truth source** — `C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`
2. **Read WORKFLOW.md before implementing** — Defines Chat vs Code roles
3. **Codex Chat** = planning, design, documentation only
4. **Codex** = ALL implementation, file edits, builds, flashing
5. **Never implement from Chat** when Code is available

---

## Session Anchor Phrase

Say **"Iris Clock workflow rules active"** at the start of any session to confirm you've loaded the project context.

---

## Project Identity

**Name**: ChronoBloom ESP32-C3 (formerly neopixelClock-esp32c3-v3 / Iris Clock)
**Hardware**: XIAO ESP32-C3, 3 NeoPixel rings (60/24/12 LEDs), VEML7700 lux sensor  
**Visual**: Concentric LED rings creating flower-like light bloom pattern  
**Purpose**: Elegant analog timekeeping through addressable LEDs

---

## Build Commands

```powershell
# Build 8" variant
pio run -e esp32c3_v3_8inch

# Flash via USB
pio run -e esp32c3_v3_8inch -t upload

# Serial monitor
pio device monitor
```

---

## Web UI Access

- 8" variant: `http://esp32c3-v3-8inch.local/`
- 15" variant: `http://esp32c3-v3-15inch.local/`
- WiFi: SSID and password set via `platformio.ini` build flags (`WIFI_SSID` / `WIFI_PASSWORD`)

---

**For detailed information, see the linked documentation above.**
