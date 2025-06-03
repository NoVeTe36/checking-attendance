#include <WiFi.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// Network configuration
const char* ssid = "VietTung2003";
const char* password = "0888092811";
const char* backendIP = "192.168.1.100"; // Backend ESP32 IP
const int backendPort = 81;

// WebSocket connections
WebSocketsClient backendClient;  // To backend
WebSocketsServer clientServer(82); // For camera clients

// Image processing variables
String lastCameraFrame = "";
bool motionDetected = false;
unsigned long lastMotionTime = 0;
int motionThreshold = 30; // Adjust based on testing

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Proxy Server started!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Connect to backend
  backendClient.begin(backendIP, backendPort, "/");
  backendClient.onEvent(backendWebSocketEvent);
  backendClient.setReconnectInterval(5000);
  
  // Start client server
  clientServer.begin();
  clientServer.onEvent(clientWebSocketEvent);
  
  Serial.println("Proxy ready - connecting clients to backend");
}

void loop() {
  backendClient.loop();
  clientServer.loop();
  
  // Process any pending camera data
  processCameraData();
  
  delay(10);
}

void backendWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from backend");
      break;
      
    case WStype_CONNECTED:
      Serial.printf("Connected to backend: %s\n", payload);
      // Register as proxy
      DynamicJsonDocument doc(256);
      doc["type"] = "proxy_register";
      doc["proxy_id"] = "PROXY_" + WiFi.macAddress();
      
      String message;
      serializeJson(doc, message);
      backendClient.sendTXT(message);
      break;
      
    case WStype_TEXT:
      {
        // Forward backend messages to connected clients
        clientServer.broadcastTXT((char*)payload);
      }
      break;
  }
}

void clientWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("Camera client [%u] disconnected\n", num);
      break;
      
    case WStype_CONNECTED:
      Serial.printf("Camera client [%u] connected\n", num);
      break;
      
    case WStype_TEXT:
      {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        handleCameraMessage(num, doc);
      }
      break;
  }
}

void handleCameraMessage(uint8_t clientId, DynamicJsonDocument& doc) {
  String type = doc["type"];
  
  if (type == "camera_frame") {
    processCameraFrame(clientId, doc);
  }
  else if (type == "camera_data") {
    // Raw camera data processing
    String frameData = doc["frame_data"];
    String detectedAction = analyzeMotion(frameData);
    
    if (!detectedAction.isEmpty()) {
      // Send to backend
      DynamicJsonDocument response(512);
      response["type"] = "camera_input";
      response["action"] = detectedAction;
      response["client_id"] = clientId;
      response["timestamp"] = millis();
      
      String message;
      serializeJson(response, message);
      backendClient.sendTXT(message);
      
      Serial.println("Motion detected: " + detectedAction);
    }
  }
  else {
    // Forward other messages to backend
    String message;
    serializeJson(doc, message);
    backendClient.sendTXT(message);
  }
}

void processCameraFrame(uint8_t clientId, DynamicJsonDocument& doc) {
  String frameData = doc["frame_data"];
  int frameWidth = doc["width"];
  int frameHeight = doc["height"];
  
  // Analyze frame for motion/gestures
  String action = analyzeMotion(frameData);
  
  if (!action.isEmpty()) {
    // Send action to backend
    DynamicJsonDocument response(512);
    response["type"] = "proxy_data";
    response["camera_data"] = frameData.substring(0, 100); // Truncated for performance
    response["detected_action"] = action;
    response["client_id"] = clientId;
    response["confidence"] = 0.8; // Mock confidence score
    
    String message;
    serializeJson(response, message);
    backendClient.sendTXT(message);
  }
}

String analyzeMotion(String frameData) {
  // Simple motion detection algorithm
  if (lastCameraFrame.length() == 0) {
    lastCameraFrame = frameData;
    return "";
  }
  
  // Count differences between frames
  int differences = 0;
  int minLength = min(frameData.length(), lastCameraFrame.length());
  
  for (int i = 0; i < minLength; i += 10) { // Sample every 10th character for performance
    if (frameData[i] != lastCameraFrame[i]) {
      differences++;
    }
  }
  
  lastCameraFrame = frameData;
  
  // Detect significant motion
  if (differences > motionThreshold) {
    unsigned long currentTime = millis();
    
    // Debounce - prevent multiple detections
    if (currentTime - lastMotionTime > 500) {
      lastMotionTime = currentTime;
      
      // Simple gesture recognition based on motion pattern
      if (differences > motionThreshold * 2) {
        return "jump"; // High motion = jump
      } else {
        return "move"; // Medium motion = move
      }
    }
  }
  
  return ""; // No significant motion
}

void processCameraData() {
  // Additional camera data processing can be added here
  // This runs continuously to handle any buffered data
}

// Helper function to broadcast to all camera clients
void broadcastToClients(String message) {
  clientServer.broadcastTXT(message);
}

// Helper function to send data to backend
void sendToBackend(String message) {
  backendClient.sendTXT(message);
}