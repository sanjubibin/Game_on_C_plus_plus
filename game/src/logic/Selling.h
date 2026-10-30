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
    int wage = 0;
    int reputationChange = 0;
    bool skillUp = false;
    std::string staff;  // who ran it; empty when you did
    std::map<std::string, int> leftover;
    std::vector<std::string> bonusTexts;

    int Profit() const { return sales.revenue - costOfSold - fee - wage; }
};

// Whether a supplier sells this item right now (unlock / hide flags).
bool ItemAvailable(const ItemDef& item, const GameState& state);
std::vector<std::string> ItemsInStock(const GameState& state, const Content& content);
int PriceOf(const GameState& state, const Content& content, const std::string& item);
int AverageCost(const GameState& state, const std::string& item);

bool VenueTooLate(const GameState& state);
// Empty when the player can open the venue now, otherwise the reason they can't.
std::string CannotOpenReason(const GameState& state, const Content& content, const VenueDef& venue);

// --- Staff ------------------------------------------------------------------------------------
// Team members who can run a venue for you (stat staff.wage is the daily pay policy,
// staff.morale_pct how hard staff work at that pay).
std::vector<std::string> StaffCandidates(const GameState& state, const Content& content);
int StaffWage(const GameState& state);
// Empty when this team member can run the venue now, otherwise why not.
std::string StaffUnavailableReason(const GameState& state, const Content& content, const VenueDef& venue,
                                   const std::string& staffId);

// Runs one session (venue.hours long) and applies money, stock, reputation, time and flags.
// With a staffId, that team member sells for you (paid their wage up front) and your clock
// does not move.
SessionReport RunSellSession(GameState& state, const Content& content, const VenueDef& venue,
                             const std::string& staffId = {});

}  // namespace game
