#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

// ===== Communication Configuration =====
#define KEY_SIZE 128
#define MAX_MESSAGE_LEN 200
#define MAX_PEERS 5
#define RESPONSE_DELAY 200  // Delay before responding in ms

// ===== Message Types =====
enum MessageType {
  DISCOVERY_BROADCAST = 0,
  KEY_EXCHANGE = 1,
  DATA_MESSAGE = 4
};

// ===== Data Structures =====
struct PeerInfo {
  uint8_t mac[6];
  uint8_t publicKey[KEY_SIZE];
  bool active;
  char name[20];
};

struct __attribute__((packed)) SecurePacket {
  uint8_t type;
  uint8_t length;
  uint8_t data[250]; // ESP-NOW max payload is 250 bytes
};

// ===== Global Variables =====
Preferences preferences;
uint8_t privateKey[KEY_SIZE];
uint8_t publicKey[KEY_SIZE];
size_t pubKeyLen = 0;

PeerInfo peerList[MAX_PEERS];
int activePeerCount = 0;
unsigned long lastResponseTime = 0;

// ===== Function Declarations =====
void generateKeyPair();
bool loadKeys();
void saveKeys();
bool loadPeerList();
void savePeerList();
void initESPNow();
void sendPublicKey(uint8_t* targetMac);
void sendResponse(uint8_t* targetMac, const char* message);
int addPeer(const uint8_t* mac, const uint8_t* key, size_t keyLen);
void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
void onDataReceived(const uint8_t* mac_addr, const uint8_t* data, int len);
String macToString(const uint8_t* mac);

// ===== Setup Function =====
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) delay(100);
  Serial.println("\n\n===== ESP32 Backend System Starting =====");
  
  // Initialize random number generator
  randomSeed(esp_random());
  
  // Initialize storage
  preferences.begin("esp-backend", false);
  
  // Load or generate keys
  if (!loadKeys()) {
    Serial.println("Generating new key pair...");
    generateKeyPair();
    saveKeys();
  }
  
  // Initialize ESP-NOW
  initESPNow();
  
  // Load saved peer list
  loadPeerList();
  
  Serial.println("Backend ready to receive messages");
  Serial.println("This backend will respond with 'OK' or '1' based on random decision");
  Serial.println("Waiting for messages from main ESP32 system...");
}

// ===== Main Loop =====
void loop() {
  // Just keep the system running, everything is handled by callbacks
  delay(10);
}

// ===== Key Generation =====
void generateKeyPair() {
  // Simple key generation for demo purposes
  memset(privateKey, 0, KEY_SIZE);
  memset(publicKey, 0, KEY_SIZE);
  
  pubKeyLen = KEY_SIZE;
  
  // Generate random keys
  for (int i = 0; i < KEY_SIZE; i++) {
    privateKey[i] = random(256);
    publicKey[i] = privateKey[i] ^ random(256);
  }
  
  Serial.println("Key pair generated");
}

// ===== Key Management =====
bool loadKeys() {
  if (preferences.isKey("privateKey") && preferences.isKey("publicKey")) {
    preferences.getBytes("privateKey", privateKey, KEY_SIZE);
    pubKeyLen = preferences.getBytesLength("publicKey");
    preferences.getBytes("publicKey", publicKey, pubKeyLen);
    
    Serial.println("Keys loaded from storage");
    return true;
  }
  return false;
}

void saveKeys() {
  preferences.putBytes("privateKey", privateKey, KEY_SIZE);
  preferences.putBytes("publicKey", publicKey, pubKeyLen);
  Serial.println("Keys saved to storage");
}

// ===== Peer Management =====
bool loadPeerList() {
  // Initialize peer list
  for (int i = 0; i < MAX_PEERS; i++) {
    peerList[i].active = false;
  }
  
  // Load saved peers
  activePeerCount = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    char keyName[16];
    sprintf(keyName, "peer_%d", i);
    
    if (preferences.isKey(keyName)) {
      size_t dataSize = preferences.getBytesLength(keyName);
      if (dataSize > 0) {
        uint8_t data[sizeof(PeerInfo)];
        preferences.getBytes(keyName, data, dataSize);
        
        PeerInfo* peer = (PeerInfo*)data;
        if (peer->active) {
          memcpy(&peerList[i], peer, sizeof(PeerInfo));
          activePeerCount++;
          
          // Add as ESP-NOW peer
          esp_now_peer_info_t peerInfo = {};
          memcpy(peerInfo.peer_addr, peerList[i].mac, 6);
          peerInfo.channel = 0;
          peerInfo.encrypt = false;
          
          esp_now_add_peer(&peerInfo);
        }
      }
    }
  }
  
  Serial.printf("Loaded %d peers from storage\n", activePeerCount);
  return activePeerCount > 0;
}

void savePeerList() {
  for (int i = 0; i < MAX_PEERS; i++) {
    char keyName[16];
    sprintf(keyName, "peer_%d", i);
    
    if (peerList[i].active) {
      preferences.putBytes(keyName, &peerList[i], sizeof(PeerInfo));
    } else if (preferences.isKey(keyName)) {
      preferences.remove(keyName);
    }
  }
  
  Serial.printf("Saved %d peers to storage\n", activePeerCount);
}

int addPeer(const uint8_t* mac, const uint8_t* key, size_t keyLen) {
  // Check if already exists
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peerList[i].active && memcmp(peerList[i].mac, mac, 6) == 0) {
      // Update key if provided
      if (key != NULL && keyLen > 0) {
        memcpy(peerList[i].publicKey, key, keyLen);
      }
      return i; // Already exists
    }
  }
  
  // Find empty slot
  int emptySlot = -1;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peerList[i].active) {
      emptySlot = i;
      break;
    }
  }
  
  if (emptySlot == -1) {
    Serial.println("Peer list full. Remove a peer first.");
    return -1;
  }
  
  // Add to peer list
  memcpy(peerList[emptySlot].mac, mac, 6);
  
  if (key != NULL && keyLen > 0) {
    memcpy(peerList[emptySlot].publicKey, key, keyLen);
  }
  
  peerList[emptySlot].active = true;
  sprintf(peerList[emptySlot].name, "Main_%d", emptySlot);
  
  // Register with ESP-NOW
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_is_peer_exist(mac) == false) {
    esp_now_add_peer(&peerInfo);
  }
  
  activePeerCount++;
  savePeerList();
  
  // Display added peer info
  Serial.printf("Added peer: %s\n", macToString(mac).c_str());
  
  return emptySlot;
}

// ===== ESP-NOW Initialization =====
void initESPNow() {
  WiFi.mode(WIFI_STA);
  
  Serial.print("Backend ESP32 MAC: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  
  Serial.println("ESP-NOW initialized");
}

// ===== Communication Functions =====
void sendPublicKey(uint8_t* targetMac) {
  // Prepare packet with public key
  SecurePacket packet;
  packet.type = KEY_EXCHANGE;
  packet.length = pubKeyLen;
  memcpy(packet.data, publicKey, pubKeyLen);
  
  // Register temporary peer if not already in list
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, targetMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_is_peer_exist(targetMac) == false) {
    esp_now_add_peer(&peerInfo);
  }
  
  // Send data
  esp_err_t result = esp_now_send(targetMac, (uint8_t*)&packet, 
                                 sizeof(packet.type) + sizeof(packet.length) + packet.length);
  
  if (result == ESP_OK) {
    Serial.println("Public key sent successfully");
  } else {
    Serial.println("Failed to send public key");
  }
}

void sendResponse(uint8_t* targetMac, const char* message) {
  // Prepare packet
  SecurePacket packet;
  packet.type = DATA_MESSAGE;
  packet.length = strlen(message);
  memcpy(packet.data, message, packet.length);
  
  // Send data
  esp_err_t result = esp_now_send(targetMac, (uint8_t*)&packet, 
                                 sizeof(packet.type) + sizeof(packet.length) + packet.length);
  
  if (result == ESP_OK) {
    Serial.printf("Response sent: %s\n", message);
  } else {
    Serial.println("Failed to send response");
  }
}

// ===== Helper Functions =====
String macToString(const uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// ===== ESP-NOW Callbacks =====
void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

void onDataReceived(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
  SecurePacket* packet = (SecurePacket*)data;
  
  switch (packet->type) {
    case DISCOVERY_BROADCAST:
      {
        Serial.printf("Discovered main ESP32: %s\n", macToString(mac_addr).c_str());
        
        // Add to peer list without key
        int idx = addPeer(mac_addr, NULL, 0);
        
        // Send our public key to establish secure connection
        if (idx >= 0) {
          sendPublicKey((uint8_t*)mac_addr);
        }
      }
      break;
      
    case KEY_EXCHANGE:
      {
        Serial.printf("Received public key from: %s\n", macToString(mac_addr).c_str());
        
        // Add/update peer with key
        int peerIdx = addPeer(mac_addr, packet->data, packet->length);
        
        // Send our key in response
        sendPublicKey((uint8_t*)mac_addr);
        
        Serial.println("Key exchange completed");
      }
      break;
      
    case DATA_MESSAGE:
      {
        // Check if this is from a known peer
        int fromPeerIdx = -1;
        for (int i = 0; i < MAX_PEERS; i++) {
          if (peerList[i].active && memcmp(peerList[i].mac, mac_addr, 6) == 0) {
            fromPeerIdx = i;
            break;
          }
        }
        
        if (fromPeerIdx >= 0) {
          // Get the message
          char message[MAX_MESSAGE_LEN + 1];
          size_t msgLen = packet->length > MAX_MESSAGE_LEN ? MAX_MESSAGE_LEN : packet->length;
          memcpy(message, packet->data, msgLen);
          message[msgLen] = '\0';
          
          // Print received message
          Serial.printf("Message from %s: %s\n", peerList[fromPeerIdx].name, message);
          
          // Check if this is from Mode A ESP32 (MAC: 14:33:5C:2F:35:F4)
          // Convert mac_addr to string for comparison
          char macStr[18];
          sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
                mac_addr[0], mac_addr[1], mac_addr[2], 
                mac_addr[3], mac_addr[4], mac_addr[5]);
          
          bool isFromModeA = (strncmp(macStr, "14:33:5C:2F:35:F4", 17) == 0);
          
          if (isFromModeA) {
            // This is from Mode A - check if it contains a Mode B MAC address
            if (strchr(message, ':') != NULL) {
              // It contains a MAC address - decide randomly to approve or not
              if (random(10) > 2) {  // 80% chance of approval
                // Generate random times for traffic lights
                int newRedTime = random(5000, 15000);  // 5-15 seconds
                int newBlueTime = random(3000, 10000); // 3-10 seconds
                
                // Create response with timing values
                char response[50];
                sprintf(response, "OK %d %d", newRedTime, newBlueTime);
                
                // Send response
                sendResponse((uint8_t*)mac_addr, response);
                Serial.printf("Sending new timing values - Red: %d ms, Blue: %d ms\n", 
                            newRedTime, newBlueTime);
              } else {
                // Send NOT OK response
                sendResponse((uint8_t*)mac_addr, "NOT OK");
                Serial.println("Sending NOT OK - keeping current timing");
              }
            } else {
              // Not a MAC address message, just echo back
              char response[32];
              sprintf(response, "Echo: %s", message);
              sendResponse((uint8_t*)mac_addr, response);
            }
          } else {
            // Not from Mode A, just respond with echo
            char response[32];
            sprintf(response, "Echo: %s", message);
            sendResponse((uint8_t*)mac_addr, response);
          }
          
          lastResponseTime = millis();
        } else {
          Serial.println("Message received from unknown peer");
        }
      }
      break;
      
    default:
      Serial.println("Unknown packet type");
      break;
  }
}