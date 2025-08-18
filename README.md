# Weather Radar

This project combines an ESP32-based weather radar sweep, an embedded browser display, and a Python cockpit display client.

## Files

- `weather_radar.ino`: ESP32 firmware for the servo sweep, RF sampling, IMU updates, GPS parsing, and HTTP endpoints.
- `html.h`: Embedded web UI served by the ESP32.
- `display.py`: Desktop Python display for viewing sweep data from the ESP32 or a demo feed.

## Hardware

- ESP32 development board
- Servo motor on pin 13
- Radar or RF RSSI source on pin 34
- GY-85 IMU on I2C pins 21 and 22
- GPS on Serial2 pins 16 and 17

## Firmware Setup

1. Install the ESP32 Arduino board package.
2. Install the `ESP32Servo` and `TinyGPS++` libraries.
3. Open `weather_radar.ino` in the Arduino IDE.
4. Select your ESP32 board and upload the sketch.

The ESP32 starts a Wi-Fi access point with these defaults:

- SSID: `WxRadar`
- Password: `wx1234567`
- Web UI: `http://192.168.4.1`

## Python Display

Install the desktop dependencies:

```bash
pip install requests matplotlib numpy cartopy
```

Run against the ESP32:

```bash
python display.py --host 192.168.4.1
```

Run in demo mode:

```bash
python display.py --demo
```