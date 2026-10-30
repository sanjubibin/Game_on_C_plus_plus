#include "logic/Bank.h"

#include <algorithm>

#include "logic/Rules.h"

namespace game {

bool IsWeekStart(int day, const EconomyDef& economy) {
    return day > 1 && (day - 1) % economy.weekLength == 0;
}

int NextWeekStart(int day, const EconomyDef& economy) {
    return ((day - 1) / economy.weekLength + 1) * economy.weekLength + 1;
}

int SavingsInterest(int savings, int ratePercent) {
    const int interest = savings * ratePercent / 100;
    // Even small savings earn something, so children can see it grow.
    return std::max(savings >= 10 ? 1 : 0, interest);
}

int LoanTotal(int amount, int ratePercent) {
    return amount + (amount * ratePercent + 99) / 100;
}

int WeeklyPayment(int total, int weeks) {
    return weeks <= 0 ? total : (total + weeks - 1) / weeks;
}

bool Deposit(GameState& state, int amount) {
    if (amount <= 0 || amount > state.Get("money")) {
        return false;
    }
    state.Add("money", -amount);
    state.Add("bank.savings", amount);
    return true;
}

bool Withdraw(GameState& state, int amount) {
    if (amount <= 0 || amount > state.Get("bank.savings")) {
        return false;
    }
    state.Add("bank.savings", -amount);
    state.Add("money", amount);
    return true;
}

namespace {

void LoanCleared(GameState& state) {
    state.Set("bank.payment", 0);
    state.todayLog.push_back("The loan is fully paid back. No more weekly payments!");
    AddMemory(state, "Paid back our loan in full.");
}

void PayRent(GameState& state) {
    const int rent = state.Get("branch.rent");
    if (!state.HasFlag("branch_open") || rent <= 0) {
        return;
    }
    const int fromPocket = std::min(rent, state.Get("money"));
    const int fromSavings = std::min(rent - fromPocket, state.Get("bank.savings"));
    if (fromPocket + fromSavings < rent) {
        state.SetFlag("rent_late");
        state.Add("reputation", -5);
        state.todayLog.push_back("Not enough coins for the city shop's rent (" + std::to_string(rent) +
                                 ")! Mrs. Osei is not happy. (Reputation -5)");
        return;
    }
    state.Add("money", -fromPocket);
    state.Add("bank.savings", -fromSavings);
    state.ClearFlag("rent_late");
    std::string line = "City shop rent: " + std::to_string(rent) + " coins";
    if (fromSavings > 0) {
        line += " (" + std::to_string(fromSavings) + " taken from your savings)";
    }
    state.todayLog.push_back(line + ".");
}

void PayPremium(GameState& state) {
    const int premium = state.Get("bank.premium");
    if (!state.HasFlag("insured") || premium <= 0) {
        return;
    }
    if (state.Get("money") >= premium) {
        state.Add("money", -premium);
        state.todayLog.push_back("Insurance: " + std::to_string(premium) + " coins. Your pushcart is covered.");
    } else if (state.Get("bank.savings") >= premium) {
        state.Add("bank.savings", -premium);
        state.todayLog.push_back("Insurance: " + std::to_string(premium) + " coins (from savings).");
    } else {
        state.ClearFlag("insured");
        state.todayLog.push_back("You couldn't pay the insurance, so your pushcart is no longer covered!");
    }
}

}  // namespace

bool RepayLoan(GameState& state, int amount) {
    amount = std::min(amount, state.Get("bank.loan"));
    if (amount <= 0 || amount > state.Get("money")) {
        return false;
    }
    state.Add("money", -amount);
    state.Add("bank.loan", -amount);
    if (state.Get("bank.loan") == 0) {
        LoanCleared(state);
    }
    return true;
}

void TakeLoan(GameState& state, int amount, int ratePercent, int weeks) {
    const int total = LoanTotal(amount, ratePercent);
    state.Add("bank.loan", total);
    state.Set("bank.payment", WeeklyPayment(state.Get("bank.loan"), weeks));
    state.Add("money", amount);
    state.todayLog.push_back("Borrowed " + std::to_string(amount) + " coins. You'll pay back " +
                             std::to_string(total) + " in weekly payments of " +
                             std::to_string(state.Get("bank.payment")) + ".");
}

void ProcessWeek(GameState& state, const EconomyDef& economy) {
    const int savings = state.Get("bank.savings");
    if (savings > 0) {
        const int interest = SavingsInterest(savings, economy.savingsInterestPercent);
        if (interest > 0) {
            state.Add("bank.savings", interest);
            state.Add("bank.interest_earned", interest);
            state.todayLog.push_back("The bank paid " + std::to_string(interest) +
                                     " coins of interest on your savings. Money growing while you sleep!");
        }
    }

    PayRent(state);
    PayPremium(state);

    const int owed = state.Get("bank.loan");
    if (owed <= 0) {
        return;
    }
    const int due = std::min(owed, std::max(1, state.Get("bank.payment")));
    if (state.Get("money") >= due) {
        state.Add("money", -due);
        state.Add("bank.loan", -due);
        if (state.Get("bank.loan") == 0) {
            LoanCleared(state);
        } else {
            state.todayLog.push_back("Loan payment: " + std::to_string(due) + " coins. Still owed: " +
                                     std::to_string(state.Get("bank.loan")) + ".");
        }
    } else {
        state.Add("bank.loan", economy.lateFee);
        state.SetFlag("loan_missed");
        state.todayLog.push_back("Not enough coins for the loan payment! A late fee of " +
                                 std::to_string(economy.lateFee) + " coins was added to what you owe.");
    }
}

}  // namespace game
