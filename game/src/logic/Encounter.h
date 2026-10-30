#pragma once

#include <set>
#include <string>

#include "logic/Content.h"
#include "logic/GameState.h"

namespace game {

// Rules of a turn-based social encounter (no combat). Each action can be used once; some only
// work well after a "reveals" action has uncovered what the other person really needs.

enum class EncounterResult { Playing, Won, Lost };

struct EncounterState {
    int mood = 0;
    int turn = 0;
    bool revealed = false;
    std::set<std::string> used;
    EncounterResult result = EncounterResult::Playing;
    std::string message;
    int lastDelta = 0;
};

EncounterState BeginEncounter(const EncounterDef& encounter);
bool CanUse(const EncounterState& es, const EncounterAction& action, const GameState& state);
// How much an action would change the mood right now.
int MoodChange(const EncounterState& es, const EncounterAction& action, const GameState& state);
// Resolves one action and decides whether the encounter is won or lost.
// The caller applies action.effects (so it can announce them).
void UseAction(const EncounterDef& encounter, EncounterState& es, const EncounterAction& action,
               const GameState& state);

}  // namespace game
