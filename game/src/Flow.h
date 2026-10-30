#pragma once

#include <json/value.h>

#include <functional>
#include <string>
#include <vector>

#include "GameSession.h"
#include "engine/Application.h"

// Glue between scenes: starting games, playing story events, running actions, the day cycle.
namespace flow {

void InitNewState(GameSession& session);
void NewGame(engine::Application& app, GameSession& session);
bool ContinueGame(engine::Application& app, GameSession& session, std::string* error);

// Plays events in order as a dialogue overlay, then calls onDone.
void PlayEvents(engine::Application& app, GameSession& session, std::vector<std::string> events,
                std::function<void()> onDone = {});
// Actions: open_supplier, open_sell:<venue>, end_day, home, chapter_end, event:<id>,
// encounter:<id>.
void RunAction(engine::Application& app, GameSession& session, const std::string& action);
void TalkTo(engine::Application& app, GameSession& session, const game::NpcDef& npc);

// Dinner, then the day summary. late = sent home because it got dark.
void StartEvening(engine::Application& app, GameSession& session, bool late = false);
// Called by the day summary: advance the day, save, and play the next morning.
void FinishDay(engine::Application& app, GameSession& session);
void PlaceAtHome(GameSession& session);

// Applies effects and shows toasts for stat and quest changes.
void ApplyEffects(GameSession& session, const Json::Value& effects);

// Remembers quest progress so changes made by any code can be announced as toasts.
struct QuestSnapshot {
    std::vector<int> progress;  // -1 hidden, else number of completed steps
};
QuestSnapshot TakeQuestSnapshot(const GameSession& session);
void ToastQuestChanges(GameSession& session, const QuestSnapshot& before);

}  // namespace flow
