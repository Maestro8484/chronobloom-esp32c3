# Iris Clock — Quick Session Primer

**Use this at the start of any Claude Chat or Claude Code session.**

---

## Project Identity
**Name**: Iris Clock  
**Repo**: `C:\Users\SuperMaster\Documents\PlatformIO\neopixelClock-esp32c3-v3`  
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
- ✅ Web UI (live SVG preview, per-ring controls, settings persist EEPROM)
- ✅ VEML7700 auto-brightness (manual/auto/scheduled modes)
- ✅ Time-interval animations (quarter/half/hour escalating intensity)
- ✅ NTP sync, WiFi, mDNS hostname
- ⬜ OTA updates (pending)
- ⬜ WiFi provisioning portal (pending)

## Known Issues
- **GPIO3/4 button conflict**: JTAG pins cause spurious ISR fires when USB connected
- **WiFi jitter**: Interrupt preemption during web requests causes LED flicker
- See [REVIEW.md](../REVIEW.md) for full technical audit

## Quick Pin Reference
```
GPIO10 → NeoPixel rings (300Ω resistor)
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
- WiFi: SSID `MAINFRAME007`, password `Secure!4`

## Documentation Map
- **[HARDWARE.md](HARDWARE.md)** — Pin maps, sensors, physical specs
- **[FEATURES.md](FEATURES.md)** — Current/planned/rejected features
- **[ANIMATIONS.md](ANIMATIONS.md)** — Animation catalog and triggers
- **[HISTORY.md](HISTORY.md)** — Project lineage (Steve/Mike/Maestro)
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** — Common issues, debugging
- **[API.md](API.md)** — Web endpoints, settings structure

## Session Anchor Phrase
Say **"Iris Clock workflow rules active"** to confirm context loaded.

---

**Total read time: 2 minutes. For details, see linked docs above.**
