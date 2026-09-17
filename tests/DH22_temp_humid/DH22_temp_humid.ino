#include <Arduino.h>
#include <DHT.h>

#define DHT_PIN  4
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: DHT22 on GPIO4 ==="));
  dht.begin();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println(F("[DHT] read failed (NaN) - check wiring/pull-up"));
  } else {
    Serial.print(F("T="));
    Serial.print(t, 2);
    Serial.print(F(" C   H="));
    Serial.print(h, 2);
    Serial.println(F(" %"));
  }
  delay(2500);
}