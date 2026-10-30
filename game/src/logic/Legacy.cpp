#include "logic/Legacy.h"

#include <algorithm>
#include <array>
#include <utility>

namespace game {

namespace {

// Flags about one person's own progress; the next generation earns these again.
bool IsPersonalFlag(const std::string& flag) {
    return flag == "first_sale" || flag == "bought_stock" || flag == "bought_bulk" || flag.ends_with("_sale") ||
           flag.starts_with("bought_from_") || flag.starts_with("today.") || flag.ends_with("_upset") ||
           flag.starts_with("manager_") || flag == "stall_rented" || flag == "shop_partner" ||
           flag == "branch_open" || flag == "rent_late" || flag == "loan_missed";
}

}  // namespace

LegacyReport StartNextGeneration(GameState& state, const Content& content) {
    LegacyReport report;
    report.generation = CurrentGeneration(state);
    report.wheel = ComputeLifeWheel(state, content);
    report.wealth = state.Get("money") + state.Get("bank.savings") - state.Get("bank.loan");
    const int fund = std::max(0, report.wealth);
    report.gift = std::clamp(fund / 5, 10, 40);
    report.childSavings = std::min(fund / 4, 120);
    report.familyReputation = 30 + std::max(0, state.Get("reputation") - 30) / 2;

    // What became of everyone - told on the legacy screen and remembered by the town.
    auto has = [&](const char* flag) { return state.HasFlag(flag); };
    report.legacies.push_back(has("shop_partner") ? "The corner shop is still open - your family ran it together."
                                                  : "The corner shop is still open.");
    if (has("branch_open")) {
        report.legacies.push_back("Maya runs the city shop you opened.");
    }
    if (has("fridge")) {
        report.legacies.push_back("The old fridge still hums in the corner shop.");
    }
    if (has("leo_joined")) {
        report.legacies.push_back("Leo became an artist. He still paints your family's signs.");
    }
    if (has("priya_joined")) {
        report.legacies.push_back("Priya started her own business. She says she learned it at your pushcart.");
    }
    if (has("kofi_friend")) {
        report.legacies.push_back("Kofi's farm supplies half the town. Your families are still partners.");
    }
    if (has("maya_forgiven")) {
        report.legacies.push_back("Maya never forgot that you listened to her side of the story.");
    }
    if (has("grumble_regular")) {
        report.legacies.push_back("Mrs. Grumble's grandchildren are now your family's regular customers.");
    }
    if (has("has_loan") && state.Get("bank.loan") == 0) {
        report.legacies.push_back(has("loan_missed") ? "Every loan was paid back - even after a missed payment."
                                                     : "Every loan was paid back on time.");
    }
    report.legacies.push_back("The family fund: " + std::to_string(fund) + " coins.");

    // The family talent: the skill this generation grew most.
    std::string talent;
    int best = -1;
    for (const auto& [key, value] : state.stats) {
        if (key.starts_with("skill.") && value > best) {
            best = value;
            talent = key;
        }
    }
    report.familyTalent = talent.empty() ? "" : StatLabel(talent, content);

    // Remembered relationships carry into the next generation.
    const std::array<std::pair<const char*, const char*>, 7> carry = {{{"mom", "grandma"},
                                                                       {"dad", "grandpa2"},
                                                                       {"grandpa", "great_grandpa"},
                                                                       {"maya", "aunt_maya"},
                                                                       {"leo", "leo2"},
                                                                       {"priya", "priya2"},
                                                                       {"kofi", "kofi2"}}};
    std::map<std::string, int> carried;
    for (const auto& [from, to] : carry) {
        carried[std::string(to) + ".trust"] = std::max(40, state.Get(std::string(from) + ".trust"));
    }

    // --- The new life begins ---
    if (has("branch_open")) {
        state.SetFlag("legacy_branch");  // the city shop is now Aunt Maya's
    }
    state.Set("generation", report.generation + 1);
    state.day = 1;
    state.minutes = 8 * 60;
    state.stock.clear();
    state.prices.clear();
    state.scheduled.clear();
    state.todayLog.clear();
    state.party = {"nia"};
    std::erase_if(state.flags, IsPersonalFlag);
    state.SetFlag("next_generation");

    for (auto& [key, value] : state.stats) {
        if (key.starts_with("skill.")) {
            value = 1;
        }
    }
    if (!talent.empty()) {
        state.Set(talent, 2);
    }
    state.Set("money", report.gift);
    state.Set("bank.savings", report.childSavings);
    state.Set("bank.loan", 0);  // debts belong to whoever borrowed
    state.Set("bank.payment", 0);
    state.Set("bank.interest_earned", 0);
    state.Set("branch.rent", 0);
    state.Set("reputation", report.familyReputation);
    state.Set("health", 80);
    state.Set("happiness", 60);
    state.Set("staff.wage", 0);
    state.Set("staff.morale_pct", 100);
    for (const auto& [key, value] : carried) {
        state.Set(key, value);
    }
    for (const char* id : {"nia", "parent"}) {
        state.Set(std::string(id) + ".trust", 70);
        state.Set(std::string(id) + ".fun", 65);
        state.Set(std::string(id) + ".respect", 50);
    }
    state.moneyAtDayStart = state.Get("money");
    state.memories.push_back({1, "A new generation begins."});
    return report;
}

}  // namespace game
