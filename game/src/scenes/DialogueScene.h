#pragma once

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"
#include "engine/Ui.h"

// Overlay that plays a queue of story events: lines with a typewriter effect, then choices.
// Follow-up events ("then") play in the same overlay; actions run after it closes.
class DialogueScene : public engine::Scene {
public:
    DialogueScene(engine::Application& app, GameSession& session, std::vector<std::string> events,
                  std::function<void()> onDone);

    void Update(float dt) override;
    void Draw(float alpha) const override;

    void DevSkipLines();  // jump to the choices (used for screenshots)

private:
    void StartNextEvent();
    void NextLine();
    void FinishEvent(const std::string& then, const std::string& action);
    void ShowChoices();
    void Choose(std::size_t index);
    void Close();

    std::vector<Rectangle> ChoiceRects() const;
    const game::Line* CurrentLine() const;

    GameSession& s_;
    std::deque<std::string> queue_;
    std::function<void()> onDone_;
    std::vector<std::string> pendingActions_;

    const game::EventDef* event_ = nullptr;
    std::size_t line_ = 0;
    float typed_ = 0.0f;
    bool showingChoices_ = false;
    std::vector<std::size_t> visible_;  // indices of choices whose show_when holds
    bool closed_ = false;
    engine::ui::Menu menu_;
};
