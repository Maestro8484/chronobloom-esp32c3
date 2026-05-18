# ChronoBloom Animation Redesign — Planning Handoff
**For**: Claude Chat planning session
**Date**: 2026-05-17
**Firmware baseline**: v2.3.6 (all preview/style bugs fixed; animations now play to completion)
**Task**: Review all 29 animations and redesign them to be smooth, cinematic, and palette-aware

---

## Why This Handoff Exists

The preview/style system was broken since v2.1.0 (uint32 underflow killed every preview in 1 frame).
Now that it is fixed in v2.3.6, the user can see all animations for the first time. They are described as:

> "terrible abrupt not smooth animations"

The existing animations were written when the preview system was broken, so visual quality was never iterable. This planning task defines what "smooth" means, audits every animation, and produces an implementation spec for Claude Code.

---

## Hardware Context

- **Outer ring**: 60 LEDs (RING_OUTER_60), offset 1 from strip start
- **Middle ring**: 24 LEDs (RING_MIDDLE_24)
- **Inner ring**: 12 LEDs (RING_INNER_12)
- **Center pixel**: index 97 (separate, always set via `setCenterPixel`)
- **Sacrificial pixel**: index 0, always dark (`setSacrificialPixelDark`)
- **Strip**: Adafruit NeoPixel, `setBrightness()` scales buffer in-place
- **Loop rate**: ~90ms full render, animation frames run as fast as loop() allows (~10ms)

## Animation System Architecture (read before designing)

### Key helpers available in `ClockRenderer`
```cpp
uint32_t paletteColor(uint8_t position, bool useReminderPalette = false)
// Maps position 0-255 → palette-aware RGB color, brightness baked in via animationBrightness

uint32_t scaledElapsed(uint32_t elapsed)
// Scales elapsed ms by animationSpeed (1=0.5x, 2=0.75x, 3=1.0x, 4=1.5x, 5=2.0x)

uint32_t scale(uint32_t color, uint8_t factor)
// Scale a color's brightness: scale(c, 0)=off, scale(c, 255)=full, scale(c, 128)=half

uint32_t dim(uint32_t color, uint8_t factor)
// Similar to scale; used for ambient dimming

uint32_t pulse(uint32_t color, uint8_t n, uint8_t max)
// Returns color scaled by n/max (0→0, max→full)

void setRingPixel(RingConfig ring, uint8_t i, uint32_t color)
// Set pixel i on given ring (handles offset, direction)

void setCenterPixel(uint32_t color)
// Set the center pixel

void setSacrificialPixelDark()
// Always call at end of renderAnimFrame (already called by renderAnimFrame)
```

### Brightness note — IMPORTANT
`paletteColor()` bakes `animationBrightness` into the returned color as the HSV `val` parameter.
`strip_.setBrightness(animationBrightness)` is called separately in the animation function.
This causes **double-application** (brightness squared). This is a known issue, acceptable for now.
Do NOT introduce a third application.

### Fade in/out pattern — USE THIS
```cpp
// Fade in: first 300ms
uint8_t fadeIn = (elapsed < 300) ? (uint8_t)(elapsed * 255 / 300) : 255;
// Fade out: last 300ms of `duration`
uint8_t fadeOut = (elapsed > duration - 300) ? (uint8_t)((duration - elapsed) * 255 / 300) : 255;
uint8_t fade = (uint8_t)((uint16_t)fadeIn * fadeOut / 255);
// Then: scale(paletteColor(pos), fade)
```

### Animation function contract
```cpp
void animXX(uint32_t now) {
    uint32_t elapsed = scaledElapsed(now - animStartMs_);
    if (elapsed >= DURATION_MS) { animPhase_ = ANIM_IDLE; return; }
    strip_.setBrightness(settings_.get().animationBrightness);
    // ... render pixels using paletteColor() and scale() ...
    // DO NOT call setSacrificialPixelDark() — renderAnimFrame does this
    // DO NOT call strip_.show() — renderAnimFrame does this
}
```

### Duration guidelines
- **Quarter animations (Q1-Q6)**: 2000–3000 ms. Should feel like a punctuation mark.
- **Half-hour animations (H1-H7)**: 4000–6000 ms. Should feel like a breath.
- **Hour animations (Hr1-Hr10)**: 6000–10000 ms. Should feel like a moment of ceremony.
- **Reminder animations (Rem1-Rem6)**: 2000–4000 ms. Should feel like a gentle tap.

---

## Current Animation Inventory & Quality Audit

### Quarter Animations (Q1–Q6, triggered at :15/:45 by default; Q3 at :15)

| ID | Name | Duration | Uses palette | Uses speed | Smooth | Notes |
|----|------|----------|-------------|-----------|--------|-------|
| Q1 | Sparkle burst | 600ms | No (white) | No | Poor | Way too fast; single white flash; no fade |
| Q2 | Pulse markers | 2500ms | Via ringColor only | No | Poor | Abrupt on/off; markers only; ignores palette |
| Q3 | Ring shimmer | 2500ms | Yes | Yes | Fair | OK concept; bright/dim toggle abrupt; no fade in/out |
| Q4 | Laser ping | ? | Yes | ? | Unknown | Needs review |
| Q5 | DNA twist | ? | Yes | ? | Unknown | Needs review |
| Q6 | Tick spark | ? | Yes | ? | Unknown | Needs review |

### Half-Hour Animations (H1–H7, triggered at :30)

| ID | Name | Duration | Uses palette | Uses speed | Smooth | Notes |
|----|------|----------|-------------|-----------|--------|-------|
| H1 | Rainbow sweep | 5000ms | Yes | Yes | Fair | Good concept; no fade in/out; abrupt start |
| H2 | Dual flash | ? | No? | No | Poor | Named "flash" — likely strobing |
| H3 | Tidal pulse | ? | No? | No? | Unknown | Needs review |
| H4 | Comet chase | ? | Yes | ? | Unknown | Needs review |
| H5 | Color explosion | ? | Yes | ? | Unknown | Needs review |
| H6 | Knight Rider bounce | ? | Yes | ? | Unknown | Needs review |
| H7 | Strobe party | ? | No | No | Poor | Strobing by name; eliminate or redesign |

### Hour Animations (Hr1–Hr10, triggered at :00)

| ID | Name | Duration | Uses palette | Uses speed | Smooth | Notes |
|----|------|----------|-------------|-----------|--------|-------|
| Hr1 | Chime sweep | ? | No? | No | Unknown | Needs review |
| Hr2 | Firework burst | ? | Yes | ? | Unknown | Needs review |
| Hr3 | Zenith cascade | ? | Yes | ? | Unknown | Needs review |
| Hr4 | Rainbow spiral | ? | Yes | Yes | Unknown | Default hour anim; needs review |
| Hr5 | Breathing mandala | ? | Yes | Yes | Unknown | "Breathing" implies smooth — verify |
| Hr6 | Supernova | ? | Yes | ? | Unknown | Needs review |
| Hr7 | Matrix rain | ? | Yes | ? | Unknown | Needs review |
| Hr8 | Galaxy spin | ? | Yes | ? | Unknown | Needs review |
| Hr9 | Color wipe | ? | Yes | ? | Unknown | Needs review |
| Hr10 | Thunderstorm | ? | No | No | Poor | Likely random flashing; redesign or remove |

### Reminder Animations (Rem1–Rem6)

| ID | Name | Notes |
|----|------|-------|
| Rem1 | Amber pulse | Should be smooth breathing |
| Rem2 | Attention ring | Review — "attention" often means abrupt |
| Rem3 | Heartbeat | Two-beat pulse with pause; can be smooth |
| Rem4 | Sunrise wake | Slow fill from bottom — naturally smooth |
| Rem5 | Campfire flicker | Intentional flicker OK, but needs palette |
| Rem6 | Neon sign | Review |

---

## What "Smooth" Means — Design Principles

1. **Fade in**: Every animation should fade up from black over ~200–400ms at start
2. **Fade out**: Every animation should fade to black over ~200–400ms at end
3. **No hard cuts**: No pixel going instantly from 0 to full or full to 0 except intentional sparkle
4. **Easing**: Use sine or quadratic easing for sweeps and pulses, not linear ramps
5. **Palette-aware**: All animations must use `paletteColor()` — no hardcoded colors except white accents
6. **Speed-aware**: All animations must use `scaledElapsed()` — speed slider must have visible effect
7. **Eliminate strobing**: H2 (dual flash), H7 (strobe party), Hr10 (thunderstorm) — redesign or rename with non-strobe behavior
8. **Trail decay**: Moving elements (comets, sweeps) should leave a decaying trail using `scale(c, trail_brightness)`

### Smoothing math reference

```cpp
// Sine ease (0→1→0 over full duration) — for pulsing/breathing
float phase = (float)elapsed / duration;
float sine = (sinf(phase * M_PI * 2.0f - M_PI / 2.0f) + 1.0f) / 2.0f;  // 0→1→0
uint8_t brightness = (uint8_t)(sine * 255);

// Linear fade in/out with clamping (simpler, no float)
uint8_t fadeEnvelope(uint32_t elapsed, uint32_t duration, uint32_t fadeMs) {
    if (elapsed < fadeMs) return (uint8_t)(elapsed * 255 / fadeMs);
    if (elapsed > duration - fadeMs) return (uint8_t)((duration - elapsed) * 255 / fadeMs);
    return 255;
}

// Comet trail: position p, head at headPos, trail length T
uint8_t trailBrightness(uint8_t p, uint8_t headPos, uint8_t total, uint8_t trailLen) {
    int8_t dist = (headPos - p + total) % total;
    if (dist == 0) return 255;  // head
    if (dist < trailLen) return (uint8_t)(255 - dist * 255 / trailLen);
    return 0;
}
```

---

## Implementation Spec for Claude Code

The planning output should produce, for each animation:

1. **Revised name** (if changed)
2. **Duration** (in ms, at speed=3)
3. **Fade in/out** (yes/no, ms)
4. **Per-ring behavior** description (outer / middle / inner / center)
5. **Palette usage** (which position range maps to which ring)
6. **Speed scaling note** (what speeds up with scaledElapsed)
7. **Pseudocode** or reference pattern (enough for Claude Code to implement without design decisions)

---

## Suggested Priority Order for Implementation

1. **Q3** — default quarter animation, user sees most often; needs fade in/out and softer shimmer
2. **H1** — default half-hour animation; needs fade in/out
3. **Hr4** — default hour animation; needs review and fade
4. **Q1, Q2** — too short/abrupt; lengthen and add palette
5. **H2, H7, Hr10** — strobe/flash candidates; redesign behavior
6. **All others** — audit and add fade envelope + palette + speed if missing

---

## Files to Read for Implementation

Claude Code should read these before touching any animation:
- `docs/symmap.json` — get exact line ranges for each animation function
- `docs/FUNCTION_INVENTORY.md` — function purpose summaries
- Targeted `src/main.cpp` lines for each function being rewritten

Do NOT read `src/main.cpp` whole. Use symmap line ranges + offset/limit.

---

## Constraints

- No new EEPROM fields without a version bump and sanitize/valid update
- `trailLength` (EEPROM, range 2–12) is available as a user-configurable trail parameter — use it in comet/sweep animations
- `animationBrightness` (EEPROM, range 50–255) is the peak LED brightness during animations
- No heap allocations in animation functions (stack only, no `new`/`malloc`)
- Animation functions are called from ISR-adjacent context — no blocking, no Serial in hot path
