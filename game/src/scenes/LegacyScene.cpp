#include "scenes/LegacyScene.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Audio.h"
#include "engine/Ui.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

float F(int value) {
    return static_cast<float>(value);
}

Rectangle ContinueButton() {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    return {w / 2.0f - 190.0f, h - 78.0f, 380.0f, 54.0f};
}

}  // namespace

LegacyScene::LegacyScene(engine::Application& app, GameSession& session, game::LegacyReport report,
                         std::function<void()> onDone)
    : Scene(app), s_(session), report_(std::move(report)), onDone_(std::move(onDone)) {
    engine::audio::Play("success");
}

void LegacyScene::Update(float dt) {
    time_ += dt;
    if (time_ > 1.5f && (ui::ConfirmPressed() || ui::Clicked(ContinueButton()))) {
        App().Scenes().Pop();
        if (onDone_) {
            onDone_();
        }
    }
}

void LegacyScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    // Sunset fading into a new dawn.
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{70, 50, 110, 255},
                           Color{255, 170, 120, 255});
    for (int i = 0; i < 30; ++i) {
        const float x = F((i * 211) % GetScreenWidth());
        const float y = F((i * 97) % (GetScreenHeight() / 2));
        const float twinkle = 0.5f + 0.5f * std::sin(time_ * 2.0f + F(i));
        DrawCircleV({x, y}, 1.2f + twinkle, Fade(WHITE, 0.3f + 0.5f * twinkle));
    }

    const float appear = std::min(1.0f, time_ * 1.2f);
    ui::TextCentered("A LIFE WELL LIVED", {w / 2.0f, 48.0f}, 46.0f, Fade(WHITE, appear), true);
    ui::TextCentered("Generation " + std::to_string(report_.generation) + " hands the old pushcart to the next",
                     {w / 2.0f, 92.0f}, 22.0f, Fade(Color{255, 236, 210, 255}, appear));

    const Rectangle panel{w / 2.0f - 580.0f, 126.0f, 1160.0f, h - 220.0f};
    ui::Panel(panel, Fade(kPaper, 0.96f), kPanelBorder, 3.0f);

    ui::Text("The life you lived", {panel.x + 36.0f, panel.y + 20.0f}, 22.0f, kInk, true);
    DrawLifeWheel({panel.x + 210.0f, panel.y + 230.0f}, 120.0f, report_.wheel, true);

    float x = panel.x + 440.0f;
    float y = panel.y + 20.0f;
    ui::Text("What you passed on", {x, y}, 22.0f, kInk, true);
    y += 36.0f;
    for (const std::string& line : report_.legacies) {
        DrawCircleV({x + 8.0f, y + 11.0f}, 5.0f, kAccent);
        y += ui::TextWrapped(line, {x + 24.0f, y, panel.width - 480.0f, 50.0f}, 19.0f, kInk) + 4.0f;
    }

    const Rectangle child{x, panel.y + panel.height - 150.0f, panel.width - 476.0f, 128.0f};
    ui::Panel(child, Color{255, 236, 205, 255}, kAccent, 2.0f);
    ui::Text("Your child begins with...", {child.x + 16.0f, child.y + 10.0f}, 19.0f, kAccent, true);
    std::string talent = report_.familyTalent.empty() ? "" : "   Family talent: " + report_.familyTalent + ".";
    ui::TextWrapped(std::to_string(report_.gift) + " coins in their pocket and " + std::to_string(report_.childSavings) +
                        " in a savings account at the Town Bank." + talent,
                    {child.x + 16.0f, child.y + 38.0f, child.width - 32.0f, 44.0f}, 18.0f, kInk);
    ui::TextWrapped("A family name people trust (reputation " + std::to_string(report_.familyReputation) +
                        "). Your debts stay with you - children don't inherit loans.",
                    {child.x + 16.0f, child.y + 82.0f, child.width - 32.0f, 44.0f}, 16.0f, kMuted);

    ui::DrawButton(ContinueButton(), "Meet the next generation", true, time_ > 1.5f, 24.0f);
}
