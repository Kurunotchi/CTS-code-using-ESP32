ESP32 Battery Capacity Testing Station (CTS)

A comprehensive 4-channel battery capacity tester for 18650 and other Li-ion cells, featuring real-time monitoring, cycle testing, and cloud logging to Google Sheets.

Features
4 Independent Channels (Slots A, B, C, D) for simultaneous testing

Real-time Monitoring of voltage, current, and capacity

Cycle Testing with configurable charge/discharge cycles

Simpson's Rule Integration for precise capacity calculation

Google Sheets Integration for automatic data logging

WiFi Dashboard with TCP server for Python-based remote monitoring

Persistent Storage of battery numbers using Preferences library

20x4 LCD Display with intuitive menu system

4x4 Keypad for local control

INA226 Power Sensors for accurate current/voltage measurement

Hardware Requirements
Core Components
ESP32 Development Board

20x4 I2C LCD Display (0x27 address)

4x4 Matrix Keypad

4x INA226 Power Sensors (addresses: 0x40, 0x41, 0x44, 0x45)

8x Relay Modules (2 per channel - charge/discharge control)

Relay Pin Mapping

Slot A: DMS (23), DCD (19)

Slot B: BMS (18), BCD (5)

Slot C: AMS (17), ACD (16)

Slot D: CMS (32), CCD (33)

Software Dependencies

Install these libraries via Arduino Library Manager:

Keypad

LiquidCrystal_I2C

INA226

WiFi

Preferences

ESP_Google_Sheet_Client

ArduinoJson

Academic Context

This project was developed as a capstone requirement for the Bachelor of Science in Electrical Engineering program at Palawan State University (PalSU). It demonstrates:

Power Electronics: Battery charging/discharging circuits

Embedded Systems: ESP32 programming and real-time control

Data Acquisition: INA226 sensor interfacing

Cloud Integration: Google Sheets API implementation

Numerical Methods: Simpson's Rule for capacity calculation

UI/UX Design: LCD and keypad interface design

Project Team

Student Researchers: Thirdy Randale D. Mangcucang, Jemuel Gabayno, and Lexus Gavilo.

Technical Consultant: Kurunotchi
