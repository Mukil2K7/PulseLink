# 💓 PulseLink — IoT Health Monitoring System

A real-time heart rate monitoring system built with ESP32 and Pulse Sensor, featuring a live web dashboard and WhatsApp emergency alerts.

## 🔧 Hardware
- ESP32 Microcontroller
- PulseSensor.com Analog Pulse Sensor
- Mobile Hotspot (WiFi)

## ✨ Features
- 📊 Live BPM monitoring with ECG chart
- 🚨 WhatsApp SOS alert via CallMeBot API
- 📋 BPM History with CSV download
- 💊 Pharmacy with UPI + Card payment
- 👤 Patient profile & medical records
- 🕐 Live clock display

## 🛠️ Tech Stack
- ESP32 WebServer
- HTML + Tailwind CSS + JavaScript
- Chart.js
- CallMeBot WhatsApp API

## ⚡ Circuit
| Sensor Pin | ESP32 Pin |
|------------|-----------|
| Signal (Purple) | GPIO 34 |
| VCC (Red) | 3.3V |
| GND (Black) | GND |

## 🚀 Setup
1. Install ESP32 board in Arduino IDE
2. Fill in WiFi credentials and CallMeBot API key
3. Upload CarePulse.ino to ESP32
4. Open browser → http://[ESP32-IP]
```

---

**STEP 5 — Add to LinkedIn**
- Go to LinkedIn → **Profile** → **Add section** → **Projects**
- Fill in:
  - **Name:** CarePulse — IoT Health Monitoring System
  - **Description:**
```
Built a real-time heart rate monitoring system using ESP32 microcontroller and pulse sensor. Features a live web dashboard with ECG chart, WhatsApp SOS emergency alerts via CallMeBot API, BPM history with CSV export, pharmacy with UPI/Card payment UI, and patient profile management. Entire web server is hosted on the ESP32 itself.
