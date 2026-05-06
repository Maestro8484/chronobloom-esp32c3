# Animation Catalog

## Animation Design Principles

### Core Philosophy
Animations exist to **acknowledge the passage of time** and **provide visual feedback**, not to obscure the clock display. All animations are time-bound and return to normal clock display.

### Rules
1. **Hands always win** — After animation ends, clock display must be accurate
2. **Escalating intensity** — More frequent events = shorter/subtler animations
3. **Configurable** — User can disable or select specific animations
4. **Time-bound** — All animations have fixed maximum duration
5. **Brightness-aware** — Respect user's brightness settings (day/night/auto)

---

## Animation Triggers

### Time-Interval Animations

**Quarter-Hour** (:15, :30, :45) — **Subtle acknowledgment** (2-3 sec)
- Frequency: 4x per hour
- Intensity: Low
- Purpose: Mark 15-minute intervals without disruption

**Half-Hour** (:00, :30) — **Medium celebration** (4-6 sec)
- Frequency: 2x per hour
- Intensity: Medium
- Purpose: Significant time marker, brief visual interest

**Top of Hour** (:00 only) — **Full spectacle** (8-12 sec)
- Frequency: 1x per hour
- Intensity: High
- Purpose: Hourly celebration, showcase LED capabilities

### Status Animations

**Wi-Fi Connecting** (up to 20 sec)
- Blue spinning dot on inner ring
- Shown during boot WiFi connection attempt
- Transitions to success/fail animation

**Wi-Fi Success** (1.5 sec)
- Green spinning dot on inner ring
- Confirms successful connection

**Wi-Fi Failure** (2 sec)
- Red spinning dot on inner ring
- Indicates connection timeout

**Button Press** (0.7 sec)
- Orange spinning dot on inner ring
- Immediate visual feedback for manual time adjustment

**Time Sync** (1.2-1.5 sec)
- Cyan spinning dot on inner ring
- Shown after NTP sync or browser sync

**Settings Saved** (1.3 sec)
- Purple spinning dot on inner ring
- Confirms EEPROM write success

**OTA Update In Progress** (variable duration)
- Blue spinning dot on inner ring (brighter: 0x0000FF)
- Shown during over-the-air firmware upload
- Duration: until upload completes or fails (typically 5-30 sec)
- No user action needed during update

**OTA Update Success** (2 sec)
- Green spinning dot on inner ring (bright: 0x00FF00)
- Confirms firmware written and verified successfully
- Device reboots automatically after animation

**OTA Update Failed** (2+ sec)
- Red spinning dot on inner ring (bright: 0xFF0000)
- Indicates upload error (connection lost, verification failed, etc.)
- Portal may reappear on next boot

**Center Idle** (continuous)
- Slow breathing pulse on center pixel (1.8 sec period)
- Only active when no other status showing
- Can be disabled via `statusAnimations` setting

---

## Quarter-Hour Animations (2-3 sec)

### 1. Sparkle Burst
**Mode**: `quarterAnimation = 1`

**Sequence**:
1. Center pixel flashes bright white (100ms)
2. Ripple effect to all quarter-hour markers (positions 0, 15, 30, 45 on outer ring)
3. Quick fade (20ms steps)

**Visual effect**: Starburst from center outward to cardinal points

### 2. Quarter Pulse
**Mode**: `quarterAnimation = 2`

**Sequence**:
1. All quarter-hour markers (every 5th LED on outer ring) brighten 2x
2. Hold for 500ms
3. Dim back to normal
4. Repeat pulse once more

**Visual effect**: Breathing on hour markers (12, 3, 6, 9 o'clock positions)

### 3. Ring Shimmer (Default)
**Mode**: `quarterAnimation = 3`

**Sequence**:
1. Outer ring brightens to 100% for 400ms, returns to normal (200ms)
2. Middle ring brightens to 100% for 400ms, returns to normal (200ms)
3. Inner ring brightens to 100% for 400ms, returns to normal (200ms)

**Visual effect**: Brightness wave cascading inward

---

## Half-Hour Animations (4-6 sec)

### 1. Rainbow Sweep (Default)
**Mode**: `halfHourAnimation = 1`

**Sequence**:
1. Rainbow color wave sweeps clockwise around outer ring (2 sec)
2. Simultaneously sweep middle and inner rings (2 sec)
3. Brief hold (1 sec)

**Visual effect**: Color wheel rotating through all rings

### 2. Dual Flash
**Mode**: `halfHourAnimation = 2`

**Sequence**:
1. Outer ring splits: left half red, right half cyan
2. Hold 300ms
3. Colors swap (left cyan, right red)
4. Hold 300ms
5. Repeat swap 2 more times (3 total)

**Visual effect**: Hemisphere color battle

### 3. Tidal Pulse
**Mode**: `halfHourAnimation = 3`

**Sequence**:
1. All LEDs dim to 25%
2. Outer ring wave pattern (sine wave brightness)
3. Middle ring counter-rotating wave
4. Inner ring breathing
5. All return to normal brightness together

**Visual effect**: Ocean wave simulation

---

## Top of Hour Animations (8-12 sec)

### 1. Hourly Chime (Original)
**Mode**: `hourAnimation = 1`

**Sequence**:
1. Sweep animation on outer ring
2. Middle ring synchronized pulse
3. Center pixel bright pulse (180ms period, 4 cycles)

**Visual effect**: Classic chime, maintains compatibility with original firmware

**Duration**: 6 seconds (legacy timing)

### 2. Firework Burst
**Mode**: `hourAnimation = 2`

**Sequence**:
1. All LEDs go dark (500ms pause)
2. Center pixel explodes bright white (100ms)
3. Color radiates outward: inner → middle → outer (400ms per ring)
4. Each ring "detonates" with random hue sparkles
5. Random twinkling for 3 seconds
6. Fade back to clock display

**Visual effect**: New Year's Eve firework

**Duration**: 8-9 seconds

### 3. Zenith Cascade
**Mode**: `hourAnimation = 3`

**Sequence**:
1. Golden/yellow color starts at 12 o'clock position (top)
2. Flows clockwise AND counter-clockwise simultaneously
3. Colors meet at 6 o'clock position (bottom) — 1.2 sec
4. Collision creates white flash at bottom (200ms)
5. Color drains upward like closing theater curtain (1.2 sec)
6. Clock display revealed underneath

**Visual effect**: Golden waterfall pooling at bottom, draining away

**Duration**: 6-7 seconds

### 4. Rainbow Spiral (Default)
**Mode**: `hourAnimation = 4`

**Sequence**:
1. Outer ring rainbow chase clockwise (2 laps, 2 sec)
2. Middle ring joins counter-clockwise (2 laps, 2 sec)
3. Inner ring joins clockwise (2 laps, 2 sec)
4. All three spinning simultaneously for 2 sec
5. Slow down together, freeze on current time colors

**Visual effect**: Triple-helix rainbow vortex

**Duration**: 10-11 seconds

### 5. Breathing Mandala
**Mode**: `hourAnimation = 5`

**Sequence**:
1. All rings pulse brightness 50% → 100% → 50% (1.5 sec per breath)
2. Each breath shifts hue slightly around color wheel
3. 6 breaths total (9 sec)
4. Final breath lands on configured hand colors

**Visual effect**: Meditative breathing flower

**Duration**: 9-10 seconds

---

## Implementation Reference

### Trigger Logic (loop)
```cpp
const ClockTime time = timeModel.get();
static uint8_t lastMinute = 255;

if (settingsStore.get().intervalAnimationsEnabled && time.minute != lastMinute && time.second == 0) {
  lastMinute = time.minute;
  
  if (time.minute == 0) {
    // Top of hour
    renderer.triggerHourAnimation(millis());
  } else if (time.minute == 30) {
    // Half-hour
    renderer.triggerHalfHourAnimation(millis());
  } else if (time.minute % 15 == 0) {
    // Quarter-hour
    renderer.triggerQuarterAnimation(millis());
  }
}
```

### Animation Method Signature
```cpp
void renderAnimationName(uint32_t now) {
  // Animation code here
  // Must restore clock display at end
}
```

### Key Helper Functions
```cpp
uint32_t scale(uint32_t color, uint8_t amount);        // Scale RGB by 0-255
uint32_t pulse(uint32_t color, uint32_t now, ...);     // Breathing effect
void setRingPixel(const RingConfig &ring, uint8_t idx, uint32_t color);
void setCenterPixel(uint32_t color);
```

---

## Adding New Animations

### Process
1. **Design** — Sketch timing and visual effect on paper
2. **Implement** — Add `renderMyAnimation(uint32_t now)` method to `ClockRenderer` class
3. **Integrate** — Add enum value to appropriate trigger (quarter/half/hour)
4. **Web UI** — Add option to dropdown selector
5. **Test** — Verify animation duration, clean return to clock display

### Guidelines
- **Respect brightness** — Use `settings_.get().dayBrightness` as reference
- **Time limit** — Quarter=3s, Half=6s, Hour=12s maximum
- **Clean exit** — Always return to accurate clock display
- **No blocking delays >50ms** — Use state machine for longer animations
- **Test at extremes** — Full brightness and minimum brightness
- **Test all variants** — 8" and 15" clocks may look different

### Example Template
```cpp
void renderMyAnimation(uint32_t now) {
  const ClockSettings &settings = settings_.get();
  strip_.clear();
  
  // Animation sequence here
  // Use strip_.show() to update display
  // Use delay() sparingly (non-blocking preferred)
  
  // Restore normal display at end
  timeModel.markDirty();  // Force full redraw
}
```

---

## Animation Timing Chart

| Trigger          | Frequency | Duration | Intensity | User Configurable |
|------------------|-----------|----------|-----------|-------------------|
| Quarter-hour     | 4x/hour   | 2-3 sec  | Low       | ✅ 3 variants + off |
| Half-hour        | 2x/hour   | 4-6 sec  | Medium    | ✅ 3 variants + off |
| Top of hour      | 1x/hour   | 8-12 sec | High      | ✅ 5 variants + off |
| Wi-Fi connecting | Boot only | <20 sec  | Low       | ❌ Always on       |
| Wi-Fi success    | Boot only | 1.5 sec  | Low       | ✅ statusAnimations|
| Wi-Fi failure    | Boot only | 2 sec    | Low       | ✅ statusAnimations|
| Button press     | User      | 0.7 sec  | Low       | ✅ statusAnimations|
| Time sync        | Variable  | 1.2 sec  | Low       | ✅ statusAnimations|
| Settings saved   | User      | 1.3 sec  | Low       | ✅ statusAnimations|
| Center idle      | Continuous| N/A      | Minimal   | ✅ statusAnimations|

---

## Future Animation Ideas

### Sensor-Triggered
- **Sunrise detected** (VEML7700): 5-minute warm color fade (dark blue → orange → yellow)
- **Sunset detected** (VEML7700): 5-minute cool color fade (yellow → orange → deep purple)
- **Storm darkness** (sudden lux drop): Lightning flash effect (random white strobes)
- **Motion detected** (lux spike): Welcome rainbow chase

### Calendar-Triggered
- **New Year (Jan 1 00:00)**: Firework burst + countdown
- **Valentine's Day**: Pink/red heartbeat pulse
- **Halloween**: Orange flicker with random LED dropouts (spooky)
- **Christmas**: Red/green alternating with snowflake sparkles
- **User birthday**: Confetti explosion, extended party mode

### Temperature-Triggered (Future BME280)
- **Heat wave** (>30°C): Red pulsing intensity
- **Cold snap** (<5°C): Blue icy shimmer
- **Comfortable** (18-24°C): Green ambient glow

### Weather-Triggered (Future API)
- **Rain forecast**: Blue droplets falling animation
- **Snow forecast**: White snowflakes drifting down
- **Thunderstorm**: Purple/white lightning flashes
- **Clear sky**: Golden sun rays radiating outward
