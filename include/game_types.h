#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <Arduino.h>
#include <vector>
#include <string>

// Structure to hold game information
struct Game {
    String name;          // Name of the game
    String path;          // Path to the game's HTML file
    String description;   // Description of the game
};

// Class to manage the collection of games
class GameManager {
public:
    void addGame(const Game& game);
    void removeGame(const String& gameName);
    std::vector<Game> getGames() const;

private:
    std::vector<Game> games; // List of games
};

#endif // GAME_TYPES_H