# IoT_AirSensor
Air Quality Workshop – Build Your Own Sensor

# Air Quality Workshop – Build Your Own Sensor

Respiras aire todos los días… pero, ¿sabes qué hay en él?

This workshop introduces participants to the design and construction of a simple, fully functional air quality sensing device using low-cost hardware and accessible tools.

Participants will assemble, program, and test their own environmental sensor capable of measuring and logging real-world data.

---

## Overview

In this workshop, you will build a standalone air quality sensor based on the **Seeed Studio XIAO ESP32C6** and the **BME688 environmental sensor**.

The device:

- measures temperature, humidity, pressure, and gas response
- calculates a simple **relative IAQ (Indoor Air Quality) index**
- creates its own Wi-Fi network (Access Point)
- serves a local web interface accessible from your phone
- logs sensor data into a CSV file stored on the device
- runs on battery using an active / deep sleep cycle

This is a hands-on workshop focused on understanding how environmental sensing systems actually work.

---

## Learning Objectives

By the end of the workshop, participants will:

- understand how environmental sensors operate
- assemble and connect electronic components
- upload and modify firmware using Arduino IDE
- interpret sensor data and limitations
- visualize data through a simple web interface
- log and export data for further analysis

No previous experience in electronics or programming is required.

---

## Hardware

The system is built using:

- Seeed Studio XIAO ESP32C6
- BME688 environmental sensor (Bosch)
- LiPo battery (3.7V)
- jumper wires or soldered connections
- optional 3D printed enclosure

---

## How the Device Works

The system follows a cyclical operation designed to balance data quality and battery life:

### Measurement cycle

- The device wakes up
- Creates a Wi-Fi network
- Takes **10 measurements** (one every 30 seconds)
- Each measurement records:
  - temperature
  - humidity
  - pressure
  - gas resistance
  - simple IAQ
- Displays live data and running averages on a web page
- Stores **all raw samples** in a CSV file

### Power strategy

- ~5 minutes active (data collection + web server)
- ~5 minutes deep sleep
- Repeat cycle

This allows extended battery operation (~24–30 hours depending on conditions).

---

## Web Interface

When the device is awake:

1. Connect your phone to the Wi-Fi network:

AirSensor_BME688
Password: 12345678


2. Open your browser and go to:

http://192.168.4.1


From the web page you can:

- view real-time sensor values
- see running averages for the current cycle
- check number of stored records
- see file size
- download the CSV file
- clear stored data

---

## Data Logging

The system stores **raw sensor data**, not only averages.

Each row in the CSV includes:

timestamp,temp_C,humidity_percent,pressure_hPa,gas_kOhms,iaq_simple


Example:

2026-04-13 12:00:00,23.41,30.49,755.50,38.40,104.18
2026-04-13 12:00:30,23.43,30.61,755.53,38.12,104.93


---

## About IAQ (Important)

The IAQ value used in this project is a **simple relative index**, calculated as:

IAQ = 100 × (baseline / current gas resistance)


Where:

- higher IAQ = more detected gases (worse air)
- lower IAQ = fewer reactive gases (cleaner air)

⚠️ This is NOT:
- an official AQI
- not Bosch BSEC
- not a calibrated measurement

It is a **relative indicator**, useful for detecting changes and trends.

---

## Limitations

This system:

- does NOT measure PM2.5 or particulate matter
- does NOT measure true CO₂
- does NOT provide certified air quality metrics
- is sensitive to temperature and humidity variations
- depends on environment-specific baseline behavior

This is a **learning and prototyping tool**, not a certified instrument.

---

## Repository Structure

firmware/ → Arduino code
docs/ → workshop documentation
hardware/ → BOM, components, enclosure files
data/ → example datasets


---

## Getting Started

1. Install Arduino IDE
2. Install ESP32 board support
3. Install required libraries:
   - Adafruit BME680
   - Adafruit Sensor
   - LittleFS
4. Upload the firmware from:

   firmware/IoT_AirSensor/IoT_AirSensor.ino

5. Power the device and connect via Wi-Fi

---

## Notes

- The device uses a **manual initial time setting**
- The internal clock continues during deep sleep
- If power is lost completely, the clock resets

---

## License

GNU General Public License v3.0

---

## Final Note

This project is intentionally simple.

The goal is not to build a perfect air quality monitor, but to understand:

- how sensors behave
- how data is generated
- how systems are designed

Once you understand this, you can improve it.

That’s the point.
