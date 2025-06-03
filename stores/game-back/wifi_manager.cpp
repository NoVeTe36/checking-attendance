#include <WiFi.h>
#include "wifi_manager.h"
#include "config.h"

WiFiManager::WiFiManager() {}

void WiFiManager::connect() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    Serial.println("Disconnected from WiFi");
}