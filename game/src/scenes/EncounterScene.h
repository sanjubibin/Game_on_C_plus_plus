#pragma once

#include <raylib.h>

#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"
#include "engine/Ui.h"
#include "logic/Encounter.h"

// A turn-based social challenge (no combat): use life skills to change someone's mood.
// The rules live in logic/Encounter; this scene shows them and takes input.
class EncounterScene : public engine::Scene {
public:
    EncounterScene(engine::Application& app, GameSession& session, const game::EncounterDef& encounter);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    std::vector<Rectangle> ActionRects() const;

    GameSession& s_;
    const game::EncounterDef& enc_;
    game::EncounterState es_;
    float shownMood_ = 0.0f;
    float endTimer_ = 0.0f;
    engine::ui::Menu menu_;
};
