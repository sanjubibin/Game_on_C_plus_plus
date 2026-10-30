#pragma once

#include <raylib.h>

#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"
#include "engine/Ui.h"

class PauseScene : public engine::Scene {
public:
    PauseScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    std::vector<Rectangle> ButtonRects() const;

    GameSession& s_;
    engine::ui::Menu menu_;
};
