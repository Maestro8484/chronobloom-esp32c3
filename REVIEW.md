# ChronoBloom ESP32-C3 — Firmware Review
Date: 2026-05-04 (updated 2026-05-15)

---

## Session 12 — 2026-05-15 Animation Expansion (v2.1.0)

Added 17 new animations, a palette system, and animation style controls.

**New ClockSettings fields** (SETTINGS_VERSION 10→11):
- `animationPalette` (0-7), `animationSpeed` (1-5), `animationBrightness` (50-255), `trailLength` (2-12), `reminderPalette` (0-3)

**New helpers** in `ClockRenderer`: `paletteColor()` (palette-mapped color), `scaledElapsed()` (speed-scaled timing).

**New AnimPhase values**: ANIM_Q4-Q6 (quarter), ANIM_H4-H7 (half), ANIM_HR6-HR10 (hour), ANIM_REM1-REM6 (reminder — warm palette, distinct from time animations).

**New endpoint**: `POST /previewAnimation?type=quarter|halfhour|hour|reminder&mode=N` — fires animation directly without touching settings.

**Web UI**: Preview button on every animation selector + style preview type picker; Animation Style panel (palette, speed, brightness, trail); 12-hour AM/PM time display.

**Build result**: RAM 11.1%, Flash 56.3%. Zero errors both envs. symmap: 102 functions.

---

## Session 11 — 2026-05-13 WebUI Crash Fix + Heap Hardening (v2.1.0)

### Issue: 15inch variant crashes on root WebUI page load

**Symptom**: `esp32c3-v3-15inch.local/` triggered a watchdog reset or OOM panic
immediately on page load. Animations became laggy in the seconds before the crash.
Power supply ruled out (4.9V, 0.8A measured at device).

**Root cause**: `htmlPage()` returned a ~6KB `String` built from a raw string literal,
allocated in one shot on the heap. `server_.send()` then blocked the main loop for the
full TCP write duration. On the 15inch variant (two NeoPixel strips, larger pixel buffer,
less heap headroom) this double-allocation caused heap exhaustion. Secondary issue: five
JSON handlers used repeated `String +` concatenation causing heap fragmentation over long
uptime.

**Fix (v2.1.0, commit bf06398)**:
- `htmlPage()` replaced with three `PROGMEM const char[]` chunks (`HTML_P1/P2/P3`)
  streamed via `setContentLength(CONTENT_LENGTH_UNKNOWN)` + `sendContent_P()`. No heap
  allocation for the page payload at request time.
- `/wifi` GET: PROGMEM chunks + two `sendContent()` calls for dynamic SSID/status values.
  Eliminated `.replace()` call on a heap-allocated String.
- `/update` GET: two PROGMEM chunks; fully static page.
- `settingsJson()`: `snprintf` into `char buf[900]` with inline hex color formatting.
  Zero heap allocs except final `String(buf)` return.
- `/time`, `/net`, `/diag` JSON handlers: `snprintf` into stack `char buf[]`; payload
  passed directly to `server_.send()` with no intermediate `String` object.

**Confirmed result (2026-05-13, user-measured)**:
```json
{"firmware_version":10,"boot_reason":"software","free_heap":236056}
```
236KB free heap on 15inch variant after OTA flash. `boot_reason: software` = expected
post-OTA reboot. Root page loads without crash, animations uninterrupted.

---

## Session S?? — 2026-05-09 Issues, Causes, and Fixes

### Issue A: Noon mark 1 LED off + center LED static / inner-12 LED pulsing

**Symptom**: At 12:00:00, the outer-ring hour marker appeared 1 LED counterclockwise of
true top. The center pixel was dark; the LED at inner-ring position 11 (12 o'clock on
the inner ring) was pulsing with the center animation color.

**Root cause**: User physically added a sacrificial WS2812B LED at the start of the LED
chain (physical index 0) to protect the data line. The build config was NOT updated.
With `RING_PIXEL_OFFSET=0` and a sacrificial pixel at physical 0:
- All three rings shifted 1 LED counterclockwise (outer ring started at physical 0 =
  sacrificial pixel instead of the first real outer ring LED at physical 1).
- `CENTER_PIXEL_INDEX=96` pointed to physical 96, which was the LAST LED of the inner
  ring (inner ring occupied physical 85-96 with sacrificial shift), not the center LED.
- The center idle pulse animation ran on inner-ring LED 11 (12 o'clock inner position).
- The actual center LED at physical 97 was beyond `CLOCK_PIXEL_COUNT=97` — never written.

**Fix** (`platformio.ini`, `esp32c3_v3_8inch` env):
```
CLOCK_PIXEL_COUNT  97 → 98
CENTER_PIXEL_INDEX 96 → 97
RING_PIXEL_OFFSET   0 → 1
SACRIFICIAL_PIXEL_ENABLED 0 → 1
SACRIFICIAL_PIXEL_INDEX = 0  (new)
```
Settings version bumped 9 → 10 (wipes stale EEPROM on first boot).

**Why sacrificial LED helps**: The WS2812B data input requires a signal voltage close to
its VDD. When the ESP32-C3 drives at 3.3V but LEDs run at 5V, the first LED's input
threshold may be marginal, causing corrupted data to cascade through the chain. The
sacrificial LED level-shifts the data line via its own output stage (5V-logic output
after the first pixel), so all subsequent pixels receive a clean 5V signal.

---

### Issue B: Runtime clock rotation offset (new feature)

**Symptom**: After the physical layout fix above, fine-tuning the 12-o'clock position
without re-flashing was requested.

**Fix**: Added `outerRingOffset` (uint8_t, 0–59) to `ClockSettings`. Applied
proportionally in `setRingPixel()` across all three rings at render time. Exposed as
"Ring rotation offset" number field in the Web UI Rings panel. Saved to EEPROM.

---

### Issue C: VEML7700 brightness pulsing / middle ring going wild during lux transitions

**Symptom**: When ambient lux dropped from ~220 → 45 → 0.3 (dimming room), the 24-LED
middle ring pulsed wildly and the outer ring timing appeared jittery. All effects
disappeared once lux stabilised at 0.3.

**Root cause — two bugs**:

**Bug C1: Gain state machine oscillation**
The gain-switching code had no hysteresis and a wrong direct transition:
```cpp
// OLD (buggy):
if (reading > 1000 && gain != GAIN_1_8) → set GAIN_1_8
else if (reading < 100 && gain != GAIN_2) → set GAIN_2
```
When on `GAIN_1_8` and lux dropped below 100, `gain != GAIN_2` was TRUE so the code
jumped from GAIN_1_8 directly to GAIN_2 (bypassing GAIN_1). With GAIN_2's doubled
sensitivity the same scene read >1000 lux, switched back to GAIN_1_8, read <100 again,
back to GAIN_2 → oscillation at ~3–6 Hz. Each gain switch produced an invalid reading
followed by a 150ms lockout, during which stale `luxAvg_` was returned. With the
smoothing alpha at 0.05, each valid reading barely moved the average, so the oscillation
propagated directly to brightness as a flutter.

**Bug C2: No brightness ramping**
`autoBrightnessCached()` recalculated and immediately applied the new brightness every
500ms. A lux change of 220→0.3 in one poll interval caused brightness to snap from ~192
to ~21 in a single frame — visible as a sharp flash/dark on all rings.

**Fixes**:

*C1 fix — proper 3-state gain machine with hysteresis:*
```
GAIN_2  → GAIN_1  only when reading > 200 lux  (entry threshold was < 50)
GAIN_1  → GAIN_2  when reading < 50 lux
GAIN_1  → GAIN_1_8 when reading > 900 lux
GAIN_1_8 → GAIN_1  when reading < 300 lux  (never jumps direct to GAIN_2)
Settle lockout after gain change: 150ms → 400ms
```

*C2 fix — brightness ramping at ≤50 units/second:*
Added `rampedBrightness_` float to `LuxSensor`. `autoBrightnessCached()` computes the
lux-derived target every 500ms into `cachedBrightness_`, then each call ramps
`rampedBrightness_` toward that target by at most 50 units per second. Full range
(15→255) now takes ≈4.8 seconds. First call snaps to target (no startup ramp).
`autoBrightness()` now calls `autoBrightnessCached()` for consistency.

Serial `Lux` line now shows `br=TARGET->RAMPED(eff=EFFECTIVE)` so both values are visible.

*C3 fix — faster lux smoothing:*
`luxAvg_` exponential smoothing alpha increased from 0.05 to 0.15. At 0.05, a large
lux drop took ~65 readings (8 seconds) to converge to 63% of target. At 0.15 it takes
~19 readings (2.3 seconds) — still smooth but tracks real changes instead of lagging far behind.

---

### Hardware question: VEML7700 vs LDR photoresistor

**Asked**: Would a simple analog light-dependent resistor (LDR/photoresistor) be more
stable and smooth?

**Comparison**:

| | VEML7700 (I2C digital) | LDR + voltage divider (analog) |
|---|---|---|
| Accuracy | Calibrated lux, ±10% | Uncalibrated ADC counts, varies by part |
| Noise | Low (digital, oversampling) | Higher (ADC noise, RF interference) |
| Speed | 100ms integration time minimum | ADC reads in microseconds |
| Smoothing | Software only | Hardware RC capacitor possible (free smoothing) |
| Complexity | I2C driver, gain management | Single resistor + ADC pin |
| Failure mode | I2C lockup possible | Resistor open/short, ADC noise floor |
| Oscillation risk | Gain switching (fixed above) | None — no gain stages |
| ESP32-C3 gotcha | Stable | ADC1 channels preferred; avoid GPIO0/2/1 |

**Recommendation**: The VEML7700 is the better choice for this application — it gives
true lux values which allow the log-scaled brightness curve to be meaningful. The LDR
would work but would require per-unit calibration and wouldn't eliminate flicker
(ESP32 ADC has ~1-2% noise without external RC filter). The software fixes above
(gain hysteresis + brightness ramping) should resolve the instability without changing
hardware. If instability persists after these fixes, add a 100nF cap between VEML7700
VIN and GND on the I2C lines to reduce noise pickup.

---

## TL;DR

The most likely cause of the 5-LED backward minute-hand jump is **spurious ISR fires on GPIO3/GPIO4** (JTAG/debug pins on the XIAO ESP32-C3). This is a confirmed regression introduced by the refactor: the original Arduino Nano used polled buttons on safe pins (GPIO7/8), the early ESP32 sketch used GPIO25/26, and the v3 moved to GPIO3/4 which are JTAG TCK/TDI on the ESP32-C3. The `renderCenterAnimationFrame()` patch is safe. The draw order (face→second→minute→hour) is identical to the proven original and is not the issue.

---

## 0. What the Original Code Confirms

Three versions of the code exist before v3:
- `NeoPixelClock_V2_With_MSGEQ7_V4.0.ino` — Arduino Nano, DS3234 RTC, hardware 1Hz interrupt, **polled buttons on GPIO7/8**
- `3-ring-addressableLEDClock` — Arduino Nano variant, same button approach
- `esp32_neopixel_clock.ino` / `neopixelClockESP32/src/main.cpp` — first ESP32 port, **ISR buttons on GPIO25/26** (safe pins)

All three share the same render order: face → second → minute → hour. That order was proven working on hardware for years. **It is not the cause of the backward jump.**

The original Nano button code:
```cpp
butRight = !digitalRead(BUT_UP);  // read once
while (butRight || butLeft) {     // hold-wait loop
  butRight = !digitalRead(BUT_UP);
  if (butRight) { butState |= 1; }
}
```
No ISRs. No debounce timer. The loop-wait style guarantees a press is only counted once per physical gesture. Spurious EMI noise that doesn't hold the pin LOW for a full loop iteration is ignored naturally.

The v3 ESP32-C3 refactor changed two things simultaneously:
1. Moved from polling to ISR-driven buttons
2. Moved button pins from safe GPIOs (25/26) to GPIO3/GPIO4 (JTAG TCK/TDI on the XIAO ESP32-C3)

Either change alone could cause problems. Together they are the clear regression path.

---

## 1. The 5-LED Backward Jump

### Render order — cleared

Draw order in `render()`:
```
renderFace → renderSeconds → renderMinutes → renderHours
```

Minutes are drawn **after** seconds, so the minute pixel always wins when second == minute.

The second trail (`trail[] = {52, 28, 14, 7}` — values are /255 fractions) is drawn first inside `renderSeconds`, then the head pixel overwrites it, then `renderMinutes` overwrites that. Even with secondTrail enabled, the minute hand pixel is never lost — it gets written last at full brightness.

`progressSeconds` draws a dim arc (`scale(color, 20)` ≈ 8% brightness) but `renderMinutes` runs afterward and overwrites. Not a factor.

**Conclusion: no rendering artifact in the current code can make the minute hand appear to jump 5 LEDs backward.**

### TimeModel — can minute go backward?

Only three paths mutate `time_.minute`:
1. `tickOneSecond()` → only increments
2. `set(h, m, s)` → sets to any valid value (called by NTP sync or web endpoints)
3. `addMinutes(delta)` → adds delta, which can be negative

`addMinutes(-1)` is called only by:
- `buttons.consumeDownPress()` in the main loop
- `/subMinute` HTTP POST endpoint

`set()` is called by:
- `TimeSync::syncNow()` (NTP — every 6 hours after initial sync, or manual trigger)
- `/set` and `/syncBrowser` HTTP POST endpoints

**NTP sync** could move minute backward if the local clock has drifted ahead. If NTP fires at, say, 12:30:05 local but the actual time is 12:29:45, it sets minute from 30 to 29 — a 1-LED backward move, not 5. A 5-minute NTP correction would require 5 minutes of drift, which is implausible within a 6-hour window from the previous sync.

### Root cause: GPIO3/GPIO4 JTAG pin conflict

**This is the highest-probability explanation for 5 LED backward.**

On the Seeed XIAO ESP32-C3:
- GPIO3 = JTAG TCK
- GPIO4 = JTAG TDI

Both are used as `BUTTON_DOWN_PIN` and `BUTTON_UP_PIN` with `FALLING` edge interrupts and `INPUT_PULLUP`.

The JTAG interface is not fully disabled in software in this firmware. When the USB cable is connected (for power), the USB CDC stack and possibly the JTAG subsystem can toggle these pins, generating valid FALLING edges that the ISR treats as button presses.

The software debounce is 120ms. Five presses separated by >120ms each take only ~600ms — fast enough to appear as a sudden backward jump to an observer watching the LED ring.

**Test: power the device via 5V from a power bank or breadboard supply with no USB data connection.** If the backward jumps stop, this is the cause.

**Fix options (in order of preference):**

1. **Revert to polling** — closest to the original that was proven to work. Replace ISR-based `ButtonInput` with a polled check in the main loop. On the ESP32-C3, a simple debounced poll is safe since the main loop runs at ~1kHz (no Wi-Fi blocking for >1ms at a time normally):

```cpp
// Replace ISR approach entirely in ButtonInput:
void poll(uint32_t now) {
    bool upNow   = (digitalRead(BUTTON_UP_PIN) == LOW);
    bool downNow = (digitalRead(BUTTON_DOWN_PIN) == LOW);
    if (upNow   && !lastUp_   && now - lastUpMs_   > debounceMs_) { upPressed_   = true; lastUpMs_   = now; }
    if (downNow && !lastDown_ && now - lastDownMs_ > debounceMs_) { downPressed_ = true; lastDownMs_ = now; }
    lastUp_   = upNow;
    lastDown_ = downNow;
}
```
Call `buttons.poll(now)` at the top of `loop()`. Rising-edge on LOW (press leading edge) with debounce. No ISRs, no JTAG interference.

2. **Keep ISRs but move to safe pins** — GPIO6, GPIO7, GPIO8, or GPIO9 on the XIAO ESP32-C3 are not JTAG and support interrupts. Requires a physical wire change.

3. **Add stable-LOW guard in main loop** (software-only, no rewire) — check that the pin is still actually LOW when the ISR flag is consumed. Reduces false fires from transient EMI but doesn't fully eliminate them if the transient holds long enough.

---

## 2. renderCenterAnimationFrame() Safety

The implementation:

```cpp
strip_.setBrightness(brightness);
renderCenterIdle(millis());   // setCenterPixel() only
setSacrificialPixelDark();
strip_.show();
```

**No `strip_.clear()` call** — correct. Adafruit_NeoPixel retains pixel buffer across `show()` calls. Ring pixels from the last full `render()` persist in the buffer. Only the center pixel is modified.

**`strip_.setBrightness()` safety**: Adafruit_NeoPixel's `setBrightness()` has an internal guard:
```c
if (newBrightness != brightness) { /* re-scale all pixels */ }
```
If brightness hasn't changed (normal case), it's a no-op. If brightness does change (day/night boundary), it re-scales all existing buffer values including ring pixels — which is correct behavior, the ring will update to the new brightness without a full redraw.

**Integer rounding concern**: Each `setBrightness()` re-scale involves integer division. If called repeatedly while brightness is changing step by step (e.g., a future smooth fade), accumulated truncation could cause slight color drift. Not an issue with the current binary day/night toggle.

**`lastTime_` update**: `renderCenterAnimationFrame()` updates `lastTime_ = time` but doesn't use `lastTime_` for anything in the center-only path. `lastTime_` is only used by `needsFullAnimationFrame()` via `chimeActive(lastTime_)`. This is fine.

**Verdict: renderCenterAnimationFrame() is safe with the current Adafruit_NeoPixel retained-buffer model.**

---

## 3. Jitter / Flash

### Pre-patch behavior
`needsAnimationFrame()` returned true whenever center idle was active (i.e., whenever `statusAnimations == 1`, which is default). This drove `renderer.render()` every 80ms, doing a full `strip_.clear()` + redraw + `strip_.show()` at 12.5 Hz continuously. With Wi-Fi stack running in the same loop, this was likely the primary flash source.

### Post-patch behavior
The center-only path calls `strip_.show()` every 80ms but does not clear or rewrite ring pixels. This eliminates most of the churn.

### Remaining jitter sources
1. **Wi-Fi interrupt preemption**: The ESP32 Wi-Fi stack runs on a separate FreeRTOS task but uses the same CPU (single-core on ESP32-C3). Interrupt handlers for Wi-Fi can preempt the main loop during `strip_.show()` or between pixel writes, causing timing glitches visible as flicker on sensitive pixels.

2. **`server_.handleClient()` latency**: WebServer's `handleClient()` processes one incoming request per call. A request that involves JSON string construction (e.g., `/settings`) can take 1–5ms. This delays the next tick/render cycle but shouldn't directly cause LED flash.

3. **`strip_.setBrightness()` in center-only path**: Called every 80ms. Since it's a no-op when brightness hasn't changed (see above), this is not a flash source.

**Recommendation**: If jitter persists after the center-only path, consider moving the NeoPixel updates to use DMA or a hardware timer ISR rather than the main loop. Alternatively, disable center idle animation during initial debugging to isolate whether `strip_.show()` frequency is the cause.

---

## 4. Status Animations Overwriting Hour Pixels

`renderStatus()` writes to `RING_INNER_12` (spinning dot) and the center pixel. This overwrites the hour hand LEDs on the inner ring during status animations (STATUS_WIFI_OK, STATUS_BUTTON, etc.).

This is a short-lived effect (700–1500ms) and intentional — status feedback takes priority over clock display briefly. It does not affect the outer ring minute/second hand.

Not a bug, but worth noting: after status expires and `statusActive()` returns false, the next full `render()` call (triggered by dirty flag from tick) will redraw the inner ring correctly.

---

## 5. Brightness Debounce

`effectiveBrightness()` is a pure function — no hysteresis. At the exact `nightStartHour` (e.g., 22:00:00), the brightness switches instantly. If the clock is at exactly 22:00:xx and ticking, this is a one-time switch per day. Not a flash source.

If a future feature adds smooth brightness transitions, calling `setBrightness()` with many intermediate values in the center-only path could accumulate rounding error (see §2). At that point, only apply `setBrightness()` in full renders.

---

## 6. Compositing Model — Fragility Assessment

The current render order (face → seconds → minutes → hours) gives an implicit priority:
- Hours win over minutes (inner/middle rings only — no outer ring conflict)
- Minutes win over seconds
- Seconds head wins over trail
- Everything wins over filler/marker

This is correct but fragile. A future feature (e.g., progress arc that runs past the minute position) could silently break priority. The handoff's suggestion to build an explicit 60-pixel composite array before writing is the right long-term fix.

For immediate debugging, the current order is fine.

---

## 7. Checklist — Answering Handoff Questions

| Question | Answer |
|----------|--------|
| Can 5-LED jump be explained by renderer composition? | No — minute always wins over seconds in current draw order |
| Could secondTrail/progressSeconds create illusion? | No — both are overwritten by renderMinutes |
| Can TimeModel.minute move backward unexpectedly? | Only via addMinutes(-1) or NTP set() |
| Could button ISR noise cause repeated addMinutes(-1)? | **YES — most likely root cause. GPIO3/GPIO4 are JTAG pins.** |
| Could NTP sync appear as minute movement? | Possible but would be ≤1 min, not 5 |
| Is renderCenterAnimationFrame() safe? | Yes |
| Should center idle be rate-limited separately? | Already is — 80ms gate shared with full animation, which is fine |
| Should status animations avoid overwriting hand pixels? | Status only touches inner ring + center — outer ring hands are safe |
| Should renderer use explicit compositing? | Yes, recommended for long-term robustness, not urgent |
| Could Wi-Fi interrupt/delay cause flash? | Yes, residual jitter likely from Wi-Fi preemption |
| Should brightness changes be debounced? | Current binary day/night is fine; add only if smooth fading added later |
| Should button interrupts be replaced with polled reads? | **Yes — supplement ISR with stable-LOW poll to guard against GPIO3/4 noise** |

---

## 8. Recommended Next Steps (Priority Order)

1. **Isolate button noise**: Power device without USB data (use 5V only). Observe whether 5-LED backward jumps stop. If yes, implement the stable-LOW poll guard in the main loop.

2. **Add `/diag` endpoint** that returns: current h/m/s, last time-change reason (tick/NTP/button-up/button-down/web-set), button event counters, NTP sync count, last NTP delta. This gives definitive proof of what's causing minute changes.

3. **Upload the patched firmware** (secondTrail off, center-only animation path) and A/B test flash/jitter.

4. **If jitter persists**: disable center idle animation (`statusAnimations = 0` via web UI) and observe. If jitter stops, the 80ms `strip_.show()` in the center-only path is the source, and we need to find a way to reduce it further (e.g., increase interval to 160ms).

5. **LED mapping test mode**: add a `/ledtest?pixel=N` or `/ledwalk` endpoint for physical verification of logical-to-physical mapping.

6. **Explicit composite array**: replace the 4-pass outer-ring rendering with a single 60-element array pass. Implement in a separate commit after hardware is validated.

---

## Maturation Goal: OTA-First Wall-Mounted Operation

**Goal**: ChronoBloom should be flashable, debuggable, and verifiable entirely over WiFi
with the clock mounted on the wall -- no USB cable, no serial monitor, no physical access
required for normal development and testing cycles.

This is the target state for the project to be considered stable for use beyond its
creator.

---

### Prerequisite: Baseline OTA Verification

Before any stability work below, confirm OTA works end-to-end:

1. Flash via USB once with current firmware
2. Confirm `esp32c3-v3-8inch.local` resolves on the network
3. Run: `pio run -e esp32c3_v3_8inch -t upload --upload-port esp32c3-v3-8inch.local:3232`
4. Confirm it completes without timeout

If this fails, all tasks below are premature.

---

### OTA Stability Task List

#### Task 1: `/diag` Endpoint
**Priority**: High -- blocks all wall-mount verification workflows
**Status**: Done (Session 3, 2026-05-13, v2.0.6)

Add a `/diag` HTTP GET endpoint returning JSON with:
- `uptime` (seconds since boot)
- `firmware_version` (SETTINGS_VERSION or build timestamp)
- `boot_reason` (OTA / power-on / watchdog / exception)
- `free_heap` (bytes)
- `wifi_ssid`
- `wifi_rssi`
- `wifi_ip`
- `ntp_synced` (bool)
- `ntp_last_delta` (seconds, last correction applied)
- `button_events` (count since boot)

OTA verification workflow once implemented:
1. Note version at `/diag` before flash
2. Run OTA
3. Hit `/diag` after reboot -- confirm version changed and `boot_reason=OTA`

#### Task 2: WiFi Auto-Reconnect Explicit Guard
**Priority**: High -- OTA fails silently if WiFi drops
**Status**: Done (Session 2, 2026-05-11, v2.0.5) — `WiFi.setAutoReconnect(true)` confirmed present in `setupWiFi()`

Confirm `WiFi.setAutoReconnect(true)` is explicitly called in `setupWiFi()`. Do not
rely on SDK defaults. If absent, add it.

#### Task 3: OTA Error Handler
**Priority**: High
**Status**: Done (Session 2, 2026-05-11, v2.0.5) — `ArduinoOTA.onError()` handler confirmed present, calls `ESP.restart()`

Confirm `ArduinoOTA.onError()` handler exists and calls `ESP.restart()`. If a transfer
stalls mid-flash (network drop, power fluctuation), the device must reboot to last good
firmware rather than hang in OTA wait state.

#### Task 4: Software Watchdog in loop()
**Priority**: Medium
**Status**: Done (Session 4, 2026-05-11)

Add software watchdog feed in `loop()` with a 10-second window. If `loop()` stops
executing (I2C blocking, WiFi manager blocking, deadlock), watchdog fires and reboots.
Verify hardware watchdog is not being suppressed by a tight feed loop.

#### Task 5: Button-Hold Factory Reset on Boot
**Priority**: Medium -- eliminates last physical-access recovery path
**Status**: Done (Session 6, 2026-05-13). `SettingsStore::resetToDefaults()` invalidates EEPROM magic; `Preferences "factory/portal"` flag checked in `setupWiFi()` to force WiFiManager portal and skip build-time credentials.

Hold UP (GPIO5) at power-on, then add DOWN (GPIO9) and hold both for 3 seconds:
- GPIO9 is the XIAO BOOT pin — holding it LOW at the reset instant enters USB download mode.
  GPIO5 has no strapping conflict, so it is safe to hold from power-on.
- LEDs show all-red once UP is detected; 5-second window to add DOWN
- If both held for 3 continuous seconds: clear EEPROM, reboot into WiFi provisioning portal
- On confirmation: all-white flash x2

Without this, corrupted EEPROM post-flash requires USB serial access to recover.

#### Task 6: Physical Buttons Re-Added
**Priority**: Medium -- prerequisite for Task 5; last-resort input when WiFi unavailable
**Status**: Planned -- blocked on Tasks 1 and 2

Buttons removed in 2.0.4 due to GPIO3/GPIO4 JTAG interference. Re-added Session 5.

Implementation:
- GPIO9 (D9) = UP, GPIO5 (D3) = DOWN — momentary buttons, shared GND
- Polled reads in `loop()` via `ButtonInput` class — no ISRs, 50ms debounce
- GPIO8 ruled out: Seeedstudio docs confirm GPIO8=0 + GPIO9=0 simultaneously = invalid
  boot-strapping state. GPIO5 has no conflicts.
- See REVIEW.md Section 1 for polling implementation template

---

### Maturation Checklist

| Task | Status | Blocked By |
|---|---|---|
| Baseline OTA verified end-to-end | **Done** (Session 2, 2026-05-11) | -- |
| Task 1: `/diag` endpoint | **Done** — `GET /diag` returns 10-field JSON; `TimeSync::lastDeltaSec_`, `g_buttonEventCount` added; verified 2026-05-13 | -- |
| Task 2: `WiFi.setAutoReconnect(true)` confirmed | **Done** — added main.cpp:1297 | -- |
| Task 3: `ArduinoOTA.onError()` handler confirmed | **Done** — `ESP.restart()` added main.cpp:1356 | -- |
| Task 4: Software watchdog in `loop()` | **Done** — `esp_task_wdt` 10s window, Session 4 2026-05-11 | -- |
| Web UI `/update` OTA | **Done** — FormData + UPDATE_SIZE_UNKNOWN fix, Session 4 2026-05-11 | -- |
| Task 5: Button-hold factory reset on boot | **Done** — hold UP+DOWN 3s at power-on: all-red LEDs during hold, EEPROM magic invalidated, `Preferences "factory/portal"` flag forces WiFiManager portal on reboot, white flash x2; Session 6 2026-05-13 | -- |
| Task 6: Physical buttons re-added (GPIO5/9, polled) | **Done** — `ButtonInput` polled class, GPIO9(UP)/GPIO5(DOWN), Session 5 2026-05-13 | Tasks 1, 2 |
| Task 7: Button hold-to-repeat + no-WiFi time adjust | **Done** — hold >500ms→+1min/150ms repeat, >2000ms→+60min/fire; GPIO swap UP=5 DOWN=9; 2026-05-13 | Task 6 |

#### Session 2 OTA Test Finding (2026-05-11)
Mid-transfer WiFi block (via router admin) left device **hung in OTA wait state** — `onError` never fired,
web server became unresponsive, required power cycle to recover. This confirms Task 4 (software watchdog)
is the critical fix for wall-mount reliability. `onError` + `ESP.restart()` (Task 3) only fires for
library-detected errors; a silent TCP stall bypasses it entirely.
