#include "scenes/DaySummaryScene.h"

#include <raylib.h>

#include <cmath>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
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
    return {w / 2.0f - 170.0f, h - 110.0f, 340.0f, 58.0f};
}

}  // namespace

DaySummaryScene::DaySummaryScene(engine::Application& app, GameSession& session)
    : Scene(app), s_(session) {}

void DaySummaryScene::Update(float dt) {
    time_ += dt;
    if (time_ > 0.5f && (ui::ConfirmPressed() || ui::Clicked(ContinueButton()))) {
        flow::FinishDay(App(), s_);
    }
}

void DaySummaryScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    const game::GameState& st = s_.state;

    // Night sky with a few twinkling stars.
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{30, 26, 70, 255},
                           Color{90, 60, 120, 255});
    for (int i = 0; i < 40; ++i) {
        const float x = F((i * 197) % GetScreenWidth());
        const float y = F((i * 89) % (GetScreenHeight() / 2));
        const float twinkle = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 2.0f + F(i));
        DrawCircleV({x, y}, 1.5f + twinkle, Fade(WHITE, 0.4f + 0.5f * twinkle));
    }
    DrawCircleV({w - 140.0f, 110.0f}, 44.0f, Color{250, 240, 200, 255});
    DrawCircleV({w - 122.0f, 98.0f}, 40.0f, Color{38, 32, 80, 255});

    const Rectangle panel{w / 2.0f - 440.0f, 60.0f, 880.0f, h - 190.0f};
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);
    const float x = panel.x + 40.0f;
    float y = panel.y + 28.0f;
    ui::Text("End of Day " + std::to_string(st.day), {x, y}, 40.0f, kInk, true);
    y += 64.0f;

    const int start = st.moneyAtDayStart;
    const int now = st.Get("money");
    const int diff = now - start;
    DrawCoin({x + 12.0f, y + 15.0f}, 12.0f);
    ui::Text("Coins: " + std::to_string(start) + "  ->  " + std::to_string(now), {x + 34.0f, y}, 28.0f, kInk, true);
    ui::Text("(" + std::string(diff >= 0 ? "+" : "") + std::to_string(diff) + " today)", {x + 400.0f, y + 4.0f},
             24.0f, diff >= 0 ? kGood : kBad, true);
    y += 54.0f;

    ui::Text("What happened today", {x, y}, 22.0f, kAccent, true);
    y += 34.0f;
    if (st.todayLog.empty()) {
        ui::Text("A quiet day.", {x + 10.0f, y}, 22.0f, kMuted);
        y += 32.0f;
    }
    for (const std::string& line : st.todayLog) {
        DrawCircleV({x + 10.0f, y + 12.0f}, 4.0f, kAccent);
        y += ui::TextWrapped(line, {x + 26.0f, y, panel.width - 106.0f, 60.0f}, 21.0f, kInk);
        if (y > panel.y + panel.height - 140.0f) {
            break;
        }
    }

    const std::vector<std::string>& tips = s_.content.start.tips;
    if (!tips.empty()) {
        const std::string& tip = tips[static_cast<std::size_t>(st.day - 1) % tips.size()];
        const Rectangle box{x, panel.y + panel.height - 110.0f, panel.width - 80.0f, 84.0f};
        ui::Panel(box, Color{255, 236, 205, 255}, kAccent, 2.0f);
        ui::Text("Tip", {box.x + 16.0f, box.y + 10.0f}, 18.0f, kAccent, true);
        ui::TextWrapped(tip, {box.x + 16.0f, box.y + 36.0f, box.width - 32.0f, 44.0f}, 20.0f, kInk);
    }

    ui::DrawButton(ContinueButton(), "Sleep and save", true, true, 26.0f);
    if (s_.devMode) {
        ui::TextCentered("(dev mode: not saving)", {w / 2.0f, h - 30.0f}, 16.0f, Color{220, 210, 240, 255});
    }
}
