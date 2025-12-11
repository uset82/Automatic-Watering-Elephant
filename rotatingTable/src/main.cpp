#if defined(__has_include) && __has_include(<Arduino.h>)
#include <Arduino.h>
#elif !defined(__has_include) && defined(ARDUINO)
#include <Arduino.h>
#else
// Minimal Arduino stubs so clang/desktop parses without Arduino SDK
#include <chrono>
#include <thread>

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}
inline void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline unsigned long millis() {
  using namespace std::chrono;
  return static_cast<unsigned long>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count());
}

struct SerialMock {
  void begin(unsigned long) {}
  template <typename T> void print(const T &) {}
  template <typename T> void println(const T &) {}
  void println() {}
} Serial;
#endif

/*
  Rotating Table + RP2350 Pump Control

  SKIP SOIL SENSOR - Manual trigger after learning

  Arduino controls: Rotating table motor
  RP2350 controls: Pump (GP7)

  Communication:
  - Arduino Pin 6 -> RP2350 GP10 (TABLE_READY) - use voltage divider!
  - RP2350 GP11 -> Arduino Pin 7 (WATER_DONE)

  Flow:
  1. Learning Mode: Save positions A and B
  2. Simulation starts automatically after both saved
  3. Move to A -> signal TABLE_READY -> wait for WATER_DONE
  4. Move to B -> signal TABLE_READY -> wait for WATER_DONE
  5. Sleep
*/

// Motor pins
const int MOTOR_IN1 = 8;
const int MOTOR_IN2 = 9;
const int MOTOR_IN3 = 10;
const int MOTOR_IN4 = 11;

// Button pins
const int BUTTON_A = 2;
const int BUTTON_B = 3;
const int RESET_BTN = 5;

// PUMP CONTROL (direct from Arduino - bypassing RP2350)
const int PUMP_PIN = 12;                  // Relay control
const bool RELAY_ACTIVE_LOW = true;       // Most relay modules are active-LOW
const unsigned long PUMP_DURATION = 3000; // 3 seconds

// Half-step sequence
const int halfStep[8][4] = {{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0},
                            {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 1},
                            {0, 0, 0, 1}, {1, 0, 0, 1}};
int stepPhase = 0;

// Position tracking
long currentPos = 0;
long positionA = -1;
long positionB = -1;

// Mode flags
bool learningMode = true;

void stepMotor(bool clockwise) {
  if (clockwise) {
    stepPhase++;
    if (stepPhase >= 8)
      stepPhase = 0;
    currentPos++;
  } else {
    stepPhase--;
    if (stepPhase < 0)
      stepPhase = 7;
    currentPos--;
  }

  digitalWrite(MOTOR_IN1, halfStep[stepPhase][0]);
  digitalWrite(MOTOR_IN2, halfStep[stepPhase][1]);
  digitalWrite(MOTOR_IN3, halfStep[stepPhase][2]);
  digitalWrite(MOTOR_IN4, halfStep[stepPhase][3]);
  delay(2);
}

void stopMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, LOW);
}

void moveToPosition(long target) {
  long stepsNeeded = target - currentPos;
  bool clockwise = stepsNeeded > 0;
  long absSteps = stepsNeeded > 0 ? stepsNeeded : -stepsNeeded;

  Serial.print("Moving ");
  Serial.print(absSteps);
  Serial.println(clockwise ? " steps CW" : " steps CCW");

  for (long i = 0; i < absSteps; i++) {
    stepMotor(clockwise);
  }
  stopMotor();
  Serial.println("Arrived!");
}

void pumpON() {
  digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
  Serial.println(">>> PUMP ON!");
}

void pumpOFF() {
  digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  Serial.println(">>> PUMP OFF!");
}

void waterPlant() {
  Serial.println(">>> Starting pump for 3 seconds...");
  pumpON();

  // Blink LED while pumping
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_BUILTIN, i % 2);
    delay(500);
  }

  pumpOFF();
  Serial.println(">>> Watering complete!");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
}

void runSimulation() {
  Serial.println("");
  Serial.println("=============================");
  Serial.println("   SIMULATION STARTING");
  Serial.println("   (Arduino controls pump!)");
  Serial.println("=============================");
  Serial.println("");

  // Step 1: Move to Position A and water
  Serial.println(">> Step 1: Moving to Position A...");
  moveToPosition(positionA);
  waterPlant();

  delay(1000);

  // Step 2: Move to Position B and water
  Serial.println(">> Step 2: Moving to Position B...");
  moveToPosition(positionB);
  waterPlant();

  // Step 3: Sleep
  Serial.println("");
  Serial.println("=============================");
  Serial.println("   SIMULATION COMPLETE");
  Serial.println("   >>> SLEEPING <<<");
  Serial.println("=============================");

  stopMotor();
  pumpOFF(); // Make sure pump is OFF
  digitalWrite(LED_BUILTIN, LOW);
  learningMode = false;
}

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);

  // Button pins
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);

  // Pump relay (Arduino direct control)
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW); // Start with pump OFF

  // LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("");
  Serial.println("=============================");
  Serial.println("  ROTATING TABLE + PUMP");
  Serial.println("  (Skip soil sensor)");
  Serial.println("=============================");
  Serial.println("");
  Serial.println("LEARNING MODE:");
  Serial.println("  HOLD Button A = rotate CW");
  Serial.println("  CLICK Button A = save Position A");
  Serial.println("  HOLD Button B = rotate CCW");
  Serial.println("  CLICK Button B = save Position B");
  Serial.println("");
  Serial.println("After both saved -> Pump runs at each position!");
  Serial.println("");
}

void loop() {
  // === RESET BUTTON - CHECK FIRST (works even in sleep mode!) ===
  if (digitalRead(RESET_BTN) == LOW) {
    Serial.println("");
    Serial.println(">>> RESET! <<<");
    positionA = -1;
    positionB = -1;
    currentPos = 0;
    learningMode = true;
    pumpOFF(); // Make sure pump is OFF
    digitalWrite(LED_BUILTIN, HIGH);

    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }

    while (digitalRead(RESET_BTN) == LOW)
      delay(10);

    Serial.println("Reset complete. Ready for new positions!");
    Serial.println("");
    return;
  }

  // Sleep mode - do nothing except check reset
  if (!learningMode) {
    delay(100);
    return;
  }

  // === BUTTON A ===
  if (digitalRead(BUTTON_A) == LOW) {
    unsigned long pressStart = millis();
    bool isHold = false;

    while (digitalRead(BUTTON_A) == LOW) {
      if (millis() - pressStart > 300) {
        isHold = true;
        stepMotor(true);
      }
    }
    stopMotor();

    if (!isHold) {
      positionA = currentPos;
      Serial.print(">>> Position A SAVED at step ");
      Serial.println(positionA);

      Serial.print("    A=");
      Serial.print(positionA);
      Serial.print(" B=");
      Serial.println(positionB);

      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
      }

      if (positionA != -1 && positionB != -1) {
        Serial.println(">>> BOTH POSITIONS SET! Starting simulation...");
        delay(500);
        runSimulation();
      } else {
        Serial.println(">>> Waiting for both positions to be set...");
      }
    }
    delay(100);
  }

  // === BUTTON B ===
  if (digitalRead(BUTTON_B) == LOW) {
    unsigned long pressStart = millis();
    bool isHold = false;

    while (digitalRead(BUTTON_B) == LOW) {
      if (millis() - pressStart > 300) {
        isHold = true;
        stepMotor(false);
      }
    }
    stopMotor();

    if (!isHold) {
      positionB = currentPos;
      Serial.print(">>> Position B SAVED at step ");
      Serial.println(positionB);

      Serial.print("    A=");
      Serial.print(positionA);
      Serial.print(" B=");
      Serial.println(positionB);

      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
      }

      if (positionA != -1 && positionB != -1) {
        Serial.println(">>> BOTH POSITIONS SET! Starting simulation...");
        delay(500);
        runSimulation();
      } else {
        Serial.println(">>> Waiting for both positions to be set...");
      }
    }
    delay(100);
  }

  // === OUTPUT FOR SERIAL PLOTTER (every 500ms) ===
  static unsigned long lastPlotterOutput = 0;
  if (millis() - lastPlotterOutput > 500) {
    lastPlotterOutput = millis();
    // Format: Servo: XX Motor: XXX ON/OFF
    int servoAngle = map(currentPos % 23210, 0, 23210, 0, 180);
    Serial.print("Servo: ");
    Serial.print(servoAngle);
    Serial.print(" Motor: ");
    Serial.print(digitalRead(PUMP_PIN) == (RELAY_ACTIVE_LOW ? LOW : HIGH) ? 255
                                                                          : 0);
    Serial.print(" ");
    Serial.println(digitalRead(PUMP_PIN) == (RELAY_ACTIVE_LOW ? LOW : HIGH)
                       ? "ON"
                       : "OFF");
  }

  delay(10);
}
