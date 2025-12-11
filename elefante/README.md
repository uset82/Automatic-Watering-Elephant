# RP2350-Zero - Sensor Hub & Display

This firmware handles environmental sensors (DHT11, soil moisture) and the OLED display.

## Hardware Connections

| Pin | Function |
|-----|----------|
| GP4 | OLED SDA |
| GP5 | OLED SCL |
| GP6 | DHT11 Data |
| GP10 | TABLE_READY ← Arduino (via voltage divider!) |
| GP11 | WATER_DONE → Arduino |
| GP26 | Soil Moisture Sensor (ADC) |

## Build & Upload

```bash
cd elefante
pio run --target upload
```

> ⚠️ If upload fails, hold BOOTSEL while plugging in USB.

## Serial Output (115200 baud)

Outputs: `Temp: 25.5 Humidity: 45 Pot: 2100 Motor: 0 OFF`
