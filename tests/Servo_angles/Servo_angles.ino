#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13

Servo ventServo;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Vent servo sweep on GPIO13 ==="));
  ventServo.setPeriodHertz(50);
  ventServo.attach(SERVO_PIN, 500, 2400);
}

void loop() {
  Serial.println(F("-> 0 deg (closed)"));
  ventServo.write(0);
  delay(1500);

  Serial.println(F("-> 45 deg"));
  ventServo.write(45);
  delay(1500);

  Serial.println(F("-> 90 deg (open)"));
  ventServo.write(90);
  delay(1500);

  Serial.println(F("-> 45 deg"));
  ventServo.write(45);
  delay(1500);
}