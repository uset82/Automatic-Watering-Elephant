/*
  Automatic Watering Elephant - RP2350-Zero Version
  INTEGRATED with Arduino Rotating Table + OLED + Sensors

  Functionality:
  1. Displays Status, Temp, Humidity, Soil Moisture on OLED
  2. Responds to Arduino TABLE_READY signal
  3. Controls Pump (or simulates it) and signals WATER_DONE

  Hardware Connections:
    SENSORS/DISPLAY:
    - GP26        <- Soil Moisture Sensor (ADC)
    - GP6         <- DHT11 Data
    - GP4         -> OLED SDA
    - GP5         -> OLED SCL

    PUMP CONTROL:
    - GP9         -> Relay IN (pump control) - GP7 was damaged

    ARDUINO COMMUNICATION:
    - GP10        <- Arduino Pin 6 (TABLE_READY) - via voltage divider!
    - GP11        -> Arduino Pin 7 (WATER_DONE)
    - GND         -> Arduino GND (COMMON GROUND!)
*/

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>

// ============================================================
// HARDWARE PIN DEFINITIONS
// ============================================================
// Communication with Arduino
const int TABLE_READY_PIN = 10; // GP10 (INPUT) <- Arduino Pin 6
const int WATER_DONE_PIN = 11;  // GP11 (OUTPUT) -> Arduino Pin 7

// Peripherals
const int RELAY_PIN = 9;        // GP9 for Relay (GP7 damaged)
const int SOIL_SENSOR_PIN = 26; // GP26 (A0)
const int DHT_PIN = 6;          // GP6

// I2C for OLED (Use Wire1 for RP2040/RP2350 typically, or Wire with setSDA/SCL)
const int I2C_SDA = 4;
const int I2C_SCL = 5;

// ============================================================
// SETTINGS
// ============================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define DHTTYPE DHT11

const bool RELAY_ACTIVE_LOW = true;          // Active-LOW relay
const unsigned long PUMP_DURATION_MS = 3000; // 3 seconds

// ============================================================
// OBJECTS & GLOBALS
// ============================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHTTYPE);

bool pumpRunning = false;
unsigned long pumpStartTime = 0;
int wateringStep = 0; // 0=Idle, 1=Watering Plant 1, 2=Watering Plant 2

void updateDisplay(String status, float temp, float hum);

// ============================================================
// HARDWARE CONTROL FUNCTIONS
// ============================================================

void setPump(bool on) {
  pumpRunning = on;
  if (on) {
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
    Serial.println(">> PUMP ON!");
  } else {
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
    Serial.println(">> PUMP OFF!");
  }
}

bool isTableReady() { return digitalRead(TABLE_READY_PIN) == HIGH; }

void setWaterDone(bool done) {
  digitalWrite(WATER_DONE_PIN, done ? HIGH : LOW);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial

  // 1. Initialize I2C
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  // 2. Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    // Don't freeze, continue without display
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RP2350 Initializing..."));
    display.display();
  }

  // 3. Initialize DHT
  dht.begin();

  // 4. Initialize Pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW); // OFF

  pinMode(TABLE_READY_PIN, INPUT);
  pinMode(WATER_DONE_PIN, OUTPUT);
  digitalWrite(WATER_DONE_PIN, LOW);

  pinMode(SOIL_SENSOR_PIN, INPUT);
  analogReadResolution(12);

  Serial.println(">> System Ready. Waiting for Arduino...");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  // Read Sensors
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t))
    t = 0.0;
  if (isnan(h))
    h = 0.0;

  int soilRaw = analogRead(SOIL_SENSOR_PIN);
  int soilPercent = map(soilRaw, 4095, 1500, 0, 100); // Adjusted for 12-bit
  soilPercent = constrain(soilPercent, 0, 100);

  // State Machine
  String statusMsg = "Idle";

  if (!pumpRunning) {
    // IDLE - waiting for TABLE_READY
    if (isTableReady()) {
      Serial.println(">> TABLE_READY received!");

      // Determine which plant (flip-flop logic could be better but simple
      // toggle works)
      wateringStep++;
      if (wateringStep > 2)
        wateringStep = 1;

      statusMsg = "Watering Plant " + String(wateringStep);
      updateDisplay(statusMsg, t, h);

      // Activate Pump
      setPump(true);
      pumpStartTime = millis();
    } else {
      statusMsg = "Waiting for Table...";
      if (wateringStep > 0 && millis() % 2000 < 1000) {
        statusMsg = "Next: Plant " + String(wateringStep == 2 ? 1 : 2);
      }
    }
  } else {
    // PUMPING
    statusMsg = "PUMP ON!";
    if (millis() - pumpStartTime >= PUMP_DURATION_MS) {
      setPump(false);

      statusMsg = "Done!";
      updateDisplay(statusMsg, t, h);

      // Signal Arduino
      Serial.println(">> Signaling WATER_DONE...");
      setWaterDone(true);
      delay(500);
      setWaterDone(false);

      // Wait for Table to move away
      while (isTableReady()) {
        delay(50);
      }
    }
  }

  // Update Display periodically if not critical state
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000 && !pumpRunning) {
    updateDisplay(statusMsg, t, h);
    lastUpdate = millis();

    // === OUTPUT FOR SERIAL PLOTTER ===
    // Output each value on the same line with clear separators
    Serial.print("Temp: ");
    Serial.print(t, 1);
    Serial.print(" Humidity: ");
    Serial.print((int)h);
    Serial.print(" Pot: ");
    Serial.print(soilRaw);
    Serial.print(" Motor: ");
    Serial.print(pumpRunning ? 255 : 0);
    Serial.print(" ");
    Serial.println(pumpRunning ? "ON" : "OFF");
  }

  delay(50);
}

void updateDisplay(String status, float temp, float hum) {
  display.clearDisplay();

  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Watering Elephant"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Status
  display.setCursor(0, 15);
  display.print(F("Status: "));
  display.print(status);

  // Sensors
  display.setCursor(0, 35);
  display.print(F("Temp: "));
  display.print(temp, 1);
  display.print(F(" C"));

  display.setCursor(0, 45);
  display.print(F("Hum:  "));
  display.print(hum, 1);
  display.print(F(" %"));

  display.display();
}
