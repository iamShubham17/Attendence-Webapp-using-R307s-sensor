# ESP8266 Fingerprint Attendance System (R307 Sensor)

A **biometric attendance system** using the **ESP8266** and **R307 Fingerprint Sensor**, integrated with a **Firebase-powered Web Dashboard**.  
This project enables secure attendance management with fingerprint enrollment, scanning, and real-time employee logs.

---

## 🚀 Features
- Fingerprint-based authentication using **R307 sensor**
- REST API (via ESP8266 WebServer) to:
  - Enroll new fingerprints
  - Scan and verify attendance
  - Delete fingerprint data
  - Check sensor status
  - Get stored fingerprint count
- Admin dashboard with:
  - **Registration & Login (Firebase Auth)**
  - **Add employees with fingerprint scan**
  - **View & delete employees**
  - **Real-time login activity logs**
- Responsive and modern UI with glassmorphism effects
- WiFi-enabled system (can work in labs, offices, or classrooms)

---

## 🛠️ Tech Stack
### Hardware
- ESP8266 (NodeMCU / Wemos D1 Mini)
- R307 Fingerprint Sensor
- Voltage divider for TX (3.3V ↔ 5V safe level shifting)

### Firmware
- Arduino C++  
- Libraries:
  - `Adafruit_Fingerprint`
  - `ESP8266WiFi`
  - `ESP8266WebServer`
  - `ArduinoJson`
  - `SoftwareSerial`

### Web App
- HTML, CSS, JavaScript
- Firebase (Auth + Realtime Database)

---

## ⚡ Circuit Connections
