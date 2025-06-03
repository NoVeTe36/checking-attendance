#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "web_server.h"
#include "config.h"

// Create an AsyncWebServer instance
AsyncWebServer server(80);

// Function to serve static files from LittleFS
void serveFiles() {
    server.serveStatic("/", LittleFS, "/");
    
    // Handle root path with fallback
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(404, "text/plain", "File not found");
        }
    });

    // Handle dinosaur game
    server.on("/games/dinosaur/game.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/games/dinosaur/game.html")) {
            request->send(LittleFS, "/games/dinosaur/game.html", "text/html");
        } else {
            request->send(404, "text/plain", "Game not found");
        }
    });

    // Handle multiplayer lobby
    server.on("/games/multiplayer/lobby.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/games/multiplayer/lobby.html")) {
            request->send(LittleFS, "/games/multiplayer/lobby.html", "text/html");
        } else {
            request->send(404, "text/plain", "Lobby not found");
        }
    });

    // Handle 404 errors
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "File not found");
    });

    // Start the server
    server.begin();
    Serial.println("HTTP server started");
}

void setupWebServer() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // Serve files from LittleFS
    serveFiles();
}