# Ring Dimming Investigation — Findings & Patch Plan

Generated: 2026-05-17  
Session: ring-dimming-investigation

---

## Files Read This Session

| File | Why |
|------|-----|
| `CLAUDE.md` | Session anchor — mandatory first read |
| `docs/FUNCTION_INVENTORY.md` | Locate line ranges before reading main.cpp |
| `docs/symmap.json` | Confirm line ranges (SHA-pinned) |
| `src/main.cpp` lines 1–160 | Build-time constants, RingConfig layout |
| `src/main.cpp` lines 197–300 | LuxSensor::lux, autoBrightnessCached |
| `src/main.cpp` lines 303–380 | LuxSensor members, ClockSettings struct |
| `src/main.cpp` lines 412–489 | SettingsStore::defaults, sanitize |
| `src/main.cpp` lines 575–640 | TimeModel tail, ClockRenderer class header |
| `src/main.cpp` lines 770–921 | render, nightActive, effectiveBrightness, renderFace, renderSeconds, renderMinutes, renderHours, secondColor, ringColor |
| `src/main.cpp` lines 1738–1808 | dim, scale, pulse, setCenterPixel, setSacrificialPixelDark, setRingPixel |
| `src/main.cpp` lines 2335–2413 | /diag handler, /settings POST handler |
| `src/main.cpp` lines 2916–2956 | logRuntimeStatus |

---

## Root Cause — Confirmed

### Primary cause: hardcoded ambient scale factors in `renderFace()` (lines 840–843)

```cpp
// main.cpp:840-843
const uint32_t middleAmbient = scale(ringColor(settings.hoursRed, settings.hoursGreen,
                                              settings.hoursBlue, 255), 22);
const uint32_t innerAmbient = scale(ringColor(settings.centerRed, settings.centerGreen,
                                             settings.centerBlue, 255), 24);
```

The ambient fill for middle and inner rings is hardcoded at **22/255 ≈ 8.6%** and **24/255 ≈ 9.4%**
of full color intensity. The outer ring is drawn at:

| Element | Scale | Effective intensity |
|---------|-------|-------------------|
| Outer markers (×12 LEDs, every 5th) | outerMarkerLevel = 220 | ~86% |
| Outer filler (×48 LEDs) | outerFillerLevel = 130 | ~51% |
| Middle ambient (×22 LEDs) | hardcoded 22/255 | **~8.6%** |
| Middle hour indicator (1–2 LEDs) | hoursLevel = 255 | 100% |
| Inner ambient (×11 LEDs) | hardcoded 24/255 | **~9.4%** |
| Inner hour indicator (1 LED) | hoursLevel = 255 | 100% |

**Gap: outer ring ambient is 6–10× brighter than middle/inner ambient fill.**

The intent is clear — middle/inner glow very softly as ambience, with only the hour
markers bright. But at 9% the ambient fill is nearly invisible, and the rings look
"off" compared to the blazing outer ring.

---

## Secondary Factors

### Single global `setBrightness()` — no per-ring hardware compensation

`effectiveBrightness()` (line 818) returns one scalar. `strip_.setBrightness(br)` at
line 775 applies it uniformly to all 98 LEDs in the chain. There is no per-ring
brightness multiplier at the hardware level.

### Auto-brightness is dominated by the outer ring's output

The VEML7700 reads the total ambient light from the face. With 60 LEDs at 51–86%
fill on the outer ring vs. ~9% ambient on the inner two, the sensor reads
predominantly outer-ring output. Auto-brightness settles to whatever looks correct
for the outer ring — the inner rings have no influence on this feedback loop.

Rough relative lux contribution per ring (default settings):
- Outer (60 LEDs): 12×220 + 48×130 = 2640 + 6240 = **8880 units**
- Middle (24 LEDs): 22×22 + 2×255 = 484 + 510 = **994 units**
- Inner (12 LEDs): 11×24 + 1×255 = 264 + 255 = **519 units**

Outer ring contributes ~86% of total face output → dominates lux feedback.

### No per-ring ambient level setting exists

`ClockSettings` has `outerMarkerLevel`, `outerFillerLevel`, `secondsLevel`,
`minutesLevel`, `hoursLevel`, `centerLevel` — but no `middleAmbientScale` or
`innerAmbientScale`. The 22/24 numbers are compile-time constants with no UI knob.

### `hoursLevel` is partially effective but incomplete

`hoursLevel` (default 255) controls the 1–2 active hour indicator pixels on the
middle ring and the 1 pixel on the inner ring. It does **not** affect the ambient
fill. A user reducing `hoursLevel` makes the indicator dimmer but the ambient
fill stays at 22/24 regardless.

### Physical optics (secondary, not root cause)

The inner rings are physically smaller and may have a different diffuser geometry.
This can compound the firmware gap but is not the primary cause — the firmware
establishes a 6–10× baseline imbalance before optics have any effect.

---

## What the `/diag` Endpoint Currently Reports

Existing `/diag` (line 2335) already returns: `lux`, `brightness_target`,
`brightness_ramped`, `free_heap`, `clock_pixel_count`, `ring_pixel_offset`.

Missing from `/diag` for this issue:
- `middle_ambient_scale` and `inner_ambient_scale` (the hardcoded 22/24)
- `effective_brightness` (the `br` value from `effectiveBrightness()`)
- `outer_filler_level` / `outer_marker_level` (from current settings)

---

## Proposed Patch Batch (Two Steps)

### Step 1 — Diagnostic only (Patch A): extend `/diag`

**File:** `src/main.cpp`  
**Where:** Inside the `/diag` handler at line 2341–2361 (the `snprintf` buffer)  
**Change:** Add these fields to the JSON response:

```
"outer_marker_level":<val>,
"outer_filler_level":<val>,
"middle_ambient_scale":22,
"inner_ambient_scale":24,
"hours_level":<val>,
"center_level":<val>
```

**Risk:** Zero — read-only diagnostic endpoint. No EEPROM change.  
**Rollback:** Revert 1 snprintf block.  
**Purpose:** Verify from the browser (no serial monitor needed) that the live values
match the analysis before touching `renderFace()`.

---

### Step 2 — Fix (Patch B): raise ambient scale in `renderFace()`

**File:** `src/main.cpp`  
**Where:** Lines 840–843 (`renderFace`)  
**Recommended change (Option B2 — ties ambient to existing settings):**

```cpp
// BEFORE:
const uint32_t middleAmbient = scale(ringColor(settings.hoursRed, settings.hoursGreen,
                                              settings.hoursBlue, 255), 22);
const uint32_t innerAmbient = scale(ringColor(settings.centerRed, settings.centerGreen,
                                             settings.centerBlue, 255), 24);

// AFTER:
// Ambient glow is ~1/6 of the indicator brightness, so the ratio
// stays constant as the user adjusts hoursLevel/centerLevel via the UI.
const uint32_t middleAmbient = scale(ringColor(settings.hoursRed, settings.hoursGreen,
                                              settings.hoursBlue, 255),
                                    max((uint8_t)1, (uint8_t)(settings.hoursLevel / 6)));
const uint32_t innerAmbient = scale(ringColor(settings.centerRed, settings.centerGreen,
                                             settings.centerBlue, 255),
                                    max((uint8_t)1, (uint8_t)(settings.centerLevel / 6)));
```

With defaults (`hoursLevel = 255`, `centerLevel = 180`):
- middleAmbient scale: 255/6 = **42** (up from 22) → ~16.5% fill
- innerAmbient scale: 180/6 = **30** (up from 24) → ~11.8% fill

This roughly halves the gap vs. outer filler (51%) while keeping rings visually
distinct. The user can tune the ratio further by adjusting `hoursLevel` and
`centerLevel` in the existing web UI — no new settings needed.

**Alternative (Option B1 — bump hardcoded values, simpler):**

```cpp
// Change 22 → 60 and 24 → 55
const uint32_t middleAmbient = scale(ringColor(settings.hoursRed, ..., 255), 60);
const uint32_t innerAmbient  = scale(ringColor(settings.centerRed, ..., 255), 55);
```
60/255 ≈ 24% and 55/255 ≈ 22% — still dimmer than outer filler at 51%,
but close enough to look intentional rather than broken. Dead simple, zero coupling.

**Risk:** 2 lines changed, no new settings, no EEPROM version bump required.  
**Rollback:** `git revert` or manually restore 22 and 24 to those two lines.  
**Deploy:** OTA via `http://esp32c3-v3-8inch.local/update`

---

## Implementation Order

1. **Approve this plan** (current step).
2. Implement Patch A (extend `/diag`). Build + OTA flash. Hit `/diag` from browser,
   confirm `middle_ambient_scale:22`, `inner_ambient_scale:24` appear correctly.
3. Implement Patch B2 (or B1 if simpler is preferred). Build + OTA flash.
4. Observe face. If balance is improved, done. If inner rings still look too dim
   relative to outer, increase the divisor (e.g., `/5` instead of `/6`).
5. Close session per `docs/SESSION_CLOSURE.md`.

---

## Out of Scope (Do Not Implement Now)

- Per-ring auto-brightness: would require multiple lux sensors or masking — major
  architectural change, not worth it.
- New EEPROM `middleAmbientScale` / `innerAmbientScale` settings: only add these if
  the hardcoded values are insufficient after user testing.
- Lux sensor replacement: the VEML7700 is not the problem here.
- Diffuser/optics changes: physical, cannot be addressed in firmware.
