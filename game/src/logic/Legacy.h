#pragma once

#include <string>
#include <vector>

#include "logic/Content.h"
#include "logic/GameState.h"
#include "logic/Rules.h"

namespace game {

// What one generation passes on to the next.
struct LegacyReport {
    int generation = 1;    // the generation that just ended
    LifeWheel wheel;       // their Life Wheel at the end
    int wealth = 0;        // pocket + savings - loans
    int gift = 0;          // pocket money for the child
    int childSavings = 0;  // a savings account opened for the child
    int familyReputation = 0;
    std::string familyTalent;           // the skill the child starts a little better at
    std::vector<std::string> legacies;  // what became of the people and the business
};

// Ends the current generation and starts the next: the child keeps the town's history (flags,
// seen events, memories) but starts their own life - day 1, a little money from the family
// fund, a savings account, half the family reputation, fresh skills with one family talent,
// and their cousin Nia on the team. Debts stay with the parent who borrowed.
LegacyReport StartNextGeneration(GameState& state, const Content& content);

}  // namespace game
