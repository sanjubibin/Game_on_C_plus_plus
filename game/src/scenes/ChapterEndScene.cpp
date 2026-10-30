#include "scenes/ChapterEndScene.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>

#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Ui.h"
#include "logic/Rules.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

float F(int value) {
    return static_cast<float>(value);
}

Rectangle ContinueButton() {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    return {w / 2.0f - 160.0f, h - 84.0f, 320.0f, 56.0f};
}

}  // namespace

ChapterEndScene::ChapterEndScene(engine::Application& app, GameSession& session) : Scene(app), s_(session) {}

void ChapterEndScene::Update(float dt) {
    time_ += dt;
    if (time_ > 1.0f && (ui::ConfirmPressed() || ui::Clicked(ContinueButton()))) {
        s_.state.Set("chapter", std::max(1, s_.state.Get("chapter")) + 1);
        App().Scenes().Pop();
    }
}

void ChapterEndScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{255, 196, 140, 255},
                           Color{120, 80, 150, 255});
    // Festival lanterns drifting up.
    for (int i = 0; i < 18; ++i) {
        const float speed = 18.0f + F(i % 5) * 6.0f;
        const float y = h - std::fmod(time_ * speed + F(i * 97), h + 60.0f);
        const float x = F((i * 173) % GetScreenWidth()) + std::sin(time_ + F(i)) * 12.0f;
        DrawCircleV({x, y}, 7.0f, Color{255, 220, 120, 200});
        DrawCircleV({x, y}, 13.0f, Color{255, 220, 120, 50});
    }

    const float appear = std::min(1.0f, time_ * 1.5f);
    const int chapter = std::max(1, s_.state.Get("chapter"));
    ui::TextCentered("CHAPTER " + std::to_string(chapter) + " COMPLETE", {w / 2.0f, 54.0f}, 48.0f, Fade(WHITE, appear),
                     true);
    ui::TextCentered(s_.content.ChapterTitle(chapter), {w / 2.0f, 100.0f}, 26.0f,
                     Fade(Color{255, 240, 220, 255}, appear));

    const Rectangle panel{w / 2.0f - 560.0f, 136.0f, 1120.0f, h - 236.0f};
    ui::Panel(panel, Fade(kPaper, 0.96f), kPanelBorder, 3.0f);

    const game::LifeWheel wheel = game::ComputeLifeWheel(s_.state, s_.content);
    ui::Text("Your Life Wheel", {panel.x + 40.0f, panel.y + 24.0f}, 24.0f, kInk, true);
    DrawLifeWheel({panel.x + 250.0f, panel.y + 250.0f}, 140.0f, wheel, true);

    const float x = panel.x + 540.0f;
    float y = panel.y + 24.0f;
    ui::Text("Moments that mattered", {x, y}, 24.0f, kInk, true);
    y += 40.0f;
    const auto& memories = s_.state.memories;
    const std::size_t first = memories.size() > 7 ? memories.size() - 7 : 0;
    for (std::size_t i = first; i < memories.size(); ++i) {
        DrawCircleV({x + 8.0f, y + 11.0f}, 5.0f, kAccent);
        ui::Text("Day " + std::to_string(memories[i].day), {x + 22.0f, y}, 16.0f, kAccent, true);
        ui::Text(memories[i].text, {x + 90.0f, y}, 19.0f, kInk);
        y += 30.0f;
    }
    if (memories.empty()) {
        ui::Text("A quiet chapter. Next time, talk to more people!", {x, y}, 19.0f, kMuted);
    }

    const Rectangle think{x, panel.y + panel.height - 120.0f, panel.width - 580.0f, 96.0f};
    ui::Panel(think, Color{255, 236, 205, 255}, kAccent, 2.0f);
    ui::Text("Think about it", {think.x + 16.0f, think.y + 10.0f}, 18.0f, kAccent, true);
    ui::TextWrapped("Which moment would you do differently next time? Which part of your Life Wheel would you "
                    "like to grow in Chapter " + std::to_string(chapter + 1) + "?",
                    {think.x + 16.0f, think.y + 36.0f, think.width - 32.0f, 56.0f}, 18.0f, kInk);

    ui::DrawButton(ContinueButton(), "Continue", true, time_ > 1.0f, 26.0f);
}
