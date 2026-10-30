#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace game {

struct StockBatch {
    std::string item;
    int qty = 0;
    int unitCost = 0;
    int dayBought = 1;
    bool dodgy = false;  // bought from a scammer; removed by a story event
};

struct ScheduledEvent {
    int day = 0;
    std::string event;
};

// A moment worth remembering, shown at the end of a chapter.
struct Memory {
    int day = 0;
    std::string text;
};

// Everything that changes during play. This is exactly what the save file stores.
struct GameState {
    int day = 1;
    int minutes = 8 * 60;  // time of day, in minutes since midnight
    std::uint32_t seed = 12345;

    std::map<std::string, int> stats;   // "money", "reputation", "skill.empathy", "maya.trust", ...
    std::set<std::string> flags;        // story flags; "today.*" flags reset every morning
    std::set<std::string> seenEvents;
    std::vector<std::string> party;     // companions following the player
    std::vector<StockBatch> stock;      // oldest first
    std::map<std::string, int> prices;  // last selling price chosen per item
    std::vector<ScheduledEvent> scheduled;
    std::vector<Memory> memories;

    int moneyAtDayStart = 0;
    std::vector<std::string> todayLog;  // shown on the end-of-day summary

    int Get(const std::string& key) const;
    void Set(const std::string& key, int value);
    void Add(const std::string& key, int delta);

    bool HasFlag(const std::string& flag) const;
    void SetFlag(const std::string& flag);
    void ClearFlag(const std::string& flag);

    bool InParty(const std::string& id) const;
    void JoinParty(const std::string& id);

    int StockOf(const std::string& item) const;
    void AddStock(const StockBatch& batch);
    // Removes up to qty units, oldest first. Returns the total cost price of the removed units.
    int TakeStock(const std::string& item, int qty);

    int Hour() const { return minutes / 60; }
    std::string ClockText() const;
};

// Keeps a stat in its valid range: relationships, reputation, health and happiness 0-100,
// skills 0-10, money >= 0.
void ClampStat(const std::string& key, int& value);

}  // namespace game
