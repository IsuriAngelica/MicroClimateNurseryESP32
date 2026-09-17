#include <Arduino.h>

float tempOpenC    = 28.0f;
float tempCloseC   = 26.0f;
float outsideTempC = 15.0f;

bool  tempOverrideActive = false;
float tempOverrideValueC = 25.0f;
bool  humidityOverrideActive = false;
float humidityOverrideValuePct = 50.0f;

void handleSerialCommands() {
  static String line;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        line.trim();
        if (line.startsWith("outside ")) {
          outsideTempC = line.substring(8).toFloat();
          Serial.print(F("[CMD] outside temperature set to "));
          Serial.println(outsideTempC);
        } else if (line.startsWith("threshold ")) {
          tempOpenC  = line.substring(10).toFloat();
          tempCloseC = tempOpenC - 2.0f;
          Serial.print(F("[CMD] vent-open threshold set to "));
          Serial.print(tempOpenC);
          Serial.print(F("C (close at "));
          Serial.print(tempCloseC);
          Serial.println(F("C)"));
        } else if (line.startsWith("settemp ")) {
          String arg = line.substring(8); arg.trim();
          if (arg == "auto") {
            tempOverrideActive = false;
            Serial.println(F("[CMD] indoor temperature back to live sensor"));
          } else {
            tempOverrideValueC = arg.toFloat();
            tempOverrideActive = true;
            Serial.print(F("[CMD] indoor temperature overridden to "));
            Serial.print(tempOverrideValueC);
            Serial.println(F("C (type 'settemp auto' to release)"));
          }
        } else if (line.startsWith("sethumidity ")) {
          String arg = line.substring(12); arg.trim();
          if (arg == "auto") {
            humidityOverrideActive = false;
            Serial.println(F("[CMD] humidity back to live sensor"));
          } else {
            humidityOverrideValuePct = arg.toFloat();
            humidityOverrideActive = true;
            Serial.print(F("[CMD] humidity overridden to "));
            Serial.print(humidityOverrideValuePct);
            Serial.println(F("% (type 'sethumidity auto' to release)"));
          }
        } else if (line == "status") {
          Serial.print(F("[STATUS] tempOpen="));
          Serial.print(tempOpenC);
          Serial.print(F(" tempClose="));
          Serial.print(tempCloseC);
          Serial.print(F(" outside="));
          Serial.print(outsideTempC);
          Serial.print(F(" tempOverride="));
          Serial.print(tempOverrideActive ? "ON" : "off");
          Serial.print(F(" humidityOverride="));
          Serial.println(humidityOverrideActive ? "ON" : "off");
        } else {
          Serial.println(F("[CMD] Unknown. Try: outside <C> | threshold <C> | settemp <C>|auto | sethumidity <%>|auto | status"));
        }
      }
      line = "";
    } else {
      line += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: Serial command parser ==="));
  Serial.println(F("Try: outside 5, threshold 30, settemp 40, settemp auto, status"));
}

void loop() {
  handleSerialCommands();
}