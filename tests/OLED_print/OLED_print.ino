#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_ADDR   0x3C

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: SSD1306 OLED (SDA=21, SCL=22, addr 0x3C) ==="));
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[OLED] not found - check wiring & address"));
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("OLED OK"));
  display.println(F("SDA=21 SCL=22"));
  display.println(F("addr 0x3C"));
  display.println(F("Harry Potter"));
  display.display();
}

void loop() {
  static uint32_t counter = 0;
  display.fillRect(0, 40, OLED_WIDTH, 16, SSD1306_BLACK);
  display.setCursor(0, 40);
  display.print(F("uptime s: "));
  display.print(millis() / 1000);
  display.setCursor(0, 52);
  display.print(F("tick: "));
  display.print(counter++);
  display.display();
  delay(500);
}