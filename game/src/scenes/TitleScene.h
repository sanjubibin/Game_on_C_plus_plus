#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"
#include "engine/Ui.h"

// Main menu: Continue (when a save exists), New game, Quit.
class TitleScene : public engine::Scene {
public:
    TitleScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void FixedUpdate(float dt) override;
    void Draw(float alpha) const override;

private:
    std::vector<Rectangle> ButtonRects() const;

    GameSession& s_;
    bool hasSave_ = false;
    float time_ = 0.0f;
    std::string error_;
    engine::ui::Menu menu_;
};
