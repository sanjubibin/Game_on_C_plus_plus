#include "scenes/PauseScene.h"

#include <memory>
#include <string>

#include "Visuals.h"
#include "engine/Application.h"
#include "scenes/JournalScene.h"
#include "scenes/TitleScene.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

enum Item { kResume, kJournal, kMusic, kSound, kFullscreen, kQuit, kCount };

std::string OnOff(bool value) {
    return value ? "On" : "Off";
}

}  // namespace

PauseScene::PauseScene(engine::Application& app, GameSession& session) : Scene(app), s_(session) {}

std::vector<Rectangle> PauseScene::ButtonRects() const {
    const float cx = static_cast<float>(GetScreenWidth()) / 2.0f;
    const float cy = static_cast<float>(GetScreenHeight()) / 2.0f;
    std::vector<Rectangle> rects;
    for (int i = 0; i < kCount; ++i) {
        rects.push_back({cx - 170.0f, cy - 150.0f + static_cast<float>(i) * 58.0f, 340.0f, 48.0f});
    }
    return rects;
}

void PauseScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        App().Scenes().Pop();
        return;
    }
    Settings& settings = s_.settings;
    auto changed = [&]() {
        ApplySettings(settings);
        SaveSettings(s_.settingsPath, settings);
    };
    switch (menu_.Update(ButtonRects(), {})) {
        case kResume:
            App().Scenes().Pop();
            break;
        case kJournal:
            App().Scenes().Replace(std::make_unique<JournalScene>(App(), s_));
            break;
        case kMusic:
            settings.music = !settings.music;
            changed();
            break;
        case kSound:
            settings.sound = !settings.sound;
            changed();
            break;
        case kFullscreen:
            settings.fullscreen = !settings.fullscreen;
            changed();
            break;
        case kQuit:
            App().Scenes().Clear();
            App().Scenes().Push(std::make_unique<TitleScene>(App(), s_));
            break;
        default:
            break;
    }
}

void PauseScene::Draw(float /*alpha*/) const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    DrawRectangleRec({0.0f, 0.0f, w, h}, Color{20, 12, 20, 150});
    const Rectangle panel{w / 2.0f - 230.0f, h / 2.0f - 280.0f, 460.0f, 560.0f};
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);
    ui::TextCentered("Paused", {w / 2.0f, panel.y + 44.0f}, 40.0f, kInk, true);
    ui::TextCentered("The game saves each night when you sleep.", {w / 2.0f, panel.y + 84.0f}, 18.0f, kMuted);

    const Settings& settings = s_.settings;
    const std::string labels[kCount] = {"Resume",
                                        "Journal",
                                        "Music: " + OnOff(settings.music),
                                        "Sound effects: " + OnOff(settings.sound),
                                        "Fullscreen: " + OnOff(settings.fullscreen),
                                        "Quit to title"};
    const std::vector<Rectangle> rects = ButtonRects();
    for (std::size_t i = 0; i < rects.size(); ++i) {
        ui::DrawButton(rects[i], labels[i], static_cast<int>(i) == menu_.focused, true, 22.0f);
    }
}
