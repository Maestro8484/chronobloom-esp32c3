# Claude Development Instructions

**This file is the session anchor. Read it first, every session, before touching anything else.**

---

## File-Reading Budget Rules

These rules are mandatory. Violating them wastes token budget on large files that are not needed.

### Never read these files unless the task explicitly names them
- `docs/SESSIONS.md` — session history only; if needed use `tail: 50` to get recent entries
- `REVIEW.md` — debugging archive; only read when diagnosing a specific named issue
- `docs/TROUBLESHOOTING.md` — only read when the task is fixing a known symptom
- `docs/CHANGELOG.md` and root `CHANGELOG.md` — only read when the task involves version history
- `WORKFLOW.md` — only read on first session or when workflow itself is the topic
- `README.md` — only read when build commands or hardware specs are the explicit task
- `docs/ANIMATIONS.md`, `docs/FEATURES.md`, `docs/HISTORY.md`, `docs/API.md` — only when directly relevant to the task

### Targeted reads only for large files
- `src/main.cpp` (~3100 lines) — NEVER read whole. Look up function in `docs/FUNCTION_INVENTORY.md`, get line range from `docs/symmap.json`, read only those lines with `head`/`tail` or offset+limit.
- `src/web_html.h` (~36KB) — read directly only when editing HTML/JS/CSS content. Never read to understand firmware logic.
- `docs/SESSIONS.md` — if needed, use `tail: 50`. Never read whole.

### Minimum read set by task type

| Task type | Read these files — nothing else unless needed |
|---|---|
| Firmware change | CLAUDE.md, FUNCTION_INVENTORY.md, symmap.json, targeted main.cpp lines |
| Docs update (inventory, primer, sessions) | CLAUDE.md + only the specific doc being updated |
| Web UI HTML/JS change | CLAUDE.md, web_html.h |
| New EEPROM setting | CLAUDE.md, FUNCTION_INVENTORY.md, symmap.json, targeted main.cpp lines |
| Debugging specific issue | CLAUDE.md, REVIEW.md (relevant section only), targeted main.cpp lines |
| Build/flash only | CLAUDE.md — commands are in this file |

### State what you read and why
Every session must open with a list of files read and the reason each was necessary. If a file is not on that list, it was not read.

---

## Documentation Index

### Always available here (no read needed)
Build commands, web UI URLs, project identity, and firmware file structure are in this file — do not read README.md or WORKFLOW.md just to find them.

### Read when needed
- **[docs/PRIMER-extended.md](docs/PRIMER-extended.md)** — Full current-state reference: feature list, pin map, all build commands, doc map
- **[WORKFLOW.md](WORKFLOW.md)** — Chat vs Code roles, source of truth hierarchy (read only when workflow is the topic)
- **[REVIEW.md](REVIEW.md)** — Technical audit, known issues, debugging notes (read only for specific named issues)

### Code navigation — read before touching main.cpp
- **[docs/FUNCTION_INVENTORY.md](docs/FUNCTION_INVENTORY.md)** — 139 functions: purpose, line range, state reads/writes
- **[docs/symmap.json](docs/symmap.json)** — Machine-readable `{ name, start_line, end_line }`, SHA256-pinned to source

### Detailed docs — read only when directly relevant
- **[docs/HARDWARE.md](docs/HARDWARE.md)** — Pin maps, sensors, physical specs
- **[docs/FEATURES.md](docs/FEATURES.md)** — Current/planned/rejected features, EEPROM schema
- **[docs/ANIMATIONS.md](docs/ANIMATIONS.md)** — Animation catalog, triggers, implementation guide
- **[docs/API.md](docs/API.md)** — Web endpoints, settings structure, client examples
- **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** — Common issues, diagnostic steps (read only when fixing a known symptom)
- **[docs/SESSIONS.md](docs/SESSIONS.md)** — Session history (tail: 50 only; never read whole)
- **[docs/SESSION_CLOSURE.md](docs/SESSION_CLOSURE.md)** — Mandatory end-of-session checklist

---

## Project Identity

**Name**: ChronoBloom ESP32-C3 (formerly neopixelClock-esp32c3-v3 / Iris Clock)
**Hardware**: XIAO ESP32-C3, 3 NeoPixel rings (60/24/12 LEDs), VEML7700 lux sensor
**Firmware**: `src/main.cpp` (~3100 lines) + `src/web_html.h` (~36KB PROGMEM HTML/JS)
**Visual**: Concentric LED rings, flower-like light bloom pattern
**Repo**: `C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`

---

## Build Commands

```powershell
pio run -e esp32c3_v3_8inch                                                          # Build
pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COM6  # USB flash
pio device monitor                                                                    # Serial 115200
```

OTA preferred post-first-flash: `http://esp32c3-v3-8inch.local/update`

---

## Web UI Access

- 8" variant: `http://esp32c3-v3-8inch.local/`
- 15" variant: `http://esp32c3-v3-15inch.local/`
- WiFi credentials: set via `/wifi` page or `platformio.ini` build flags (`WIFI_SSID` / `WIFI_PASSWORD`)

---

## Critical Workflow Rules

1. **Local repo is #1 truth source** — supersedes GitHub and all prior chat context
2. **File-reading budget rules above are mandatory** — state what you read and why
3. **Claude Chat** = planning, design, documentation only (unless explicitly requested otherwise)
4. **Claude Code** = ALL implementation, file edits, builds, flashing
5. **Never implement from Chat** when Code is available
6. **Session closure is mandatory** — run `docs/SESSION_CLOSURE.md` checklist before ending any session

---

## Session Anchor Phrase

**"Iris Clock workflow rules active"** — say at start of any new session to confirm context loaded.
