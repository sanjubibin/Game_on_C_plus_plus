#pragma once

#include "GameSession.h"
#include "engine/Scene.h"

// End of the day: what happened, money in and out, a tip. Continuing saves the game.
class DaySummaryScene : public engine::Scene {
public:
    DaySummaryScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    GameSession& s_;
    float time_ = 0.0f;
};
