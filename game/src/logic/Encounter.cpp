#include "logic/Encounter.h"

#include <algorithm>

#include "logic/Rules.h"

namespace game {

EncounterState BeginEncounter(const EncounterDef& encounter) {
    EncounterState es;
    es.mood = encounter.startMood;
    es.message = encounter.intro;
    return es;
}

bool CanUse(const EncounterState& es, const EncounterAction& action, const GameState& state) {
    if (es.result != EncounterResult::Playing || es.used.contains(action.id)) {
        return false;
    }
    if (!action.party.empty() && !state.InParty(action.party)) {
        return false;
    }
    return CheckCondition(action.requirement, state);
}

int MoodChange(const EncounterState& es, const EncounterAction& action, const GameState& state) {
    if (action.needsReveal && !es.revealed) {
        return action.baseUnrevealed;
    }
    int change = action.base;
    if (!action.skill.empty()) {
        change += action.perSkill * state.Get("skill." + action.skill);
    }
    return change;
}

void UseAction(const EncounterDef& encounter, EncounterState& es, const EncounterAction& action,
               const GameState& state) {
    if (!CanUse(es, action, state)) {
        return;
    }
    es.used.insert(action.id);
    if (action.leaves) {
        es.message = action.text;
        es.lastDelta = 0;
        es.result = EncounterResult::Lost;
        return;
    }

    const bool blocked = action.needsReveal && !es.revealed;
    es.lastDelta = MoodChange(es, action, state);
    es.message = blocked ? action.textUnrevealed : action.text;
    if (action.reveals) {
        es.revealed = true;
    }
    es.mood = std::clamp(es.mood + es.lastDelta, 0, 100);
    ++es.turn;

    const bool anyLeft = std::any_of(encounter.actions.begin(), encounter.actions.end(),
                                     [&](const EncounterAction& a) { return !a.leaves && CanUse(es, a, state); });
    if (es.mood >= encounter.goal) {
        es.result = EncounterResult::Won;
    } else if (es.turn >= encounter.maxTurns || !anyLeft) {
        es.result = EncounterResult::Lost;
    }
}

}  // namespace game
