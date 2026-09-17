
/*
TEST LED1
#include <Arduino.h>

#define LED1_PIN 5

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: LED1 digitalWrite (GPIO5) ==="));
  pinMode(LED1_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
}

void loop() {
  digitalWrite(LED1_PIN, HIGH);
  Serial.println(F("LED1 HIGH"));
  delay(500);
  digitalWrite(LED1_PIN, LOW);
  Serial.println(F("LED1 LOW"));
  delay(500);
}

TEST LED2

#include <Arduino.h>

#define LED2_PIN 2

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: LED2 digitalWrite (GPIO2) ==="));
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED2_PIN, LOW);
}

void loop() {
  digitalWrite(LED2_PIN, HIGH);
  Serial.println(F("LED2 HIGH"));
  delay(500);
  digitalWrite(LED2_PIN, LOW);
  Serial.println(F("LED2 LOW"));
  delay(500);
}


TEST LED3
#include <Arduino.h>

#define LED3_PIN 27

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: LED3 digitalWrite (GPIO27) ==="));
  pinMode(LED3_PIN, OUTPUT);
  digitalWrite(LED3_PIN, LOW);
}

void loop() {
  digitalWrite(LED3_PIN, HIGH);
  Serial.println(F("LED3 HIGH"));
  delay(500);
  digitalWrite(LED3_PIN, LOW);
  Serial.println(F("LED3 LOW"));
  delay(500);
}
*/


// TEST ALL 3 LEDS TOGETHER (GROW LIGHTS)

#include <Arduino.h>

#define LED1_PIN 5
#define LED2_PIN 2
#define LED3_PIN 27

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: all three grow LEDs via digitalWrite ==="));
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
}

void loop() {
  Serial.println(F("Grow lights ON"));
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  digitalWrite(LED3_PIN, HIGH);
  delay(1000);

  Serial.println(F("Grow lights OFF"));
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  delay(1000);
}