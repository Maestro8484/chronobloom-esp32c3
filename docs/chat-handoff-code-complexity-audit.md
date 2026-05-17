# Claude Chat Handoff — Code Complexity & Consistency Audit

**Date**: 2026-05-17  
**Repo**: `C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`  
**Firmware**: `src/main.cpp` (~3100 lines), `src/web_html.h` (~36KB PROGMEM HTML/JS)

---

## What Triggered This

During session 19 we discovered that `renderFace()` computed middle and inner ring ambient brightness using two different formulas (`hoursLevel/6` and `centerLevel/6`), producing inconsistent results because `hoursLevel` and `centerLevel` have different defaults (255 vs 180). Before that fix, the original code had those values hardcoded as `22` and `24` — arbitrary small numbers written by feel with no documented rationale.

The pattern: a value was written intuitively, never questioned, and later compounded by a "fix" that added complexity instead of simplifying. The question is how many other places in the codebase follow the same pattern.

---

## What to Look For

These are the categories of disproportionate complexity to audit. For each finding, note: **file + line range**, **what is overcomplicated**, and **what simpler form would look like**.

### 1. Magic numbers with no rationale

Constants written by feel that have no comment explaining their value. Examples already found:
- `renderFace()` ambient scale: was `22`, `24` (now `50`, `50`)
- `autoBrightnessCached()`: `normalized * 240.0f + 15.0f` — why 240 and 15?
- `autoBrightnessCached()`: ramp rate `0.050f` per ms (50 units/sec) — documented, acceptable
- `lux()` smoothing alpha `0.85f / 0.15f` — documented
- `pulse()` in `renderCenterIdle`: hardcoded `1800, 3, 75` — period, floor, ceiling

Look for: bare numeric literals in rendering, brightness, timing, and scaling code that have no adjacent comment and no named constant.

### 2. Parallel structures that should be unified

Places where the same operation is done multiple times with slight variations that could be a single parameterised call. Examples to check:
- `renderHours()` writes 1 or 2 pixels to the middle ring depending on minute — is this logic symmetric and correct, or does it have off-by-one edge cases at :00 and :59?
- `renderStatus()` and `renderHourlyChime()` both manually construct ring pixel sequences — do they share any helper logic, or is it copy-paste?
- The 30 `anim*` functions — do any of them contain repeated sub-patterns that suggest a missed abstraction?

### 3. Settings with disproportionate reach

Settings that silently control more than their name implies:
- `hoursLevel` now controls *both* the active hour indicator pixel AND (via the ambient formula) the ambient fill brightness for the entire middle ring. A user changing "hours color intensity" does not expect it to change the ambient background of the ring.
- `centerLevel` has the same dual role for the inner ring.
- This was introduced in session 19 as an intermediate fix but was then corrected to a flat `50`. **Verify the current code uses the flat `50` (it should) and flag any remaining settings that have unintended dual roles.**

### 4. Auto-brightness formula complexity

`autoBrightnessCached()` lines 263–300 uses a log10 curve:
```cpp
float normalized = log10(max(0.1f, lx) + 1.0f) / log10(10001.0f);
cachedBrightness_ = constrain(normalized * 240.0f + 15.0f, 15.0f, 255.0f);
```
- Why `10001.0f` as the denominator? This assumes a lux ceiling of 10000 — is that documented anywhere?
- Why `240.0f + 15.0f`? The output range is 15–255. Is this intentional headroom, or an artifact of iteration?
- The 3-state gain machine (GAIN_2 / GAIN_1 / GAIN_1_8) with hysteresis bands (50, 200, 300, 900) — are those thresholds empirically validated or guessed?

### 5. The `draw()` JS function in web_html.h

The client-side preview (`draw()`) hardcodes its own ambient scale values:
```js
for(let i=0;i<24;i++) setLed(leds.middle[i], color('hours'), 22, 'marker');
for(let i=0;i<12;i++) setLed(leds.inner[i],  color('center'), 24, 'marker');
```
These are the **original** firmware values (`22` and `24`) and were **not updated** when the firmware was fixed to `50`. The web UI preview now shows a ring balance that does not match what the hardware renders. This is a concrete bug to flag.

### 6. `renderSeconds()` trail values

```cpp
const uint8_t trail[] = {52, 28, 14, 7};
```
Four hardcoded trail brightness values. No rationale. Are these a geometric decay? (`52 → 28 → 14 → 7` is roughly halving each step.) If so, that should be expressed as such. The `trailLength` setting controls LED count but not these brightness levels — is that intentional?

### 7. `setRingPixel()` rotation formula

```cpp
uint8_t rotated = rot
    ? static_cast<uint8_t>((static_cast<uint32_t>(logicalIndex) +
                            (static_cast<uint32_t>(rot) * ring.count + 30) / 60) % ring.count)
    : logicalIndex;
```
The `+ 30` bias exists to round to the nearest LED rather than truncate. This was added in session 16 to fix middle/inner ring proportional rotation. The formula is correct but opaque — it deserves a comment, and it may be worth verifying that it handles all three ring sizes (60, 24, 12) symmetrically.

---

## What NOT to Change in Chat

Chat = planning and analysis only. Do not produce implementation diffs.  
Flag findings as: **Bug** (wrong behavior), **Smell** (correct but fragile/opaque), or **Cosmetic** (style only).  
Prioritise Bugs first, then Smells that have user-visible impact.

## Concrete First Action for Chat

Start with item 5 (the `draw()` JS mismatch) — it is a confirmed bug, not a smell, and it is the direct consequence of the session 19 renderFace fix. The JS preview shows `22`/`24` ambient, the firmware renders `50`/`50`. That discrepancy should be flagged for Claude Code to fix in the next session.
