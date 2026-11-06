#pragma once

#include <cmath>
#include <vector>

namespace quantedge::utils {

// Convert a price history into log returns.
//
// Log returns are useful because they add cleanly over time and match the
// modelling assumptions used by Black-Scholes and geometric Brownian motion:
//
// r_t = log(S_t / S_{t-1})
inline std::vector<double> log_returns(const std::vector<double>& prices) {
    std::vector<double> returns;
    if (prices.size() < 2) {
        return returns;
    }
    returns.reserve(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); ++i) {
        // Each return measures the percentage-like move from one price to the
        // next in continuously compounded form.
        returns.push_back(std::log(prices[i] / prices[i - 1]));
    }
    return returns;
}

} // namespace quantedge::utils
