#include "logic/ShopSim.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace game {

int SellResult::TotalSold() const {
    int total = 0;
    for (const auto& [item, qty] : sold) {
        total += qty;
    }
    return total;
}

int SellResult::ReputationChange() const {
    return std::clamp(fairBuys / 3 - tooExpensive / 4, -5, 5);
}

int CustomerCount(const SellParams& params) {
    const double perHour = 5.0 * (0.6 + params.reputation / 100.0) * params.efficiency *
                           (1.0 + params.customerBonus);
    return std::max(0, static_cast<int>(std::lround(perHour * params.hours)));
}

SellResult SimulateSales(const SellParams& params) {
    SellResult result;
    if (params.items.empty()) {
        return result;
    }

    std::mt19937 rng(params.seed);
    // Most people happily pay the usual value; a few pay up to 50% more, a few want a bargain.
    std::uniform_real_distribution<double> willingness(0.8, 1.5);
    std::bernoulli_distribution tryOther(0.5);

    std::vector<double> weights;
    std::vector<int> stock;
    for (const SaleItem& item : params.items) {
        weights.push_back(std::max(0.01, static_cast<double>(item.appeal)));
        stock.push_back(item.stock);
    }
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());

    const int count = CustomerCount(params);
    for (int i = 0; i < count; ++i) {
        std::size_t target = pick(rng);
        Customer customer;
        customer.wanted = params.items[target].item;

        if (stock[target] == 0) {
            std::vector<std::size_t> alternatives;
            for (std::size_t j = 0; j < stock.size(); ++j) {
                if (stock[j] > 0) {
                    alternatives.push_back(j);
                }
            }
            if (alternatives.empty() || !tryOther(rng)) {
                customer.outcome = Outcome::SoldOut;
                ++result.soldOut;
                result.customers.push_back(customer);
                continue;
            }
            std::uniform_int_distribution<std::size_t> any(0, alternatives.size() - 1);
            target = alternatives[any(rng)];
        }

        const SaleItem& item = params.items[target];
        const double maxPrice = item.value * willingness(rng);
        if (item.price <= maxPrice) {
            --stock[target];
            customer.outcome = Outcome::Bought;
            customer.bought = item.item;
            customer.paid = item.price;
            customer.fair = item.price <= item.value;
            result.sold[item.item] += 1;
            result.revenue += item.price;
            if (customer.fair) {
                ++result.fairBuys;
            }
        } else {
            customer.outcome = Outcome::TooExpensive;
            ++result.tooExpensive;
        }
        result.customers.push_back(customer);
    }
    return result;
}

}  // namespace game
