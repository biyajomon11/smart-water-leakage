# Smart Water Leakage Detection and Real-Time Monitoring System Using ESP32
**MCA Final Year IoT Project**

A full-stack, enterprise-grade IoT water leakage detection and monitoring platform. This system connects physical ESP32 edge hardware to a modern responsive web dashboard backed by Python Flask, PostgreSQL, and Docker containerization.

---

## 1. Project Objective & Working Principle

Water leakage in commercial, industrial, or residential spaces causes catastrophic infrastructure damage and financial loss. This project delivers an autonomous, zero-latency detection and monitoring solution:

- **When Water is Detected (WET):**
  - **Red LED** turns **ON** (warning alert).
  - **Green LED** turns **OFF**.
  - **Active Buzzer** sounds an immediate acoustic **ALARM**.
  - ESP32 transmits telemetry payload (JSON) via Wi-Fi to Flask REST API.
  - Web dashboard immediately transitions to the **CRITICAL ALARM** banner without reloading.
  - A new **Leakage Incident Event** is registered in PostgreSQL with timestamp.

- **When Sensor is Dry (SAFE):**
  - **Green LED** turns **ON** (safe indicator).
  - **Red LED** turns **OFF**.
  - **Active Buzzer** remains **OFF (Silent)**.
  - ESP32 transmits updated dry baseline reading.
  - Web dashboard displays **NO LEAKAGE DETECTED**.
  - Active leakage incidents are marked as **RESOLVED** and incident duration is calculated.

- **Autonomous Failover (Offline Security):**
  - The physical buzzer and LEDs operate directly on ESP32 GPIO logic independently of Wi-Fi or backend server availability.

---

## 2. Hardware Connections (ESP32 NodeMCU-32S)

| Component | Component Pin | ESP32 Pin | Purpose |
| :--- | :--- | :--- | :--- |
| **Water Sensor** | S (Signal / Digital Out) | **GPIO 34** | Digital Input (1 = Wet, 0 = Dry) |
| **Water Sensor** | + (VCC) | **3V3** | 3.3V DC Power Supply |
| **Water Sensor** | - (GND) | **GND** | Ground |
| **Active Buzzer** | I/O (Signal) | **GPIO 25** | Digital Output (HIGH = Sound, LOW = Silent) |
| **Active Buzzer** | VCC | **3V3** | 3.3V Power |
| **Active Buzzer** | GND | **GND** | Ground |
| **Green LED** | Anode (+) with 220Ω resistor | **GPIO 26** | Safe baseline indicator |
| **Green LED** | Cathode (-) | **GND** | Ground |
| **Red LED** | Anode (+) with 220Ω resistor | **GPIO 27** | Emergency alarm indicator |
| **Red LED** | Cathode (-) | **GND** | Ground |

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
  - ESP32 C++ / Arduino (`code.ino` and `esp32/water_leakage_detector.ino`)
  - HTTPClient & WiFi library
- **Containerization:**
  - Docker & Docker Compose (Multi-container architecture with persistent data volumes)

---

## 4. Project Directory Structure

```text
c:\Users\User\Desktop\iot\
├── app.py                     # Flask backend routing & REST APIs
├── config.py                  # Database & environment configurations
├── database.py                # SQLAlchemy models (devices, readings, events, logs)
├── init_db.sql                # PostgreSQL initialization schema
├── requirements.txt           # Python dependencies
├── Dockerfile                 # Production Docker build file
├── docker-compose.yml         # Compose configuration (Flask + PostgreSQL)
├── .env.example               # Environment variables template
├── .gitignore                 # Git ignore file
├── README.md                  # Complete project documentation
├── code.ino                   # ESP32 Arduino firmware (root file)
├── esp32/
│   └── water_leakage_detector.ino # Standalone ESP32 production firmware
├── static/
│   ├── css/
│   │   └── style.css          # Custom styling, animations & LED visualizers
│   └── js/
│       ├── app.js             # Telemetry polling, UI updates & simulation logic
│       ├── chart-manager.js   # Chart.js initialization & 1h/24h/7d filtering
│       └── analytics.js       # Historical trends & distribution charts
└── templates/
    ├── base.html              # Master layout with sidebar navigation & top bar
    ├── index.html             # Main Dashboard (cards, alert banner, chart, table, logs)
    ├── live.html              # Live hardware pinout mirror & audio alarm
    ├── history.html           # Full historical logs with pagination & filters
    ├── alerts.html            # Leakage incident tracking & resolution duration
    ├── analytics.html         # Statistical analytics & graphical trends
    ├── device.html            # ESP32 pinout diagram & hardware telemetry
    ├── settings.html          # Polling rate configuration & lab simulator
    └── about.html             # System architecture & project documentation
```

---

## 5. How to Run the Application

### Option A: Hosting on Render with Blueprints (Cloud Deployment)

This repository includes a production `render.yaml` Blueprint specification. Render will automatically provision both the **Web Service** (Flask) and a **Managed PostgreSQL Database** in one click.

1. **Push your project to GitHub**:
   ```bash
   cd c:\Users\User\Desktop\iot
   git init
   git add .
   git commit -m "Deploy Smart Water Leakage Detection System"
   git branch -M main
   git remote add origin https://github.com/YOUR_GITHUB_USERNAME/water-leakage-monitoring.git
   git push -u origin main
   ```

2. **Deploy on Render**:
   - Go to [Render Dashboard](https://dashboard.render.com).
   - Click **New +** and select **Blueprint**.
   - Connect your GitHub repository.
   - Render will detect `render.yaml` and show:
     - **Web Service**: `water-leakage-monitor` (Python 3.11 + Gunicorn)
     - **Database**: `water-leakage-db` (PostgreSQL 15)
   - Click **Apply**. Render will automatically build, link the database via `DATABASE_URL`, initialize tables, and launch your live site!

3. **Get Your Cloud URL**:
   - Your site will be live at: `https://water-leakage-monitor.onrender.com` (or your assigned sub-domain).
   - In `code.ino`, set `serverUrl = "https://your-service-name.onrender.com/api/sensor-data"`.

---

### Option B: Running with Docker & Docker Compose (Local Containerization)

Ensure Docker Desktop is installed and running on your system.

1. **Clone or open the project folder in terminal:**
   ```bash
   cd c:\Users\User\Desktop\iot
   ```

2. **Start the containers using Docker Compose:**
   ```bash
   docker-compose up --build
   ```

3. **Open the web dashboard in your browser:**
   ```text
   http://localhost:5000
   ```

*The PostgreSQL database will automatically initialize using `init_db.sql` and persist all readings in a Docker volume.*

---

### Option B: Running Locally with Python (Fast Zero-Config Development)

If you don't have Docker installed, you can run the backend directly with Python. The backend automatically connects to PostgreSQL if available, or falls back to an embedded SQLite database (`water_leakage.db`) so you can run immediately without setup.

1. **Open Windows PowerShell or Command Prompt:**
   ```bash
   cd c:\Users\User\Desktop\iot
   ```

2. **(Optional) Create and activate a Python virtual environment:**
   ```bash
   python -m venv venv
   .\venv\Scripts\activate
   ```

3. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

4. **Start the Flask server:**
   ```bash
   python app.py
   ```

5. **Access the application:**
   - Web Dashboard: `http://localhost:5000`
   - From any phone/tablet on your Wi-Fi: `http://<YOUR_COMPUTER_IP>:5000`

---

## 6. Uploading Code to the Physical ESP32

1. Open `code.ino` (or `esp32/water_leakage_detector.ino`) in the **Arduino IDE**.
2. Install the **ESP32 by Espressif Systems** board package in Arduino IDE:
   - Go to `Tools` -> `Board` -> `Boards Manager` -> search for `esp32` and install.
3. Configure your network credentials at lines 28–36 in `code.ino`:
   ```cpp
   const char* ssid     = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";

   // Replace with your PC's Wi-Fi IP address (run 'ipconfig' in CMD)
   const char* serverUrl = "http://192.168.1.100:5000/api/sensor-data";
   ```
4. Select `ESP32 Dev Module` under `Tools -> Board`, choose your COM port, and click **Upload**.
5. Open the **Serial Monitor** at **115200 baud** to view real-time Wi-Fi connection and sensor telemetry.

---

## 7. Lab Simulation Mode (Testing Without Hardware)

If you are demonstrating this project during an MCA evaluation and do not have the physical hardware connected:

1. Look at the top right of the navigation header on any page, or visit `/settings`.
2. Click **Sim Wet** to simulate an immediate water leakage event:
   - Notice the top alert banner pulses **RED: WATER LEAKAGE DETECTED!**.
   - The buzzer actuator turns on in the Live Hardware Mirror (`/live`).
   - A critical incident is recorded in PostgreSQL.
3. Click **Sim Dry** to simulate drying the sensor:
   - Notice the banner restores to **GREEN: NO LEAKAGE DETECTED**.
   - The incident duration in seconds is logged and resolved.
4. All simulated readings are stored in the database and explicitly marked with `is_simulated = true` so test data remains transparent and clean.

---

## 8. REST API Reference

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/sensor-data` | `POST` | Ingestion endpoint called by ESP32 or simulated sensors. |
| `/api/latest-reading` | `GET` | Returns the latest sensor packet, active alerts, and device status. |
| `/api/readings` | `GET` | Returns time-filtered (`1h`, `24h`, `7d`, `all`) and paginated sensor readings. |
| `/api/statistics` | `GET` | Returns total readings, wet/dry counts, and incident metrics. |
| `/api/device-status` | `GET` | Returns ESP32 Wi-Fi RSSI, IP address, and online/offline heartbeat state. |
| `/api/alerts` | `GET` | Returns active and resolved leakage events with calculated duration. |
| `/api/export-csv` | `GET` | Generates downloadable CSV files (`type=readings` or `type=events`). |
| `/api/simulate` | `POST` | Lab simulation trigger (`{"state": "WET"}` or `{"state": "DRY"}`). |

---

## 9. Academic Project Verification Checklist

- [x] Autonomous hardware alarm works on ESP32 (Buzzer sounds ONLY when wet, silent when dry).
- [x] Red LED glows when wet, Green LED glows when dry.
- [x] Real-time HTTP POST telemetry transmission from ESP32 to Flask.
- [x] PostgreSQL relational database schema with indexed tables.
- [x] Dockerfile & Docker Compose containerization with persistent volumes.
- [x] Interactive Chart.js timeline with 1h/24h/7d range filtering.
- [x] Searchable and exportable CSV incident and sensor history.
- [x] Live hardware pinout visualizer (GPIO 34, 25, 26, 27).
- [x] Responsive layout with Lucide icons and dark navy aesthetic.
- [x] Clearly labelled lab simulation mode for presentations.

