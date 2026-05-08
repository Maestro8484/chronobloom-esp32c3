# ChronoBloom ESP32-C3 — Changelog

> Formerly neopixelClock-esp32c3-v3

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
git -c safe.directory=C:/Users/SuperMaster/Documents/PlatformIO/neopixelClock-esp32c3-v3 checkout -- platformio.ini src/main.cpp docs/CHANGELOG.md
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
git -c safe.directory=C:/Users/SuperMaster/Documents/PlatformIO/neopixelClock-esp32c3-v3 revert <commit-sha>
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
