
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ==========================================
// 1. WI-FI CONFIGURATION
// ==========================================

const char* ssid = "V2030";
const char* password = "12345678";

// ==========================================
// 2. CLOUD BACKEND CONFIGURATION
// ==========================================

const char* serverUrl =
  "https://water-leakage-monitor.onrender.com/api/sensor-data";

// API KEY
const char* API_KEY = "fOVQHiDEMP+3j2w8b/TckkL6iNyWmTnWt7evtJTAuJ8=";

// Device details
const char* DEVICE_ID = "ESP8266_LEAK_01";
const char* SENSOR_NAME = "Water Surface Sensor";

// ==========================================
// 3. NODEMCU PIN CONFIGURATION
// ==========================================

#define WATER_SENSOR_PIN D5
#define BUZZER_PIN D6
#define GREEN_LED_PIN D7
#define RED_LED_PIN D1

// ==========================================
// 4. SENSOR AND BUZZER SETTINGS
// ==========================================

const int BUZZER_SOUND_ON = HIGH;
const int BUZZER_SOUND_OFF = LOW;

// Change to LOW if your sensor gives LOW when wet
const int WET_STATE_VAL = HIGH;

int lastSensorReading = -1;

// ==========================================
// 5. TIMING CONFIGURATION
// ==========================================

unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000;

unsigned long lastWifiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

// HTTPS client
WiFiClientSecure secureClient;

// Function declaration
void sendSensorDataToBackend(
  int sensorVal,
  const char* waterStatus
);

// ==========================================
// 6. SCAN WI-FI NETWORKS
// ==========================================

void scanNearbyNetworks() {

  Serial.println("\nScanning nearby Wi-Fi networks...");

  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("No Wi-Fi networks found.");
  } else {

    for (int i = 0; i < n; i++) {

      Serial.print(WiFi.SSID(i));
      Serial.print(" | Signal: ");
      Serial.print(WiFi.RSSI(i));
      Serial.println(" dBm");

    }
  }

  WiFi.scanDelete();
}

// ==========================================
// 7. CONNECT TO WI-FI
// ==========================================

void connectToWiFi() {

  WiFi.mode(WIFI_STA);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {

    delay(500);
    Serial.print(".");
    attempts++;

  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected successfully!");

    Serial.print("ESP8266 IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("Wi-Fi connection failed.");
    Serial.println("Continuing in offline hardware mode.");

  }
}

// ==========================================
// 8. SETUP
// ==========================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Safe initial condition
  digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  Serial.println();
  Serial.println("======================================");
  Serial.println(" SMART WATER LEAKAGE DETECTION SYSTEM");
  Serial.println("          ESP8266 NODEMCU");
  Serial.println("======================================");

  scanNearbyNetworks();

  connectToWiFi();

  // Prototype HTTPS testing
  secureClient.setInsecure();

}

// ==========================================
// 9. MAIN LOOP
// ==========================================

void loop() {

  // Read water sensor
  int currentReading = digitalRead(WATER_SENSOR_PIN);

  bool isWet = (currentReading == WET_STATE_VAL);

  const char* statusStr = isWet ? "WET" : "DRY";

  // Control LEDs and buzzer

  if (isWet) {

    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_ON);

  } else {

    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, BUZZER_SOUND_OFF);

  }

  // Detect sensor changes
  bool stateChanged =
    (currentReading != lastSensorReading);

  // Send heartbeat every 5 seconds
  bool heartbeatDue =
    (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS);

  if (stateChanged || heartbeatDue) {

    Serial.print("Sensor Reading: ");
    Serial.print(currentReading);

    Serial.print(" | Water Status: ");
    Serial.println(statusStr);

    if (isWet) {
      Serial.println("ALERT: WATER DETECTED!");
    } else {
      Serial.println("SAFE: SENSOR DRY");
    }

    sendSensorDataToBackend(
      currentReading,
      statusStr
    );

    lastSensorReading = currentReading;
    lastHeartbeatTime = millis();

  }

  // Automatic Wi-Fi reconnection

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWifiRetryTime >= WIFI_RETRY_INTERVAL_MS) {

    Serial.println("Reconnecting to Wi-Fi...");

    WiFi.disconnect();
    WiFi.begin(ssid, password);

    lastWifiRetryTime = millis();

  }

  delay(150);

}

// ==========================================
// 10. SEND DATA TO FLASK BACKEND
// ==========================================

void sendSensorDataToBackend(
  int sensorVal,
  const char* waterStatus
) {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi unavailable. Data not uploaded.");
    return;

  }

  HTTPClient http;

  if (!http.begin(secureClient, serverUrl)) {

    Serial.println("HTTPS connection failed to start.");
    return;

  }

  // HTTP headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  http.setTimeout(10000);

  // Prepare JSON payload

  String jsonPayload = "{";

  jsonPayload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  jsonPayload += "\"sensor_name\":\"" + String(SENSOR_NAME) + "\",";
  jsonPayload += "\"sensor_value\":" + String(sensorVal) + ",";
  jsonPayload += "\"water_status\":\"" + String(waterStatus) + "\",";
  jsonPayload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  jsonPayload += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  jsonPayload += "\"wifi_ssid\":\"" + String(ssid) + "\"";

  jsonPayload += "}";

  Serial.println("\nSending sensor data to cloud...");

  Serial.println(jsonPayload);

  // Send HTTP POST request

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {

    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);

    if (httpCode >= 200 && httpCode < 300) {

      Serial.println("SUCCESS: Data uploaded to Render!");

    } else {

      Serial.println("ERROR: Server rejected the request.");

      Serial.println(http.getString());

    }

  } else {

    Serial.print("HTTP POST failed: ");
    Serial.println(http.errorToString(httpCode));

  }

  http.end();

}
