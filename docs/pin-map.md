# Pin Map Reference

**Project:** Micro-Climate Nursery System (COMP50069 — Scenario 2)  
**Board:** ESP32 DevKit V1

---

## Table 1: ESP32 Pin Assignments

| ESP32 Pin | Connected To | Signal Type | Notes |
|-----------|--------------|-------------|-------|
| GPIO4 | DHT22 Data | Digital Input | 10kΩ pull-up to 3.3V (may be onboard) |
| GPIO34 | LDR | Analog Input (ADC) | Requires 10kΩ voltage divider resistor to GND |
| GPIO13 | SG90 Servo (PWM) | PWM Output | Signal wire (orange); powered from 3.3V |
| GPIO5 | LED1 (Grow Light 1) | Digital Output | 220Ω current-limiting resistor in series |
| GPIO2 | LED2 (Grow Light 2) | Digital Output | 220Ω current-limiting resistor in series; onboard LED pin |
| GPIO27 | LED3 (Grow Light 3) | Digital Output | 220Ω current-limiting resistor in series |
| GPIO26 | Push Button | Digital Input | Active LOW; internal pull-up enabled |
| GPIO21 | OLED SDA | I2C Data | SSD1306 display at I2C address 0x3C |
| GPIO22 | OLED SCL | I2C Clock | — |
| 3V3 | VCC rail | Power | Supplies DHT22, OLED, LDR, button |
| GND | GND rail | Ground | Common ground for all components |

---

## Power & Ground Summary

| Rail | Voltage | Feeds |
|------|---------|-------|
| 3V3 | 3.3 V | DHT22, LDR divider, OLED, Push button pull-up |
| 5V (VIN) | 5 V | SG90 Servo (optional — servo can also run on 3.3V) |
| GND | 0 V | All components share common ground |

> **Note:** The SG90 servo can draw peak currents up to ~500 mA. If powered from the ESP32's 3.3V regulator during physical build, consider an external 5V supply to prevent brownouts.

---

## Wiring Diagram Reference

- Annotated physical wiring: [docs\Wokwi_Wiring_Overview-Figure_1.png](/docs/Wokwi_Wiring_Overview-Figure_1.png)
- Schematic-style pin reference: [docs\Pin_Connection_Guide-Figure_2.png](/docs/Pin_Connection_Guide-Figure_2.png)