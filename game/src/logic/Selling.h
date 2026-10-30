#pragma once

#include <map>
#include <string>
#include <vector>

#include "logic/Content.h"
#include "logic/GameState.h"
#include "logic/ShopSim.h"

namespace game {

// Everything one selling session changed, for the results screen.
struct SessionReport {
    SellResult sales;
    int costOfSold = 0;
    int fee = 0;
    int reputationChange = 0;
    bool skillUp = false;
    bool helperRan = false;
    std::map<std::string, int> leftover;
    std::vector<std::string> bonusTexts;

    int Profit() const { return sales.revenue - costOfSold - fee; }
};

// Whether the wholesaler sells this item right now (unlock / hide flags).
bool ItemAvailable(const ItemDef& item, const GameState& state);
std::vector<std::string> ItemsInStock(const GameState& state, const Content& content);
int PriceOf(const GameState& state, const Content& content, const std::string& item);
int AverageCost(const GameState& state, const std::string& item);

bool VenueTooLate(const GameState& state);
// Empty when the player can open the venue now, otherwise the reason they can't.
std::string CannotOpenReason(const GameState& state, const Content& content, const VenueDef& venue);
bool HelperCanRun(const GameState& state, const Content& content, const VenueDef& venue);

// Runs one session (venue.hours long) and applies money, stock, reputation, time and flags.
// When helperRuns, the venue's helper sells for you at 60% and your clock does not move.
SessionReport RunSellSession(GameState& state, const Content& content, const VenueDef& venue,
                             bool helperRuns);

}  // namespace game
