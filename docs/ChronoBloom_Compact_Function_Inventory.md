# ChronoBloom Compact Function Inventory
Source: `FUNCTION_INVENTORY.md`, `src/main.cpp`, SHA256 `52d8af3a811cebaa0bc073832f71de9c4910d47fe27a22d490ac1fcd3d5c498b`.
Purpose: print-friendly edit map. Common edit targets only; internals condensed at bottom.

## Page 1 - Common / Reasonably Likely Edit Targets
| Function / Structure | Telegraph meaning | Edit when |
|---|---|---|
| `SettingsStore::defaults` | Factory defaults | Default palette, brightness, feature defaults. |
| `SettingsStore::valid` | EEPROM accept rules | Change when fields/ranges change. |
| `SettingsStore::sanitize` | Clamp user input | Safety bounds for saved settings. |
| `ClockSettings` | Saved settings schema | Add field -> bump SETTINGS_VERSION. |
| `RingConfig` | Physical LED map | Ring start index, count, direction. |
| `ClockRenderer::render` | Full frame pipeline | Overall draw order/priority. |
| `ClockRenderer::renderFace` | Static ring backdrop | Markers, filler, base colors. |
| `ClockRenderer::renderSeconds` | Second hand logic | Trail/progress/dot behavior. |
| `ClockRenderer::renderMinutes` | Minute dot | Minute placement/color behavior. |
| `ClockRenderer::renderHours` | Hour hand logic | 12h mapping, half-hour advance. |
| `ClockRenderer::effectiveBrightness` | Brightness selector | Manual/lux/scheduled dimming. |
| `ClockRenderer::nightActive` | Night window test | Scheduled dimming hours. |
| `ClockRenderer::setRingPixel` | Logical -> physical LED | Rotation, offset, direction fixes. |
| `ClockRenderer::setCenterPixel` | Center LED writer | Inline vs separate center output. |
| `ClockRenderer::setSacrificialPixelDark` | Keep buffer LED black | Sacrificial first LED handling. |
| `ClockRenderer::renderCenterIdle` | Center breathing pulse | Idle center animation style. |
| `ClockRenderer::renderStatus` | Status spinner/pulse | WiFi/save/error visual feedback. |
| `ClockRenderer::renderHourlyChime` | Top-hour sweep | Hourly chime visual style. |
| `ClockRenderer::tickAnimation` | Animation state machine | Quarter/half/hour effects. |
| `triggerQuarter/Half/HourAnimation` | Start selected animation | Selector mapping and no-op rules. |
| `LuxSensor::lux` | Read + smooth lux | Gain thresholds, EMA, settle time. |
| `LuxSensor::autoBrightnessCached` | Lux -> brightness ramp | Curve, min/max feel, ramp speed. |
| `TimeSync::begin` | NTP/TZ startup | Timezone and NTP servers. |
| `TimeSync::syncNow` | Apply valid NTP time | Epoch guard and localtime conversion. |
| `TimeSync::loop` | Periodic resync | Callback + 6-hour refresh behavior. |
| `TimeModel::set/addMinutes/addHours` | Manual time edits | Browser time buttons and sync. |
| `WebUi::begin` | WiFi/web bringup | AP fallback, mDNS, OTA, NTP order. |
| `WebUi::setupRoutes` | HTTP route table | Add/change browser endpoints. |
| `WebUi::settingsJson` | Settings API output | Expose new settings to UI/JS. |
| `WebUi::htmlPage` | On-device web UI | Labels, controls, JS preview. |
| `WebUi::parseColor` | Hex color parser | Color input validation behavior. |
| `WebUi::clampByte/clampWord` | POST bounds guards | Form min/max safety. |
| `FocusReminderScheduler::checkAndFire` | Reminder gatekeeper | Days, hours, interval firing. |
| `FocusReminderScheduler::getDayOfWeek` | Weekday source | Needs pre-NTP epoch guard. |
| `FocusReminderScheduler::triggerReminderAnimation` | Reminder animation map | Map options to effects. |
| `setup` | Boot sequence | Add subsystem init here. |
| `loop` | Main scheduler | Time tick, web, render priority. |
| `logRuntimeStatus` | Serial health dump | Debug fields and cadence. |

## Rules That Prevent Pain
| Rule | Why it matters |
|---|---|
| EEPROM rule | Changing ClockSettings layout requires SETTINGS_VERSION bump or stale EEPROM will bite. |
| Rotation rule | outerRingOffset is one outer-ring step; if one step worse, reverse direction. |
| Sacrificial LED rule | Do not remove setSacrificialPixelDark; buffer LED must stay black. |
| UI setting rule | New setting usually touches ClockSettings, defaults, valid, sanitize, settingsJson, setupRoutes/htmlPage. |
| Animation rule | Visual look lives mostly in tickAnimation; selector start lives in trigger*Animation. |

## Page 2 - Rarely Edited / Support Internals
These are real functions, but normally not the first place to edit unless debugging plumbing, boot, or timing.

- `configureWiFiHostname` - hostname plumbing before DHCP
- `writeStatusLed` - GPIO LED helper; no-op when STATUS_LED_PIN < 0
- `setupWiFi` - usually edit credentials/build flags, not logic
- `setupOTA` - OTA handlers/password/log text
- `LuxSensor::begin` - VEML7700 init only
- `LuxSensor::autoBrightness` - wrapper only
- `LuxSensor::autoBrightnessTarget` - diagnostics only
- `SettingsStore::begin/update` - EEPROM load/save wrapper
- `TimeModel::get/tickOneSecond/consumeDirty/incrementSecondNoLock` - clock plumbing
- `ClockRenderer::begin` - strip init/clear/show
- `ClockRenderer::renderAnimFrame` - animation render wrapper
- `ClockRenderer::needsFullAnimationFrame / needsCenterAnimationFrame` - 80 ms render gates
- `ClockRenderer::dim/scale/pulse` - color math helpers
- `ClockRenderer::chimeActive/statusActive/centerIdleActive` - small boolean gates
- `TemperatureInput` - disabled stub in current builds
- `WebUi::loop` - handleClient + ArduinoOTA pump
- `ClockTime` - h/m/s POD copy
- `AnimPhase / StatusMode` - enum IDs for animations/status

## Fast Edit Path Cheat Sheet
- **Color/default look:** `SettingsStore::defaults`, `renderFace`, hand renderers.
- **Clock physically rotated:** `setRingPixel`, `RingConfig`, `outerRingOffset`.
- **Sacrificial WS2812B:** `setSacrificialPixelDark`, ring offsets/config, pixel count.
- **Web UI change:** `htmlPage`, `setupRoutes`, `settingsJson`, clamps/sanitize.
- **New saved option:** `ClockSettings`, defaults, valid, sanitize, settingsJson, POST handler, version bump.
- **Brightness feels wrong:** `effectiveBrightness`, `LuxSensor::lux`, `autoBrightnessCached`.
- **Reminder weirdness:** `checkAndFire`, `getDayOfWeek`, `triggerReminderAnimation`.