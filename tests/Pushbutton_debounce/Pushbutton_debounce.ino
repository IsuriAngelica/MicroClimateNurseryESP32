#include <Arduino.h>

#define BUTTON_PIN 26
const unsigned long LONG_PRESS_MS = 3000;
const unsigned long DEBOUNCE_MS   = 40;

volatile bool          buttonChanged  = false;
volatile bool          buttonLevelLow = false;
volatile unsigned long buttonEdgeMs   = 0;

void IRAM_ATTR onButtonEdge() {
  buttonLevelLow = (digitalRead(BUTTON_PIN) == LOW);
  buttonEdgeMs   = millis();
  buttonChanged  = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Button interrupt + debounce + long-press ==="));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonEdge, CHANGE);
}

void loop() {
  static bool pressedState = false;
  static unsigned long pressStartMs = 0;
  static unsigned long lastPressMs = 0;
  static unsigned long lastReleaseMs = 0;

  if (!buttonChanged) return;

  bool levelLow;
  unsigned long edgeMs;
  noInterrupts();
  levelLow      = buttonLevelLow;
  edgeMs        = buttonEdgeMs;
  buttonChanged = false;
  interrupts();

  if (levelLow && !pressedState) {
    if (edgeMs - lastPressMs < DEBOUNCE_MS) return;
    lastPressMs = edgeMs;
    pressedState = true;
    pressStartMs = edgeMs;
    Serial.println(F("[EDGE] Press (debounced)"));
  } else if (!levelLow && pressedState) {
    if (edgeMs - lastReleaseMs < DEBOUNCE_MS) return;
    lastReleaseMs = edgeMs;
    pressedState = false;
    unsigned long heldFor = edgeMs - pressStartMs;
    Serial.print(F("[EDGE] Release, held "));
    Serial.print(heldFor);
    Serial.println(F(" ms"));
    if (heldFor >= LONG_PRESS_MS) {
      Serial.println(F("  -> LONG PRESS detected"));
    } else {
      Serial.println(F("  -> SHORT PRESS detected"));
    }
  }
}