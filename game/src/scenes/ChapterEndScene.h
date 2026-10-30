#pragma once

#include "GameSession.h"
#include "engine/Scene.h"

// End of a chapter: the Life Wheel, the moments that mattered, and a question to think about.
class ChapterEndScene : public engine::Scene {
public:
    ChapterEndScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    GameSession& s_;
    float time_ = 0.0f;
};
