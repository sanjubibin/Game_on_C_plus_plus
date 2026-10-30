#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace game {

// One selling session at the pushcart, simulated customer by customer. Pure and deterministic
// for a given seed, so it is unit-testable and the scene can replay the result as animation.

struct SaleItem {
    std::string item;
    int price = 0;   // what the player charges
    int value = 0;   // what customers usually think it is worth
    int stock = 0;
    float appeal = 1.0f;
};

enum class Outcome { Bought, TooExpensive, SoldOut };

struct Customer {
    std::string wanted;
    std::string bought;  // empty unless Outcome::Bought
    Outcome outcome = Outcome::SoldOut;
    int paid = 0;
    bool fair = false;  // paid no more than the usual value
};

struct SellParams {
    std::vector<SaleItem> items;
    int reputation = 30;
    int hours = 3;
    float efficiency = 1.0f;     // < 1 when a less experienced companion runs the cart
    float customerBonus = 0.0f;  // e.g. 0.15 for a bright sign
    std::uint32_t seed = 1;
};

struct SellResult {
    std::vector<Customer> customers;
    std::map<std::string, int> sold;
    int revenue = 0;
    int fairBuys = 0;
    int tooExpensive = 0;
    int soldOut = 0;

    int TotalSold() const;
    // Fair sales build reputation; turning many people away for high prices costs a little.
    int ReputationChange() const;
};

int CustomerCount(const SellParams& params);
SellResult SimulateSales(const SellParams& params);

}  // namespace game
