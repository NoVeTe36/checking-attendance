#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* WIFI_SSID = "pumkinpatch2";
const char* WIFI_PASSWORD = "Thisisridiculous!";

// Static IP configuration for sender
IPAddress local_IP(192, 168, 86, 81);
IPAddress gateway(192, 168, 86, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Receiver ESP32 URL
const char* RECEIVER_URL = "http://192.168.86.82/checkin";

// Web server on port 5000 (to match your curl commands)
WebServer server(5000);

// Pin definitions
const int SENSOR_PIN = 22;      // MH Infrared Obstacle Sensor OUT pin
const int RED_LED_PIN = 19;     // Red LED pin
const int BLUE_LED_PIN = 18;    // Blue LED pin

// Variables
bool lastSensorState = HIGH;
unsigned long lastDetectionTime = 0;
const unsigned long DEBOUNCE_DELAY = 2000;

// LED timing variables
unsigned long ledStartTime = 0;
bool ledActive = false;

// Sensor detection state
bool motionDetected = false;
unsigned long motionStartTime = 0;
const unsigned long MOTION_WAIT_TIMEOUT = 6000; // 6 seconds

// Function declarations
bool forwardToReceiver(String jsonPayload);
void handleCheckin();
void handleStatus();
void testReceiverConnection();
void showErrorLED();


void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Initialize pins
  pinMode(SENSOR_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  
  // Turn off both LEDs at startup
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  ledActive = false;
  motionDetected = false;
  
  Serial.println("=== SENDER ESP32 HTTP SERVER ===");
  Serial.println("Both LEDs should be OFF now!");
  
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("WiFi connected! IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Target Receiver: ");
  Serial.println(RECEIVER_URL);
  
  // Setup web server routes
  server.on("/checkin", HTTP_POST, handleCheckin);
  server.on("/status", HTTP_GET, handleStatus);
  
  // Start web server
  server.begin();
  Serial.println("HTTP server started on port 5000");
  Serial.println("API endpoint: http://" + WiFi.localIP().toString() + ":5000/checkin");
  
  // Test receiver connectivity
  testReceiverConnection();
  
  Serial.println("System ready for sensor detection and HTTP requests...");
  Serial.println("Touch sensor to activate 6-second waiting window");
  Serial.println("================================");
}

void loop() {
  // Handle HTTP server requests
  server.handleClient();
  
  // Check sensor state
  bool currentSensorState = digitalRead(SENSOR_PIN);
  
  // Detect motion (HIGH to LOW transition)
  if (lastSensorState == HIGH && currentSensorState == LOW) {
    // Debounce check
    if (millis() - lastDetectionTime > DEBOUNCE_DELAY) {
      Serial.println("🚨 MOTION DETECTED! Waiting for HTTP request...");
      Serial.println("⏰ 6-second window started - send your curl request now!");
      
      motionDetected = true;
      motionStartTime = millis();
      lastDetectionTime = millis();
    }
  }
  
  // Update last sensor state
  lastSensorState = currentSensorState;
  
  // Check if motion detected and timeout reached
  if (motionDetected) {
    unsigned long elapsed = millis() - motionStartTime;
    
    // Show countdown every second
    static unsigned long lastCountdown = 0;
    if (millis() - lastCountdown > 1000) {
      int remaining = (MOTION_WAIT_TIMEOUT - elapsed) / 1000;
      if (remaining >= 0) {
        Serial.println("⏳ Waiting for request... " + String(remaining) + " seconds remaining");
      }
      lastCountdown = millis();
    }
    
    // Check timeout
    if (elapsed >= MOTION_WAIT_TIMEOUT) {
      Serial.println("❌ TIMEOUT! No HTTP request received within 6 seconds");
      Serial.println("🔴 Showing RED LED for 3 seconds");
      
      // Show red LED for timeout
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      ledStartTime = millis();
      ledActive = true;
      
      // Reset motion detection
      motionDetected = false;
    }
  }
  
  // Handle LED timing (turn off after 3 seconds)
  if (ledActive && (millis() - ledStartTime > 3000)) {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    ledActive = false;
    Serial.println("LEDs turned OFF");
  }
  
  delay(50);
}

void handleCheckin() {
  Serial.println("=== HTTP CHECKIN REQUEST RECEIVED ===");
  
  // Check if this request is within the motion detection window
  bool withinMotionWindow = motionDetected && (millis() - motionStartTime < MOTION_WAIT_TIMEOUT);
  
  if (withinMotionWindow) {
    Serial.println("✅ Request received within motion detection window!");
    // Reset motion detection since we got a valid request
    motionDetected = false;
  } else {
    Serial.println("ℹ️ Request received outside motion detection window (direct API call)");
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
    return;
  }
  
  String requestBody = server.arg("plain");
  String clientIP = server.client().remoteIP().toString();
  
  Serial.println("Received from client " + clientIP + ": " + requestBody);
  
  // Validate JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, requestBody);
  
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    
    // Show red LED for JSON error if within motion window
    if (withinMotionWindow) {
      showErrorLED();
    }
    return;
  }
  
  // Check if it has Name or token
  String name = doc["Name"] | "";
  String token = doc["token"] | "";
  
  if (name.length() == 0 && token.length() == 0) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing Name or token\"}");
    
    // Show red LED for missing data if within motion window
    if (withinMotionWindow) {
      showErrorLED();
    }
    return;
  }
  
  Serial.print("Forwarding to receiver: ");
  Serial.println(requestBody);
  
  // Forward to receiver and get response
  bool success = forwardToReceiver(requestBody);
  
  // Control LEDs based on result (only if within motion window)
  if (withinMotionWindow) {
    if (success) {
      // Blue LED for success
      digitalWrite(BLUE_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      Serial.println("💙 Check-in successful - Blue LED ON");
    } else {
      // Red LED for failure
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      Serial.println("❤️ Check-in failed - Red LED ON");
    }
    
    // Set LED timer
    ledStartTime = millis();
    ledActive = true;
  }
  
  if (!success) {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to communicate with receiver\"}");
    return;
  }
  
  Serial.println("Request processed successfully");
  Serial.println("==================================");
}

void handleStatus() {
  String motionStatus = motionDetected ? "waiting for request" : "idle";
  unsigned long timeRemaining = 0;
  
  if (motionDetected) {
    unsigned long elapsed = millis() - motionStartTime;
    timeRemaining = (MOTION_WAIT_TIMEOUT > elapsed) ? (MOTION_WAIT_TIMEOUT - elapsed) : 0;
  }
  
  String status = "{";
  status += "\"status\":\"online\",";
  status += "\"role\":\"sender\",";
  status += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  status += "\"motion_detected\":" + String(motionDetected ? "true" : "false") + ",";
  status += "\"motion_status\":\"" + motionStatus + "\",";
  status += "\"time_remaining_ms\":" + String(timeRemaining);
  status += "}";
  
  server.send(200, "application/json", status);
  Serial.println("Status request handled - Motion: " + motionStatus);
}

bool forwardToReceiver(String jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, RECEIVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  
  Serial.println("Forwarding to receiver: " + jsonPayload);
  Serial.println("URL: " + String(RECEIVER_URL));
  
  int httpResponseCode = http.POST(jsonPayload);
  
  Serial.print("Receiver response code: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Receiver response: " + response);
    
    // Forward the receiver's response back to the original client
    server.send(httpResponseCode, "application/json", response);
    
    // Parse response to determine success
    bool success = response.indexOf("\"success\":true") > -1;
    
    http.end();
    return success;
  } else {
    Serial.print("Failed to reach receiver: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

void showErrorLED() {
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, LOW);
  ledStartTime = millis();
  ledActive = true;
  Serial.println("❤️ Error - Red LED ON");
}

void testReceiverConnection() {
  Serial.println("Testing receiver connection...");
  
  HTTPClient http;
  WiFiClient client;
  
  // Test status endpoint
  String statusURL = "http://192.168.86.82/status";
  http.begin(client, statusURL);
  http.setTimeout(5000);
  
  int responseCode = http.GET();
  
  if (responseCode > 0) {
    String response = http.getString();
    Serial.println("✅ Receiver is online: " + response);
  } else {
    Serial.println("❌ Cannot reach receiver at " + statusURL);
    Serial.println("Error code: " + String(responseCode));
  }
  
  http.end();
}