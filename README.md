# NeoPixelClock v3 ESP32-C3

PlatformIO firmware for the Seeed Studio XIAO ESP32C3 version of the 3-ring NeoPixel clock. This repo is intentionally ESP32-C3-only so v3 can grow into heavier features such as OTA updates, richer web UI, Wi-Fi setup, sensors, and Home Assistant integration without being constrained by ESP8266 RAM/flash limits.

## Features

- 60 LED seconds ring with optional progress fill and fading trail
- 24 LED minutes ring with quarter-hour markers
- 12 LED hours/status ring
- Center status pixel at index 96
- Wi-Fi web UI for time setting, browser sync, NTP sync, brightness, themes, and animation toggles
- Per-ring web controls for seconds, minutes, hours, and center LED color/intensity
- NTP timezone support for Mountain time with DST
- Saved display settings in EEPROM emulation
- Night brightness schedule
- Wi-Fi, button, settings, sync, idle, and hourly chime animations
- I2C sensor support (VEML7700 ambient light sensor for auto-brightness)

## Development

See [WORKFLOW.md](WORKFLOW.md) for Claude Chat/Code role separation and development workflow rules.

See [REVIEW.md](REVIEW.md) for current technical issues and debugging notes.

## PlatformIO Environment

Build variants:
- `esp32c3_v3_8inch`: 8.5" clock variant (single NeoPixel strip, 98 LEDs total)
- `esp32c3_v3_15inch`: 15" clock variant (separate center pixel strip on GPIO20)

Build:

```powershell
pio run -e esp32c3_v3_8inch
```

Upload over USB:

```powershell
pio run -e esp32c3_v3_8inch -t upload
```

After connecting to Wi-Fi, the web UI should be reachable at `http://esp32c3-v3-8inch.local/` or the IP address shown by your router.

## LED Mapping

- Seconds ring: indexes 0-59 (outer ring)
- Minutes ring: indexes 60-83 (middle ring)
- Hours ring: indexes 84-95 (inner ring)
- Center pixel: index 96 (8" variant) or separate strip index 0 (15" variant)

## XIAO ESP32C3 Pin Assignments

```
XIAO Pin    GPIO    Function                Wire Color    Notes
────────────────────────────────────────────────────────────────
D10         GPIO10  NeoPixel data (rings)   -             Through 300Ω resistor
D7          GPIO20  NeoPixel data (center)  -             15" clock only
D1          GPIO3   Button up               -             INPUT_PULLUP, to GND
D2          GPIO4   Button down             -             INPUT_PULLUP, to GND
D4          GPIO6   I2C SDA                 Green         VEML7700 + future sensors
D5          GPIO7   I2C SCL                 Yellow        VEML7700 + future sensors
5V/VIN      -       LED power rail          Red           External 5V supply
GND         -       Common ground           Black         ESP32 + LED supply
3V3         -       Logic only              -             Do NOT power LEDs
────────────────────────────────────────────────────────────────
AVOID: GPIO2, GPIO8, GPIO9 (boot strapping pins)
```

## Hardware Notes

**VEML7700 Ambient Light Sensor** (optional):
- I2C address: 0x10
- Provides auto-brightness based on room lighting
- VCC: 3.3V, GND: common ground
- SDA/SCL: GPIO6/GPIO7 (green/yellow wires)

**Known Issues**:
- GPIO3/GPIO4 button pins conflict with JTAG on ESP32-C3, may cause spurious button presses when USB cable connected (see REVIEW.md for mitigation)

## Project History

**Original Design**: Steve Manley (2015-2016) — Arduino Nano + DS3234 RTC + 3D printed frame + UV glow-in-dark PLA

**Cost-Optimized Build**: Mike van der Sluis (2020-2021) — Modified STL files for Chinese NeoPixel ring clones, simplified firmware

**ESP32-C3 Modernization**: Maestro (2022-2026) — Multi-scale parametric design (85% and 200%), hybrid FDM + laser cutting, parchment paper diffuser, web UI + NTP, per-ring controls

Thanks to Steve Manley and Mike van der Sluis for the foundation that made this project possible.
