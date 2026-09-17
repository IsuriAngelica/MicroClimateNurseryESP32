#include <Arduino.h>

#define LDR_AO_PIN 34

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: LDR analog read (GPIO34, ADC1) ==="));
  analogReadResolution(12);
  pinMode(LDR_AO_PIN, INPUT);
}

void loop() {
  int raw = analogRead(LDR_AO_PIN);
  float volts = (raw / 4095.0f) * 3.3f;
  Serial.print(F("LDR raw="));
  Serial.print(raw);
  Serial.print(F("  approx "));
  Serial.print(volts, 2);
  Serial.println(F(" V"));
  delay(500);
}