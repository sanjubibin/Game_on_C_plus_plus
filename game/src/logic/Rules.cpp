#include "logic/Rules.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <functional>
#include <set>

#include "logic/Bank.h"

namespace game {

namespace {

const std::set<std::string> kConditionKeys = {
    "flag",     "not_flag", "day_min",      "day_max", "hour_min", "hour_below", "min",
    "below",    "in_party", "not_in_party", "seen",    "not_seen", "has_stock"};

const std::set<std::string> kEffectKeys = {
    "add",          "set",          "set_flag", "clear_flag", "join_party", "buy_stock",
    "remove_dodgy", "advance_minutes", "log",   "memory",     "schedule",   "take_loan"};

// Calls fn for a string or for every string in an array.
template <typename Fn>
bool AllStrings(const Json::Value& v, Fn fn) {
    if (v.isString()) {
        return fn(v.asString());
    }
    if (v.isArray()) {
        for (const Json::Value& item : v) {
            if (!fn(item.asString())) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// Calls fn(name, value) for every member of an object; false if any call is false.
template <typename Fn>
bool AllMembers(const Json::Value& v, Fn fn) {
    if (!v.isObject()) {
        return false;
    }
    for (const std::string& name : v.getMemberNames()) {
        if (!fn(name, v[name].asInt())) {
            return false;
        }
    }
    return true;
}

bool CheckKey(const std::string& key, const Json::Value& v, const GameState& s) {
    if (key == "flag") {
        return AllStrings(v, [&](const std::string& f) { return s.HasFlag(f); });
    }
    if (key == "not_flag") {
        return AllStrings(v, [&](const std::string& f) { return !s.HasFlag(f); });
    }
    if (key == "day_min") {
        return s.day >= v.asInt();
    }
    if (key == "day_max") {
        return s.day <= v.asInt();
    }
    if (key == "hour_min") {
        return s.Hour() >= v.asInt();
    }
    if (key == "hour_below") {
        return s.Hour() < v.asInt();
    }
    if (key == "min") {
        return AllMembers(v, [&](const std::string& stat, int n) { return s.Get(stat) >= n; });
    }
    if (key == "below") {
        return AllMembers(v, [&](const std::string& stat, int n) { return s.Get(stat) < n; });
    }
    if (key == "in_party") {
        return AllStrings(v, [&](const std::string& id) { return s.InParty(id); });
    }
    if (key == "not_in_party") {
        return AllStrings(v, [&](const std::string& id) { return !s.InParty(id); });
    }
    if (key == "seen") {
        return AllStrings(v, [&](const std::string& e) { return s.seenEvents.contains(e); });
    }
    if (key == "not_seen") {
        return AllStrings(v, [&](const std::string& e) { return !s.seenEvents.contains(e); });
    }
    if (key == "has_stock") {
        return AllMembers(v, [&](const std::string& item, int n) { return s.StockOf(item) >= n; });
    }
    return false;  // unknown keys fail safe; ValidateContent reports them
}

std::string Humanize(const std::string& id) {
    std::string out = id;
    std::replace(out.begin(), out.end(), '_', ' ');
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

void ForEachString(const Json::Value& v, const std::function<void(const std::string&)>& fn) {
    if (v.isString()) {
        fn(v.asString());
    } else if (v.isArray()) {
        for (const Json::Value& item : v) {
            fn(item.asString());
        }
    }
}

}  // namespace

bool CheckCondition(const Json::Value& when, const GameState& state) {
    if (when.isNull()) {
        return true;
    }
    if (!when.isObject()) {
        return false;
    }
    for (const std::string& key : when.getMemberNames()) {
        if (!CheckKey(key, when[key], state)) {
            return false;
        }
    }
    return true;
}

std::string DescribeUnmet(const Json::Value& when, const GameState& state, const Content& content) {
    if (!when.isObject()) {
        return "Not possible right now";
    }
    const Json::Value& min = when["min"];
    if (min.isObject()) {
        for (const std::string& stat : min.getMemberNames()) {
            const int needed = min[stat].asInt();
            if (state.Get(stat) >= needed) {
                continue;
            }
            if (stat == "money") {
                return "Needs " + std::to_string(needed) + " coins";
            }
            return "Needs " + StatLabel(stat, content) + " " + std::to_string(needed);
        }
    }
    const Json::Value& party = when["in_party"];
    if (party.isString() && !state.InParty(party.asString())) {
        return "Needs " + content.NameOf(party.asString()) + " with you";
    }
    return "Not possible right now";
}

void AddMemory(GameState& state, const std::string& text) {
    const bool known = std::any_of(state.memories.begin(), state.memories.end(),
                                   [&](const Memory& m) { return m.text == text; });
    if (!known) {
        state.memories.push_back({state.day, text});
    }
}

int ItemValue(const ItemDef& item, const GameState& state) {
    for (const ValueRule& rule : item.valueWhen) {
        if (CheckCondition(rule.when, state)) {
            return rule.value;
        }
    }
    return item.value;
}

int ShelfDays(const ItemDef& item, const GameState& state) {
    if (item.fridgeShelfDays > 0 && state.HasFlag("fridge")) {
        return item.fridgeShelfDays;
    }
    return item.shelfDays;
}

void Schedule(GameState& state, const Later& later) {
    if (!later.event.empty()) {
        state.scheduled.push_back({state.day + std::max(1, later.days), later.event});
    }
}

void ApplyEffects(const Json::Value& effects, GameState& state) {
    if (!effects.isObject()) {
        return;
    }
    for (const std::string& key : effects.getMemberNames()) {
        const Json::Value& v = effects[key];
        if (key == "add") {
            AllMembers(v, [&](const std::string& stat, int delta) {
                state.Add(stat, delta);
                return true;
            });
        } else if (key == "set") {
            AllMembers(v, [&](const std::string& stat, int value) {
                state.Set(stat, value);
                return true;
            });
        } else if (key == "take_loan") {
            TakeLoan(state, v["amount"].asInt(), v["rate"].asInt(), v["weeks"].isInt() ? v["weeks"].asInt() : 4);
        } else if (key == "set_flag") {
            ForEachString(v, [&](const std::string& f) { state.SetFlag(f); });
        } else if (key == "clear_flag") {
            ForEachString(v, [&](const std::string& f) { state.ClearFlag(f); });
        } else if (key == "join_party") {
            ForEachString(v, [&](const std::string& id) { state.JoinParty(id); });
        } else if (key == "buy_stock") {
            StockBatch batch;
            batch.item = v["item"].asString();
            batch.qty = v["qty"].asInt();
            batch.unitCost = v["unit_cost"].asInt();
            batch.dayBought = state.day;
            batch.dodgy = v["dodgy"].isBool() && v["dodgy"].asBool();
            state.Add("money", -batch.qty * batch.unitCost);
            state.AddStock(batch);
        } else if (key == "remove_dodgy") {
            std::erase_if(state.stock, [](const StockBatch& b) { return b.dodgy; });
        } else if (key == "advance_minutes") {
            state.minutes += v.asInt();
        } else if (key == "log") {
            state.todayLog.push_back(v.asString());
        } else if (key == "memory") {
            AddMemory(state, v.asString());
        } else if (key == "schedule") {
            Schedule(state, Later{v["event"].asString(), v["days"].isInt() ? v["days"].asInt() : 1});
        }
    }
}

std::string StatLabel(const std::string& key, const Content& content) {
    if (key == "money") {
        return "Coins";
    }
    if (key.starts_with("skill.")) {
        return Humanize(key.substr(6));
    }
    const std::size_t dot = key.find('.');
    if (dot != std::string::npos) {
        return content.NameOf(key.substr(0, dot)) + " " + key.substr(dot + 1);
    }
    return Humanize(key);
}

std::vector<std::string> DiffStats(const std::map<std::string, int>& before,
                                   const std::map<std::string, int>& after,
                                   const Content& content) {
    std::vector<std::string> out;
    std::set<std::string> keys;
    for (const auto& [key, value] : before) {
        keys.insert(key);
    }
    for (const auto& [key, value] : after) {
        keys.insert(key);
    }
    for (const std::string& key : keys) {
        const auto b = before.find(key);
        const auto a = after.find(key);
        const int delta = (a == after.end() ? 0 : a->second) - (b == before.end() ? 0 : b->second);
        if (delta != 0) {
            out.push_back(StatLabel(key, content) + (delta > 0 ? " +" : " ") + std::to_string(delta));
        }
    }
    return out;
}

QuestStatus GetQuestStatus(const QuestDef& quest, const GameState& state) {
    if (!quest.startFlag.empty() && !state.HasFlag(quest.startFlag)) {
        return QuestStatus::Hidden;
    }
    return CurrentStep(quest, state) == nullptr ? QuestStatus::Done : QuestStatus::Active;
}

const QuestStep* CurrentStep(const QuestDef& quest, const GameState& state) {
    for (const QuestStep& step : quest.steps) {
        if (!state.HasFlag(step.doneFlag)) {
            return &step;
        }
    }
    return nullptr;
}

const QuestDef* TrackedQuest(const GameState& state, const Content& content) {
    const int chapter = std::max(1, state.Get("chapter"));
    const QuestDef* fallback = nullptr;
    for (const QuestDef& quest : content.quests) {
        if (GetQuestStatus(quest, state) != QuestStatus::Active) {
            continue;
        }
        if (quest.chapter == chapter) {
            return &quest;
        }
        if (fallback == nullptr) {
            fallback = &quest;
        }
    }
    return fallback;
}

bool IsHotDay(const GameState& state) {
    std::uint32_t h = state.seed ^ (static_cast<std::uint32_t>(state.day) * 2654435761u);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h % 3 == 0;
}

std::vector<std::string> StartDay(GameState& state, const Content& content) {
    state.minutes = 8 * 60;
    std::erase_if(state.flags, [](const std::string& f) { return f.starts_with("today."); });
    state.todayLog.clear();
    state.moneyAtDayStart = state.Get("money");

    // Spoil stock that is past its shelf life.
    std::map<std::string, int> spoiled;
    std::erase_if(state.stock, [&](const StockBatch& batch) {
        const ItemDef* item = content.FindItem(batch.item);
        const int shelfDays = item ? ShelfDays(*item, state) : 30;
        if (state.day - batch.dayBought >= shelfDays) {
            spoiled[batch.item] += batch.qty;
            return true;
        }
        return false;
    });
    for (const auto& [id, qty] : spoiled) {
        const ItemDef* item = content.FindItem(id);
        state.todayLog.push_back(std::to_string(qty) + " " + (item ? item->name : id) +
                                 " went off overnight and had to be thrown away.");
    }

    if (IsHotDay(state)) {
        state.SetFlag("today.hot");
    }
    if (IsTired(state)) {
        state.todayLog.push_back("You woke up tired. Early nights help you recover.");
    }
    if (IsWeekStart(state.day, content.economy)) {
        ProcessWeek(state, content.economy);
    }

    std::vector<std::string> events;
    std::stable_sort(state.scheduled.begin(), state.scheduled.end(),
                     [](const ScheduledEvent& a, const ScheduledEvent& b) { return a.day < b.day; });
    std::erase_if(state.scheduled, [&](const ScheduledEvent& e) {
        if (e.day <= state.day) {
            events.push_back(e.event);
            return true;
        }
        return false;
    });
    for (const TriggerRule& rule : content.morning) {
        if ((rule.repeat || !state.seenEvents.contains(rule.event)) &&
            CheckCondition(rule.when, state)) {
            events.push_back(rule.event);
            break;
        }
    }
    return events;
}

void EndDay(GameState& state) {
    state.day += 1;
}

void ApplyBedtime(GameState& state, bool late) {
    if (late) {
        state.Add("health", -8);
        state.todayLog.push_back("You stayed out until dark and went to bed exhausted. (Health -8)");
    } else if (state.Hour() < 19) {
        state.Add("health", 5);
        state.todayLog.push_back("You got home early and had a good night's sleep. (Health +5)");
    } else {
        state.Add("health", 2);
    }
}

bool IsTired(const GameState& state) {
    return state.Get("health") < 40;
}

std::string PickEveningEvent(const GameState& state, const Content& content) {
    for (const TriggerRule& rule : content.evening) {
        if ((rule.repeat || !state.seenEvents.contains(rule.event)) &&
            CheckCondition(rule.when, state)) {
            return rule.event;
        }
    }
    return {};
}

std::string PickTalkEvent(const NpcDef& npc, const GameState& state) {
    for (const TalkRule& rule : npc.talk) {
        if (CheckCondition(rule.when, state)) {
            return rule.event;
        }
    }
    if (npc.idleEvents.empty()) {
        return {};
    }
    const std::size_t index =
        static_cast<std::size_t>(state.day + state.minutes / 30) % npc.idleEvents.size();
    return npc.idleEvents[index];
}

bool HasMet(const NpcDef& npc, const GameState& state, const Content& content) {
    if (state.InParty(npc.id)) {
        return true;
    }
    for (const std::string& id : state.seenEvents) {
        const EventDef* event = content.FindEvent(id);
        if (event == nullptr) {
            continue;
        }
        if (event->speaker == npc.id) {
            return true;
        }
        for (const Line& line : event->lines) {
            if (line.speaker == npc.id) {
                return true;
            }
        }
    }
    return false;
}

LifeWheel ComputeLifeWheel(const GameState& state, const Content& content) {
    LifeWheel wheel;
    // Money: coins plus the value of owning a stall, 100 coins fills the wheel.
    wheel.money = std::min(100, state.Get("money") + (state.HasFlag("stall_rented") ? 40 : 0));
    wheel.health = state.Get("health");

    int trust = 0;
    int fun = 0;
    int people = 0;
    for (const NpcDef& npc : content.npcs) {
        if (npc.relationship && HasMet(npc, state, content)) {
            trust += state.Get(npc.id + ".trust");
            fun += state.Get(npc.id + ".fun");
            ++people;
        }
    }
    wheel.relationships = people == 0 ? 0 : trust / people;
    const int averageFun = people == 0 ? 0 : fun / people;
    wheel.happiness = (averageFun + state.Get("happiness")) / 2;

    int skills = 0;
    for (const auto& [key, value] : state.stats) {
        if (key.starts_with("skill.")) {
            skills += value;
        }
    }
    wheel.knowledge = std::min(100, skills * 2);
    wheel.community = state.Get("reputation");
    return wheel;
}

bool ParseTravel(const std::string& action, TravelTarget& out) {
    if (!action.starts_with("travel:")) {
        return false;
    }
    const std::string rest = action.substr(7);
    const std::size_t a = rest.find(':');
    const std::size_t b = a == std::string::npos ? std::string::npos : rest.find(':', a + 1);
    if (a == std::string::npos || b == std::string::npos) {
        return false;
    }
    try {
        out.map = rest.substr(0, a);
        out.x = std::stoi(rest.substr(a + 1, b - a - 1));
        out.y = std::stoi(rest.substr(b + 1));
    } catch (const std::exception&) {
        return false;
    }
    return !out.map.empty();
}

std::vector<std::string> ValidateContent(const Content& content) {
    std::vector<std::string> problems;

    auto checkEvent = [&](const std::string& id, const std::string& where) {
        if (!id.empty() && !content.FindEvent(id)) {
            problems.push_back(where + ": unknown event '" + id + "'");
        }
    };
    auto checkCharacter = [&](const std::string& id, const std::string& where) {
        if (!id.empty() && id != "you" && !content.FindNpc(id)) {
            problems.push_back(where + ": unknown character '" + id + "'");
        }
    };
    auto checkCondition = [&](const Json::Value& when, const std::string& where) {
        if (when.isNull()) {
            return;
        }
        if (!when.isObject()) {
            problems.push_back(where + ": condition must be an object");
            return;
        }
        for (const std::string& key : when.getMemberNames()) {
            if (!kConditionKeys.contains(key)) {
                problems.push_back(where + ": unknown condition '" + key + "'");
            }
        }
    };
    auto checkEffects = [&](const Json::Value& effects, const std::string& where) {
        if (effects.isNull()) {
            return;
        }
        if (!effects.isObject()) {
            problems.push_back(where + ": effects must be an object");
            return;
        }
        for (const std::string& key : effects.getMemberNames()) {
            if (!kEffectKeys.contains(key)) {
                problems.push_back(where + ": unknown effect '" + key + "'");
            }
        }
        if (effects["buy_stock"].isObject() &&
            !content.FindItem(effects["buy_stock"]["item"].asString())) {
            problems.push_back(where + ": buy_stock has an unknown item");
        }
        ForEachString(effects["join_party"],
                      [&](const std::string& id) { checkCharacter(id, where + " join_party"); });
        if (effects["schedule"].isObject()) {
            checkEvent(effects["schedule"]["event"].asString(), where + " schedule");
        }
    };
    auto checkAction = [&](const std::string& action, const std::string& where) {
        if (action.empty() || action == "end_day" || action == "home" || action == "chapter_end" ||
            action == "open_bank") {
            return;
        }
        if (action.starts_with("open_supplier:")) {
            if (!content.FindSupplier(action.substr(14))) {
                problems.push_back(where + ": unknown supplier '" + action.substr(14) + "'");
            }
        } else if (action.starts_with("travel:")) {
            TravelTarget target;
            const MapDef* map = ParseTravel(action, target) ? content.FindMap(target.map) : nullptr;
            if (map == nullptr) {
                problems.push_back(where + ": bad travel action '" + action + "'");
            } else if (MapDef::IsSolid(map->TileAt(target.x, target.y))) {
                problems.push_back(where + ": travel lands on a blocked tile");
            }
        } else if (action.starts_with("open_sell:")) {
            if (!content.FindVenue(action.substr(10))) {
                problems.push_back(where + ": unknown venue '" + action.substr(10) + "'");
            }
        } else if (action.starts_with("event:")) {
            checkEvent(action.substr(6), where);
        } else if (action.starts_with("encounter:")) {
            if (!content.FindEncounter(action.substr(10))) {
                problems.push_back(where + ": unknown encounter '" + action.substr(10) + "'");
            }
        } else {
            problems.push_back(where + ": unknown action '" + action + "'");
        }
    };

    for (const auto& [id, event] : content.events) {
        const std::string where = "event " + id;
        checkCharacter(event.speaker, where);
        for (const Line& line : event.lines) {
            checkCharacter(line.speaker, where);
        }
        checkEffects(event.effects, where);
        checkEvent(event.then, where + " then");
        checkEvent(event.later.event, where + " later");
        checkAction(event.action, where);
        for (const ChoiceDef& choice : event.choices) {
            const std::string cw = where + " choice '" + choice.label + "'";
            checkCondition(choice.showWhen, cw);
            checkCondition(choice.requirement, cw);
            checkEffects(choice.effects, cw);
            checkEvent(choice.then, cw + " then");
            checkEvent(choice.later.event, cw + " later");
            checkAction(choice.action, cw);
        }
    }
    for (const TriggerRule& rule : content.morning) {
        checkEvent(rule.event, "morning trigger");
        checkCondition(rule.when, "morning trigger " + rule.event);
    }
    for (const TriggerRule& rule : content.evening) {
        checkEvent(rule.event, "evening trigger");
        checkCondition(rule.when, "evening trigger " + rule.event);
    }
    for (const NpcDef& npc : content.npcs) {
        const std::string where = "character " + npc.id;
        checkCondition(npc.presentWhen, where + " present_when");
        for (const TalkRule& rule : npc.talk) {
            checkEvent(rule.event, where + " talk");
            checkCondition(rule.when, where + " talk " + rule.event);
        }
        const MapDef* map = content.FindMap(npc.map);
        if (map == nullptr) {
            problems.push_back(where + ": unknown map '" + npc.map + "'");
        } else if (map->TileAt(static_cast<int>(npc.x), static_cast<int>(npc.y)) == 'X') {
            problems.push_back(where + ": position is outside the map");
        }
    }
    for (const auto& [id, venue] : content.venues) {
        for (const VenueBonus& bonus : venue.bonuses) {
            checkCondition(bonus.when, "venue " + id + " bonus");
        }
    }
    for (const auto& [id, supplier] : content.suppliers) {
        checkCharacter(supplier.npc, "supplier " + id);
        for (const SupplierItem& item : supplier.items) {
            if (!content.FindItem(item.item)) {
                problems.push_back("supplier " + id + ": unknown item '" + item.item + "'");
            }
        }
    }
    for (const auto& [id, item] : content.items) {
        for (const ValueRule& rule : item.valueWhen) {
            checkCondition(rule.when, "item " + id + " value_when");
        }
    }
    for (const auto& [id, enc] : content.encounters) {
        const std::string where = "encounter " + id;
        checkCharacter(enc.opponent, where);
        checkEvent(enc.winEvent, where + " win");
        checkEvent(enc.loseEvent, where + " lose");
        for (const EncounterAction& action : enc.actions) {
            checkCharacter(action.party, where + " action " + action.id);
            checkCondition(action.requirement, where + " action " + action.id);
            checkEffects(action.effects, where + " action " + action.id);
        }
    }
    for (const QuestDef& quest : content.quests) {
        if (quest.steps.empty()) {
            problems.push_back("quest " + quest.id + ": has no steps");
        }
        for (const QuestStep& step : quest.steps) {
            if (step.doneFlag.empty()) {
                problems.push_back("quest " + quest.id + ": step without done_flag");
            }
        }
    }
    for (const std::string& id : content.start.events) {
        checkEvent(id, "start");
    }
    for (const std::string& id : content.start.party) {
        checkCharacter(id, "start party");
    }
    for (const auto& [mapId, map] : content.maps) {
        const std::string where = "map " + mapId;
        if (map.width == 0 || map.height == 0) {
            problems.push_back(where + ": no tiles");
        }
        for (const Building& b : map.buildings) {
            if (b.x < 0 || b.y < 0 || b.x + b.w > map.width || b.y + b.h > map.height) {
                problems.push_back(where + " building " + b.id + ": outside the map");
            }
            if (b.doorX != -1 && (b.doorX < b.x || b.doorX >= b.x + b.w)) {
                problems.push_back(where + " building " + b.id + ": door is not on the building");
            }
            if (b.doorX == -1 && !b.action.empty()) {
                problems.push_back(where + " building " + b.id + ": has an action but no door");
            }
            checkAction(b.action, where + " building " + b.id);
        }
        for (const Spot& spot : map.spots) {
            if (map.TileAt(spot.x, spot.y) == 'X') {
                problems.push_back(where + " spot " + spot.id + ": outside the map");
            }
            checkCondition(spot.when, where + " spot " + spot.id);
            checkAction(spot.action, where + " spot " + spot.id);
        }
        for (const Exit& exit : map.exits) {
            const MapDef* target = content.FindMap(exit.to);
            if (target == nullptr) {
                problems.push_back(where + ": exit to unknown map '" + exit.to + "'");
                continue;
            }
            if (MapDef::IsSolid(target->TileAt(exit.toX, exit.toY))) {
                problems.push_back(where + ": exit to '" + exit.to + "' lands on a blocked tile");
            }
            for (const Exit& back : target->exits) {
                if (exit.toX >= back.x && exit.toX < back.x + back.w && exit.toY >= back.y &&
                    exit.toY < back.y + back.h) {
                    problems.push_back(where + ": exit to '" + exit.to + "' lands inside an exit");
                }
            }
        }
        if (map.TileAt(map.startX, map.startY) == 'X') {
            problems.push_back(where + ": start position is outside the map");
        }
        checkEvent(map.enterEvent, where + " enter_event");
        if (!map.enterEvent.empty() && map.enterFlag.empty()) {
            problems.push_back(where + ": enter_event needs an enter_flag");
        }
    }
    return problems;
}

}  // namespace game
