#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "Flow.h"
#include "GameSession.h"
#include "engine/Scene.h"
#include "logic/Selling.h"

// Selling at a venue (pushcart, market stall, corner shop): set prices, then run it yourself
// or let a team member run it for a wage. Shows customers coming by, then revenue, costs,
// fees, wages and profit.
class SellScene : public engine::Scene {
public:
    SellScene(engine::Application& app, GameSession& session, const game::VenueDef& venue);

    void Update(float dt) override;
    void Draw(float alpha) const override;

    // Skips setup and fast-forwards the animation (used for screenshots).
    void DevAutoRun(float elapsedSeconds);

private:
    enum class Phase { Setup, Running, Results };
    struct Layout {
        Rectangle panel;
        std::vector<Rectangle> rows;
        std::vector<Rectangle> minus;
        std::vector<Rectangle> plus;
        Rectangle open;
        Rectangle leave;
        std::vector<Rectangle> staff;
        Rectangle done;
    };

    Layout MakeLayout() const;
    void Run(const std::string& staffId);
    float CustomerInterval() const;
    std::string Lesson() const;
    Vector2 CounterPos() const;

    void DrawBackdrop() const;
    void DrawShopInterior() const;
    void DrawSetup(const Layout& l) const;
    void DrawRunning() const;
    void DrawResults(const Layout& l) const;

    GameSession& s_;
    const game::VenueDef& venue_;
    Phase phase_ = Phase::Setup;
    std::vector<std::string> items_;
    std::vector<std::string> staff_;  // team members who could run it
    int row_ = 0;

    game::SessionReport report_;
    float clock_ = 0.0f;
    flow::QuestSnapshot questsBefore_;
};
