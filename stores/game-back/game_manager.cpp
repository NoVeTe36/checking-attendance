#include "game_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

GameManager::GameManager() {
    // Initialize game list
}

void GameManager::loadGames() {
    // Load game configurations from JSON file
    File file = LittleFS.open("/config/games.json", "r");
    if (!file) {
        Serial.println("Failed to open games configuration file");
        return;
    }

    // Read the file and parse the JSON
    String json = file.readString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.println("Failed to parse JSON");
        return;
    }

    // Iterate through the games and add them to the game list
    JsonArray games = doc["games"].as<JsonArray>();
    for (JsonVariant game : games) {
        String name = game["name"];
        String path = game["path"];
        addGame(name, path);
    }
    file.close();
}

void GameManager::addGame(const String& name, const String& path) {
    Game game;
    game.name = name;
    game.path = path;
    games.push_back(game);
}

void GameManager::removeGame(const String& name) {
    for (auto it = games.begin(); it != games.end(); ++it) {
        if (it->name == name) {
            games.erase(it);
            break;
        }
    }
}

const std::vector<Game>& GameManager::getGames() const {
    return games;
}

void GameManager::startGame(const String& name) {
    // Logic to start a game session
    Serial.println("Starting game: " + name);
}

void GameManager::endGame(const String& name) {
    // Logic to end a game session
    Serial.println("Ending game: " + name);
}