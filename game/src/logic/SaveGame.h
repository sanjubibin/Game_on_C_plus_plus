#pragma once

#include <string>

#include "logic/GameState.h"

namespace game {

// Saves are JSON so they are easy to inspect while developing.
bool SaveGame(const std::string& path, const GameState& state, std::string* error = nullptr);
bool LoadGame(const std::string& path, GameState& state, std::string* error = nullptr);
bool SaveExists(const std::string& path);

}  // namespace game
