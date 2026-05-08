# ChronoBloom ESP32-C3 — Firmware Review
Date: 2026-05-04

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
