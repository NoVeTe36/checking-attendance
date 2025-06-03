#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Network configuration
const char* ssid = "VietTung2003";
const char* password = "0888092811";
const char* middlewareIP = "192.168.1.101"; // Middleware ESP32 IP
const int middlewarePort = 82;

// Hardware - Updated for your button setup
const int buttonPin1 = 26;  // First leg of button
const int buttonPin2 = 14;  // Second leg of button
const int ledPin = 2;       // Built-in LED (GPIO 2 on most ESP32s)

// Button handling
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Game state
WebSocketsClient webSocket;
String playerName = "Player_" + String((uint32_t)ESP.getEfuseMac(), HEX);
bool gameActive = false;
bool isConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("🎮 ESP32 Dinosaur Jump Player Starting...");
  
  // Initialize hardware
  pinMode(buttonPin1, INPUT_PULLUP);  // One side with pullup
  pinMode(buttonPin2, OUTPUT);        // Other side as output (will be LOW)
  digitalWrite(buttonPin2, LOW);      // Set second pin to LOW
  
  pinMode(ledPin, OUTPUT);
  
  // Startup LED sequence
  for(int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
    delay(200);
  }
  
  // Connect to WiFi
  Serial.println("Connecting to WiFi: " + String(ssid));
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    digitalWrite(ledPin, !digitalRead(ledPin)); // Blink while connecting
  }
  
  Serial.println("✅ WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Player ID: " + playerName);
  digitalWrite(ledPin, HIGH);
  
  // Connect to middleware
  Serial.println("Connecting to middleware...");
  webSocket.begin(middlewareIP, middlewarePort, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  Serial.println("🎯 Ready to play!");
  Serial.println("Commands: 'create', 'join', 'status'");
  Serial.println("Press the button to jump!");
}

void loop() {
  webSocket.loop();
  handleButton();
  
  // Handle serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }
  
  delay(10);
}

void handleButton() {
  // Read button state (LOW when pressed because of pullup)
  bool currentButtonState = digitalRead(buttonPin1);
  
  // Debounce logic
  if (currentButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentButtonState != buttonPressed) {
      buttonPressed = currentButtonState;
      
      // Button pressed (goes LOW)
      if (buttonPressed == LOW) {
        handleButtonPress();
      }
    }
  }
  
  lastButtonState = currentButtonState;
}

void handleButtonPress() {
  if (!isConnected) {
    Serial.println("❌ Not connected to middleware");
    // Flash LED for error
    for(int i = 0; i < 3; i++) {
      digitalWrite(ledPin, LOW);
      delay(100);
      digitalWrite(ledPin, HIGH);
      delay(100);
    }
    return;
  }
  
  // Send jump action
  DynamicJsonDocument doc(256);
  doc["type"] = "game_action";
  doc["action"] = "jump";
  doc["player"] = playerName;
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("🦘 JUMP! Button pressed");
  
  // Visual feedback - quick flash
  digitalWrite(ledPin, LOW);
  delay(100);
  digitalWrite(ledPin, HIGH);
}

void handleSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  if (command == "join") {
    joinGame();
  }
  else if (command == "jump") {
    handleButtonPress();
  }
  else if (command == "status") {
    printStatus();
  }
  else if (command == "test") {
    testConnection();
  }
  else if (command == "create") {
    createRoom();
  }
  else if (command == "help") {
    printHelp();
  }
  else {
    Serial.println("Commands: create, join, jump, status, test, help");
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("🔌 Disconnected from middleware");
      isConnected = false;
      gameActive = false;
      digitalWrite(ledPin, LOW);
      break;
      
    case WStype_CONNECTED:
      Serial.printf("✅ Connected to middleware: %s\n", payload);
      isConnected = true;
      digitalWrite(ledPin, HIGH);
      
      // Auto-join game after connection
      delay(1000);
      Serial.println("Auto-joining game...");
      joinGame();
      break;
      
    case WStype_TEXT:
      {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.println("JSON Error: " + String(error.c_str()));
          return;
        }
        handleServerMessage(doc);
      }
      break;
  }
}

void handleServerMessage(DynamicJsonDocument& doc) {
  String type = doc["type"];
  
  if (type == "game_state") {
    gameActive = true;
    
    // Check player status
    if (doc.containsKey("players")) {
      JsonArray players = doc["players"];
      Serial.println("👥 Players: " + String(players.size()));
      
      // Find this player's status
      for (JsonObject player : players) {
        if (player["name"] == playerName) {
          bool isAlive = player["isAlive"];
          int score = player["score"];
          
          if (!isAlive) {
            Serial.println("💀 You died! Score: " + String(score));
            // Flash LED rapidly when dead
            for(int i = 0; i < 5; i++) {
              digitalWrite(ledPin, LOW);
              delay(100);
              digitalWrite(ledPin, HIGH);
              delay(100);
            }
          } else {
            Serial.println("💚 Alive | Score: " + String(score));
          }
          break;
        }
      }
    }
    
    if (doc.containsKey("topScore")) {
      Serial.println("🏆 Top Score: " + String(doc["topScore"].as<int>()));
    }
  }
  else if (type == "room_joined") {
    Serial.println("🚪 Joined room: " + doc["roomId"].as<String>());
    gameActive = true;
  }
  else if (type == "room_created") {
    Serial.println("🏠 Created room: " + doc["roomId"].as<String>());
    gameActive = true;
  }
  else if (type == "action_confirmed") {
    Serial.println("✅ Jump confirmed!");
  }
  else if (type == "welcome") {
    Serial.println("👋 " + doc["message"].as<String>());
  }
  else if (type == "error") {
    Serial.println("❌ Error: " + doc["message"].as<String>());
  }
  else {
    Serial.println("📨 Received: " + type);
  }
}

void joinGame() {
  if (!isConnected) {
    Serial.println("❌ Not connected to middleware");
    return;
  }
  
  DynamicJsonDocument doc(512);
  doc["type"] = "join_game";
  doc["playerName"] = playerName;
  doc["device_type"] = "button_client";
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("🎮 Joining game as: " + playerName);
}

void createRoom() {
  if (!isConnected) {
    Serial.println("❌ Not connected to middleware");
    return;
  }
  
  DynamicJsonDocument doc(512);
  doc["type"] = "create_room";
  doc["playerName"] = playerName;
  doc["gameName"] = "dinosaur_jump";
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("🏠 Creating dinosaur jump room...");
}

void printStatus() {
  Serial.println("\n📊 === Player Status ===");
  Serial.println("Player: " + playerName);
  Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Connected" : "❌ Disconnected"));
  Serial.println("Middleware: " + String(isConnected ? "✅ Connected" : "❌ Disconnected"));
  Serial.println("Game Active: " + String(gameActive ? "🎮 Yes" : "⏸️ No"));
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Signal: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("Button Pin 1: " + String(buttonPin1) + " (INPUT_PULLUP)");
  Serial.println("Button Pin 2: " + String(buttonPin2) + " (OUTPUT LOW)");
  Serial.println("========================\n");
}

void testConnection() {
  if (!isConnected) {
    Serial.println("❌ Not connected to middleware");
    return;
  }
  
  DynamicJsonDocument doc(256);
  doc["type"] = "ping";
  doc["message"] = "test from " + playerName;
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("🏓 Ping sent");
}

void printHelp() {
  Serial.println("\n🎮 === Commands ===");
  Serial.println("create - Create new game");
  Serial.println("join   - Join existing game");
  Serial.println("jump   - Manual jump");
  Serial.println("status - Show status");
  Serial.println("test   - Test connection");
  Serial.println("help   - Show this menu");
  Serial.println("==================\n");
}