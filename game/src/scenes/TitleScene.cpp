#include "scenes/TitleScene.h"

#include <array>
#include <cmath>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "logic/SaveGame.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr std::array<const char*, 3> kItems = {"Continue", "New game", "Quit"};

}  // namespace

TitleScene::TitleScene(engine::Application& app, GameSession& session)
    : Scene(app), s_(session), hasSave_(game::SaveExists(session.savePath)) {
    menu_.focused = hasSave_ ? 0 : 1;
}

std::vector<Rectangle> TitleScene::ButtonRects() const {
    const float cx = static_cast<float>(GetScreenWidth()) / 2.0f;
    const float top = static_cast<float>(GetScreenHeight()) / 2.0f + 20.0f;
    std::vector<Rectangle> rects;
    for (std::size_t i = 0; i < kItems.size(); ++i) {
        rects.push_back({cx - 150.0f, top + static_cast<float>(i) * 66.0f, 300.0f, 54.0f});
    }
    return rects;
}

void TitleScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        App().Quit();
        return;
    }
    switch (menu_.Update(ButtonRects(), {hasSave_, true, true})) {
        case 0:
            if (!flow::ContinueGame(App(), s_, &error_)) {
                hasSave_ = false;
                menu_.focused = 1;
            }
            break;
        case 1:
            flow::NewGame(App(), s_);
            break;
        case 2:
            App().Quit();
            break;
        default:
            break;
    }
}

void TitleScene::FixedUpdate(float dt) {
    time_ += dt;
}

void TitleScene::Draw(float /*alpha*/) const {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float w = static_cast<float>(screenWidth);
    const float h = static_cast<float>(screenHeight);

    // Warm evening sky: the shop's closing time.
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, Color{255, 183, 120, 255},
                           Color{110, 70, 140, 255});
    DrawCircleV({w * 0.84f, h * 0.44f}, 70.0f, Color{255, 220, 140, 200});

    // Little skyline: the corner shop next to MegaMart.
    const float ground = h * 0.88f;
    DrawRectangleRec({0.0f, ground, w, h - ground}, Color{70, 50, 90, 255});
    DrawRectangleRec({w * 0.12f, ground - 120.0f, 180.0f, 120.0f}, Color{90, 60, 100, 255});
    DrawRectangleRec({w * 0.12f - 10.0f, ground - 150.0f, 200.0f, 36.0f}, Color{70, 120, 95, 255});
    DrawRectangleRec({w * 0.12f + 70.0f, ground - 60.0f, 40.0f, 60.0f}, Color{255, 210, 120, 255});
    DrawRectangleRec({w * 0.62f, ground - 220.0f, 380.0f, 220.0f}, Color{80, 70, 120, 255});
    for (int i = 0; i < 6; ++i) {
        DrawRectangleRec({w * 0.62f + 24.0f + static_cast<float>(i) * 60.0f, ground - 180.0f, 36.0f, 30.0f},
                         Color{255, 230, 160, 160});
    }
    DrawPushcart({w * 0.33f, ground - 22.0f}, 2.4f, false);

    const char* title = "CORNER SHOP QUEST";
    const float bob = std::sin(time_ * 1.5f) * 4.0f;
    ui::TextCentered(title, {w / 2.0f + 4.0f, h * 0.22f + 4.0f + bob}, 80.0f, Color{60, 30, 70, 160}, true);
    ui::TextCentered(title, {w / 2.0f, h * 0.22f + bob}, 80.0f, RAYWHITE, true);
    ui::TextCentered("An adventure about family, friends and business", {w / 2.0f, h * 0.22f + 70.0f}, 28.0f,
                     Color{255, 240, 220, 255});

    const std::vector<Rectangle> rects = ButtonRects();
    const std::array<bool, 3> enabled = {hasSave_, true, true};
    for (std::size_t i = 0; i < rects.size(); ++i) {
        ui::DrawButton(rects[i], kItems[i], static_cast<int>(i) == menu_.focused, enabled[i], 26.0f);
    }
    if (!error_.empty()) {
        ui::TextCentered("Could not load the save: " + error_, {w / 2.0f, rects.back().y + 90.0f}, 18.0f,
                         Color{255, 220, 220, 255});
    }
    ui::Text("Phase 1 prototype", {16.0f, h - 32.0f}, 18.0f, Color{255, 240, 220, 200});
}
