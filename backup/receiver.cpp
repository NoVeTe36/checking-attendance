#include <esp_now.h>
#include <WiFi.h>

// LED pin for status indication
const int STATUS_LED_PIN = 2;

// Structure to receive data - FIXED SYNTAX
typedef struct {
  char sensor_id[32];
  bool detection;
  unsigned long timestamp;
} struct_message;

// Structure to send response - FIXED SYNTAX
typedef struct {
  bool access_granted;
  char message[64];
} struct_response;

struct_message receivedData;
struct_response responseData;

// Store sender MAC address
uint8_t senderMAC[6];

// Function declarations
void processAndRespond();
bool simulateAccessCheck(String sensorId);

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // Store sender's MAC address
  memcpy(senderMAC, mac, 6);
  
  // Copy received data
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  Serial.println("=== DATA RECEIVED ===");
  Serial.print("Sensor ID: ");
  Serial.println(receivedData.sensor_id);
  Serial.print("Detection: ");
  Serial.println(receivedData.detection);
  Serial.print("Timestamp: ");
  Serial.println(receivedData.timestamp);
  
  // Print sender's MAC address
  Serial.print("From Sender MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", senderMAC[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  // Process the data and send response
  processAndRespond();
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Response Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Delivery Success");
  } else {
    Serial.println("Delivery Fail");
  }
  Serial.println("==================");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for serial monitor
  
  // Initialize status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // Set device as a Wi-Fi Station BEFORE ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Disconnect from any WiFi network
  
  // Print MAC Address debugging
  Serial.println("=== CENTRAL ESP32 ===");
  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // IMPORTANT: Print ESP-NOW specific MAC
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  Serial.print("ESP-NOW MAC Address: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", baseMac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("Copy the ESP-NOW MAC address to the sender module!");
  Serial.println("====================");
  
  // Init ESP-NOW AFTER WiFi setup
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW initialized successfully");
  
  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Central ESP-NOW module ready");
  Serial.println("Waiting for sensor data...");
  digitalWrite(STATUS_LED_PIN, HIGH); // Indicate ready
}

void loop() {
  // Add heartbeat to show receiver is alive
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) { // Every 10 seconds
    Serial.println("Receiver alive, waiting for data...");
    lastHeartbeat = millis();
  }
  
  delay(100);
}

void processAndRespond() {
  // Simulate database check with random result
  bool accessGranted = simulateAccessCheck(String(receivedData.sensor_id));
  
  // Prepare response
  responseData.access_granted = accessGranted;
  if (accessGranted) {
    strcpy(responseData.message, "Access granted");
  } else {
    strcpy(responseData.message, "Access denied");
  }
  
  Serial.print("Preparing response: ");
  Serial.println(responseData.message);
  
  // FIXED: Register the sender as a peer with proper structure
  esp_now_peer_info_t peerInfo = {};  // Initialize to zero
  memcpy(peerInfo.peer_addr, senderMAC, 6);
  peerInfo.channel = 0;        // Use same channel
  peerInfo.encrypt = false;    // No encryption
  peerInfo.ifidx = WIFI_IF_STA; // IMPORTANT: Specify interface
  
  // Check if peer exists, if not add it
  if (!esp_now_is_peer_exist(senderMAC)) {
    Serial.println("Adding sender as peer...");
    esp_err_t addPeerResult = esp_now_add_peer(&peerInfo);
    if (addPeerResult != ESP_OK) {
      Serial.print("Failed to add peer! Error code: ");
      Serial.println(addPeerResult);
      return;
    }
    Serial.println("Sender added as peer successfully!");
  }
  
  // Send response back to sensor
  Serial.println("Sending response to sender...");
  esp_err_t result = esp_now_send(senderMAC, (uint8_t *) &responseData, sizeof(responseData));
   
  if (result == ESP_OK) {
    Serial.println("Response sent successfully");
  } else {
    Serial.print("Error sending response, code: ");
    Serial.println(result);
  }
}

bool simulateAccessCheck(String sensorId) {
  // Simulate database check with random result
  Serial.println("Simulating database check for sensor: " + sensorId);
  
  // Random access decision (70% chance of access granted)
  int randomValue = random(0, 100);
  bool accessGranted = randomValue < 70;
  
  Serial.println("Access decision: " + String(accessGranted ? "GRANTED" : "DENIED"));
  
  return accessGranted;
}