#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// LED pin for status indication
const int STATUS_LED_PIN = 2;

// WiFi credentials
const char* WIFI_SSID = "pumkinpatch2";
const char* WIFI_PASSWORD = "Thisisridiculous!";

// Static IP configuration for receiver
IPAddress local_IP(192, 168, 86, 82);
IPAddress gateway(192, 168, 86, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Web server on port 80
WebServer server(80);

// External Flask server
const char* FLASK_SERVER_URL = "http://192.168.86.59:5000/checkin";

// Function declarations
void handleCheckin();
void handleStatus();
bool forwardToFlaskServer(String token);

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Initialize status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  Serial.println("=== WIFI RECEIVER ESP32 ===");
  
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
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  
  // Setup web server routes
  server.on("/checkin", HTTP_POST, handleCheckin);
  server.on("/status", HTTP_GET, handleStatus);
  
  // Start web server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Receiver ready at: http://" + WiFi.localIP().toString());
  Serial.println("Checkin endpoint: http://" + WiFi.localIP().toString() + "/checkin");
  
  digitalWrite(STATUS_LED_PIN, HIGH);
  Serial.println("====================");
}

void loop() {
  server.handleClient();
  
  // Heartbeat
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) {
    Serial.println("Receiver alive, waiting for check-in requests...");
    lastHeartbeat = millis();
  }
  
  delay(10);
}

void handleCheckin() {
  Serial.println("=== CHECK-IN REQUEST RECEIVED ===");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
    return;
  }
  
  String requestBody = server.arg("plain");
  Serial.println("Received data: " + requestBody);
  
  // Parse JSON using updated ArduinoJson
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, requestBody);
  
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  String token = doc["token"] | "";
  unsigned long timestamp = doc["timestamp"] | 0;
  String senderIP = server.client().remoteIP().toString();
  
  Serial.print("Token: ");
  Serial.println(token);
  Serial.print("Timestamp: ");
  Serial.println(timestamp);
  Serial.print("From Sender IP: ");
  Serial.println(senderIP);
  
  // Forward to Flask server
  bool serverResult = forwardToFlaskServer(token);
  
  // Prepare response
  String response;
  if (serverResult) {
    response = "{\"success\":true,\"message\":\"Check-in successful\"}";
    Serial.println("✅ Check-in successful");
  } else {
    response = "{\"success\":false,\"message\":\"Check-in failed\"}";
    Serial.println("❌ Check-in failed");
  }
  
  // Send response
  server.send(200, "application/json", response);
  Serial.println("Response sent to sender");
  Serial.println("==================================");
}

void handleStatus() {
  String status = "{\"status\":\"online\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", status);
  Serial.println("Status request handled");
}

bool forwardToFlaskServer(String token) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, FLASK_SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  
  // Create JSON payload
  String jsonString = "{\"token\":\"" + token + "\",\"session_id\":1}";
  
  Serial.println("Forwarding to Flask server: " + jsonString);
  Serial.println("URL: " + String(FLASK_SERVER_URL));
  
  int httpResponseCode = http.POST(jsonString);
  
  Serial.print("Flask server response code: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Flask server response: " + response);
    
    // Parse Flask response using updated ArduinoJson
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      bool success = responseDoc["success"] | false;
      http.end();
      return success;
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      // Fallback: check for success in response text
      bool success = response.indexOf("\"success\":true") > -1;
      http.end();
      return success;
    }
  } else {
    Serial.print("Flask server request failed: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}