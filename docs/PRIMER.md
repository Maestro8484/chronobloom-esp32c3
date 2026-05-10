# ChronoBloom ESP32-C3 — Quick Session Primer

**Use this at the start of any Claude Chat or Claude Code session.**

---

## Project Identity
**Name**: ChronoBloom ESP32-C3 (formerly neopixelClock-esp32c3-v3)
**Repo**: `C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`  
**Hardware**: XIAO ESP32-C3, 3 NeoPixel rings (60/24/12 LEDs), VEML7700 lux sensor  
**Interface**: Claude Desktop (Windows), VSCode PlatformIO, USB flashing  
**Visual**: Concentric LED rings creating flower-like light bloom pattern

## Critical Workflow Rules
1. **Local repo is #1 truth source** — always check local files first
2. **Read WORKFLOW.md before implementing** — defines Chat vs Code roles
3. **Claude Chat** = planning, design, documentation only
4. **Claude Code** = ALL implementation, file edits, builds, flashing
5. **Never implement from Chat** when Code is available

## Current Feature State
- ✅ Analog clock (3 rings show time via LED position + color)
- ✅ Web UI (live SVG preview, per-ring controls, settings persist EEPROM v10)
- ✅ VEML7700 auto-brightness (manual/auto/scheduled modes)
- ✅ Time-interval animations (quarter/half/hour escalating intensity)
- ✅ NTP sync, WiFi, mDNS hostname (with reconnect handler)
- ✅ OTA firmware updates (ArduinoOTA, port 3232)
- ✅ WiFi provisioning portal (captive portal on first boot; AP fallback at 192.168.4.1)
- ✅ Focus Reminders — configurable visual nudge at set interval/days/hours (v2.0)

## Known Issues
- **GPIO3/4 button conflict**: JTAG pins cause spurious ISR fires when USB connected
- **WiFi jitter**: Interrupt preemption during web requests causes LED flicker
- See [REVIEW.md](../REVIEW.md) for full technical audit

## Quick Pin Reference
```
GPIO10 → NeoPixel rings (300Ω resistor; RING_PIXEL_OFFSET=1, sacrificial pixel at physical 0)
GPIO20 → Center pixel (15" variant only)
GPIO3  → Button UP (INPUT_PULLUP)
GPIO4  → Button DOWN (INPUT_PULLUP)
GPIO6  → I2C SDA (VEML7700, green wire)
GPIO7  → I2C SCL (VEML7700, yellow wire)
```

## Build Commands
```powershell
pio run -e esp32c3_v3_8inch           # Build 8" variant
pio run -e esp32c3_v3_8inch -t upload # Flash via USB
pio device monitor                     # Serial monitor
```

## Web UI Access
- 8" variant: `http://esp32c3-v3-8inch.local/`
- 15" variant: `http://esp32c3-v3-15inch.local/`
- WiFi: credentials set via `platformio.ini` build flags (`WIFI_SSID` / `WIFI_PASSWORD`)

## Documentation Map

### Code navigation — read these before touching main.cpp
- **[FUNCTION_INVENTORY.md](FUNCTION_INVENTORY.md)** — Every function: purpose, line range, state reads/writes, build-env and WebUI effects
- **[symmap.json](symmap.json)** — Machine-readable `{ name, start_line, end_line }` for all 85 functions (SHA256-pinned)

> `src/main.cpp` is ~2200 lines (~34k tokens). Never read it whole. Look up the function in `FUNCTION_INVENTORY.md`, get its line range from `symmap.json`, then `Read` with `offset`/`limit`.

### Reference
- **[HARDWARE.md](HARDWARE.md)** — Pin maps, sensors, physical specs
- **[FEATURES.md](FEATURES.md)** — Current/planned/rejected features
- **[ANIMATIONS.md](ANIMATIONS.md)** — Animation catalog and triggers
- **[HISTORY.md](HISTORY.md)** — Project lineage (Steve/Mike/Maestro)
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** — Common issues, debugging
- **[API.md](API.md)** — Web endpoints, settings structure

## Session Anchor Phrase
Say **"Iris Clock workflow rules active"** to confirm context loaded.

> Project publicly named **ChronoBloom ESP32-C3**. Local repo folder renamed to `chronobloom-esp32c3` (May 2026). Session anchor phrase unchanged.

---

**Total read time: 2 minutes. For details, see linked docs above.**
