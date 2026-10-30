#pragma once

#include <raylib.h>

#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"
#include "engine/Ui.h"

// The Town Bank book: move coins in and out of savings, watch interest grow, pay back a loan.
class BankScene : public engine::Scene {
public:
    BankScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    struct Layout {
        Rectangle panel;
        Rectangle minus;
        Rectangle plus;
        std::vector<Rectangle> buttons;  // Deposit, Withdraw, Pay back, Leave
    };
    Layout MakeLayout() const;
    int RepayAmount() const;

    GameSession& s_;
    int amount_ = 5;
    engine::ui::Menu menu_;
};
