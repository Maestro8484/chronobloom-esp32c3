# ChronoBloom — Demo Mode Specification

## Purpose
Demo Mode is a non-blocking firmware state machine that sequences through clock
features automatically for video recording. A browser overlay at /demo/overlay
renders live subtitles synced to firmware step state. OBS composites the overlay
over physical clock footage.

## Architecture

### Firmware (DemoMode class)
- Array-driven step table: { duration_ms, subtitle }
- millis() timing, non-blocking
- Runs alongside normal clock operation
- Endpoints: POST /demo/start, POST /demo/stop
- State endpoint: GET /demo/status

### /demo/status response (JSON)
```json
{
  "active": true,
  "step": 2,
  "subtitle": "Focus reminder — ADHD hyperfocus interrupt",
  "elapsed_ms": 4200,
  "step_duration_ms": 18000
}
```

### /demo/overlay
- Full-screen HTML page, no CDN dependencies
- OBS browser source, 1920x1080
- White text, semi-transparent black pill background
- Bottom third position, centered
- Font: system sans-serif, 48px
- Fade: 300ms fade-in on step start, 300ms fade-out on step end
- Polls /demo/status every 500ms

### LuxSensor override (Step 3)
- LuxSensor::setLuxOverride(float lux) — bypasses hardware read
- LuxSensor::clearLuxOverride() — restores hardware read
- DemoMode calls setLuxOverride() to simulate room darkening

## Demo Sequence

| Step | Duration | Subtitle | Firmware action |
|------|----------|----------|-----------------|
| 0 | 12s | ChronoBloom — ESP32-C3 NeoPixel clock | Idle clock, seconds trail visible |
| 1a | 10s | Chime animations — quarter, half, top-of-hour | Fire quarter, half, hour chime in sequence |
| 1b | 8s | 8 color palettes — fully customizable per ring | Advance palette every 2s, 4 palettes |
| 2 | 18s | Focus reminder — ADHD hyperfocus interrupt | Force FocusReminderScheduler fire |
| 3 | 25s | Auto-brightness — VEML7700 ambient light sensor | Ramp lux override 220->0.3->220 |
| 4 | 20s | Open source — github / printables / hackaday.io | Idle clock, end card subtitle |

Total: ~93 seconds

## Button Behavior During Demo
Buttons ignored. Normal web endpoints continue to function.

## EEPROM / Settings
No changes. No SETTINGS_VERSION bump. No new build flags.

## OBS Setup
Scene 1 (main): camera input + browser source at http://esp32c3-v3-15inch.local/demo/overlay
Scene 2 (web UI clip): browser source at http://esp32c3-v3-15inch.local/ fullscreen
Post: DaVinci Resolve free — trim, title card, end URL card only.

## Recording Notes
- Subtitles captured live from overlay during recording, no post-production subtitle work
- Web UI and button interaction filmed as separate supplemental clips
- Demo triggered via POST /demo/start from browser or curl before hitting record
