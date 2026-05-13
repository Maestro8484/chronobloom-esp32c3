# ChronoBloom Function Inventory

Auto-generated from `src/main.cpp`. Do not edit manually — run `tools/gen_symmap.py` to regenerate.

Total functions: 96

| # | Function | Lines | Span |
|---|----------|-------|------|
| 1 | `configureWiFiHostname` | 156–161 | 6 |
| 2 | `writeStatusLed` | 163–169 | 7 |
| 3 | `LuxSensor::begin` | 178–191 | 14 |
| 4 | `LuxSensor::available` | 193–193 | 1 |
| 5 | `LuxSensor::lux` | 195–245 | 51 |
| 6 | `LuxSensor::autoBrightness` | 249–251 | 3 |
| 7 | `LuxSensor::autoBrightnessTarget` | 254–256 | 3 |
| 8 | `LuxSensor::autoBrightnessCached` | 258–295 | 38 |
| 9 | `SettingsStore::begin` | 375–382 | 8 |
| 10 | `SettingsStore::get` | 384–384 | 1 |
| 11 | `SettingsStore::update` | 386–389 | 4 |
| 12 | `SettingsStore::defaults` | 392–404 | 13 |
| 13 | `SettingsStore::valid` | 406–424 | 19 |
| 14 | `SettingsStore::sanitize` | 426–456 | 31 |
| 15 | `SettingsStore::save` | 458–461 | 4 |
| 16 | `TimeModel::get` | 475–480 | 6 |
| 17 | `TimeModel::set` | 482–490 | 9 |
| 18 | `TimeModel::tickOneSecond` | 492–497 | 6 |
| 19 | `TimeModel::addMinutes` | 499–509 | 11 |
| 20 | `TimeModel::addHours` | 511–518 | 8 |
| 21 | `TimeModel::markDirty` | 520–524 | 5 |
| 22 | `TimeModel::consumeDirty` | 526–532 | 7 |
| 23 | `TimeModel::incrementSecondNoLock` | 535–545 | 11 |
| 24 | `ClockRenderer::ClockRenderer` | 582–585 | 4 |
| 25 | `ClockRenderer::begin` | 587–600 | 14 |
| 26 | `ClockRenderer::setLuxSensor` | 602–602 | 1 |
| 27 | `ClockRenderer::setStatus` | 604–609 | 6 |
| 28 | `ClockRenderer::triggerQuarterAnimation` | 611–618 | 8 |
| 29 | `ClockRenderer::triggerHalfHourAnimation` | 620–627 | 8 |
| 30 | `ClockRenderer::triggerHourAnimation` | 629–636 | 8 |
| 31 | `ClockRenderer::animating` | 638–638 | 1 |
| 32 | `ClockRenderer::renderAnimFrame` | 640–658 | 19 |
| 33 | `ClockRenderer::needsFullAnimationFrame` | 660–662 | 3 |
| 34 | `ClockRenderer::needsCenterAnimationFrame` | 664–666 | 3 |
| 35 | `ClockRenderer::renderCenterAnimationFrame` | 668–688 | 21 |
| 36 | `ClockRenderer::render` | 690–727 | 38 |
| 37 | `ClockRenderer::nightActive` | 730–736 | 7 |
| 38 | `ClockRenderer::effectiveBrightness` | 738–753 | 16 |
| 39 | `ClockRenderer::renderFace` | 755–774 | 20 |
| 40 | `ClockRenderer::renderSeconds` | 776–794 | 19 |
| 41 | `ClockRenderer::renderMinutes` | 796–800 | 5 |
| 42 | `ClockRenderer::renderHours` | 802–821 | 20 |
| 43 | `ClockRenderer::secondColor` | 823–832 | 10 |
| 44 | `ClockRenderer::ringColor` | 834–838 | 5 |
| 45 | `ClockRenderer::renderStatus` | 840–877 | 38 |
| 46 | `ClockRenderer::renderHourlyChime` | 879–884 | 6 |
| 47 | `ClockRenderer::tickAnimation` | 886–1105 | 220 |
| 48 | `ClockRenderer::chimeActive` | 1107–1109 | 3 |
| 49 | `ClockRenderer::statusActive` | 1111–1113 | 3 |
| 50 | `ClockRenderer::logShow` | 1115–1139 | 25 |
| 51 | `ClockRenderer::dim` | 1141–1146 | 6 |
| 52 | `ClockRenderer::scale` | 1148–1155 | 8 |
| 53 | `ClockRenderer::pulse` | 1157–1164 | 8 |
| 54 | `ClockRenderer::centerIdleActive` | 1166–1168 | 3 |
| 55 | `ClockRenderer::renderCenterIdle` | 1170–1176 | 7 |
| 56 | `ClockRenderer::setCenterPixel` | 1178–1192 | 15 |
| 57 | `ClockRenderer::setSacrificialPixelDark` | 1194–1200 | 7 |
| 58 | `ClockRenderer::setRingPixel` | 1202–1210 | 9 |
| 59 | `TemperatureInput::begin` | 1234–1234 | 1 |
| 60 | `TemperatureInput::available` | 1235–1235 | 1 |
| 61 | `TemperatureInput::celsius` | 1236–1236 | 1 |
| 62 | `TimeSync::TimeSync` | 1242–1242 | 1 |
| 63 | `TimeSync::begin` | 1244–1254 | 11 |
| 64 | `TimeSync::syncNow` | 1256–1280 | 25 |
| 65 | `TimeSync::loop` | 1282–1292 | 11 |
| 66 | `TimeSync::synced` | 1294–1294 | 1 |
| 67 | `TimeSync::lastDeltaSec` | 1296–1296 | 1 |
| 68 | `setupWiFi` | 1311–1340 | 30 |
| 69 | `setupOTA` | 1343–1378 | 36 |
| 70 | `WebUi::setLuxSensor` | 1396–1396 | 1 |
| 71 | `WebUi::begin` | 1398–1456 | 59 |
| 72 | `WebUi::loop` | 1458–1462 | 5 |
| 73 | `WebUi::enabled` | 1464–1464 | 1 |
| 74 | `WebUi::setupRoutes` | 1467–1767 | 301 |
| 75 | `WebUi::settingsJson` | 1769–1807 | 39 |
| 76 | `WebUi::boolJson` | 1809–1811 | 3 |
| 77 | `WebUi::clampByte` | 1813–1817 | 5 |
| 78 | `WebUi::clampWord` | 1819–1823 | 5 |
| 79 | `WebUi::colorHex` | 1825–1829 | 5 |
| 80 | `WebUi::hexNibble` | 1831–1836 | 6 |
| 81 | `WebUi::parseColor` | 1838–1849 | 12 |
| 82 | `WebUi::wifiStatusText` | 1851–1870 | 20 |
| 83 | `WebUi::htmlPage` | 1872–2007 | 136 |
| 84 | `FocusReminderScheduler::FocusReminderScheduler` | 2024–2025 | 2 |
| 85 | `FocusReminderScheduler::checkAndFire` | 2027–2062 | 36 |
| 86 | `FocusReminderScheduler::getDayOfWeek` | 2065–2070 | 6 |
| 87 | `FocusReminderScheduler::triggerReminderAnimation` | 2072–2083 | 12 |
| 88 | `ButtonInput::begin` | 2094–2097 | 4 |
| 89 | `ButtonInput::poll` | 2099–2152 | 54 |
| 90 | `ButtonInput::consumeUp` | 2156–2161 | 6 |
| 91 | `ButtonInput::consumeDown` | 2162–2167 | 6 |
| 92 | `ButtonInput::consumeUpPress` | 2170–2170 | 1 |
| 93 | `ButtonInput::consumeDownPress` | 2171–2171 | 1 |
| 94 | `logRuntimeStatus` | 2221–2261 | 41 |
| 95 | `setup` | 2263–2346 | 84 |
| 96 | `loop` | 2348–2414 | 67 |

---
*Generated by `tools/gen_symmap.py`*
