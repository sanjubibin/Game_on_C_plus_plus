#pragma once

#include <json/value.h>

#include <map>
#include <string>
#include <vector>

#include "logic/Content.h"
#include "logic/GameState.h"

namespace game {

// --- Conditions ("when" / "requires") -------------------------------------------------------
// Keys: flag, not_flag, day_min, day_max, hour_min, hour_below, min {stat: n}, below {stat: n},
// in_party, not_in_party, seen, not_seen, has_stock {item: n}. Null means "always".
bool CheckCondition(const Json::Value& when, const GameState& state);
// Player-facing reason a requirement is not met, e.g. "Needs Money sense 2".
std::string DescribeUnmet(const Json::Value& when, const GameState& state, const Content& content);

// --- Effects ----------------------------------------------------------------------------------
// Keys: add {stat: delta}, set {stat: value}, set_flag, clear_flag, join_party,
// buy_stock {item, qty, unit_cost, dodgy}, remove_dodgy, advance_minutes, log, memory,
// schedule {event, days}, take_loan {amount, rate, weeks}.
void ApplyEffects(const Json::Value& effects, GameState& state);
void Schedule(GameState& state, const Later& later);
void AddMemory(GameState& state, const std::string& text);

// --- Items ------------------------------------------------------------------------------------
// What customers think an item is worth today (value_when rules can change it).
int ItemValue(const ItemDef& item, const GameState& state);
// Days before the item goes off (longer once the shop has a fridge).
int ShelfDays(const ItemDef& item, const GameState& state);

// "skill.money_sense" -> "Money sense", "maya.trust" -> "Maya trust", "money" -> "Coins".
std::string StatLabel(const std::string& key, const Content& content);
// Human-readable stat changes between two snapshots, e.g. "Maya trust +5".
std::vector<std::string> DiffStats(const std::map<std::string, int>& before,
                                   const std::map<std::string, int>& after,
                                   const Content& content);

// --- Quests ----------------------------------------------------------------------------------
enum class QuestStatus { Hidden, Active, Done };
QuestStatus GetQuestStatus(const QuestDef& quest, const GameState& state);
const QuestStep* CurrentStep(const QuestDef& quest, const GameState& state);
// The quest to show on screen: the current chapter's active quests come first.
const QuestDef* TrackedQuest(const GameState& state, const Content& content);

// --- Day cycle -------------------------------------------------------------------------------
bool IsHotDay(const GameState& state);
// Morning: resets the clock and daily flags, spoils old stock, and returns events to play.
std::vector<std::string> StartDay(GameState& state, const Content& content);
void EndDay(GameState& state);
// Bedtime: an early night (before 19:00) restores health, staying out until dark costs it.
void ApplyBedtime(GameState& state, bool late);
bool IsTired(const GameState& state);
// The dinner-table event for tonight, or empty.
std::string PickEveningEvent(const GameState& state, const Content& content);
// First talk event whose condition holds, else an idle line (rotating by day).
std::string PickTalkEvent(const NpcDef& npc, const GameState& state);

// --- People and the Life Wheel ---------------------------------------------------------------
// True once the player has been in a scene with this character (or they are on the team).
bool HasMet(const NpcDef& npc, const GameState& state, const Content& content);

// Six 0-100 scores; no single "winning" number.
struct LifeWheel {
    int money = 0;
    int health = 0;
    int happiness = 0;
    int relationships = 0;
    int knowledge = 0;
    int community = 0;
};
LifeWheel ComputeLifeWheel(const GameState& state, const Content& content);

// --- Content checks --------------------------------------------------------------------------
// Cross-reference problems (unknown events, characters, items, condition keys). Empty is good.
std::vector<std::string> ValidateContent(const Content& content);

}  // namespace game
