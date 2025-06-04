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
const char* FLASK_SERVER_URL = "http://192.168.86.69:5000/checkin";

// Function declarations
void handleCheckin();
void handleStatus();
bool forwardToFlaskServer(String jsonPayload);
void testFlaskServerConnection();

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
  
  // Test Flask server connectivity
  Serial.println("Testing Flask server connectivity...");
  testFlaskServerConnection();
  
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
  String senderIP = server.client().remoteIP().toString();
  
  Serial.println("Received from " + senderIP + ": " + requestBody);
  
  // Parse JSON to check what type of request this is
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, requestBody);
  
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  // Check what fields we have
  String token = doc["token"] | "";
  String name = doc["Name"] | "";
  String certId = doc["cert_id"] | "";
  
  Serial.println("Request contains:");
  if (token.length() > 0) Serial.println("  Token: " + token);
  if (name.length() > 0) Serial.println("  Name: " + name);
  if (certId.length() > 0) Serial.println("  Cert ID: " + certId);
  
  // Forward the complete request body to Flask server
  bool serverResult = forwardToFlaskServer(requestBody);
  
  // Get the actual response from Flask and forward it back
  Serial.println("==================================");
}

void handleStatus() {
  String status = "{\"status\":\"online\",\"role\":\"receiver\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", status);
  Serial.println("Status request handled");
}

bool forwardToFlaskServer(String jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    server.send(500, "application/json", "{\"success\":false,\"message\":\"WiFi not connected\"}");
    return false;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, FLASK_SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);
  
  Serial.println("Forwarding to Flask server: " + jsonPayload);
  Serial.println("URL: " + String(FLASK_SERVER_URL));
  
  int httpResponseCode = http.POST(jsonPayload);
  
  Serial.print("Flask server response code: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Flask server response: " + response);
    
    // Forward Flask server's response directly to the sender
    server.send(httpResponseCode, "application/json", response);
    
    // Determine success for return value
    bool success = response.indexOf("\"success\":true") > -1;
    
    if (success) {
      Serial.println("✅ Check-in successful");
    } else {
      Serial.println("❌ Check-in failed");
    }
    
    http.end();
    return success;
  } else {
    Serial.print("Flask server request failed: ");
    Serial.println(httpResponseCode);
    
    // Send error response
    String errorResponse = "{\"success\":false,\"message\":\"Cannot reach Flask server\"}";
    server.send(500, "application/json", errorResponse);
    
    http.end();
    return false;
  }
}

void testFlaskServerConnection() {
  HTTPClient http;
  WiFiClient client;
  
  // Test Flask server employees endpoint
  String testURL = "http://192.168.86.69:5000/employees";
  http.begin(client, testURL);
  http.setTimeout(5000);
  
  int responseCode = http.GET();
  
  if (responseCode > 0) {
    String response = http.getString();
    Serial.println("✅ Flask server is reachable");
    Serial.println("Response code: " + String(responseCode));
    Serial.println("Sample response: " + response.substring(0, 100) + "...");
  } else {
    Serial.println("❌ Cannot reach Flask server");
    Serial.println("Error code: " + String(responseCode));
    
    // Additional diagnostics
    Serial.println("Checking network connectivity...");
    
    // Test basic HTTP connectivity
    http.begin(client, "http://httpbin.org/get");
    int testCode = http.GET();
    if (testCode > 0) {
      Serial.println("✅ Internet connectivity OK");
    } else {
      Serial.println("❌ No internet connectivity");
    }
  }
  
  http.end();
}