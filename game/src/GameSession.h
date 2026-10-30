#pragma once

#include <raylib.h>

#include <algorithm>
#include <string>
#include <vector>

#include "GameAudio.h"
#include "logic/Content.h"
#include "logic/GameState.h"

struct Toast {
    std::string text;
    float time = 0.0f;
};

// Shared by every scene: authored content, the live game state, and a few presentation bits.
struct GameSession {
    game::Content content;
    game::GameState state;

    std::string currentMap = "town";
    Vector2 playerPos{0.0f, 0.0f};  // feet position in world pixels
    std::vector<Vector2> trail;     // recent player positions, newest first (for followers)
    std::vector<Toast> toasts;
    double fadeStart = -100.0;  // when the player last changed area (drives a short fade-in)

    std::string savePath = "saves/slot1.json";
    std::string settingsPath = "settings.json";
    Settings settings;
    bool devMode = false;  // started with --start; never writes the save file

    void AddToast(const std::string& text) { toasts.push_back({text, 3.5f}); }

    void UpdateToasts(float dt) {
        for (Toast& toast : toasts) {
            toast.time -= dt;
        }
        std::erase_if(toasts, [](const Toast& t) { return t.time <= 0.0f; });
    }
};
