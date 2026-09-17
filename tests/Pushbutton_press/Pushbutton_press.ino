/*
#include <Arduino.h>

#define BUTTON_PIN 26

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Pushbutton raw read (GPIO26, INPUT_PULLUP) ==="));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  int v = digitalRead(BUTTON_PIN);
  Serial.print(F("Button raw = "));
  Serial.println(v == LOW ? "LOW (pressed)" : "HIGH (released)");
  delay(200);
}

*/

// TESTING AND UNDERSTANDING RISING AND FALLING EDGE
#include <Arduino.h>

#define BUTTON_PIN 26   // same pin as your main firmware

// Mode enum mirroring your main firmware
enum SystemMode { AUTONOMOUS, MANUAL_OVERRIDE };
volatile SystemMode currentMode = AUTONOMOUS;

// ISR-shared flags
volatile bool          buttonChanged  = false;
volatile bool          buttonLevelLow = false;
volatile unsigned long buttonEdgeMs   = 0;

// Debounce window
const unsigned long DEBOUNCE_MS = 40;


void IRAM_ATTR onButtonEdge() {
  buttonLevelLow = (digitalRead(BUTTON_PIN) == LOW);
  buttonEdgeMs   = millis();
  buttonChanged  = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Interrupt-driven pushbutton mode toggle ==="));
  Serial.println(F("Press the button to switch AUTONOMOUS <-> MANUAL_OVERRIDE"));
  Serial.println(F("(Falling edge = press, Rising edge = release)"));

  // INPUT_PULLUP: pin idles HIGH, button pulls it LOW when pressed
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Attach ISR to BOTH edges so we can debounce press and release
  // independently (see main firmware for why this matters on Wokwi).
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN),
                  onButtonEdge,
                  CHANGE);

  Serial.print(F("Initial mode: "));
  Serial.println(currentMode == AUTONOMOUS ? F("AUTONOMOUS") : F("MANUAL_OVERRIDE"));
}

void loop() {
  // Main-loop processing of edges — never do this inside the ISR.
  static bool lastPressedState = false;
  static unsigned long lastPressMs   = 0;
  static unsigned long lastReleaseMs = 0;

  if (!buttonChanged) return;   // nothing new since last loop

  // Copy ISR-shared values atomically
  bool levelLow;
  unsigned long edgeMs;
  noInterrupts();
  levelLow      = buttonLevelLow;
  edgeMs        = buttonEdgeMs;
  buttonChanged = false;
  interrupts();

  // ---- Falling edge: button just PRESSED ----
  if (levelLow && !lastPressedState) {
    if (edgeMs - lastPressMs < DEBOUNCE_MS) return;   // bounce, ignore
    lastPressMs = edgeMs;
    lastPressedState = true;

    // Fire the mode toggle right here on the press edge
    if (currentMode == AUTONOMOUS) {
      currentMode = MANUAL_OVERRIDE;
      Serial.println(F("[EDGE] FALLING (press) -> MANUAL_OVERRIDE"));
    } else {
      currentMode = AUTONOMOUS;
      Serial.println(F("[EDGE] FALLING (press) -> AUTONOMOUS"));
    }

    Serial.print(F("       Mode is now: "));
    Serial.println(currentMode == AUTONOMOUS ? F("AUTONOMOUS") : F("MANUAL_OVERRIDE"));
  }

  // ---- Rising edge: button just RELEASED ----
  else if (!levelLow && lastPressedState) {
    if (edgeMs - lastReleaseMs < DEBOUNCE_MS) return;
    lastReleaseMs = edgeMs;
    lastPressedState = false;
    Serial.print(F("[EDGE] RISING  (release) after "));
    Serial.print(edgeMs - lastPressMs);
    Serial.println(F(" ms"));
  }
}