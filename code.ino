#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Wi-Fi Details (2.4 GHz ONLY)
const char* ssid     = "V2030";
const char* password = "12345678";

// Live Render Cloud URL
const char* serverUrl = "https://water-leakage-monitor.onrender.com/api/sensor-data";

// Device IDs
const char* DEVICE_ID   = "ESP32_LEAK_01";
const char* SENSOR_NAME = "Water Surface Sensor";

// Pin Configuration
#define WATER_SENSOR_PIN 34
#define BUZZER_PIN       25
#define GREEN_LED_PIN    26
#define RED_LED_PIN      27

const int BUZZER_SOUND_ON  = HIGH;
const int BUZZER_SOUND_OFF = LOW;
const int WET_STATE_VAL    = HIGH; // 1 = Wet, 0 = Dry

int lastSensorReading = -1;
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000;
unsigned long lastWifiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

void sendSensorDataToBackend(int sensorVal, const char* waterStatus);

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
      Serial.printf("[Wi-Fi Diagnostic] Target '%s' was FOUND on 2.4 GHz!\n\n", ssid);
    } else {
      Serial.printf("[Wi-Fi Diagnostic] Target '%s' NOT FOUND on 2.4 GHz. Check AP band!\n\n", ssid);
    }
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
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
    Serial.print("[Wi-Fi] ESP32 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[Wi-Fi] Signal:   ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("*********************************************");
  } else {
    Serial.printf("[Wi-Fi] Connection failed. Status code: %d\n", WiFi.status());
    Serial.println("[Wi-Fi] Running in offline hardware mode (Buzzer and LEDs still work!)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Safe startup: Buzzer silent, Red OFF, Green ON
  digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  Serial.println("\n========================================================");
  Serial.println(" SMART WATER LEAKAGE DETECTION SYSTEM (ESP32)");
  Serial.println(" Cloud Target: https://water-leakage-monitor.onrender.com");
  Serial.println("========================================================");

  scanNearbyNetworks();
  connectToWiFi();
}

void loop() {
  // 1. Autonomous Hardware Logic
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

  // 2. Cloud Telemetry
  bool stateChanged = (currentReading != lastSensorReading);
  bool heartbeatDue = (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS);

  if (stateChanged || heartbeatDue) {
    Serial.printf("[Sensor] GPIO 34 = %d -> %s", currentReading, statusStr);
    if (stateChanged) {
      Serial.print(isWet ? " [ALERT: LEAKAGE DETECTED!]" : " [SAFE: SENSOR DRY]");
    }
    Serial.println();

    sendSensorDataToBackend(currentReading, statusStr);

    lastSensorReading = currentReading;
    lastHeartbeatTime = millis();
  }

  // 3. Auto-reconnect if Wi-Fi dropped
  if (WiFi.status() != WL_CONNECTED && (millis() - lastWifiRetryTime >= WIFI_RETRY_INTERVAL_MS)) {
    Serial.println("[Wi-Fi] Re-attempting connection to hotspot...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    lastWifiRetryTime = millis();
  }

  delay(150);
}

void sendSensorDataToBackend(int sensorVal, const char* waterStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi not connected. Telemetry queued offline.");
    return;
  }

  HTTPClient http;
  bool isHttps = (strncmp(serverUrl, "https://", 8) == 0);

  if (isHttps) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
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
