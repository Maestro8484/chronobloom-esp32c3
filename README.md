# ChronoBloom ESP32-C3

> Formerly `neopixelClock-esp32c3-v3`

PlatformIO firmware for the Seeed Studio XIAO ESP32-C3 version of the 3-ring NeoPixel clock. The ESP32-C3 platform enables OTA updates, a rich web UI, WiFi provisioning, I2C sensors, and Home Assistant integration without the RAM/flash constraints of earlier ESP8266 builds.

## Features

- 60 LED seconds ring with optional progress fill and fading trail
- 24 LED minutes ring with quarter-hour markers
- 12 LED hours/status ring
- Center status pixel (index 96 on 8" variant, separate strip on 15" variant)
- Wi-Fi web UI for time setting, browser sync, NTP sync, brightness, themes, and animation toggles
- Per-ring web controls for seconds, minutes, hours, and center LED color/intensity
- NTP timezone support for Mountain time with DST
- Saved display settings in EEPROM (v8)
- Night brightness schedule and VEML7700 lux-based auto-brightness
- Wi-Fi, button, settings, sync, idle, and hourly chime animations
- Time-interval animations: 3 quarter-hour, 3 half-hour, 5 top-of-hour variants
- WiFi provisioning portal (captive portal on first boot, AP fallback if STA unavailable)
- OTA firmware updates over WiFi (ArduinoOTA, port 3232)
- mDNS hostname with automatic reconnect after WiFi drop
- **Focus Reminders** — Visual nudge animations at configurable intervals for hyperfocus interruption (ADHD support)

## Development

See [WORKFLOW.md](WORKFLOW.md) for Claude Chat/Code role separation and development workflow rules.

See [REVIEW.md](REVIEW.md) for current technical issues and debugging notes.

## PlatformIO Environment

Build variants:
- `esp32c3_v3_8inch`: 8.5" clock variant (single NeoPixel strip, 98 LEDs total — 97 active + 1 sacrificial)
- `esp32c3_v3_15inch`: 15" clock variant (separate center pixel strip on GPIO20)

Build:

```powershell
pio run -e esp32c3_v3_8inch
```

Upload over USB:

```powershell
pio run -e esp32c3_v3_8inch -t upload
```

## Wi-Fi Setup

**First Boot (WiFi Provisioning Portal)**:
On first boot, if no saved WiFi credentials exist, the device opens a captive portal:
1. Your phone/computer will detect a WiFi network named `esp32c3-clock-setup` (no password required)
2. Connect to this network
3. A browser window will auto-open (or navigate to `http://192.168.4.1`)
4. Select your WiFi network from the list and enter the password
5. Device saves credentials to EEPROM and connects
6. Portal closes, device is ready to use

**Subsequent Boots**:
The device auto-connects to saved WiFi network. If credentials become invalid (password changed, network unavailable), the provisioning portal reappears on the next boot.

## Hostname & Network Access

After WiFi connection, the device advertises its hostname via mDNS. You can access the web UI at:
- `http://esp32c3-v3-8inch.local/` (mDNS hostname, recommended)
- `http://<device-ip>/` (IP address shown in router device list)

The hostname also appears correctly in your router's device list (not as a MAC address).

## OTA Firmware Updates

After initial USB flash, firmware updates can be deployed over WiFi without USB cable:

```powershell
# Build new firmware
pio run -e esp32c3_v3_8inch

# Upload via OTA (device must be on WiFi)
pio run -e esp32c3_v3_8inch -t upload --upload-port esp32c3-v3-8inch.local:3232
```

During OTA update:
- Inner ring shows blue status animation
- Serial monitor (if available) shows progress percentage
- Device reboots automatically when complete
- Inner ring shows green on success, red on failure

## LED Mapping (8" variant)

- Sacrificial pixel: physical index 0 (always dark — level-shifts data line to 5V logic)
- Seconds ring: physical indexes 1–60 (outer ring, 60 LEDs)
- Minutes ring: physical indexes 61–84 (middle ring, 24 LEDs)
- Hours ring: physical indexes 85–96 (inner ring, 12 LEDs)
- Center pixel: physical index 97 (8" variant) or separate strip index 0 (15" variant)

> `RING_PIXEL_OFFSET=1` — ring LED 0 (12 o'clock on the outer ring) maps to physical index 1, not 0. Physical index 0 is the sacrificial WS2812B.

## XIAO ESP32C3 Pin Assignments

```
XIAO Pin    GPIO    Function                Wire Color    Notes
────────────────────────────────────────────────────────────────
D10         GPIO10  NeoPixel data (rings)   -             Through 300Ω resistor
D7          GPIO20  NeoPixel data (center)  -             15" clock only
D1          GPIO3   (removed) Button UP     -             JTAG TCK -- do not use for buttons
D2          GPIO4   (removed) Button DOWN   -             JTAG TDI -- do not use for buttons
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
- GPIO3/GPIO4 are JTAG TCK/TDI pins. Physical buttons on these pins caused spurious ISR fires when USB connected. Buttons removed in v2.0.4. If re-adding buttons, use GPIO6-9 with polled reads. See REVIEW.md Section 1.

## Project History

**Original Design**: Steve Manley (2015-2016) — Arduino Nano + DS3234 RTC + 3D printed frame + UV glow-in-dark PLA

**Cost-Optimized Build**: Mike van der Sluis (2020-2021) — Modified STL files for Chinese NeoPixel ring clones, simplified firmware

**ChronoBloom / ESP32-C3 Modernization**: Maestro (2022-2026) — Multi-scale parametric design (85% and 200%), hybrid FDM + laser cutting, parchment paper diffuser, web UI + NTP, per-ring controls, OTA, Focus Reminders

Thanks to Steve Manley and Mike van der Sluis for the foundation that made this project possible.
