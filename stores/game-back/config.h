#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// Server settings
const int serverPort = 80;

// Game settings
const char* dinosaurGamePath = "/games/dinosaur/game.html";
const char* multiplayerLobbyPath = "/games/multiplayer/lobby.html";

// Maximum number of players for multiplayer games
const int maxPlayers = 4;

#endif // CONFIG_H