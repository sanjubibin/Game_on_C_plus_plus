#include "logic/Selling.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

#include "logic/Rules.h"

namespace game {

namespace {

constexpr int kLastOpeningHour = 17;

std::string StaffFlag(const std::string& staffId) {
    return "today.staff_" + staffId;
}

std::string VenueStaffedFlag(const std::string& venueId) {
    return "today.staffed_" + venueId;
}

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
        return "You have nothing to sell. Buy stock from a supplier first!";
    }
    if (VenueTooLate(state)) {
        return "Everyone is heading home. Come back tomorrow!";
    }
    if (state.Get("money") < venue.fee) {
        return "You need " + std::to_string(venue.fee) + " coins for the fee.";
    }
    return {};
}

std::vector<std::string> StaffCandidates(const GameState& state, const Content& content) {
    std::vector<std::string> out;
    for (const std::string& member : state.party) {
        const NpcDef* npc = content.FindNpc(member);
        if (npc != nullptr && npc->staff.canWork) {
            out.push_back(member);
        }
    }
    return out;
}

int StaffWage(const GameState& state) {
    return state.Get("staff.wage");
}

std::string StaffUnavailableReason(const GameState& state, const Content& content, const VenueDef& venue,
                                   const std::string& staffId) {
    const std::string name = content.NameOf(staffId);
    if (!venue.staffable) {
        return "Only you can run this.";
    }
    if (state.HasFlag(staffId + "_upset")) {
        return name + " is too upset to work today.";
    }
    if (state.HasFlag(StaffFlag(staffId))) {
        return name + " already worked today.";
    }
    if (state.HasFlag(VenueStaffedFlag(venue.id))) {
        return "The " + venue.name + " has already been open today.";
    }
    const std::string closed = CannotOpenReason(state, content, venue);
    if (!closed.empty()) {
        return closed;
    }
    if (state.Get("money") < venue.fee + StaffWage(state)) {
        return "Not enough coins for the fee and " + name + "'s wage.";
    }
    return {};
}

std::string BranchManager(const GameState& state) {
    for (const std::string& flag : state.flags) {
        if (flag.starts_with("manager_") && flag != "manager_chosen" && flag != "manager_conflict") {
            return flag.substr(8);
        }
    }
    return {};
}

bool RunBranchDay(GameState& state, const Content& content) {
    const VenueDef* branch = content.FindVenue("branch");
    if (branch == nullptr || !state.HasFlag("branch_open")) {
        return false;
    }
    const std::string manager = BranchManager(state);
    std::string why;
    if (manager.empty()) {
        why = "no manager has been chosen yet";
    } else if (state.HasFlag(manager + "_upset")) {
        why = content.NameOf(manager) + " was too upset to work";
    } else if (state.HasFlag(StaffFlag(manager))) {
        why = content.NameOf(manager) + " was busy at another stand today";
    } else if (state.HasFlag(VenueStaffedFlag(branch->id))) {
        return false;  // you ran it yourself today
    } else if (ItemsInStock(state, content).empty()) {
        why = "there was no stock left to sell";
    } else if (state.Get("money") < branch->fee + StaffWage(state)) {
        why = "there weren't enough coins for " + content.NameOf(manager) + "'s wage";
    }
    if (!why.empty()) {
        state.todayLog.push_back("The " + branch->name + " stayed closed this evening: " + why + ".");
        return false;
    }
    // Evening trade: the session must not move your clock, and it counts as staffed.
    RunSellSession(state, content, *branch, manager);
    return true;
}

SessionReport RunSellSession(GameState& state, const Content& content, const VenueDef& venue,
                             const std::string& staffId) {
    SessionReport report;
    report.staff = staffId;
    const bool byStaff = !staffId.empty();
    const bool hot = state.HasFlag("today.hot");
    const bool festival = state.HasFlag("today.festival");
    const std::vector<std::string> items = ItemsInStock(state, content);

    SellParams params;
    for (const std::string& id : items) {
        const ItemDef* def = content.FindItem(id);
        params.items.push_back({id, PriceOf(state, content, id), def ? ItemValue(*def, state) : 1, state.StockOf(id),
                                def ? (hot ? def->hotAppeal : def->appeal) : 1.0f});
    }
    params.reputation = state.Get("reputation");
    params.hours = venue.hours;

    float skill = 1.0f;
    if (byStaff) {
        const NpcDef* npc = content.FindNpc(staffId);
        const float efficiency = npc != nullptr ? npc->staff.efficiency : 0.6f;
        const int morale = state.stats.contains("staff.morale_pct") ? state.Get("staff.morale_pct") : 100;
        skill = efficiency * static_cast<float>(morale) / 100.0f;
    }
    const bool rainedOff = venue.outdoor && state.HasFlag("today.rain");
    params.efficiency = skill * venue.customers * (festival ? venue.festivalCustomers : 1.0f) *
                        (rainedOff ? 0.5f : 1.0f);
    if (rainedOff) {
        report.bonusTexts.push_back("Rain kept most people at home.");
    }

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
    params.customerBonus = std::max(params.customerBonus, -0.9f);
    std::uint32_t staffSalt = 0;
    for (const char ch : staffId) {
        staffSalt = staffSalt * 31u + static_cast<unsigned char>(ch);
    }
    params.seed = state.seed ^ (static_cast<std::uint32_t>(state.day) * 7919u) ^
                  (static_cast<std::uint32_t>(state.minutes) * 31u) ^ (staffSalt * 0x9e3779b9u) ^
                  static_cast<std::uint32_t>(std::hash<std::string>{}(venue.id));

    report.sales = SimulateSales(params);
    for (const auto& [item, qty] : report.sales.sold) {
        report.costOfSold += state.TakeStock(item, qty);
    }
    report.fee = venue.fee;
    report.wage = byStaff ? StaffWage(state) : 0;
    state.Add("money", report.sales.revenue - report.fee - report.wage);
    report.reputationChange = report.sales.ReputationChange();
    state.Add("reputation", report.reputationChange);

    if (byStaff) {
        state.SetFlag(StaffFlag(staffId));
    } else {
        state.minutes += venue.hours * 60;
    }
    state.SetFlag(VenueStaffedFlag(venue.id));
    if (report.sales.TotalSold() > 0) {
        if (!state.HasFlag("first_sale")) {
            AddMemory(state, "Made our very first sale!");
        }
        state.SetFlag("first_sale");
        state.SetFlag(venue.id + "_sale");
        if (festival && !state.HasFlag(venue.id + "_festival_sale")) {
            state.SetFlag(venue.id + "_festival_sale");
            AddMemory(state, "Sold at the " + venue.name + " on Festival Day!");
        }
    }
    report.skillUp = report.Profit() > 0 && !byStaff && state.Get("skill.money_sense") < 4;
    if (report.skillUp) {
        state.Add("skill.money_sense", 1);
    }
    for (const std::string& id : items) {
        report.leftover[id] = state.StockOf(id);
    }

    std::string who = venue.name;
    if (byStaff) {
        who = content.NameOf(staffId) + " ran the " + venue.name;
    }
    state.todayLog.push_back(who + ": sold " + std::to_string(report.sales.TotalSold()) + " items, profit " +
                             std::to_string(report.Profit()) + " coins.");
    return report;
}

}  // namespace game
