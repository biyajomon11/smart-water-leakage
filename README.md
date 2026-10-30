# Smart Water Leakage Detection and Real-Time Monitoring System Using ESP8266 NodeMCU
**MCA Final Year IoT Project**

A full-stack, enterprise-grade IoT water leakage detection and monitoring platform. This system connects physical ESP8266 NodeMCU edge hardware to a modern responsive web dashboard backed by Python Flask, PostgreSQL, and Docker containerization.

---

## 1. Project Objective & Working Principle

Water leakage in commercial, industrial, or residential spaces causes catastrophic infrastructure damage and financial loss. This project delivers an autonomous, zero-latency detection and monitoring solution:

- **When Water is Detected (WET):**
  - **Red LED** turns **ON** (warning alert).
  - **Green LED** turns **OFF**.
  - **Active Buzzer** sounds an immediate acoustic **ALARM**.
  - ESP8266 NodeMCU transmits telemetry payload (JSON) via Wi-Fi to Flask REST API over HTTPS.
  - Web dashboard immediately transitions to the **CRITICAL ALARM** banner without reloading.
  - A new **Leakage Incident Event** is registered in PostgreSQL with timestamp.

- **When Sensor is Dry (SAFE):**
  - **Green LED** turns **ON** (safe indicator).
  - **Red LED** turns **OFF**.
  - **Active Buzzer** remains **OFF (Silent)**.
  - ESP8266 NodeMCU transmits updated dry baseline reading.
  - Web dashboard displays **NO LEAKAGE DETECTED**.
  - Active leakage incidents are marked as **RESOLVED** and incident duration is calculated.

- **Autonomous Failover (Offline Security):**
  - The physical buzzer and LEDs operate directly on ESP8266 GPIO logic independently of Wi-Fi or backend server availability.

---

## 2. Hardware Connections (ESP8266 NodeMCU)

> **Important:** ESP8266 NodeMCU only supports **2.4 GHz Wi-Fi**. Ensure your hotspot/router is broadcasting on the 2.4 GHz band.

| Component | Component Pin | NodeMCU Pin | GPIO | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Water Sensor** | S (Signal / Digital Out) | **D5** | GPIO 14 | Digital Input (HIGH = Wet, LOW = Dry) |
| **Water Sensor** | + (VCC) | **3V3** | — | 3.3V DC Power Supply |
| **Water Sensor** | - (GND) | **GND** | — | Ground |
| **Active Buzzer** | I/O (Signal) | **D6** | GPIO 12 | Digital Output (HIGH = Sound, LOW = Silent) |
| **Active Buzzer** | VCC | **3V3** | — | 3.3V Power |
| **Active Buzzer** | GND | **GND** | — | Ground |
| **Green LED** | Anode (+) with 220Ω resistor | **D7** | GPIO 13 | Safe baseline indicator |
| **Green LED** | Cathode (-) | **GND** | — | Ground |
| **Red LED** | Anode (+) with 220Ω resistor | **D1** | GPIO 5 | Emergency alarm indicator |
| **Red LED** | Cathode (-) | **GND** | — | Ground |

---

## 3. Technology Stack

- **Frontend:**
  - HTML5 & CSS3
  - Tailwind CSS (Modern, responsive UI with navy blue `#0f172a` accents)
  - Vanilla JavaScript (ES6+ modular controllers)
  - Chart.js (Real-time stepped line telemetry, doughnut distribution, and trend bar charts)
  - Lucide Icons
- **Backend:**
  - Python 3.11
  - Flask REST API (JSON communication, CORS enabled)
  - SQLAlchemy ORM & psycopg2
  - Gunicorn WSGI production server
- **Database:**
  - PostgreSQL 15 (`water_leakage_db`)
  - Automatic fallback to SQLite (`water_leakage.db`) for immediate zero-config local testing
- **IoT Firmware:**
  - ESP8266 NodeMCU C++ / Arduino (`code.ino`)
  - `ESP8266WiFi.h`, `ESP8266HTTPClient.h`, `WiFiClientSecure.h`
  - HTTPS POST with fingerprint bypass for Render.com cloud endpoint
- **Containerization:**
  - Docker & Docker Compose (Multi-container architecture with persistent data volumes)
- **Cloud Deployment:**
  - Render.com (Web Service + PostgreSQL via `render.yaml` Blueprint)

---

## 4. Project Directory Structure

```text
iot/
├── app.py                     # Flask backend routing & REST APIs
├── config.py                  # Database & environment configurations
├── database.py                # SQLAlchemy models (devices, readings, events, logs)
├── init_db.sql                # PostgreSQL initialization schema
├── requirements.txt           # Python dependencies
├── Dockerfile                 # Production Docker build file
├── docker-compose.yml         # Compose configuration (Flask + PostgreSQL)
├── render.yaml                # Render.com Blueprint for one-click cloud deploy
├── .env.example               # Environment variables template
├── .gitignore                 # Git ignore file
├── README.md                  # Complete project documentation
├── code.ino                   # ESP8266 NodeMCU Arduino firmware
├── static/
│   ├── css/
│   │   └── style.css          # Custom styling, animations & LED visualizers
│   └── js/
│       ├── app.js             # Telemetry polling, UI updates & simulation logic
│       ├── chart-manager.js   # Chart.js initialization & 1h/24h/7d filtering
│       └── analytics.js       # Historical trends & distribution charts
└── templates/
    ├── base.html              # Master layout: sidebar, navbar, top device pill
    ├── index.html             # Main dashboard: live cards & activity log
    ├── live.html              # Live hardware mirror (LED/buzzer visual)
    ├── device.html            # Full device spec & ESP8266 NodeMCU pinout
    ├── history.html           # Paginated sensor readings table
    ├── alerts.html            # Leakage incident events & resolution log
    ├── analytics.html         # Chart.js telemetry & distribution graphs
    ├── settings.html          # Simulation controls & system settings
    └── about.html             # Project overview & system architecture
```

---

## 5. Running the Project

### Option A: Cloud Deployment on Render.com (Live)

The project is deployed live at:
**[https://water-leakage-monitor.onrender.com](https://water-leakage-monitor.onrender.com)**

To deploy your own instance:
1. Fork/clone this repository to your GitHub account.
2. Go to [render.com](https://render.com), create an account, and click **New Blueprint**.
3. Connect your GitHub repo — Render reads `render.yaml` automatically.
4. Set the `DATABASE_URL` environment variable to your Render PostgreSQL connection string.
5. In `code.ino`, set `serverUrl = "https://your-service-name.onrender.com/api/sensor-data"`.

---

### Option B: Running with Docker & Docker Compose (Local)

Ensure Docker Desktop is installed and running.

1. Open terminal in the project folder:
   ```bash
   cd c:\Users\User\Desktop\iot
   ```

2. Start the containers:
   ```bash
   docker-compose up --build
   ```

3. Open the web dashboard:
   ```
   http://localhost:5000
   ```

---

### Option C: Running Locally with Python (Fast Zero-Config)

1. Open PowerShell in the project folder:
   ```bash
   cd c:\Users\User\Desktop\iot
   ```

2. (Optional) Create and activate a virtual environment:
   ```bash
   python -m venv venv
   .\venv\Scripts\activate
   ```

3. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

4. Start Flask:
   ```bash
   python app.py
   ```

5. Access the application:
   - Dashboard: `http://localhost:5000`
   - From any device on the same Wi-Fi: `http://<YOUR_COMPUTER_IP>:5000`

---

## 6. Uploading Firmware to ESP8266 NodeMCU

1. Open `code.ino` in the **Arduino IDE** (version 1.8.x or 2.x).
2. Install the **ESP8266 by ESP8266 Community** board package:
   - Go to `File` -> `Preferences` -> paste this URL in "Additional Board Manager URLs":
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - Then go to `Tools` -> `Board` -> `Boards Manager` -> search `esp8266` and install.
3. Select **NodeMCU 1.0 (ESP-12E Module)** under `Tools -> Board`.
4. Set your credentials in `code.ino`:
   ```cpp
   const char* ssid     = "YOUR_WIFI_NAME";      // Must be 2.4 GHz!
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* serverUrl = "https://water-leakage-monitor.onrender.com/api/sensor-data";
   ```
5. Choose the correct COM port under `Tools -> Port`.
6. Click **Upload** (right arrow button).
7. Open **Serial Monitor** at **115200 baud** to view live connection and sensor logs.

> **Note:** ESP8266 NodeMCU only supports **2.4 GHz** Wi-Fi bands. If using a mobile hotspot, ensure the AP band is set to 2.4 GHz in your phone's hotspot settings.

---

## 7. Lab Simulation Mode (Testing Without Hardware)

If demonstrating during an MCA evaluation without physical hardware:

1. Visit the dashboard at `https://water-leakage-monitor.onrender.com` (or `localhost:5000`).
2. Click **Sim Wet** in the top navbar or Settings page:
   - The top alert banner pulses **RED: WATER LEAKAGE DETECTED!**
   - A critical incident is recorded in PostgreSQL.
3. Click **Sim Dry** to resolve:
   - Banner restores to **GREEN: NO LEAKAGE DETECTED**
   - Incident duration is logged and marked resolved.
4. All simulated readings are marked `is_simulated = true` in the database.

---

## 8. REST API Reference

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/sensor-data` | `POST` | Ingestion endpoint called by ESP8266 NodeMCU or simulator. |
| `/api/latest-reading` | `GET` | Returns latest sensor packet, active alerts, and device status. |
| `/api/readings` | `GET` | Returns time-filtered (`1h`, `24h`, `7d`, `all`) sensor readings. |
| `/api/statistics` | `GET` | Returns total readings, wet/dry counts, and incident metrics. |
| `/api/device-status` | `GET` | Returns ESP8266 Wi-Fi RSSI, IP address, and heartbeat state. |
| `/api/alerts` | `GET` | Returns active and resolved leakage events with duration. |
| `/api/export-csv` | `GET` | Generates downloadable CSV (`type=readings` or `type=events`). |
| `/api/simulate` | `POST` | Lab simulation trigger (`{"state": "WET"}` or `{"state": "DRY"}`). |

---

## 9. Academic Project Verification Checklist

- [x] Autonomous hardware alarm works on ESP8266 NodeMCU (Buzzer sounds ONLY when wet).
- [x] Red LED glows when wet, Green LED glows when dry.
- [x] Real-time HTTPS POST telemetry from ESP8266 to Flask REST API.
- [x] PostgreSQL relational database schema with indexed tables.
- [x] Dockerfile & Docker Compose containerization with persistent volumes.
- [x] Interactive Chart.js timeline with 1h/24h/7d range filtering.
- [x] Searchable and exportable CSV incident and sensor history.
- [x] Live hardware pinout visualizer (D5/D6/D7/D1 NodeMCU pins).
- [x] Responsive layout with Lucide icons and dark navy aesthetic.
- [x] Clearly labelled lab simulation mode for presentations.
- [x] One-click cloud deployment via Render.com Blueprint (`render.yaml`).

---

## 10. Device ID Reference

| Device | ID |
| :--- | :--- |
| ESP8266 NodeMCU (Active) | `ESP8266_LEAK_01` |

---

*MCA Final Year IoT Project — Smart Water Leakage Detection System*
