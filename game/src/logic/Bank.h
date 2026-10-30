#pragma once

#include "logic/Content.h"
#include "logic/GameState.h"

namespace game {

// The Town Bank: savings earn interest every week; loans are paid back in weekly instalments.
// Balances live in stats: bank.savings, bank.loan (still owed), bank.payment (due each week),
// bank.interest_earned (lifetime total, for the bank book).

bool IsWeekStart(int day, const EconomyDef& economy);
int NextWeekStart(int day, const EconomyDef& economy);

int SavingsInterest(int savings, int ratePercent);
// What you owe in total for borrowing `amount` at `ratePercent` (interest rounded up).
int LoanTotal(int amount, int ratePercent);
int WeeklyPayment(int total, int weeks);

bool Deposit(GameState& state, int amount);
bool Withdraw(GameState& state, int amount);
bool RepayLoan(GameState& state, int amount);
void TakeLoan(GameState& state, int amount, int ratePercent, int weeks);

// Start of each week: interest on savings, then the loan payment - or a late fee if there
// isn't enough money. Everything is written to the day log.
void ProcessWeek(GameState& state, const EconomyDef& economy);

}  // namespace game
