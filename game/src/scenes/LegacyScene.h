#pragma once

#include <functional>

#include "GameSession.h"
#include "engine/Scene.h"
#include "logic/Legacy.h"

// The end of a generation: the life that was lived, what it passed on, and how the child starts.
class LegacyScene : public engine::Scene {
public:
    LegacyScene(engine::Application& app, GameSession& session, game::LegacyReport report,
                std::function<void()> onDone);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    GameSession& s_;
    game::LegacyReport report_;
    std::function<void()> onDone_;
    float time_ = 0.0f;
};
