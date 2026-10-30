#include "scenes/EncounterScene.h"

#include <algorithm>
#include <memory>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Audio.h"
#include "logic/Rules.h"
#include "scenes/DialogueScene.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

float F(int value) {
    return static_cast<float>(value);
}

Rectangle PanelRect() {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    return {w / 2.0f - 540.0f, h / 2.0f - 340.0f, 1080.0f, 680.0f};
}

}  // namespace

EncounterScene::EncounterScene(engine::Application& app, GameSession& session, const game::EncounterDef& encounter)
    : Scene(app), s_(session), enc_(encounter), es_(game::BeginEncounter(encounter)),
      shownMood_(static_cast<float>(encounter.startMood)) {}

std::vector<Rectangle> EncounterScene::ActionRects() const {
    const Rectangle panel = PanelRect();
    std::vector<Rectangle> rects;
    float y = panel.y + 140.0f;
    for (std::size_t i = 0; i < enc_.actions.size(); ++i) {
        rects.push_back({panel.x + 600.0f, y, 440.0f, 58.0f});
        y += 64.0f;
    }
    return rects;
}

void EncounterScene::Update(float dt) {
    s_.UpdateToasts(dt);
    shownMood_ += (F(es_.mood) - shownMood_) * std::min(1.0f, dt * 6.0f);

    if (es_.result != game::EncounterResult::Playing) {
        endTimer_ += dt;
        if (endTimer_ > 0.6f && (ui::ConfirmPressed() || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            const bool won = es_.result == game::EncounterResult::Won;
            const std::string next = won ? enc_.winEvent : enc_.loseEvent;
            App().Scenes().Replace(std::make_unique<DialogueScene>(App(), s_, std::vector<std::string>{next},
                                                                   std::function<void()>{}));
        }
        return;
    }

    std::vector<bool> enabled;
    for (const game::EncounterAction& action : enc_.actions) {
        enabled.push_back(game::CanUse(es_, action, s_.state));
    }
    const int picked = menu_.Update(ActionRects(), enabled);
    if (picked >= 0) {
        const game::EncounterAction& action = enc_.actions[static_cast<std::size_t>(picked)];
        game::UseAction(enc_, es_, action, s_.state);
        flow::ApplyEffects(s_, action.effects);
        if (es_.result == game::EncounterResult::Won) {
            engine::audio::Play("success");
        } else if (es_.result == game::EncounterResult::Lost) {
            engine::audio::Play("fail");
        }
    }
}

void EncounterScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    DrawRectangleRec({0.0f, 0.0f, w, h}, Color{20, 12, 20, 160});
    const Rectangle panel = PanelRect();
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);

    const game::NpcDef* npc = s_.content.FindNpc(enc_.opponent);
    const std::string name = s_.content.NameOf(enc_.opponent);
    ui::Text(enc_.title, {panel.x + 36.0f, panel.y + 24.0f}, 38.0f, kInk, true);
    ui::Text("Turn " + std::to_string(std::min(es_.turn + 1, enc_.maxTurns)) + " of " + std::to_string(enc_.maxTurns),
             {panel.x + panel.width - 180.0f, panel.y + 34.0f}, 22.0f, kMuted, true);

    // Left: the person, their mood and what you know about them.
    DrawPortrait({panel.x + 120.0f, panel.y + 176.0f}, 70.0f, LookOf(npc));
    ui::Text(name, {panel.x + 210.0f, panel.y + 116.0f}, 28.0f, kInk, true);
    ui::Text(enc_.moodLabel, {panel.x + 210.0f, panel.y + 154.0f}, 18.0f, kMuted);
    const Rectangle bar{panel.x + 210.0f, panel.y + 182.0f, 340.0f, 22.0f};
    const Color moodColor = ColorLerp(Color{120, 140, 200, 255}, kGood, shownMood_ / 100.0f);
    ui::Bar(bar, shownMood_ / 100.0f, moodColor, Color{225, 215, 200, 255});
    const float goalX = bar.x + bar.width * F(enc_.goal) / 100.0f;
    DrawLineEx({goalX, bar.y - 6.0f}, {goalX, bar.y + bar.height + 6.0f}, 3.0f, kAccent);
    ui::Text("goal", {goalX - 14.0f, bar.y + bar.height + 6.0f}, 14.0f, kAccent, true);

    const Rectangle need{panel.x + 36.0f, panel.y + 270.0f, 530.0f, 130.0f};
    ui::Panel(need, Color{255, 252, 246, 255}, Color{220, 200, 180, 255}, 1.5f);
    ui::Text("What's going on?", {need.x + 16.0f, need.y + 12.0f}, 18.0f, kAccent, true);
    ui::TextWrapped(es_.revealed ? enc_.need : "??? You don't know yet. Maybe ask and listen?",
                    {need.x + 16.0f, need.y + 40.0f, need.width - 32.0f, 86.0f}, 19.0f, es_.revealed ? kInk : kMuted);

    const Rectangle speech{panel.x + 36.0f, panel.y + 420.0f, 530.0f, 200.0f};
    ui::Panel(speech, Color{255, 236, 205, 255}, kAccent, 2.0f);
    ui::TextWrapped(es_.message, {speech.x + 18.0f, speech.y + 16.0f, speech.width - 36.0f, 140.0f}, 20.0f, kInk);
    if (es_.lastDelta != 0) {
        ui::Text((es_.lastDelta > 0 ? "+" : "") + std::to_string(es_.lastDelta) + " " + enc_.moodLabel,
                 {speech.x + 18.0f, speech.y + speech.height - 36.0f}, 20.0f, es_.lastDelta > 8 ? kGood : kBad, true);
    }

    // Right: what you can try.
    ui::Text("What will you try?", {panel.x + 600.0f, panel.y + 104.0f}, 22.0f, kInk, true);
    const std::vector<Rectangle> rects = ActionRects();
    const bool playing = es_.result == game::EncounterResult::Playing;
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const game::EncounterAction& action = enc_.actions[i];
        const bool enabled = game::CanUse(es_, action, s_.state);
        const bool focused = static_cast<int>(i) == menu_.focused && playing;
        ui::Panel(rects[i],
                  !enabled  ? Color{230, 224, 216, 255}
                  : focused ? Color{255, 214, 153, 255}
                            : Color{255, 248, 236, 255},
                  focused ? kAccent : kPanelBorder, focused ? 3.0f : 2.0f);
        ui::Text(std::to_string(i + 1) + ".  " + action.label, {rects[i].x + 16.0f, rects[i].y + 7.0f}, 20.0f,
                 enabled ? kInk : kMuted, true);
        std::string tag;
        if (es_.used.contains(action.id)) {
            tag = "already tried";
        } else if (!action.party.empty() && !s_.state.InParty(action.party)) {
            tag = "needs " + s_.content.NameOf(action.party) + " on your team";
        } else if (!game::CheckCondition(action.requirement, s_.state)) {
            tag = game::DescribeUnmet(action.requirement, s_.state, s_.content);
        } else if (!action.skill.empty()) {
            tag = game::StatLabel("skill." + action.skill, s_.content) + " " +
                  std::to_string(s_.state.Get("skill." + action.skill));
        } else if (!action.party.empty()) {
            tag = "with " + s_.content.NameOf(action.party);
        }
        ui::Text(tag, {rects[i].x + 44.0f, rects[i].y + 34.0f}, 15.0f, kMuted);
    }

    if (!playing) {
        const bool won = es_.result == game::EncounterResult::Won;
        const Rectangle banner{panel.x + 600.0f, panel.y + panel.height - 90.0f, 440.0f, 74.0f};
        ui::Panel(banner, won ? kGood : Color{150, 130, 120, 255}, kInk, 2.0f);
        ui::TextCentered(won ? enc_.winText : enc_.loseText, {banner.x + banner.width / 2.0f, banner.y + 26.0f}, 26.0f,
                         WHITE, true);
        ui::TextCentered("Press E to continue", {banner.x + banner.width / 2.0f, banner.y + 55.0f}, 16.0f,
                         Color{255, 245, 230, 255});
    }
}
