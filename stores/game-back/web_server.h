#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Function to initialize the web server
void setupWebServer();

// Function to serve static files
void serveStaticFiles();

// Function to handle game requests
void handleGameRequests();

#endif // WEB_SERVER_H