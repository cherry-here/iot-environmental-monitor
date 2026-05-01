# IoT Environmental Monitoring System

> Real-time air quality, temperature, humidity and pressure monitoring using ESP32

## Features
- Live sensor readings every 5 seconds
- MQTT data publishing to cloud broker
- Built-in web dashboard (access via browser on same WiFi)
- JSON API endpoint for integration

## Hardware
| Component | Purpose |
|-----------|---------|
| ESP32 DevKit | Main controller + WiFi |
| DHT22 | Temperature and Humidity |
| MQ135 | Air Quality (CO2, NH3) |
| BMP280 | Atmospheric Pressure |

## Setup
1. Install Arduino IDE + ESP32 board package
2. Install libraries: PubSubClient, DHT, Adafruit BMP280, ArduinoJson
3. Edit WiFi credentials in code
4. Upload > open Serial Monitor at 115200 baud
5. Visit http://<ESP32_IP>/data for live JSON

## Author
**Mothi Charan Naik Desavath** - Embedded Systems Engineer