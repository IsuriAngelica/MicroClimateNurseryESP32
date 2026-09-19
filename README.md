# Micro-Climate Nursery System

ESP32 firmware for COMP50069 Scenario 2 - The Automated Commercial Micro-Climate Nursery.

## Overview
IoT-based embedded system that monitors temperature, humidity, and light levels, automatically controlling a servo vent and grow lights.

## Hardware
- ESP32 DevKit V1
- DHT22 Temperature & Humidity Sensor
- LDR (Light Dependent Resistor)
- SG90 Servo Motor (vent)
- 3x LEDs (grow lights)
- Push Button (manual override)
- SSD1306 OLED Display (I2C)

## Pin Mapping
| Component | ESP32 Pin |
|-----------|-----------|
| DHT22 Data | GPIO4 |
| LDR Analog | GPIO34 |
| Servo PWM | GPIO13 |
| LED1 | GPIO5 |
| LED2 | GPIO2 |
| LED3 | GPIO27 |
| Button | GPIO26 |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |

## Operating Modes
1. **AUTONOMOUS** - Automatic control with sub-states (IDLE, VENT_OPEN, BURST_VENTING, EMERGENCY_COOLING)
2. **MANUAL_OVERRIDE** - Vent locked open for maintenance
3. **SENSOR_FAULT** - Safe posture on DHT22 failure

## Serial Commands
- `outside <C>` - Set outside temperature
- `threshold <C>` - Set vent-open threshold
- `settemp <C>|auto` - Override indoor temperature
- `sethumidity <%>|auto` - Override humidity
- `status` - Print current state

## Wokwi Simulation
https://wokwi.com/projects/475586073236795393

## Build Instructions
1. Install libraries: DHT sensor library, ESP32Servo, Adafruit SSD1306, Adafruit GFX
2. Open `MicroClimateNursery.ino` in Arduino IDE
3. Select ESP32 Dev Module
4. Upload
