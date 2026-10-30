#include "scenes/PauseScene.h"

#include <array>
#include <memory>

#include "Visuals.h"
#include "engine/Application.h"
#include "scenes/JournalScene.h"
#include "scenes/TitleScene.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr std::array<const char*, 3> kItems = {"Resume", "Journal", "Quit to title"};

}  // namespace

PauseScene::PauseScene(engine::Application& app, GameSession& session) : Scene(app), s_(session) {}

std::vector<Rectangle> PauseScene::ButtonRects() const {
    const float cx = static_cast<float>(GetScreenWidth()) / 2.0f;
    const float cy = static_cast<float>(GetScreenHeight()) / 2.0f;
    std::vector<Rectangle> rects;
    for (std::size_t i = 0; i < kItems.size(); ++i) {
        rects.push_back({cx - 160.0f, cy - 60.0f + static_cast<float>(i) * 64.0f, 320.0f, 52.0f});
    }
    return rects;
}

void PauseScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        App().Scenes().Pop();
        return;
    }
    switch (menu_.Update(ButtonRects(), {})) {
        case 0:
            App().Scenes().Pop();
            break;
        case 1:
            App().Scenes().Replace(std::make_unique<JournalScene>(App(), s_));
            break;
        case 2:
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
    const Rectangle panel{w / 2.0f - 220.0f, h / 2.0f - 170.0f, 440.0f, 340.0f};
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);
    ui::TextCentered("Paused", {w / 2.0f, panel.y + 48.0f}, 40.0f, kInk, true);
    ui::TextCentered("The game saves each night when you sleep.", {w / 2.0f, panel.y + 88.0f}, 18.0f, kMuted);

    const std::vector<Rectangle> rects = ButtonRects();
    for (std::size_t i = 0; i < rects.size(); ++i) {
        ui::DrawButton(rects[i], kItems[i], static_cast<int>(i) == menu_.focused);
    }
}
