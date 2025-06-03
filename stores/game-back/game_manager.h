#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

class GameManager {
public:
    GameManager();
    void addGame(const String& name, const String& path);
    void removeGame(const String& name);
    void listGames();
    void loadGameConfig();
    void handleGameRequest(AsyncWebServerRequest *request, const String& gameName);

private:
    struct Game {
        String name;
        String path;
    };

    std::vector<Game> games;
};

#endif // GAME_MANAGER_H