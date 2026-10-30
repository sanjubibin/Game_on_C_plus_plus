#pragma once

#include <raylib.h>

#include <map>
#include <string>
#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"

// Lin's Wholesale: buy stock at cost price. Teaches cost vs. selling price and budgeting.
class SupplierScene : public engine::Scene {
public:
    SupplierScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

private:
    struct Layout {
        Rectangle panel;
        std::vector<Rectangle> rows;
        std::vector<Rectangle> minus;
        std::vector<Rectangle> plus;
        Rectangle buy;
        Rectangle leave;
    };
    Layout MakeLayout() const;
    int Total() const;
    void Buy();

    GameSession& s_;
    std::vector<std::string> items_;  // what Mrs. Lin sells you right now
    std::map<std::string, int> qty_;
    int row_ = 0;  // item rows, then Buy, then Leave
};
