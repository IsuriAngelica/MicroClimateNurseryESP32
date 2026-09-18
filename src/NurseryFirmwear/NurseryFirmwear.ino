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

void setup() {
  

}

void loop() {
  

}

