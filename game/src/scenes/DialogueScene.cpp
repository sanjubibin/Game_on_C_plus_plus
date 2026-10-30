#include "scenes/DialogueScene.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "logic/Rules.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr float kCharsPerSecond = 55.0f;
constexpr float kTextSize = 26.0f;

Rectangle DialogueBox() {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    return {80.0f, h - 236.0f, w - 160.0f, 208.0f};
}

std::string Upper(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

}  // namespace

DialogueScene::DialogueScene(engine::Application& app, GameSession& session,
                             std::vector<std::string> events, std::function<void()> onDone)
    : Scene(app), s_(session), queue_(events.begin(), events.end()), onDone_(std::move(onDone)) {
    StartNextEvent();
}

void DialogueScene::StartNextEvent() {
    event_ = nullptr;
    showingChoices_ = false;
    while (!queue_.empty() && event_ == nullptr) {
        event_ = s_.content.FindEvent(queue_.front());
        queue_.pop_front();
    }
    if (event_ == nullptr) {
        return;  // Update closes the overlay
    }
    line_ = 0;
    typed_ = 0.0f;
    s_.state.seenEvents.insert(event_->id);
    flow::ApplyEffects(s_, event_->effects);
    game::Schedule(s_.state, event_->later);

    if (event_->lines.empty()) {
        ShowChoices();
    }
}

void DialogueScene::ShowChoices() {
    visible_.clear();
    for (std::size_t i = 0; i < event_->choices.size(); ++i) {
        if (game::CheckCondition(event_->choices[i].showWhen, s_.state)) {
            visible_.push_back(i);
        }
    }
    if (visible_.empty()) {
        FinishEvent(event_->then, event_->action);
        return;
    }
    showingChoices_ = true;
    menu_.focused = 0;
}

const game::Line* DialogueScene::CurrentLine() const {
    if (event_ == nullptr || line_ >= event_->lines.size()) {
        return nullptr;
    }
    return &event_->lines[line_];
}

void DialogueScene::NextLine() {
    ++line_;
    typed_ = 0.0f;
    if (line_ < event_->lines.size()) {
        return;
    }
    ShowChoices();
}

void DialogueScene::FinishEvent(const std::string& then, const std::string& action) {
    if (!action.empty()) {
        pendingActions_.push_back(action);
    }
    if (!then.empty()) {
        queue_.push_front(then);
    }
    StartNextEvent();
}

void DialogueScene::Choose(std::size_t index) {
    const game::ChoiceDef& choice = event_->choices[index];
    flow::ApplyEffects(s_, choice.effects);
    game::Schedule(s_.state, choice.later);
    if (!event_->action.empty()) {
        pendingActions_.push_back(event_->action);
    }
    FinishEvent(choice.then.empty() ? event_->then : choice.then, choice.action);
}

void DialogueScene::DevSkipLines() {
    while (event_ != nullptr && !showingChoices_ && line_ < event_->lines.size() && !event_->choices.empty() &&
           event_->then.empty()) {
        NextLine();
    }
}

void DialogueScene::Close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    App().Scenes().Pop();
    // onDone first, so scenes opened by actions end up on top of whatever onDone opens
    // (e.g. the chapter-end screen shows before the evening's day summary).
    if (onDone_) {
        onDone_();
    }
    for (const std::string& action : pendingActions_) {
        flow::RunAction(App(), s_, action);
    }
}

std::vector<Rectangle> DialogueScene::ChoiceRects() const {
    std::vector<Rectangle> rects;
    if (event_ == nullptr) {
        return rects;
    }
    const Rectangle box = DialogueBox();
    const float width = std::min(720.0f, box.width);
    const float height = 50.0f;
    const float gap = 10.0f;
    const std::size_t count = visible_.size();
    float y = box.y - 18.0f - static_cast<float>(count) * (height + gap);
    for (std::size_t i = 0; i < count; ++i) {
        rects.push_back({box.x + box.width - width, y, width, height});
        y += height + gap;
    }
    return rects;
}

void DialogueScene::Update(float dt) {
    s_.UpdateToasts(dt);
    if (event_ == nullptr) {
        Close();
        return;
    }

    if (showingChoices_) {
        std::vector<bool> enabled;
        for (const std::size_t index : visible_) {
            enabled.push_back(game::CheckCondition(event_->choices[index].requirement, s_.state));
        }
        const int picked = menu_.Update(ChoiceRects(), enabled);
        if (picked >= 0) {
            Choose(visible_[static_cast<std::size_t>(picked)]);
        }
        return;
    }

    const game::Line* line = CurrentLine();
    const float length = line ? static_cast<float>(line->text.size()) : 0.0f;
    typed_ = std::min(length, typed_ + dt * kCharsPerSecond);
    if (ui::ConfirmPressed() || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (typed_ < length) {
            typed_ = length;
        } else {
            NextLine();
        }
    }
}

void DialogueScene::Draw(float /*alpha*/) const {
    if (event_ == nullptr) {
        return;
    }
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const bool theirSide = event_->style == "their_side";

    if (theirSide) {
        // Sepia "memory" look: the player sees the scene through someone else's eyes.
        DrawRectangleRec({0.0f, 0.0f, screenW, screenH}, Color{92, 58, 24, 215});
        const std::string banner = Upper(s_.content.NameOf(event_->speaker)) + "'S SIDE";
        ui::TextCentered(banner, {screenW / 2.0f, 92.0f}, 64.0f, Color{255, 236, 200, 255}, true);
        ui::TextCentered("See it through their eyes", {screenW / 2.0f, 148.0f}, 24.0f,
                         Color{255, 220, 170, 255});
        const game::NpcDef* npc = s_.content.FindNpc(event_->speaker);
        DrawPortrait({screenW / 2.0f, screenH / 2.0f - 60.0f}, 90.0f, LookOf(npc));
    } else {
        DrawRectangleRec({0.0f, 0.0f, screenW, screenH}, Color{0, 0, 0, 70});
    }

    const Rectangle box = DialogueBox();
    const game::Line* line = CurrentLine();
    // While choosing, keep the last line on screen so the question stays visible.
    const bool choosingAfterLines = line == nullptr && showingChoices_ && !event_->lines.empty();
    if (choosingAfterLines) {
        line = &event_->lines.back();
    }
    const bool narration = theirSide || line == nullptr || line->speaker.empty();

    const Color boxFill = theirSide ? Color{255, 240, 214, 245} : kPanel;
    ui::Panel(box, boxFill, kPanelBorder, 3.0f);

    float textX = box.x + 36.0f;
    if (!narration) {
        const game::NpcDef* npc = s_.content.FindNpc(line->speaker);
        const Look look = line->speaker == "you" ? PlayerLook() : LookOf(npc);
        DrawPortrait({box.x + 96.0f, box.y + box.height / 2.0f}, 66.0f, look);
        textX = box.x + 190.0f;

        const std::string name = s_.content.NameOf(line->speaker);
        const Vector2 m = ui::Measure(name, 24.0f, true);
        const Rectangle plate{box.x + 170.0f, box.y - 22.0f, m.x + 36.0f, 40.0f};
        ui::Panel(plate, kAccent, Color{170, 80, 40, 255}, 2.0f);
        ui::Text(name, {plate.x + 18.0f, plate.y + 8.0f}, 24.0f, WHITE, true);
    }

    if (line != nullptr) {
        const Rectangle textBox{textX, box.y + 36.0f, box.x + box.width - textX - 36.0f,
                                box.height - 60.0f};
        const Color ink = narration && !theirSide ? Color{80, 62, 70, 255} : kInk;
        ui::TextWrapped(line->text, textBox, kTextSize, ink, choosingAfterLines ? -1 : static_cast<int>(typed_));

        if (!showingChoices_ && typed_ >= static_cast<float>(line->text.size())) {
            const float bounce = std::sin(static_cast<float>(GetTime()) * 6.0f) * 3.0f;
            const Vector2 tip{box.x + box.width - 40.0f, box.y + box.height - 26.0f + bounce};
            DrawTriangle({tip.x - 9.0f, tip.y - 8.0f}, {tip.x, tip.y + 4.0f},
                         {tip.x + 9.0f, tip.y - 8.0f}, kAccent);
        }
    } else if (showingChoices_) {
        ui::TextWrapped("What do you do?", {textX, box.y + 36.0f, box.width - 72.0f, 40.0f},
                        kTextSize, kMuted);
    }

    if (showingChoices_) {
        const std::vector<Rectangle> rects = ChoiceRects();
        for (std::size_t i = 0; i < rects.size(); ++i) {
            const game::ChoiceDef& choice = event_->choices[visible_[i]];
            const bool enabled = game::CheckCondition(choice.requirement, s_.state);
            const bool focused = static_cast<int>(i) == menu_.focused;
            std::string label = std::to_string(i + 1) + ".  " + choice.label;
            if (!enabled) {
                label += "   (" + game::DescribeUnmet(choice.requirement, s_.state, s_.content) + ")";
            }
            const Rectangle r = rects[i];
            ui::Panel(r,
                      !enabled  ? Color{230, 224, 216, 245}
                      : focused ? Color{255, 214, 153, 250}
                                : Color{255, 248, 236, 250},
                      focused && enabled ? kAccent : kPanelBorder, focused ? 3.0f : 2.0f);
            ui::Text(label, {r.x + 18.0f, r.y + 13.0f}, 22.0f, enabled ? kInk : kMuted);
        }
    }
}
