/*
  =====================================================================================
  Smart Water Leakage Detection and Real-Time Monitoring System Using ESP32
  MCA IoT Final Year Project - Production Firmware
  Supports both Local LAN (HTTP) and Cloud Hosting on Render (HTTPS)
  =====================================================================================

  Hardware Pinout:
  - Water Sensor Digital Output (S) -> GPIO 34 (Input only)
  - Active Buzzer (I/O)              -> GPIO 25 (Active HIGH)
  - Green LED (Status: Dry / Safe)  -> GPIO 26
  - Red LED (Status: Leakage Alert) -> GPIO 27

  Power Connections:
  - Sensor VCC -> ESP32 3V3 | Sensor GND -> ESP32 GND
  - Buzzer VCC -> ESP32 3V3 | Buzzer GND -> ESP32 GND
  =====================================================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// -----------------------------------------------------------------------------------
// 1. Wi-Fi & Backend Server Configuration
// -----------------------------------------------------------------------------------
// IMPORTANT: ESP32 only supports 2.4 GHz Wi-Fi!
// If using a mobile hotspot (e.g. Vivo V2030), ensure the AP Band is set to "2.4 GHz"
const char* ssid     = "V2030";
const char* password = "12345678";

// Live Render Cloud URL
const char* serverUrl = "https://water-leakage-monitor.onrender.com/api/sensor-data";

// Unique Device Identifier
const char* DEVICE_ID   = "ESP32_LEAK_01";
const char* SENSOR_NAME = "Water Surface Sensor";

// -----------------------------------------------------------------------------------
// 2. Pin Assignments & Output Logic
// -----------------------------------------------------------------------------------
#define WATER_SENSOR_PIN 34
#define BUZZER_PIN       25
#define GREEN_LED_PIN    26
#define RED_LED_PIN      27

// Active Buzzer Logic:
// Active buzzer turns ON on HIGH and OFF on LOW.
const int BUZZER_SOUND_ON  = HIGH;
const int BUZZER_SOUND_OFF = LOW;

// Sensor Logic:
// Water sensor outputs HIGH (1) when wet and LOW (0) when dry.
const int WET_STATE_VAL    = HIGH; 

// -----------------------------------------------------------------------------------
// 3. Timing & State Tracking Variables
// -----------------------------------------------------------------------------------
int lastSensorReading = -1;
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000; // Heartbeat telemetry every 5s
unsigned long lastWifiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 15000; // Reconnect retry interval

// Helper: print human-readable Wi-Fi status
void printWiFiStatus(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     Serial.println("WL_IDLE_STATUS"); break;
    case WL_NO_SSID_AVAIL:   Serial.println("WL_NO_SSID_AVAIL (Hotspot not found! Check 2.4 GHz)"); break;
    case WL_CONNECTED:       Serial.println("WL_CONNECTED (Success!)"); break;
    case WL_CONNECT_FAILED:  Serial.println("WL_CONNECT_FAILED (Wrong password?)"); break;
    case WL_DISCONNECTED:    Serial.println("WL_DISCONNECTED"); break;
    default:                 Serial.printf("Status code: %d\n", status); break;
  }
}

// Forward declaration
void sendSensorDataToBackend(int sensorVal, const char* waterStatus);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure hardware pins
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Safe initial startup state: Buzzer silent, Red OFF, Green ON
  digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  Serial.println("\n========================================================");
  Serial.println(" SMART WATER LEAKAGE DETECTION AND MONITORING (ESP32)  ");
  Serial.println(" Cloud-Connected: https://water-leakage-monitor.onrender.com ");
  Serial.println("========================================================");
  Serial.print("Device ID:  ");
  Serial.println(DEVICE_ID);
  Serial.print("Target URL: ");
  Serial.println(serverUrl);

  // Initialize Wi-Fi connection cleanly
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable modem sleep for stable link
  WiFi.disconnect(true); // Clear previous connection state
  delay(200);

  Serial.print("Connecting to Wi-Fi SSID: [");
  Serial.print(ssid);
  Serial.println("] ...");

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[Wi-Fi] Connected successfully!");
    Serial.print("[Wi-Fi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[Wi-Fi] Signal (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.print("[Wi-Fi] Initial connection failed. Reason: ");
    printWiFiStatus(WiFi.status());
    Serial.println("[Wi-Fi] Local alarm protection is active offline.");
  }
}

void loop() {
  // ---------------------------------------------------------------------------------
  // 1. Instant Local Hardware Detection (Autonomous, zero-latency)
  // ---------------------------------------------------------------------------------
  int currentReading = digitalRead(WATER_SENSOR_PIN);
  bool isWet = (currentReading == WET_STATE_VAL);
  const char* statusStr = isWet ? "WET" : "DRY";

  if (isWet) {
    // WATER DETECTED:
    // - Red LED turns ON
    // - Green LED turns OFF
    // - Buzzer produces ALARM sound
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_ON);
  } else {
    // SENSOR DRY:
    // - Green LED turns ON
    // - Red LED turns OFF
    // - Buzzer remains OFF
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  }

  // ---------------------------------------------------------------------------------
  // 2. Detect State Changes & Periodic Heartbeat Transmission
  // ---------------------------------------------------------------------------------
  bool stateChanged = (currentReading != lastSensorReading);
  bool heartbeatDue = (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS);

  if (stateChanged || heartbeatDue) {
    Serial.print("[Sensor] Reading: ");
    Serial.print(currentReading);
    Serial.print(" -> State: ");
    Serial.println(statusStr);

    if (stateChanged) {
      if (isWet) {
        Serial.println(">>> [ALARM TRIGGERED] WATER LEAKAGE DETECTED! <<<");
      } else {
        Serial.println(">>> [SAFE RESTORED] SENSOR IS DRY. LEAKAGE CLEARED. <<<");
      }
    }

    // Transmit telemetry to Render Cloud Backend
    sendSensorDataToBackend(currentReading, statusStr);

    lastSensorReading = currentReading;
    lastHeartbeatTime = millis();
  }

  // ---------------------------------------------------------------------------------
  // 3. Wi-Fi Auto-Reconnect Failsafe (Clean disconnect + begin)
  // ---------------------------------------------------------------------------------
  if (WiFi.status() != WL_CONNECTED && (millis() - lastWifiRetryTime >= WIFI_RETRY_INTERVAL_MS)) {
    Serial.println("[Wi-Fi] Disconnected. Re-initiating connection...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    lastWifiRetryTime = millis();
  }

  delay(150);
}

// -----------------------------------------------------------------------------------
// 4. Send HTTPS POST JSON Request to Render Backend
// -----------------------------------------------------------------------------------
void sendSensorDataToBackend(int sensorVal, const char* waterStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi offline. Data transmission skipped.");
    return;
  }

  HTTPClient http;
  bool isHttps = (strncmp(serverUrl, "https://", 8) == 0);

  // Initialize client based on protocol
  if (isHttps) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Allows connecting to Render HTTPS without CA certs
    if (!http.begin(secureClient, serverUrl)) {
      Serial.println("[HTTPS] Unable to connect to host.");
      return;
    }
  } else {
    WiFiClient client;
    if (!http.begin(client, serverUrl)) {
      Serial.println("[HTTP] Unable to connect to host.");
      return;
    }
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000); // 5-second timeout for cloud latency

  // Construct JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  jsonPayload += "\"sensor_name\":\"" + String(SENSOR_NAME) + "\",";
  jsonPayload += "\"sensor_value\":" + String(sensorVal) + ",";
  jsonPayload += "\"water_status\":\"" + String(waterStatus) + "\",";
  jsonPayload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  jsonPayload += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  jsonPayload += "\"wifi_ssid\":\"" + String(ssid) + "\"";
  jsonPayload += "}";

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("[HTTP] POST Response Code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("[HTTP] POST Failed. Error: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}
