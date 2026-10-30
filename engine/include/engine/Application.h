#pragma once

#include <string>

#include "engine/SceneManager.h"

namespace engine {

struct AppConfig {
    std::string title = "Game";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool resizable = true;
    bool msaa4x = true;
    int targetFps = 0;  // 0 = uncapped (vsync still applies)
    float fixedTimestep = 1.0f / 60.0f;

    // Development aid: save a PNG of frame N to this path, then quit.
    std::string screenshotPath;
    int screenshotFrame = 0;
};

// Owns the window and the main loop: per-frame Update, fixed-rate FixedUpdate, then Draw.
class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run();
    void Quit() { running_ = false; }

    SceneManager& Scenes() { return scenes_; }
    const AppConfig& Config() const { return config_; }

private:
    AppConfig config_;
    SceneManager scenes_;
    bool running_ = true;
    int frame_ = 0;
};

}  // namespace engine
