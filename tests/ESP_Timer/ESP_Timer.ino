#include <Arduino.h>
#include "esp_timer.h"

static portMUX_TYPE tickMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t   tickCounter = 0;
esp_timer_handle_t  periodicTimer;

const uint32_t TIMER_TICK_MS = 100;

void onTimerTick(void* arg) {
  portENTER_CRITICAL(&tickMux);
  tickCounter++;
  portEXIT_CRITICAL(&tickMux);
}

uint32_t getTicks() {
  uint32_t t;
  portENTER_CRITICAL(&tickMux);
  t = tickCounter;
  portEXIT_CRITICAL(&tickMux);
  return t;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== TEST: esp_timer periodic 100 ms tick ==="));
  const esp_timer_create_args_t timerArgs = {
    .callback = &onTimerTick,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "test_tick"
  };
  esp_timer_create(&timerArgs, &periodicTimer);
  esp_timer_start_periodic(periodicTimer, (uint64_t)TIMER_TICK_MS * 1000ULL);
}

void loop() {
  static uint32_t last = 0;
  uint32_t t = getTicks();
  if (t - last >= 10) {
    last = t;
    Serial.print(F("ticks = "));
    Serial.print(t);
    Serial.print(F("  (~"));
    Serial.print(t * TIMER_TICK_MS / 1000.0f, 1);
    Serial.println(F(" s since boot)"));
  }
}