# 🌱 Automatic Plant Watering System - Wiring Guide

## System Overview

This system combines:
- **RP2350-Zero**: Brain - monitors soil moisture, controls pump, communicates with Arduino
- **Arduino Uno**: Muscles - controls rotating table stepper motor
- **2 Avocado Plants** on rotating table (same pot size, same germination date)
- **1 Soil Sensor** shared for both plants (identical conditions = same watering needs)

## Power Rails

| Rail | Voltage | Components |
|------|---------|------------|
| Logic Rail | 3.3V | RP2350, OLED, DHT11, Soil Sensor |
| Motor Rail | 5V | Arduino Uno, Stepper Motor, Relay VCC |
| Pump Rail | 5-6V External | Water Pump (via relay) |

⚠️ **IMPORTANT**: Common GND between ALL components!

---

## RP2350-Zero Pinout

### Sensors
| Pin | Function | Component |
|-----|----------|-----------|
| GP26 (ADC0) | Soil Moisture | Capacitive sensor (shared for both plants) |
| GP6 | DHT11 DATA | Temperature/Humidity sensor |
| GP4 | I2C SDA | OLED Display |
| GP5 | I2C SCL | OLED Display |

### Control
| Pin | Function | Component |
|-----|----------|-----------|
| GP7 | Relay IN | Pump control (active LOW) |

### Arduino Communication
| RP2350 Pin | Direction | Arduino Pin | Signal |
|------------|-----------|-------------|--------|
| GP8 | OUTPUT → | Pin 4 | WATER_SIGNAL |
| GP10 | ← INPUT | Pin 6 | TABLE_READY |
| GP11 | OUTPUT → | Pin 7 | WATER_DONE |
| GND | ↔ | GND | Common Ground |

---

## Arduino Uno Pinout

### Motor Control (28BYJ-48 Stepper via ULN2003)
| Pin | Function |
|-----|----------|
| Pin 8 | Motor IN1 |
| Pin 9 | Motor IN2 |
| Pin 10 | Motor IN3 |
| Pin 11 | Motor IN4 |

### Position Buttons
| Pin | Function |
|-----|----------|
| Pin 2 | Button 1 - Set Plant 1 Position |
| Pin 3 | Button 2 - Set Plant 2 Position |

### RP2350 Communication
| Arduino Pin | Direction | RP2350 Pin | Signal |
|-------------|-----------|------------|--------|
| Pin 4 | ← INPUT | GP8 | WATER_SIGNAL |
| Pin 6 | OUTPUT → | GP10 | TABLE_READY |
| Pin 7 | ← INPUT | GP11 | WATER_DONE |
| GND | ↔ | GND | Common Ground |

---

## ⚠️ Voltage Level Shifting

The RP2350 uses **3.3V logic**, Arduino Uno uses **5V logic**.

### RP2350 → Arduino (3.3V to 5V)
✅ **Safe!** Arduino reads 3.3V as HIGH (threshold ~2.5V)
- GP8 → Pin 4 (direct connection OK)
- GP11 → Pin 7 (direct connection OK)

### Arduino → RP2350 (5V to 3.3V)
⚠️ **Needs voltage divider!** 5V will damage RP2350!

**TABLE_READY Signal (Pin 6 → GP10):**
```
Arduino Pin 6 ──[1K]──┬──[2K]── GND
                      │
                      └─── RP2350 GP10

Voltage at GP10 = 5V × (2K / (1K + 2K)) = 3.33V ✓
```

---

## Soil Moisture Sensor Wiring

Single capacitive soil moisture sensor (shared for both plants):
```
Sensor      RP2350
──────      ──────
VCC    →    3.3V
GND    →    GND
AOUT   →    GP26
```

---

## DHT11 Wiring

```
DHT11       RP2350
─────       ──────
VCC    →    3.3V
GND    →    GND
DATA   →    GP6 (internal pull-up enabled)
```

---

## OLED Display Wiring (SSD1306 128x64)

```
OLED        RP2350
────        ──────
VCC    →    3.3V
GND    →    GND
SDA    →    GP4
SCL    →    GP5
```

---

## Relay Module Wiring

```
Relay       RP2350          External Power
─────       ──────          ──────────────
VCC    →    5V (from Arduino or external)
GND    →    GND
IN     →    GP7

Relay NO    →    Pump +
Relay COM   →    External 5-6V +
Pump -      →    External GND
```

---

## Watering Sequence

```
1. RP2350 detects soil is dry (< 30% moisture)
2. RP2350 sets WATER_SIGNAL = HIGH
3. Arduino moves table to Plant 1 position
4. Arduino sets TABLE_READY = HIGH
5. RP2350 runs pump for 5 seconds
6. RP2350 sets WATER_DONE = HIGH (pulse)
7. Arduino sees WATER_DONE, moves to Plant 2 position
8. Arduino sets TABLE_READY = HIGH
9. RP2350 runs pump for 5 seconds
10. RP2350 sets WATER_DONE = HIGH (pulse)
11. Both plants watered! System enters 1 minute cooldown
```

### Signal Timing

| Signal | Duration | Purpose |
|--------|----------|---------|
| WATER_SIGNAL | Entire watering cycle | Request watering for both plants |
| TABLE_READY | Until WATER_DONE pulse | Table is at plant position |
| WATER_DONE | 500ms pulse | Current plant watering complete |

---

## Setup Procedure

1. **Power on system** - Arduino starts in Learning Mode (LED ON)

2. **Set Plant 1 position:**
   - Wait for table to rotate to Plant 1
   - Press Button 1 (Pin 2)
   - LED blinks 3 times = Position saved

3. **Set Plant 2 position:**
   - Wait for table to rotate to Plant 2 (must be >90° from Plant 1)
   - Press Button 2 (Pin 3)
   - LED blinks 5 times = Position saved
   - System enters Watering Mode (LED OFF)

4. **Automatic operation:**
   - RP2350 monitors soil moisture
   - When soil is dry → water Plant 1 → water Plant 2 → cooldown
   - 1 minute cooldown between watering cycles

---

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| Table doesn't move | No signal from RP2350 | Check GP8 → Pin 4 wire |
| RP2350 crashes | 5V on GP10 | Add voltage divider! |
| Pump doesn't run | TABLE_READY not seen | Check voltage divider output |
| Only Plant 1 watered | WATER_DONE not received | Check GP11 → Pin 7 wire |
| Soil always reads 0% | Sensor not in soil | Push sensor into soil |
| Soil always reads 100% | Wrong calibration | Adjust SOIL_DRY_VALUE/SOIL_WET_VALUE |
