/**
 * @file    NurseryFirmwear.ino
 * @brief   ESP32 firmware for COMP50069 Scenario 2 - "The Automated Commercial
 *          Micro-Climate Nursery".
 *
 * @details Implements the three operational modes described in the report:
 *            - AUTONOMOUS      (with sub-states IDLE, VENT_OPEN, BURST_VENTING,
 *                                EMERGENCY_COOLING)
 *            - MANUAL_OVERRIDE (vent locked open for maintenance)
 *            - SENSOR_FAULT    (safe posture on DHT22 failure)
 *
 *          Wiring (matches the supplied Wokwi diagram.json):
 *            DHT22 data  -> GPIO4
 *            LDR         -> GPIO34 
 *            Vent servo  -> GPIO13 (PWM via ESP32Servo/LEDC)
 *            Grow LED 1  -> GPIO5  (through 220R to GND)
 *            Grow LED 2  -> GPIO2  (through 220R to GND)
 *            Grow LED 3  -> GPIO27 (through 220R to GND)
 *            Pushbutton  -> GPIO26 (other leg to GND, INPUT_PULLUP)
 *            OLED SSD1306-> I2C, SDA=GPIO21, SCL=GPIO22 (VCC, RES to 3.3V, GND, DC, CS to GND after soldering)
 *
 *          Required embedded features demonstrated:
 *           - Digital I/O      : button input (interrupt), 
 *                                 LED outputs via digitalWrite
 *           - UART             : status logging + "outside <C>", "threshold
 *                                 <C>", "settemp <C>|auto", and
 *                                 "sethumidity <%>|auto" serial commands
 *                                 (the latter two allows the environmental 
 *                                 conditions to be set through the serial monitor 
 *                                 for testing, bypassing the live DHT22)
 *            - ADC              : LDR light level on GPIO34
 *            - Timers/Interrupts: esp_timer hardware periodic timer drives
 *                                 all scheduling (no delay() in loop());
 *                                 button uses a GPIO interrupt
 *            - PWM              : servo angle calculated from sensor logic
 *            - I2C              : SSD1306 OLED status display
 *            - Modes            : AUTONOMOUS / MANUAL_OVERRIDE / SENSOR_FAULT
 *            - Scheduled/timed  : burst-venting cycle, DHT sample interval,
 *                                 OLED refresh, long-press fault reset - all
 *                                 driven off the hardware timer tick counter
 *
 * @note    Libraries required (Arduino Library Manager):
 *            "DHT sensor library" (Adafruit) + "Adafruit Unified Sensor"
 *            "ESP32Servo" (Kevin Harrington / madhephaestus)
 *            "Adafruit SSD1306" + "Adafruit GFX Library"

 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "esp_timer.h"

// --------------------------------------------------------------------------------------------------------------------------
// Pin definitions (per wiring diagram - docs\Wokwi_Wiring_Overview-Figure_1.png and /docs/Pin_Connection_Guide-Figure_2.png)
// --------------------------------------------------------------------------------------------------------------------------
#define DHT_PIN     4
#define DHT_TYPE    DHT22
#define LDR_AO_PIN  34
#define SERVO_PIN   13
#define LED1_PIN    5
#define LED2_PIN    2
#define LED3_PIN    27
#define BUTTON_PIN  26

#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_ADDR    0x3C

// ---------------------------------------------------------------------------
// Tunable thresholds
//
// Adjustable at runtime via Serial Monitor (see S2 in report MoSCoW table):
//   - tempOpenC          → serial command: "threshold <C>"
//   - outsideTempC       → serial command: "outside <C>"
//
// Fixed at compile time (require re-upload to change):
//   - tempCloseC, tempEmergencyC, tempEmergencyExitC
//   - OUTSIDE_COLD_C
// ---------------------------------------------------------------------------
float tempOpenC          = 28.0f;  ///< Vent opens above this temperature
float tempCloseC         = 26.0f;  ///< Vent closes below this (hysteresis)
float tempEmergencyC     = 35.0f;  ///< Emergency cooling entry
float tempEmergencyExitC = 32.0f;  ///< Emergency cooling exit (hysteresis)
float outsideTempC       = 15.0f;  ///< Operator-configured "outside" temp
const float OUTSIDE_COLD_C = 10.0f;

int   LDR_DARK_THRESHOLD = 1500;   ///< 0-4095 ADC counts; tune to your module
const bool LDR_LOGIC_INVERTED = false; ///< flip if your module reads opposite

// Timing constants, expressed in 100 ms "ticks" produced by the hardware timer
const uint32_t TIMER_TICK_MS       = 100;
const uint32_t SENSOR_READ_TICKS   = 25;   // 2.5 s   (DHT22 needs >=2 s)
const uint32_t OLED_REFRESH_TICKS  = 5;    // 0.5 s
const uint32_t BURST_OPEN_TICKS    = 50;   // 5 s
const uint32_t BURST_CLOSE_TICKS   = 300;  // 30 s
const unsigned long LONG_PRESS_MS  = 3000; // 3 s hold to clear SENSOR_FAULT
const unsigned long DEBOUNCE_MS    = 40;

const int VENT_CLOSED_DEG = 0;
const int VENT_OPEN_DEG   = 90;

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
/** @brief Top-level operating modes required by the assignment brief. */
enum SystemMode { AUTONOMOUS, MANUAL_OVERRIDE, SENSOR_FAULT };

/** @brief Sub-states that only apply while in AUTONOMOUS mode. */
enum AutoSubState { IDLE, VENT_OPEN, BURST_VENTING, EMERGENCY_COOLING };
//Learning point -> enum concept

SystemMode   currentMode = AUTONOMOUS;
AutoSubState subState    = IDLE;

// ---------------------------------------------------------------------------
// Sensor / actuator state
// ---------------------------------------------------------------------------
float temperatureC   = NAN;
float humidityPct    = NAN;
int   lightRaw        = 0;
bool  lastReadingValid = false;
int   lastVentAngle    = -1; // force first write

// Serial test overrides - when active, readSensors() uses these instead of
// the live DHT22, so the operator can drive the state machine precisely
// through the serial monitor (see handleSerialCommands()).
bool  tempOverrideActive     = false;
float tempOverrideValueC     = 25.0f;
bool  humidityOverrideActive = false;
float humidityOverrideValuePct = 50.0f;

// ---------------------------------------------------------------------------
// Hardware timer scheduling (replaces delay()/millis() polling in loop())
// ---------------------------------------------------------------------------
static portMUX_TYPE tickMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t   tickCounter = 0;      ///< incremented every TIMER_TICK_MS
esp_timer_handle_t  periodicTimer;

uint32_t lastSensorTick = 0;
uint32_t lastOledTick   = 0;
uint32_t burstPhaseTick = 0;
bool     burstVentOpenPhase = true;

// ---------------------------------------------------------------------------
// Button handling (GPIO interrupt + software debounce/long-press detection)
// ---------------------------------------------------------------------------
volatile bool          buttonChanged   = false;
volatile bool          buttonLevelLow  = false; // true while pressed
volatile unsigned long buttonEdgeMs    = 0;

// ---------------------------------------------------------------------------
// Peripheral objects
// ---------------------------------------------------------------------------
DHT dht(DHT_PIN, DHT_TYPE);
Servo ventServo;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void IRAM_ATTR onButtonEdge();
void onTimerTick(void* arg);
uint32_t getTicks();
void readSensors();
void evaluateSensorFault();
void updateAutonomousLogic(uint32_t ticks);
void applyVentAngle(int angleDeg);
void setGrowLights(bool on);
void updateGrowLights();
void updateOLED();
void handleSerialCommands();
void handleButtonLogic(uint32_t ticks);

/**
 * @brief   Arduino setup routine: configure peripherals, start the hardware
 *          timer, and attach the button interrupt.
 * @return  void
 */
/**
 * @brief   Arduino setup routine: configure peripherals, start the hardware
 *          timer, and attach the button interrupt.
 * @return  void
 */
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);

  dht.begin();

  ventServo.setPeriodHertz(50);
  ventServo.attach(SERVO_PIN, 500, 2400);
  applyVentAngle(VENT_CLOSED_DEG);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[FAULT] SSD1306 OLED not found - check wiring"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonEdge, CHANGE);

  const esp_timer_create_args_t timerArgs = {
    .callback = &onTimerTick,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "nursery_tick"
  };
  esp_timer_create(&timerArgs, &periodicTimer);
  esp_timer_start_periodic(periodicTimer, (uint64_t)TIMER_TICK_MS * 1000ULL);

  Serial.println(F("=== Micro-Climate Nursery booted: AUTONOMOUS / IDLE ==="));
}

/**
 * @brief   Main loop. Entirely non-blocking - every scheduled action is
 *          gated by the hardware-timer tick counter, never by delay().
 * @return  void
 */
void loop() {
  uint32_t ticks = getTicks();

  handleSerialCommands();
  handleButtonLogic(ticks);

  if (ticks - lastSensorTick >= SENSOR_READ_TICKS) {
    lastSensorTick = ticks;
    readSensors();
    evaluateSensorFault();
  }

  switch (currentMode) {
    case AUTONOMOUS:
      updateAutonomousLogic(ticks);
      break;
    case MANUAL_OVERRIDE:
      applyVentAngle(VENT_OPEN_DEG); // locked open for maintenance
      break;
    case SENSOR_FAULT:
      applyVentAngle(VENT_CLOSED_DEG); // safe posture
      break;
  }

  updateGrowLights();

  if (ticks - lastOledTick >= OLED_REFRESH_TICKS) {
    lastOledTick = ticks;
    updateOLED();
  }
}

// ---------------------------------------------------------------------------
// Timer / interrupt infrastructure
// ---------------------------------------------------------------------------

/**
 * @brief   esp_timer callback fired every TIMER_TICK_MS. Increments a
 *          protected tick counter used by loop() to schedule all timed
 *          behaviour without delay().
 * @param   arg Unused callback argument (required by esp_timer API).
 * @return  void
 */
void onTimerTick(void* arg) {
  portENTER_CRITICAL(&tickMux);
  tickCounter++;
  portEXIT_CRITICAL(&tickMux);
}

/**
 * @brief   Thread-safe read of the current tick counter.
 * @return  uint32_t Number of TIMER_TICK_MS periods elapsed since boot.
 */
uint32_t getTicks() {
  uint32_t t;
  portENTER_CRITICAL(&tickMux);
  t = tickCounter;
  portEXIT_CRITICAL(&tickMux);
  return t;
}

/**
 * @brief   GPIO interrupt service routine for the pushbutton. Records only
 *          the edge time and level; all debounce/long-press logic runs in
 *          handleButtonLogic() outside interrupt context.
 * @return  void
 */
void IRAM_ATTR onButtonEdge() {
  buttonLevelLow = (digitalRead(BUTTON_PIN) == LOW);
  buttonEdgeMs = millis();
  buttonChanged = true;
}

/**
 * @brief   Processes button edges recorded by the ISR: debounces, detects
 *          short presses (mode toggle) and long presses (>=3 s, used only
 *          to clear SENSOR_FAULT).
 * @param   ticks Current hardware-timer tick count (unused directly here,
 *                kept for API symmetry with the other update functions).
 * @return  void
 */
void handleButtonLogic(uint32_t ticks) {
  static bool pressedState = false;
  static unsigned long pressStartMs = 0;
  // NOTE: press and release each get their own debounce timestamp. Sharing
  // a single timestamp between both edges caused a real bug: Wokwi's
  // simulated click fires press+release faster than one debounce window
  // apart, so the release edge was being discarded and the button latched
  // "pressed" forever after the very first click.
  static unsigned long lastPressMs = 0;
  static unsigned long lastReleaseMs = 0;

  if (!buttonChanged) return;

  bool levelLow;
  unsigned long edgeMs;
  noInterrupts();
  levelLow = buttonLevelLow;
  edgeMs   = buttonEdgeMs;
  buttonChanged = false;
  interrupts();

  if (levelLow && !pressedState) {
    // Falling edge = button just pressed
    if (edgeMs - lastPressMs < DEBOUNCE_MS) return; // debounce presses only
    lastPressMs = edgeMs;
    pressedState = true;
    pressStartMs = edgeMs;
  } else if (!levelLow && pressedState) {
    // Rising edge = button just released
    if (edgeMs - lastReleaseMs < DEBOUNCE_MS) return; // debounce releases only
    lastReleaseMs = edgeMs;
    pressedState = false;
    unsigned long heldFor = edgeMs - pressStartMs;

    if (currentMode == SENSOR_FAULT) {
      if (heldFor >= LONG_PRESS_MS && lastReadingValid) {
        currentMode = AUTONOMOUS;
        subState = IDLE;
        Serial.println(F("[RECOVER] Long-press reset accepted -> AUTONOMOUS/IDLE"));
      } else {
        Serial.println(F("[FAULT] Reset requires a valid DHT reading AND a 3s hold"));
      }
    } else if (currentMode == AUTONOMOUS) {
      currentMode = MANUAL_OVERRIDE;
      Serial.println(F("[MODE] Button press -> MANUAL_OVERRIDE (vent locked open)"));
    } else if (currentMode == MANUAL_OVERRIDE) {
      currentMode = AUTONOMOUS;
      subState = IDLE;
      Serial.println(F("[MODE] Button press -> AUTONOMOUS"));
    }
  }
}

// ---------------------------------------------------------------------------
// Sensors
// ---------------------------------------------------------------------------

/**
 * @brief   Samples temperature, humidity, and light level. Temperature and
 *          humidity each come from the live DHT22 unless a serial test
 *          override is active (see "settemp"/"sethumidity" commands), in
 *          which case the operator-supplied value is used instead - this
 *          lets you drive the whole state machine from the serial monitor
 *          without needing exact DHT22 slider positions in Wokwi.
 * @return  void
 */
void readSensors() {
  temperatureC = tempOverrideActive ? tempOverrideValueC : dht.readTemperature();
  humidityPct  = humidityOverrideActive ? humidityOverrideValuePct : dht.readHumidity();
  lightRaw     = analogRead(LDR_AO_PIN);

  Serial.print(F("[SENSOR] T="));
  Serial.print(temperatureC);
  Serial.print(tempOverrideActive ? F("C(override)") : F("C"));
  Serial.print(F(" H="));
  Serial.print(humidityPct);
  Serial.print(humidityOverrideActive ? F("%(override)") : F("%"));
  Serial.print(F(" Light="));
  Serial.println(lightRaw);
}

/**
 * @brief   Validates the most recent DHT22 reading and, on failure, forces
 *          the system into SENSOR_FAULT mode (unless already recovering).
 * @return  void
 */
void evaluateSensorFault() {
  bool valid = !isnan(temperatureC) && !isnan(humidityPct) &&
               temperatureC >= -40.0f && temperatureC <= 85.0f &&
               humidityPct  >= 0.0f   && humidityPct  <= 100.0f;

  lastReadingValid = valid;

  if (!valid && currentMode != SENSOR_FAULT) {
    currentMode = SENSOR_FAULT;
    Serial.println(F("[FAULT] DHT22 invalid/disconnected -> SENSOR_FAULT"));
  }
}

// ---------------------------------------------------------------------------
// Autonomous mode logic (state machine + burst venting)
// ---------------------------------------------------------------------------

/**
 * @brief   Evaluates the AUTONOMOUS sub-state machine (IDLE / VENT_OPEN /
 *          BURST_VENTING / EMERGENCY_COOLING) using hysteresis thresholds,
 *          then drives the vent servo accordingly. Burst venting timing is
 *          entirely tick-based (non-blocking).
 * @param   ticks Current hardware-timer tick count.
 * @return  void
 */
void updateAutonomousLogic(uint32_t ticks) {
  bool cold = outsideTempC < OUTSIDE_COLD_C;

  switch (subState) {
    case IDLE:
      if (temperatureC > tempEmergencyC) {
        subState = EMERGENCY_COOLING;
      } else if (temperatureC > tempOpenC && cold) {
        subState = BURST_VENTING;
        burstPhaseTick = ticks;
        burstVentOpenPhase = true;
      } else if (temperatureC > tempOpenC && !cold) {
        subState = VENT_OPEN;
      }
      break;

    case VENT_OPEN:
      if (temperatureC > tempEmergencyC) {
        subState = EMERGENCY_COOLING;
      } else if (cold) {
        subState = BURST_VENTING;
        burstPhaseTick = ticks;
        burstVentOpenPhase = true;
      } else if (temperatureC <= tempCloseC) {
        subState = IDLE;
      }
      break;

    case BURST_VENTING:
      if (temperatureC > tempEmergencyC) {
        subState = EMERGENCY_COOLING;
      } else if (!cold) {
        subState = VENT_OPEN;
      } else if (temperatureC <= tempCloseC) {
        subState = IDLE;
      }
      break;

    case EMERGENCY_COOLING:
      if (temperatureC <= tempEmergencyExitC) {
        subState = VENT_OPEN;
      }
      break;
  }

  switch (subState) {
    case IDLE:
      applyVentAngle(VENT_CLOSED_DEG);
      break;
    case VENT_OPEN:
    case EMERGENCY_COOLING:
      applyVentAngle(VENT_OPEN_DEG);
      break;
    case BURST_VENTING:
      if (burstVentOpenPhase) {
        applyVentAngle(VENT_OPEN_DEG);
        if (ticks - burstPhaseTick >= BURST_OPEN_TICKS) {
          burstVentOpenPhase = false;
          burstPhaseTick = ticks;
        }
      } else {
        applyVentAngle(VENT_CLOSED_DEG);
        if (ticks - burstPhaseTick >= BURST_CLOSE_TICKS) {
          burstVentOpenPhase = true;
          burstPhaseTick = ticks;
        }
      }
      break;
  }
}

/**
 * @brief   Moves the vent servo to the requested angle, only issuing a new
 *          PWM write when the angle actually changes (reduces servo jitter).
 * @param   angleDeg Target angle in degrees (0 = closed, 90 = open).
 * @return  void
 */
void applyVentAngle(int angleDeg) {
  if (angleDeg != lastVentAngle) {
    ventServo.write(angleDeg);
    lastVentAngle = angleDeg;
  }
}

// ---------------------------------------------------------------------------
// Grow lights (LDR-driven, with SENSOR_FAULT override)
// ---------------------------------------------------------------------------

/**
 * @brief   Drives all three grow-light LEDs using the standard Arduino
 *          digitalWrite() API.
 * @param   on true to turn the grow lights on, false to turn them off.
 */
void setGrowLights(bool on) {
  digitalWrite(LED1_PIN, on ? HIGH : LOW);
  digitalWrite(LED2_PIN, on ? HIGH : LOW);
  digitalWrite(LED3_PIN, on ? HIGH : LOW);
}

/**
 * @brief   Decides whether the grow lights should be on: forced ON during
 *          SENSOR_FAULT (baseline lighting), otherwise driven by the LDR
 *          reading in both AUTONOMOUS and MANUAL_OVERRIDE modes.
 * @return  void
 */
void updateGrowLights() {
  if (currentMode == SENSOR_FAULT) {
    setGrowLights(true);
    return;
  }
  bool dark = LDR_LOGIC_INVERTED ? (lightRaw > LDR_DARK_THRESHOLD)
                                  : (lightRaw < LDR_DARK_THRESHOLD);
  setGrowLights(dark);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

/**
 * @brief   Redraws the SSD1306 OLED with live temperature, humidity, light
 *          level, current mode/sub-state, and vent status.
 * @return  void
 */
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print(F("T:"));
  display.print(isnan(temperatureC) ? -99 : temperatureC, 1);
  display.print(F("C H:"));
  display.print(isnan(humidityPct) ? -99 : humidityPct, 0);
  display.println(F("%"));

  display.print(F("Light: "));
  display.println(lightRaw);

  display.setTextSize(1);
  display.setCursor(0, 24);
  switch (currentMode) {
    case AUTONOMOUS: {
      display.println(F("MODE: AUTONOMOUS"));
      const char* names[] = {"IDLE", "VENT_OPEN", "BURST_VENT", "EMERGENCY"};
      display.print(F("State: "));
      display.println(names[subState]);
      break;
    }
    case MANUAL_OVERRIDE:
      display.setTextSize(1);
      display.println(F("MANUAL OVERRIDE"));
      display.println(F("VENTS LOCKED OPEN"));
      break;
    case SENSOR_FAULT:
      display.println(F("*** SENSOR FAULT ***"));
      display.println(F("DHT22 FAILED"));
      display.println(F("Hold btn 3s to reset"));
      break;
  }

  display.setCursor(0, 56);
  display.print(F("Vent:"));
  display.print(lastVentAngle);
  display.println(F(" deg"));

  display.display();
}

