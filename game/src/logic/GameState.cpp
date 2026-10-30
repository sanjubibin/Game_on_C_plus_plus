#include "logic/GameState.h"

#include <algorithm>
#include <cstdio>

namespace game {

namespace {

bool EndsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

void ClampStat(const std::string& key, int& value) {
    if (key == "money") {
        value = std::max(value, 0);
    } else if (key == "reputation" || key == "health" || key == "happiness" ||
               EndsWith(key, ".trust") || EndsWith(key, ".fun") || EndsWith(key, ".respect")) {
        value = std::clamp(value, 0, 100);
    } else if (key.starts_with("skill.")) {
        value = std::clamp(value, 0, 10);
    }
}

int GameState::Get(const std::string& key) const {
    const auto it = stats.find(key);
    return it == stats.end() ? 0 : it->second;
}

void GameState::Set(const std::string& key, int value) {
    ClampStat(key, value);
    stats[key] = value;
}

void GameState::Add(const std::string& key, int delta) {
    Set(key, Get(key) + delta);
}

bool GameState::HasFlag(const std::string& flag) const {
    return flags.contains(flag);
}

void GameState::SetFlag(const std::string& flag) {
    flags.insert(flag);
}

void GameState::ClearFlag(const std::string& flag) {
    flags.erase(flag);
}

bool GameState::InParty(const std::string& id) const {
    return std::find(party.begin(), party.end(), id) != party.end();
}

void GameState::JoinParty(const std::string& id) {
    if (!InParty(id)) {
        party.push_back(id);
    }
}

int GameState::StockOf(const std::string& item) const {
    int total = 0;
    for (const StockBatch& batch : stock) {
        if (batch.item == item) {
            total += batch.qty;
        }
    }
    return total;
}

void GameState::AddStock(const StockBatch& batch) {
    if (batch.qty <= 0) {
        return;
    }
    for (StockBatch& existing : stock) {
        if (existing.item == batch.item && existing.unitCost == batch.unitCost &&
            existing.dayBought == batch.dayBought && existing.dodgy == batch.dodgy) {
            existing.qty += batch.qty;
            return;
        }
    }
    stock.push_back(batch);
}

int GameState::TakeStock(const std::string& item, int qty) {
    int cost = 0;
    for (StockBatch& batch : stock) {
        if (qty <= 0) {
            break;
        }
        if (batch.item != item) {
            continue;
        }
        const int taken = std::min(qty, batch.qty);
        batch.qty -= taken;
        qty -= taken;
        cost += taken * batch.unitCost;
    }
    std::erase_if(stock, [](const StockBatch& batch) { return batch.qty <= 0; });
    return cost;
}

std::string GameState::ClockText() const {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buffer;
}

}  // namespace game
