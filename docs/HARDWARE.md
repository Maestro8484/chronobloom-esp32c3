# ChronoBloom ESP32-C3 — Hardware Specifications

## LED Configuration

### Ring Layout
- **Outer ring**: 60 LEDs (seconds hand + minute hand + quarter-hour markers)
- **Middle ring**: 24 LEDs (hour hand display + ambient fill)
- **Inner ring**: 12 LEDs (hour hand display + ambient fill)
- **Center pixel**: 1 LED (status indicator, breathing animation)

### Variants
**8" Clock (esp32c3_v3_8inch)**:
- Total: 97 LEDs on single strip
- Center pixel at index 96
- No sacrificial pixel — physical index 0 is the first ring LED (12 o'clock, outer ring)
- Ring offset: 0

**15" Clock (esp32c3_v3_15inch)**:
- Main strip: 96 LEDs (rings only)
- Separate center strip: 1 LED on GPIO20
- No sacrificial pixel — physical index 0 is the first ring LED
- Ring offset: 0

---

## Pin Assignments (XIAO ESP32-C3)

### NeoPixel Data
| Pin    | GPIO   | Function                  | Notes                          |
|--------|--------|---------------------------|--------------------------------|
| D10    | GPIO10 | NeoPixel rings (DIN)      | 300Ω resistor inline to strip |
| D7     | GPIO20 | Center pixel (DIN)        | 15" variant only               |

### User Input
| Pin    | GPIO   | Function     | Notes                  |
|--------|--------|--------------|------------------------|
| D1     | GPIO3  | Button UP    | INPUT_PULLUP, to GND   |
| D2     | GPIO4  | Button DOWN  | INPUT_PULLUP, to GND   |

⚠️ **Known Issue**: GPIO3/GPIO4 are JTAG TCK/TDI pins. USB data connection can cause spurious button ISR fires.

### I2C Bus (Sensors)
| Pin    | GPIO   | Function  | Wire Color | Connected Devices   |
|--------|--------|-----------|------------|---------------------|
| D4     | GPIO6  | I2C SDA   | Green      | VEML7700 (0x10)     |
| D5     | GPIO7  | I2C SCL   | Yellow     | Future: BME280/SHT31|

### Power
| Pin       | Function                | Notes                              |
|-----------|-------------------------|------------------------------------|
| 5V / VIN  | LED power rail          | External 5V supply (2-5A)          |
| GND       | Common ground           | ESP32 + LED supply must connect    |
| 3V3       | Logic power only        | **Do NOT power LEDs from 3.3V**    |

### Avoid These Pins
**GPIO2, GPIO8, GPIO9** — ESP32-C3 boot strapping pins. Do not use for any peripherals.

---

## Sensors

### VEML7700 Ambient Light Sensor
- **I2C Address**: 0x10 (default, fixed)
- **Range**: 0-120,000 lux with auto-gain
- **Resolution**: 16-bit
- **Power**: 3.3V (< 1mA typical)
- **Purpose**: Auto-brightness, sunrise/sunset detection
- **Library**: Adafruit_VEML7700

**Wiring**:
```
VEML7700 VCC → XIAO 3.3V
VEML7700 GND → XIAO GND
VEML7700 SDA → GPIO6 (green wire)
VEML7700 SCL → GPIO7 (yellow wire)
```

### Future: Temperature/Humidity Sensor
**BME280** (recommended):
- I2C Address: 0x76 or 0x77
- Temp + Humidity + Pressure
- Can coexist with VEML7700 on same I2C bus

**SHT31** (alternative):
- I2C Address: 0x44
- Temp + Humidity (higher accuracy than BME280)
- Can coexist with VEML7700

---

## Physical Construction

### Materials
**Frame**:
- UV glow-in-dark PLA (3D printed)
- Charges from LEDs and ambient room light
- Glows softly in darkness

**Diffuser**:
- Parchment paper (laser-safe, optimal light distribution)
- Creates soft petal-like bloom effect visible in photos

**Face**:
- Laser-cut acrylic (clear or frosted)
- 600mm x 410mm CO2 laser bed used for 15" variant

### Scale Variants
**8.5" Clock**:
- 85% scale (0.85x original)
- Maximizes Bambu Labs P1S print bed (256mm x 256mm)
- Single-piece parts where possible

**15" Clock**:
- 200% scale (2x original)
- Parts split into thirds for assembly
- Requires laser cutter for acrylic face (600mm x 410mm bed)
- Modular assembly with alignment pins

### Assembly Notes
- Frame prints in thirds, joined with alignment pins
- Diffuser sandwiched between frame and acrylic face
- LED strips mounted to back of frame with cable ties or hot glue
- Wire management: route all wires through center, exit at bottom

---

## Power Requirements

### LED Power Consumption
- **Per LED**: ~60mA at full white (all channels 100%)
- **Typical usage**: ~20mA per LED (colored, not full brightness)
- **Total worst-case** (97 LEDs): 5.8A @ 5V (rare, only during full-white test)
- **Typical operation**: 1.5-2A @ 5V (clock display mode)

### Recommended Power Supply
- **Minimum**: 5V 2A (wall adapter)
- **Recommended**: 5V 3A (handles animations + brightness spikes)
- **Best**: 5V 5A (headroom for future expansion)

### ESP32-C3 Power
- **Active WiFi**: ~120mA @ 3.3V
- **Deep sleep**: ~10μA (not used in this project)
- **Powered via**: USB-C (XIAO board) or VIN pin

---

## Environmental Specifications

### Operating Conditions
- **Temperature**: 0°C to 50°C (indoor use)
- **Humidity**: 20-80% RH (non-condensing)
- **Light sensitivity**: VEML7700 auto-adjusts gain (0.0036 - 120,000 lux range)

### Mounting
- **Wall-mounted**: Keyhole slots in back frame (15" variant)
- **Desktop stand**: 3D printed kickstand (8" variant)
- **Weight**: ~500g (8"), ~1.2kg (15")

---

## Firmware Pin Mapping (Code Reference)

### Build Flags (platformio.ini)
```ini
-D LED_DATA_PIN=10
-D CENTER_PIXEL_PIN=20           # 15" variant only
-D CENTER_PIXEL_SEPARATE_OUTPUT=1 # 15" variant only
-D LUX_SENSOR_I2C_SDA=6
-D LUX_SENSOR_I2C_SCL=7
```

### Ring Config (main.cpp)
```cpp
// RING_PIXEL_OFFSET = 0 (no sacrificial pixel)
constexpr RingConfig RING_OUTER_60  = {60, RING_PIXEL_OFFSET, true};      // indexes 0-59
constexpr RingConfig RING_MIDDLE_24 = {24, RING_PIXEL_OFFSET + 60, true}; // indexes 60-83
constexpr RingConfig RING_INNER_12  = {12, RING_PIXEL_OFFSET + 84, true}; // indexes 84-95
```

### LED Indexing
- **Physical strip index 0**: Outer ring LED 0 (12 o'clock position)
- **Physical strip indexes 0-59**: Outer ring (seconds/minutes)
- **Physical strip indexes 60-83**: Middle ring (hours)
- **Physical strip indexes 84-95**: Inner ring (hours)
- **Physical strip index 96**: Center pixel (8" variant)
- **Separate strip index 0**: Center pixel (15" variant on GPIO20)

> Sacrificial pixel removed in firmware v2.0.2. If upgrading from an older build, rewire GPIO10 data line directly to DIN of physical LED index 0 (first ring LED).

---

## Testing & Calibration

### I2C Bus Scan (Check Sensor Connection)
```cpp
Wire.begin(6, 7);
for (uint8_t addr = 1; addr < 127; addr++) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    Serial.print("Found I2C device at 0x");
    Serial.println(addr, HEX);
  }
}
// Expected: 0x10 (VEML7700)
```

### LED Mapping Test
Add to `setup()` for visual verification:
```cpp
// Light each ring in sequence
for (int i = 0; i < 60; i++) {
  strip.setPixelColor(i + RING_PIXEL_OFFSET, strip.Color(255, 0, 0));
  strip.show();
  delay(50);
}
```

### Brightness Calibration
- **Dark room** (0-1 lux): Target brightness ~15
- **Evening light** (10 lux): Target brightness ~60
- **Indoor day** (100 lux): Target brightness ~120
- **Bright window** (1000 lux): Target brightness ~200
- **Direct sun** (10,000+ lux): Target brightness 255

Adjust logarithmic curve in `LuxSensor::autoBrightness()` if needed.
