#include <esp_now.h>
#include <WiFi.h>

// Central ESP32 MAC Address - Use the one from receiver!
uint8_t centralMAC[] = {0xA0, 0xA3, 0xB3, 0x2A, 0xC5, 0x14};

// Pin definitions
const int SENSOR_PIN = 22;      // MH Infrared Obstacle Sensor OUT pin
const int RED_LED_PIN = 19;     // Red LED pin
const int BLUE_LED_PIN = 18;    // Blue LED pin

// Variables
bool lastSensorState = HIGH;   // Assume no obstacle initially
unsigned long lastDetectionTime = 0;
const unsigned long DEBOUNCE_DELAY = 2000; // 2 second debounce

// LED timing variables
unsigned long ledStartTime = 0;
bool ledActive = false;

// Structure to send data
typedef struct {
  char sensor_id[32];
  bool detection;
  unsigned long timestamp;
} struct_message;

// Structure to receive response
typedef struct {
  bool access_granted;
  char message[64];
} struct_response;

struct_message sensorData;
struct_response receivedResponse;

// Function declarations
void sendDataToCentral();
void setLEDStatus(String color);
void updateLEDs();
void turnOffAllLEDs();

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("SUCCESS - Waiting for response...");
  } else {
    Serial.print("FAILED - Status code: ");
    Serial.println(status);
  }
}

// Callback when data is received (response from central)
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("*** RESPONSE RECEIVED ***");
  memcpy(&receivedResponse, incomingData, sizeof(receivedResponse));
  
  Serial.print("Response: ");
  Serial.println(receivedResponse.message);
  
  // Show result on LEDs based on response
  if (receivedResponse.access_granted) {
    Serial.println(">>> BLUE LED ON (Access Granted)");
    setLEDStatus("blue");
  } else {
    Serial.println(">>> RED LED ON (Access Denied)");
    setLEDStatus("red");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for serial monitor
  
  // Initialize pins
  pinMode(SENSOR_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  
  // CRITICAL: Turn off both LEDs at startup
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  ledActive = false;
  
  Serial.println("=== SENDER ESP32 ===");
  Serial.println("Both LEDs should be OFF now!");
  
  // Set device as a Wi-Fi Station BEFORE initializing ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Disconnect from any WiFi network
  
  // Print MAC addresses for debugging
  Serial.print("Sender WiFi MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Print ESP-NOW MAC (might be different)
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  Serial.print("Sender ESP-NOW MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", baseMac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  Serial.print("Target Receiver MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", centralMAC[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  Serial.println("==================");
  
  // Init ESP-NOW AFTER WiFi setup
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW initialized successfully");
  
  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // FIXED: Register peer with proper structure
  esp_now_peer_info_t peerInfo = {};  // Initialize to zero
  memcpy(peerInfo.peer_addr, centralMAC, 6);
  peerInfo.channel = 0;        // Use same channel as receiver
  peerInfo.encrypt = false;    // No encryption
  peerInfo.ifidx = WIFI_IF_STA; // IMPORTANT: Specify interface
  
  // Add peer        
  esp_err_t addPeerResult = esp_now_add_peer(&peerInfo);
  if (addPeerResult != ESP_OK) {
    Serial.print("Failed to add peer! Error code: ");
    Serial.println(addPeerResult);
    Serial.println("Check if receiver is running and MAC address is correct!");
    return;
  }
  
  Serial.println("Peer added successfully!");
  Serial.println("ESP-NOW ready. Waiting for motion...");
}

void loop() {
  // DEBUG: Print sensor state every 3 seconds
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 3000) {
    Serial.print("Sensor PIN 22 state: ");
    Serial.println(digitalRead(SENSOR_PIN) == LOW ? "LOW (Motion)" : "HIGH (No motion)");
    lastDebugPrint = millis();
  }
  
  bool currentSensorState = digitalRead(SENSOR_PIN);
  
  // Check for obstacle detection (LOW means obstacle detected)
  if (currentSensorState == LOW && lastSensorState == HIGH) {
    unsigned long currentTime = millis();
    
    // Debounce check
    if (currentTime - lastDetectionTime > DEBOUNCE_DELAY) {
      Serial.println("\n*** MOTION DETECTED ***");
      Serial.println("Sending to receiver...");
      sendDataToCentral();
      lastDetectionTime = currentTime;
    }
  }
  
  lastSensorState = currentSensorState;
  
  // Update LED status (non-blocking)
  updateLEDs();
  
  delay(50);
}

void sendDataToCentral() {
  // Prepare data to send
  strcpy(sensorData.sensor_id, "ESP32_SENSOR_01");
  sensorData.detection = true;
  sensorData.timestamp = millis();
  
  Serial.println("Preparing to send data...");
  Serial.print("Data size: ");
  Serial.println(sizeof(sensorData));
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(centralMAC, (uint8_t *) &sensorData, sizeof(sensorData));
   
  if (result == ESP_OK) {
    Serial.println("Send command executed successfully");
  } else {
    Serial.print("Send ERROR! Code: ");
    Serial.println(result);
    Serial.println("Check if receiver is running and MAC address is correct!");
  }
}

void setLEDStatus(String color) {
  // Turn off any existing LEDs first
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  
  ledStartTime = millis();
  ledActive = true;
  
  if (color == "red") {
    digitalWrite(RED_LED_PIN, HIGH);
    Serial.println("RED LED ON");
  } else if (color == "blue") {
    digitalWrite(BLUE_LED_PIN, HIGH);
    Serial.println("BLUE LED ON");
  }
}

void updateLEDs() {
  // Turn off LEDs after 3 seconds
  if (ledActive && (millis() - ledStartTime > 3000)) {
    turnOffAllLEDs();
    ledActive = false;
    Serial.println("LEDs turned OFF");
  }
}

void turnOffAllLEDs() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
}