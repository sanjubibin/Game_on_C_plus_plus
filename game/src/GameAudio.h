#pragma once

#include <string>

// The game's sound effects and music, synthesized at start-up.
// Sounds: "click", "coin", "buy", "success", "fail".
void LoadGameAudio();

struct Settings {
    bool music = true;
    bool sound = true;
    bool fullscreen = false;
};

Settings LoadSettings(const std::string& path);
void SaveSettings(const std::string& path, const Settings& settings);
// Applies audio switches and window mode.
void ApplySettings(const Settings& settings);
