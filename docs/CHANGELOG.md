# ChronoBloom ESP32-C3 — Changelog

> Formerly neopixelClock-esp32c3-v3

## [2.4.1] - 2026-05-18

### Fixed
- 8-inch factory defaults now use the same software ring rotation that the physical clock requires: `DEFAULT_OUTER_RING_OFFSET=1`.
- Reset-to-defaults now restores `outerRingOffset` from the build-time default instead of hardcoding `0`.
- 8-inch LED geometry remains explicit and valid: physical pixel 0 is sacrificial, ring pixels are 1-96, center pixel is 97, total strip count is 98.

### Changed
- `/diag` now reports `default_outer_ring_offset` beside the live saved `outer_ring_offset`.
- Serial LED status now prints `defaultRot` beside the live software rotation.
- Project docs now use the ChronoBloom anchor phrase and no longer reference the retired project nickname.
- `FIRMWARE_VERSION` bumped `2.3.6` -> `2.4.1`.

### Functions modified
- `SettingsStore::defaults()` - uses `DEFAULT_OUTER_RING_OFFSET` for factory `outerRingOffset`.
- `SettingsStore::sanitize()` - falls back to `DEFAULT_OUTER_RING_OFFSET` for invalid persisted ring rotations.
- `WebUi::setupRoutes()` - adds `default_outer_ring_offset` to `/diag`.
- `logRuntimeStatus()` - prints default software rotation in the serial status line.

### Files changed
- `platformio.ini` - firmware version, 8-inch/15-inch default outer ring rotation build flags.
- `src/main.cpp` - build-time default rotation macro, validation, defaults, diagnostics.
- `AGENTS.md`, `CLAUDE.md`, `WORKFLOW.md` - ChronoBloom wording cleanup.
- `docs/symmap.json`, `docs/FUNCTION_INVENTORY.md` - regenerated.
- `docs/CHANGELOG.md`, `docs/SESSIONS.md`, `docs/SESSION_CLOSURE.md` - closure documentation.

---

## [2.4.0] - 2026-05-18

### Changed
- Replaced the previous 29 animation modes with a smaller set of 16 smoother, palette-aware animations: quarter x3, half-hour x3, hour x5, reminder x5.
- Tightened animation validation and sanitization limits to match the reduced mode counts.
- Updated reminder routing and animation phase names so removed animation modes are no longer exposed internally.
- Updated Web UI animation and focus reminder dropdown labels to match the new animation set.

### Functions modified
- `SettingsStore::valid()` - clamps animation mode maxima to the new supported mode counts.
- `SettingsStore::sanitize()` - resets removed animation modes to `0`.
- `ClockRenderer::triggerReminderDirectAnimation()` - maps reminder modes to the five remaining dedicated reminder animations.
- `ClockRenderer::animPhaseName()` - removes labels for deleted animation phases.
- `ClockRenderer::tickAnimation()` - dispatches only the retained animation functions.
- `ClockRenderer::animQ1()` through `ClockRenderer::animRem5()` - replaced with the new animation implementations.

### Files changed
- `src/main.cpp` - animation implementation overhaul and mode bounds.
- `src/web_html.h` - Web UI animation option labels and removed modes.
- `docs/symmap.json`, `docs/FUNCTION_INVENTORY.md` - regenerated for shifted function ranges and reduced function count.
- `docs/SESSIONS.md` - session closure entry.

---

## [2.1.0] — 2026-05-16 (docs)

### Documentation
- LICENSE (Apache 2.0) and LICENSE-HARDWARE (CC BY 4.0) added to repo root
- NOTICE file with attribution chain (Steve Manley → Mike van der Sluis → Maestro)
- docs/publish/PUBLISH.md: publishing tracker, platform targets, STL notes
- docs/publish/DEMO_MODE.md: Demo Mode firmware spec — step table, /demo/status, /demo/overlay, LuxSensor override API

---

## [2.1.0] — 2026-05-15

### Added
- Animation palette system: 8 color palettes (Rainbow, Fire, Ocean, Forest, Candy, Neon, Monochrome, Clock)
- Reminder palette system: 4 warm palettes (Amber, Red, Magenta, Cyan-warm) for focus reminder animations
- Animation speed control: 5-step scale (Dreamy slow → Hyperactive)
- Animation peak brightness control: 50-255 range, independent of clock brightness
- Trail length control: 2-12 LEDs for chase/sweep animations
- 3 new quarter-hour animations: Laser ping, DNA twist, Tick spark
- 4 new half-hour animations: Comet chase, Color explosion, Knight Rider, Strobe party
- 5 new hour animations: Supernova, Matrix rain, Galaxy spin, Color wipe, Thunderstorm
- 6 dedicated focus reminder animations: Amber pulse, Attention ring, Heartbeat, Sunrise wake, Campfire flicker, Neon sign
- Web UI: Preview button for each animation selector
- Web UI: Animation Style panel (palette, speed, brightness, trail length)
- Web UI: 12-hour time format with AM/PM in clock display
- `/previewAnimation` POST endpoint
- `/diag`: `fw` field showing firmware version string (2.1.0)

### Changed
- SETTINGS_VERSION bumped 10 → 11 (settings reset to defaults on first boot after update)
- `focusReminder_animation` modes 0-5 now labeled as animation delegates; modes 6-11 are dedicated reminder animations
- `triggerReminderAnimation()` routing consolidated into `ClockRenderer::triggerReminderDirectAnimation()`

---

## [2.1.0] - 2026-05-13

### Fixed
- **WebUI crash on 15inch variant** — `htmlPage()` (~6KB String on heap) replaced with three `PROGMEM const char[]` chunks streamed via `server_.setContentLength(CONTENT_LENGTH_UNKNOWN)` + `sendContent_P()`. No heap allocation for the page payload.
- **`/wifi` GET handler** — large inline HTML with `.replace()` converted to PROGMEM chunks + two small `sendContent()` calls for dynamic SSID/status values.
- **`/update` GET handler** — large inline HTML converted to two PROGMEM chunks.
- **`settingsJson()`** — ~30 `String +` concatenations (repeated heap reallocs) replaced with `snprintf` into `char buf[900]` + inline color hex formatting. Zero heap allocs except final `String(buf)` return.
- **`/time`, `/net`, `/diag` JSON handlers** — `String +` concatenation chains replaced with `snprintf` into stack `char buf[]` (128/256 bytes). Payload passed directly to `server_.send()` with no heap `String` object.

### Files changed
- `src/main.cpp` — `WebUi::setupRoutes()`, `WebUi::settingsJson()`, `WebUi::htmlPage()`
- `docs/symmap.json`, `docs/FUNCTION_INVENTORY.md` — regenerated

---

## [2.0.9] - 2026-05-13

### Added
- **`GET /wifi` — WiFi settings page** (`WebUi::setupRoutes`)
  - Dark-themed standalone page matching `/update` visual style
  - Shows current saved SSID and live WiFi status; password never echoed
  - SSID/password form POSTs to `/wifi`; JS handles response and reconnect feedback
- **`POST /wifi` — credential save + reconnect** (`WebUi::setupRoutes`)
  - Validates SSID length (1–32 chars); saves `ssid`/`pass` to Preferences `"wifi"` namespace
  - Responds 200, then `WiFi.disconnect()` + `WiFi.begin()` with new credentials
- **Preferences `"wifi"` tier in `setupWiFi()`** — priority 1 before build-time SSID
  - On forcePortal (factory reset): `"wifi"` namespace cleared before launching portal
- **30s non-blocking STA reconnect poll** in `WebUi::loop()` — skipped if `apMode_` is true
- **`WebUi::apMode() const`** accessor
- **WiFi Settings link** added to Admin panel in `htmlPage()`

### Files changed
- `src/main.cpp` — `setupWiFi()`, `WebUi::loop()`, `WebUi::apMode()`, `WebUi::setupRoutes()`, `WebUi::htmlPage()`
- `docs/symmap.json`, `docs/FUNCTION_INVENTORY.md` — regenerated (98 functions)

---

## [2.0.8] - 2026-05-13

### Added
- **Button-hold factory reset on boot** (`src/main.cpp`, `platformio.ini`)
  - Hold UP (GPIO5) at power-on → LEDs turn red; add DOWN (GPIO9) and hold both 3s → factory reset
  - GPIO5 used for initial detection (GPIO9 is XIAO BOOT pin — holding it at reset instant enters download mode)
  - `SettingsStore::resetToDefaults()`: writes zero to EEPROM magic byte, forces defaults on next boot
  - `Preferences "factory/portal"` flag persists across reboot; consumed in `setupWiFi()` to force WiFiManager portal
  - `setupWiFi()`: `WiFi.disconnect(false, true)` + `wm.resetSettings()` + `wm.startConfigPortal()` bypasses hardcoded credentials
  - Portal LED feedback: all LEDs turn blue when `esp32c3-clock-setup` SSID is broadcasting
  - Portal stays open indefinitely (`setConfigPortalTimeout(0)`) until credentials saved
  - `wm.setConnectRetries(1)` — reduces pre-portal wait from ~60s to ~8s on credential failure
  - `#include <Preferences.h>` added; forward declaration `extern Adafruit_NeoPixel ledStrip` added before `setupWiFi()`

### Changed
- **`platformio.ini`**: both envs now use `upload_protocol = esptool` / `upload_port = COM6`; removed espota from 8inch env
- **`platformio.ini`**: `RING_PIXEL_OFFSET` remains 1 — confirmed correct; offset 0 is invalid when `SACRIFICIAL_PIXEL_ENABLED=1`

### Files changed
- `src/main.cpp` — `SettingsStore::resetToDefaults()`, factory reset block in `setup()`, `setupWiFi()` portal logic + LED callback
- `platformio.ini` — upload protocol/port both envs; ring offset correction; sacrificial pixel confirmed

---

## [2.0.7] - 2026-05-13

### Added
- **`ButtonInput` hold-to-repeat** (`src/main.cpp`)
  - Short press: +1/-1 minute (unchanged)
  - Hold >500ms: auto-repeat at +1/-1 minute every 150ms
  - Hold >2000ms: switches to +60/-60 minute per fire (hour jump)
  - Release: stops repeat, resets all hold state
  - New state per button: `pressedAt`, `lastRepeatAt`, `holdPhase` (enum `IDLE`/`REPEAT_MIN`/`REPEAT_HOUR`)
  - New `consumeUp()`/`consumeDown()` return int delta; `loop()` passes delta to `addMinutes()`
  - No ISRs, no mode changes, no LED feedback changes

### Changed
- **GPIO assignments swapped** (`platformio.ini`, `src/main.cpp` fallback defines)
  - `BUTTON_UP_PIN`: 9 → 5 (GPIO5 / D3)
  - `BUTTON_DOWN_PIN`: 5 → 9 (GPIO9 / D9)

### Files changed
- `src/main.cpp` — `ButtonInput` class (hold-to-repeat state machine, `consumeUp`/`consumeDown`), fallback `#define` for `BUTTON_DOWN_PIN` fixed (was 5, now 9)
- `platformio.ini` — `BUTTON_UP_PIN`/`BUTTON_DOWN_PIN` values swapped

---

## [2.0.6] - 2026-05-13

### Added
- **`GET /diag` diagnostic endpoint** (`src/main.cpp`)
  - Returns JSON with 10 fields: `uptime`, `firmware_version`, `boot_reason`, `free_heap`, `wifi_ssid`, `wifi_rssi`, `wifi_ip`, `ntp_synced`, `ntp_last_delta`, `button_events`
  - `TimeSync::lastDeltaSec_`: tracks seconds delta between model time and NTP time at each sync
  - `g_buttonEventCount`: global counter incremented on every consumed button press (up or down)
- **`tools/gen_symmap.py`** — canonical script to regenerate `docs/symmap.json` and `docs/FUNCTION_INVENTORY.md` from `src/main.cpp`

### Changed
- **`docs/symmap.json` + `docs/FUNCTION_INVENTORY.md`** regenerated: 93 functions (was 87); ButtonInput class (4 methods) + 2 constructors newly tracked; all line numbers updated for S5 button re-add shift

### Files changed
- `src/main.cpp` — `/diag` route, `TimeSync::lastDeltaSec_`, `g_buttonEventCount`
- `tools/gen_symmap.py` — new file
- `docs/symmap.json`, `docs/FUNCTION_INVENTORY.md` — regenerated

---

## [2.0.5] - 2026-05-11

### Added
- **Software watchdog** (`src/main.cpp`)
  - `esp_task_wdt_init(10, true)` + `esp_task_wdt_add(NULL)` in `setup()` — 10-second window, reboot on timeout
  - `esp_task_wdt_reset()` at top of `loop()`, in `ArduinoOTA.onProgress`, and in `UPLOAD_FILE_WRITE` handler
  - Ensures device recovers automatically from OTA hang, I2C block, or WiFi manager stall
- **Web UI firmware update (`/update` page)** (`src/main.cpp`)
  - Fixed raw-binary XHR upload OOM crash at ~40% (switched to `FormData` for streaming multipart)
  - Fixed `Update.begin(0)` failure with FormData by using `UPDATE_SIZE_UNKNOWN`
  - Guard: skip too-large check when `upload.totalSize == 0` (FormData reports 0 at `UPLOAD_FILE_START`)

### Changed
- **OTA password removed** — `ArduinoOTA.setPassword("iris_ota_2026")` removed; no auth required
- **`platformio.ini`** — consolidated `esp32c3_v3_8inch_ota` env into `esp32c3_v3_8inch`; `upload_protocol = espota` and `upload_port = 192.168.1.110` now default for 8" variant

### Files changed
- `src/main.cpp` — watchdog init/feed, FormData fix, UPDATE_SIZE_UNKNOWN, OTA password removed
- `platformio.ini` — espota as default protocol, removed separate OTA env
- `docs/PRIMER.md`, `docs/SESSIONS.md`, `docs/TROUBLESHOOTING.md`, `README.md` — OTA command references updated

---

## [2.0.4] - 2026-05-10

### Removed
- **Physical buttons removed (both variants)**
  - GPIO3 (Button UP) and GPIO4 (Button DOWN) are no longer wired or used
  - Removed from hardware on both 8" and 15" variants
  - All manual time adjustment now handled exclusively via WebUI

### Reason for removal
  - GPIO3 and GPIO4 are JTAG TCK/TDI pins on the XIAO ESP32-C3
  - USB data connection caused spurious ISR fires on both pins, triggering
    unintended addMinutes(-1) calls and backward minute-hand jumps
  - Buttons were not essential given full WebUI time control
  - Removal eliminates the highest-confidence known input-corruption path
    documented in REVIEW.md

### Re-addition note
  - Buttons may be reintroduced in a future version
  - If re-added, implementation MUST use polled reads (not ISRs) and MUST
    avoid GPIO3/GPIO4 -- use GPIO6, GPIO7, GPIO8, or GPIO9 instead
  - See REVIEW.md Section 1 for full technical rationale and the polling
    implementation template

### Files changed
- `platformio.ini` -- removed BUTTON_UP_PIN and BUTTON_DOWN_PIN build flags
- `src/main.cpp` -- removed ButtonInput class, button poll/consume calls, button status animation
- `docs/HARDWARE.md` -- removed button pin rows from pin table, added removal note
- `docs/FEATURES.md` -- moved physical buttons to Removed Features section
- `README.md` -- removed button references from hardware notes and pin table

## [2.0.3] - 2026-05-09

### Changed
- **`renderHours`: thirds-based hour ring progression** (`src/main.cpp`)
  - Middle ring (24 LED) now advances in three steps across each hour:
    - `:00–:19` → single pixel at exact hour position (clean top-of-hour anchor)
    - `:20–:39` → two-pixel straddle (mid-transit between positions)
    - `:40–:59` → single pixel at advanced position (approaching next hour)
  - Previously used a binary half-hour offset (`minute >= 30 ? +1 : 0`), which left `:15` visually ambiguous (straddling when it should have been a clean anchor)
  - Inner ring (12 LED) unchanged in behaviour but clarified: single pixel at `:00–:29`, two-pixel straddle at `:30–:59`

### Files changed
- `src/main.cpp` — `ClockRenderer::renderHours` only

---

## [2.0.2] - 2026-05-08

### Fixed
- **Sacrificial LED removed (both variants)**
  - `RING_PIXEL_OFFSET` set to 0 on both 8" and 15" (rings now start at physical index 0)
  - 8": `CLOCK_PIXEL_COUNT` 98→97, `CENTER_PIXEL_INDEX` 97→96
  - 15": `CLOCK_PIXEL_COUNT` 97→96
  - `SACRIFICIAL_PIXEL_ENABLED=0` on both variants; `SACRIFICIAL_PIXEL_INDEX` removed
  - **Hardware required:** rewire GPIO10 data line directly to DIN of first ring LED

- **15" ring LED (GPIO10) failure**
  - Root cause: dead sacrificial WS2812B at chain position 0 blocked all signal to rings
  - Resolved by above pixel offset/count changes + hardware rework

- **WiFi: AP fallback when STA unavailable**
  - Added `wm.setConfigPortalTimeout(120)` — portal releases after 2 min instead of blocking forever
  - If all STA attempts fail, device starts `WiFi.softAP(DEVICE_HOSTNAME)` and runs web server at `192.168.4.1`
  - Clock, web UI, and settings all functional in AP mode; NTP/mDNS/OTA skipped until STA available

- **mDNS hostname lost after WiFi reconnect**
  - Added `WiFi.onEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP)` handler to re-run `MDNS.begin()` on reconnect

- **FocusReminder day-of-week always fired as Sunday**
  - `getDayOfWeek()` now uses `time(nullptr)` + `localtime_r()` for actual NTP weekday
  - Graceful fallback to Sunday (0) before first NTP sync

### Files changed
- `platformio.ini` — pixel counts, offsets, sacrificial flags
- `src/main.cpp` — WiFi AP fallback, portal timeout, mDNS reconnect handler, DOW fix

---

## [2.0.1] - 2026-05-08

### Changed
- `platformio.ini`
  - Changed `default_envs = esp32c3_v3_8inch` to `default_envs = esp32c3_v3_15inch` so plain `pio run` / VS Code default tasks target the installed 15-inch clock variant.
  - Added `-D ARDUINO_USB_MODE=1` to the shared ESP32-C3 build flags so the native USB interface is configured for USB CDC operation.
  - Added `-D ARDUINO_USB_CDC_ON_BOOT=1` to the shared ESP32-C3 build flags so `Serial` output appears on the Windows COM port after boot.
- `src/main.cpp`
  - Replaced WiFiManager-only `setupWiFi()` behavior with a build-time credential first path.
  - Added a guard that skips build-time credential mode only when `WIFI_SSID` is still the placeholder `clock-ssid`.
  - Added serial logging before the build-time connection attempt: `[WiFi] Trying build-time SSID: ...`.
  - Added `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)` so credentials from `platformio.ini` are actually used.
  - Added a timed connection loop using `WIFI_CONNECT_TIMEOUT_MS`.
  - Added progress dot output every 500 ms during the build-time Wi-Fi attempt.
  - Added a newline after the Wi-Fi connection attempt finishes.
  - Added success handling that logs `[WiFi] Connected with build-time credentials` and returns `true`.
  - Added failure handling that logs the Wi-Fi status and then falls back to the existing `esp32c3-clock-setup` WiFiManager portal.
  - Added `WiFi.disconnect(false)` and a short delay before starting the fallback portal.
  - Kept the original `wm.autoConnect("esp32c3-clock-setup", "")` path as fallback behavior.
  - Added global `lastStatusLogMs` state for periodic runtime serial logging.
  - Added `logRuntimeStatus(uint32_t now)`, which prints uptime, Wi-Fi status code, hostname, connected SSID, IP address, RSSI, and free heap every 10 seconds.
  - Added `logRuntimeStatus(now)` to the main loop after `timeSync.loop()` and `webUi.loop()` so serial status is available even if the monitor attaches after boot.
  - Removed trailing blank lines at the end of `src/main.cpp`.

### Verified
- Flashed `esp32c3_v3_15inch` over USB on `COM9`.
- Confirmed the device connects to configured SSID from `platformio.ini`.
- Confirmed mDNS resolves `esp32c3-v3-15inch.local` to device IP.
- Confirmed `http://esp32c3-v3-15inch.local/` returns HTTP 200.
- Confirmed `/net` reports correct SSID, `status=3`, and expected RSSI range.
- Confirmed serial monitor on `COM9` at `115200` prints runtime status lines such as:
  ```text
  [Status] uptime=10s wifi=3 host=esp32c3-v3-15inch ssid=<YOUR_SSID> ip=192.168.1.x rssi=-73 heap=242972
  ```

### Restore-Back Instructions

Use one of these restore paths depending on whether the changes have been committed.

#### Restore with Git Before Commit
From the repo root:

```powershell
git -c safe.directory=C:/Users/SuperMaster/Documents/PlatformIO/chronobloom-esp32c3 checkout -- platformio.ini src/main.cpp docs/CHANGELOG.md
```

This removes the 15-inch default env change, USB CDC serial flags, build-time Wi-Fi credential path, periodic status heartbeat, and this changelog entry.

#### Restore Manually
1. In `platformio.ini`, change:
   ```ini
   default_envs = esp32c3_v3_15inch
   ```
   back to:
   ```ini
   default_envs = esp32c3_v3_8inch
   ```
2. In `platformio.ini`, remove these two lines from `[common] build_flags`:
   ```ini
   -D ARDUINO_USB_MODE=1
   -D ARDUINO_USB_CDC_ON_BOOT=1
   ```
3. In `src/main.cpp`, replace the expanded `setupWiFi()` body with the previous WiFiManager-only implementation:
   ```cpp
   bool setupWiFi() {
     WiFiManager wm;
     wm.setConnectRetries(3);
     bool connected = wm.autoConnect("esp32c3-clock-setup", "");
     return connected;
   }
   ```
4. In `src/main.cpp`, remove:
   ```cpp
   uint32_t lastStatusLogMs = 0;
   ```
5. In `src/main.cpp`, remove the whole `logRuntimeStatus(uint32_t now)` function.
6. In `src/main.cpp`, remove this call from `loop()`:
   ```cpp
   logRuntimeStatus(now);
   ```
7. In `docs/CHANGELOG.md`, remove this entire `2.0.1` section.
8. Rebuild the desired variant:
   ```powershell
   pio run -e esp32c3_v3_8inch
   pio run -e esp32c3_v3_15inch
   ```

#### Restore After Commit
If these changes have already been committed, revert that commit instead of editing files by hand:

```powershell
git -c safe.directory=C:/Users/SuperMaster/Documents/PlatformIO/chronobloom-esp32c3 revert <commit-sha>
```

Then rebuild and flash the desired environment.

## [2.0.0] - 2026-05-06

### Added
- **Focus Reminders (ADHD MVP)** - Visual nudge system for hyperfocus interruption
  - Single configurable reminder rule with enable/disable toggle
  - Configurable active hours window (start/end hour)
  - Repeat interval in minutes (1-1440 min)
  - Days-of-week selector (Sun-Sat bitmask)
  - Reuses existing animation system (Quarter Pulse, Half-Hour Sweep, Hour Chime)
  - Persistent storage via EEPROM (16 bytes added to ClockSettings)
  - WebUI panel: "Focus Reminders (ADHD)" with form controls
  - Serial logging of reminder fires for debugging

### Changed
- **SETTINGS_VERSION bumped 7 → 8**
  - Old v7 settings auto-reset to v8 defaults on first boot (backward compatible)
  - `ClockSettings` struct extended by 16 bytes (13 used + 3 reserved for v2)
  - Added `FocusReminderScheduler` class to firmware main loop

### Technical Details
- **EEPROM footprint:** +16 bytes (total 256 bytes, ~60 bytes headroom remaining)
- **Code footprint:** ~300 lines added (FocusReminderScheduler class, WebUI panel, helpers)
- **Loop performance:** Reminder check runs in < 1ms, non-blocking
- **Files modified:** `src/main.cpp` (only file changed)

### Known Limitations (v1)
- Day-of-week calculation hardcoded to Sunday (0) - placeholder pending NTP weekday integration
- No test-now button (manual time adjustment required to validate)
- No animation queueing (reminder can overlap with status/chime animations)
- No quiet-mode or sleep-mode exemption logic
- Single reminder rule only (multi-reminder planned for v2.1)

### Future Roadmap (v2.1+)
- Multiple reminder rules (3-5 concurrent reminders)
- Day-of-week auto-calculation from NTP system time
- Test-now button on WebUI
- Animation queue to prevent overlap
- Quiet/sleep mode exemption flag per reminder
- Custom reminder labels/names
- Optional soft-sound/buzzer integration (for non-visual users)

### Migration Guide (v1.x → v2.0)
1. Upload new firmware (v2.0 binary)
2. Device boots, EEPROM auto-resets to v8 defaults
3. Old display/animation settings preserved
4. New "Focus Reminders (ADHD)" panel appears on WebUI
5. Configure reminder as desired; click "Save reminder"
6. Test by setting time to window + interval, verify animation fires
