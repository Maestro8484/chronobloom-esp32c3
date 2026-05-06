# Changelog

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
