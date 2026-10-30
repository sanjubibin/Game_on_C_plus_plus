#include "engine/Application.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>

#include "engine/Audio.h"

namespace engine {

Application::Application(const AppConfig& config) : config_(config) {
    unsigned int flags = 0;
    if (config_.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    if (config_.resizable) {
        flags |= FLAG_WINDOW_RESIZABLE;
    }
    if (config_.msaa4x) {
        flags |= FLAG_MSAA_4X_HINT;
    }
    SetConfigFlags(flags);

    InitWindow(config_.width, config_.height, config_.title.c_str());
    if (config_.targetFps > 0) {
        SetTargetFPS(config_.targetFps);
    }

    // ESC is left to scenes (pause menus) instead of closing the window.
    SetExitKey(KEY_NULL);
    // Resolve "assets/..." relative to the executable, not the launch directory.
    ChangeDirectory(GetApplicationDirectory());
}

Application::~Application() {
    // Scenes may own GPU resources, so destroy them while the window still exists.
    scenes_.Clear();
    scenes_.ApplyPending();
    CloseWindow();
}

void Application::Run() {
    // Clamp long frames (breakpoints, window drags) so the simulation can't spiral.
    constexpr float kMaxFrameTime = 0.25f;
    const float step = config_.fixedTimestep;
    float accumulator = 0.0f;

    scenes_.ApplyPending();
    while (running_ && !WindowShouldClose() && !scenes_.Empty()) {
        const float frameTime = std::min(GetFrameTime(), kMaxFrameTime);
        accumulator += frameTime;

        audio::Update();
        scenes_.Update(frameTime);
        while (accumulator >= step) {
            scenes_.FixedUpdate(step);
            accumulator -= step;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        scenes_.Draw(accumulator / step);
        ++frame_;
        if (!config_.screenshotPath.empty() && frame_ == config_.screenshotFrame) {
            rlDrawRenderBatchActive();  // flush pending draws before reading the back buffer
            Image shot = LoadImageFromScreen();
            ExportImage(shot, config_.screenshotPath.c_str());
            UnloadImage(shot);
            running_ = false;
        }
        EndDrawing();

        scenes_.ApplyPending();
    }
}

}  // namespace engine
