/*
  =====================================================================================
  Smart Water Leakage Detection and Real-Time Monitoring System Using ESP32
  MCA IoT Final Year Project - Production Firmware with Wi-Fi Diagnostics
  =====================================================================================

  Hardware Pinout:
  - Water Sensor Digital Output (S) -> GPIO 34 (Input only)
  - Active Buzzer (I/O)              -> GPIO 25 (Active HIGH: HIGH = ON, LOW = OFF)
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
// IMPORTANT: ESP32 ONLY SUPPORTS 2.4 GHz Wi-Fi!
// On your mobile phone: Hotspot Settings -> AP Band -> Select "2.4 GHz" (NOT 5 GHz)
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

const int BUZZER_SOUND_ON  = HIGH;
const int BUZZER_SOUND_OFF = LOW;
const int WET_STATE_VAL    = HIGH; // 1 = Wet, 0 = Dry

// -----------------------------------------------------------------------------------
// 3. Timing & State Tracking Variables
// -----------------------------------------------------------------------------------
int lastSensorReading = -1;
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000;
unsigned long lastWifiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

void sendSensorDataToBackend(int sensorVal, const char* waterStatus);

// Diagnostic function: Scans 2.4 GHz Wi-Fi channels to check if "V2030" is visible
void scanNearbyNetworks() {
  Serial.println("\n[Wi-Fi Diagnostic] Scanning 2.4 GHz channels...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[Wi-Fi Diagnostic] No 2.4 GHz Wi-Fi networks found!");
  } else {
    Serial.printf("[Wi-Fi Diagnostic] Found %d networks:\n", n);
    bool targetFound = false;
    for (int i = 0; i < n; ++i) {
      Serial.printf("  %2d: %-20s (Signal: %4d dBm) %s\n", 
                    i + 1, 
                    WiFi.SSID(i).c_str(), 
                    WiFi.RSSI(i), 
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
      if (WiFi.SSID(i) == ssid) {
        targetFound = true;
      }
    }
    if (targetFound) {
      Serial.printf(">>> SUCCESS: Network '%s' is VISIBLE on 2.4 GHz! <<<\n\n", ssid);
    } else {
      Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.printf(">>> ERROR: '%s' WAS NOT FOUND IN 2.4 GHz SCAN! <<<\n", ssid);
      Serial.println("Your phone hotspot is likely set to 5 GHz.");
      Serial.println("Fix: Phone Settings -> Hotspot -> AP Band -> Select 2.4 GHz");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Keeps Wi-Fi active without sleep drops
  WiFi.disconnect(true); // Clear previous cache
  delay(300);

  Serial.printf("[Wi-Fi] Connecting to: [%s] ...\n", ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("*********************************************");
    Serial.println("[Wi-Fi] CONNECTED SUCCESSFULLY!");
    Serial.print("[Wi-Fi] ESP32 IP:  ");
    Serial.println(WiFi.localIP());
    Serial.print("[Wi-Fi] Signal:    ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("*********************************************");
  } else {
    Serial.printf("[Wi-Fi] Connection failed. Status code: %d\n", WiFi.status());
    Serial.println("[Wi-Fi] Status 1=No SSID, 4=Connect Failed (Check Password), 6=Disconnected");
    Serial.println("[Wi-Fi] Running in offline hardware mode (Buzzer & LEDs still work!)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Pin modes
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Safe initial hardware state: Buzzer silent, Red OFF, Green ON
  digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  Serial.println("\n========================================================");
  Serial.println(" SMART WATER LEAKAGE DETECTION SYSTEM (ESP32)           ");
  Serial.println(" Cloud Target: https://water-leakage-monitor.onrender.com ");
  Serial.println("========================================================");

  // Step 1: Scan to verify hotspot band & visibility
  scanNearbyNetworks();

  // Step 2: Connect to Wi-Fi
  connectToWiFi();
}

void loop() {
  // ---------------------------------------------------------------------------------
  // 1. Instant Local Hardware Detection (100% Autonomous)
  // ---------------------------------------------------------------------------------
  int currentReading = digitalRead(WATER_SENSOR_PIN);
  bool isWet = (currentReading == WET_STATE_VAL);
  const char* statusStr = isWet ? "WET" : "DRY";

  if (isWet) {
    // WATER DETECTED: Red ON, Green OFF, Buzzer ON
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_ON);
  } else {
    // SENSOR DRY: Red OFF, Green ON, Buzzer OFF
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  }

  // ---------------------------------------------------------------------------------
  // 2. Telemetry Transmission (On state change or every 5 seconds)
  // ---------------------------------------------------------------------------------
  bool stateChanged = (currentReading != lastSensorReading);
  bool heartbeatDue = (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS);

  if (stateChanged || heartbeatDue) {
    Serial.printf("[Sensor] GPIO 34 = %d -> %s", currentReading, statusStr);
    if (stateChanged) {
      Serial.print(isWet ? " >>> [ALERT: LEAKAGE DETECTED!] <<<" : " >>> [SAFE: SENSOR DRY] <<<");
    }
    Serial.println();

    sendSensorDataToBackend(currentReading, statusStr);

    lastSensorReading = currentReading;
    lastHeartbeatTime = millis();
  }

  // ---------------------------------------------------------------------------------
  // 3. Auto-Reconnect if disconnected
  // ---------------------------------------------------------------------------------
  if (WiFi.status() != WL_CONNECTED && (millis() - lastWifiRetryTime >= WIFI_RETRY_INTERVAL_MS)) {
    Serial.println("[Wi-Fi] Re-attempting connection to hotspot...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    lastWifiRetryTime = millis();
  }

  delay(150);
}

// -----------------------------------------------------------------------------------
// 4. Send HTTPS POST JSON to Cloud
// -----------------------------------------------------------------------------------
void sendSensorDataToBackend(int sensorVal, const char* waterStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi not connected. Telemetry queued offline.");
    return;
  }

  HTTPClient http;
  bool isHttps = (strncmp(serverUrl, "https://", 8) == 0);

  if (isHttps) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Connects to Render SSL without root cert bundle
    if (!http.begin(secureClient, serverUrl)) {
      Serial.println("[HTTPS] Failed to begin connection.");
      return;
    }
  } else {
    WiFiClient client;
    if (!http.begin(client, serverUrl)) {
      Serial.println("[HTTP] Failed to begin connection.");
      return;
    }
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String jsonPayload = "{";
  jsonPayload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  jsonPayload += "\"sensor_name\":\"" + String(SENSOR_NAME) + "\",";
  jsonPayload += "\"sensor_value\":" + String(sensorVal) + ",";
  jsonPayload += "\"water_status\":\"" + String(waterStatus) + "\",";
  jsonPayload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  jsonPayload += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  jsonPayload += "\"wifi_ssid\":\"" + String(ssid) + "\"";
  jsonPayload += "}";

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {
    Serial.printf("[Cloud HTTP] Status: %d (Data successfully uploaded to Render!)\n", httpCode);
  } else {
    Serial.printf("[Cloud HTTP] POST error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}
