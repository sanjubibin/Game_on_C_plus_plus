#include "logic/Selling.h"

#include <cmath>
#include <cstdint>

#include "logic/Rules.h"

namespace game {

namespace {

constexpr int kLastOpeningHour = 17;
constexpr float kHelperEfficiency = 0.6f;

}  // namespace

bool ItemAvailable(const ItemDef& item, const GameState& state) {
    if (!item.unlockFlag.empty() && !state.HasFlag(item.unlockFlag)) {
        return false;
    }
    return item.hideFlag.empty() || !state.HasFlag(item.hideFlag);
}

std::vector<std::string> ItemsInStock(const GameState& state, const Content& content) {
    std::vector<std::string> out;
    for (const std::string& id : content.itemOrder) {
        if (state.StockOf(id) > 0) {
            out.push_back(id);
        }
    }
    return out;
}

int PriceOf(const GameState& state, const Content& content, const std::string& item) {
    const auto it = state.prices.find(item);
    if (it != state.prices.end()) {
        return it->second;
    }
    const ItemDef* def = content.FindItem(item);
    return def ? def->value : 1;
}

int AverageCost(const GameState& state, const std::string& item) {
    int qty = 0;
    int cost = 0;
    for (const StockBatch& batch : state.stock) {
        if (batch.item == item) {
            qty += batch.qty;
            cost += batch.qty * batch.unitCost;
        }
    }
    return qty == 0 ? 0 : static_cast<int>(std::lround(static_cast<double>(cost) / qty));
}

bool VenueTooLate(const GameState& state) {
    return state.Hour() >= kLastOpeningHour;
}

std::string CannotOpenReason(const GameState& state, const Content& content, const VenueDef& venue) {
    if (ItemsInStock(state, content).empty()) {
        return "You have nothing to sell. Buy stock at Lin's Wholesale first!";
    }
    if (VenueTooLate(state)) {
        return "Everyone is heading home. Come back tomorrow!";
    }
    if (state.Get("money") < venue.fee) {
        return "You need " + std::to_string(venue.fee) + " coins for the fee.";
    }
    return {};
}

bool HelperCanRun(const GameState& state, const Content& content, const VenueDef& venue) {
    return !venue.helper.empty() && CannotOpenReason(state, content, venue).empty() &&
           state.InParty(venue.helper) && !state.HasFlag("today.helper_ran") &&
           !state.HasFlag(venue.helper + "_upset");
}

SessionReport RunSellSession(GameState& state, const Content& content, const VenueDef& venue,
                             bool helperRuns) {
    SessionReport report;
    report.helperRan = helperRuns;
    const bool hot = state.HasFlag("today.hot");
    const bool festival = state.HasFlag("today.festival");
    const std::vector<std::string> items = ItemsInStock(state, content);

    SellParams params;
    for (const std::string& id : items) {
        const ItemDef* def = content.FindItem(id);
        params.items.push_back({id, PriceOf(state, content, id), def ? def->value : 1, state.StockOf(id),
                                def ? (hot ? def->hotAppeal : def->appeal) : 1.0f});
    }
    params.reputation = state.Get("reputation");
    params.hours = venue.hours;
    params.efficiency = (helperRuns ? kHelperEfficiency : 1.0f) * venue.customers *
                        (festival ? venue.festivalCustomers : 1.0f);

    for (const std::string& member : state.party) {
        const NpcDef* npc = content.FindNpc(member);
        if (npc != nullptr && npc->bonus.customers > 0.0f) {
            params.customerBonus += npc->bonus.customers;
            report.bonusTexts.push_back(npc->bonus.text);
        }
    }
    for (const VenueBonus& bonus : venue.bonuses) {
        if (CheckCondition(bonus.when, state)) {
            params.customerBonus += bonus.customers;
            report.bonusTexts.push_back(bonus.text);
        }
    }
    params.seed = state.seed ^ (static_cast<std::uint32_t>(state.day) * 7919u) ^
                  (static_cast<std::uint32_t>(state.minutes) * 31u) ^ (helperRuns ? 0x9e3779b9u : 0u);

    report.sales = SimulateSales(params);
    for (const auto& [item, qty] : report.sales.sold) {
        report.costOfSold += state.TakeStock(item, qty);
    }
    report.fee = venue.fee;
    state.Add("money", report.sales.revenue - report.fee);
    report.reputationChange = report.sales.ReputationChange();
    state.Add("reputation", report.reputationChange);

    if (helperRuns) {
        state.SetFlag("today.helper_ran");
    } else {
        state.minutes += venue.hours * 60;
    }
    if (report.sales.TotalSold() > 0) {
        if (!state.HasFlag("first_sale")) {
            state.memories.push_back({state.day, "Made our very first sale!"});
        }
        state.SetFlag("first_sale");
        state.SetFlag(venue.id + "_sale");
        if (festival && !state.HasFlag(venue.id + "_festival_sale")) {
            state.SetFlag(venue.id + "_festival_sale");
            state.memories.push_back({state.day, "Sold at the " + venue.name + " on Festival Day!"});
        }
    }
    report.skillUp = report.Profit() > 0 && !helperRuns && state.Get("skill.money_sense") < 4;
    if (report.skillUp) {
        state.Add("skill.money_sense", 1);
    }
    for (const std::string& id : items) {
        report.leftover[id] = state.StockOf(id);
    }

    std::string who = venue.name;
    if (helperRuns) {
        who = content.NameOf(venue.helper) + " ran the " + venue.name;
    }
    state.todayLog.push_back(who + ": sold " + std::to_string(report.sales.TotalSold()) + " items, profit " +
                             std::to_string(report.Profit()) + " coins.");
    return report;
}

}  // namespace game
