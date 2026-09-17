#include <Arduino.h>
#include <DHT.h>

#define DHT_PIN  4
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

enum SystemMode { AUTONOMOUS, MANUAL_OVERRIDE, SENSOR_FAULT };
SystemMode currentMode = AUTONOMOUS;
bool lastReadingValid  = false;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Sensor-fault detection ==="));
  dht.begin();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  bool valid = !isnan(t) && !isnan(h) &&
               t >= -40.0f && t <= 85.0f &&
               h >= 0.0f   && h <= 100.0f;

  lastReadingValid = valid;

  if (!valid && currentMode != SENSOR_FAULT) {
    currentMode = SENSOR_FAULT;
    Serial.println(F("[FAULT] DHT22 invalid -> SENSOR_FAULT"));
  } else if (valid && currentMode == SENSOR_FAULT) {
    Serial.println(F("[INFO] DHT22 recovered - still latched until long-press"));
  }

  Serial.print(F("T="));
  Serial.print(t);
  Serial.print(F(" H="));
  Serial.print(h);
  Serial.print(F(" valid="));
  Serial.print(valid ? "Y" : "N");
  Serial.print(F(" mode="));
  Serial.println((int)currentMode);

  delay(2500);
}