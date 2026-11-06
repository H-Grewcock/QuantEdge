#pragma once

#include "quantedge/pricing/pricer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace quantedge::pricing {

namespace detail {

// Same maturity payoff as the analytic pricer, repeated here so the Monte
// Carlo header can stand alone even if black_scholes.hpp is not included first.
inline double monte_carlo_payoff(OptionType type, double spot, double strike) {
    if (type == OptionType::Call) {
        return std::max(spot - strike, 0.0);
    }
    return std::max(strike - spot, 0.0);
}

} // namespace detail

// Monte Carlo European option pricer.
//
// This simulates many possible terminal prices S_T, computes the option payoff
// in each scenario, averages those payoffs, then discounts the average back to
// today. It is slower than Black-Scholes, but it generalises well to harder
// products and gives a standard-error estimate.
class MonteCarloPricer final : public IPricer {
public:
    // paths controls accuracy versus runtime. More paths usually reduce random
    // error, but they cost more CPU time. This is exactly the trade-off the
    // future adaptive runtime will control.
    explicit MonteCarloPricer(std::size_t paths = 100000, std::uint32_t seed = 42)
        : paths_(std::max<std::size_t>(paths, 1)), seed_(seed) {}

    PriceResult price(const OptionContract& option, const MarketSnapshot& market) const override {
        // Fixed seed gives reproducible tests and demos. A production engine
        // could inject a seed sequence or random source instead.
        std::mt19937_64 rng(seed_);
        std::normal_distribution<double> normal(0.0, 1.0);

        // Again, clamp positive quantities away from zero to keep calculations
        // numerically safe for demo inputs.
        const double s = std::max(market.spot, 1.0e-12);
        const double t = std::max(option.maturity, 1.0e-12);
        const double sigma = std::max(market.volatility, 1.0e-12);

        // Risk-neutral geometric Brownian motion terminal price:
        //
        // S_T = S_0 * exp((r - q - 0.5 sigma^2)T + sigma sqrt(T) Z)
        //
        // Z is a standard normal draw. The -0.5 sigma^2 term is the correction
        // that makes the expected discounted asset price consistent under the
        // risk-neutral measure.
        const double drift = (market.rate - market.dividendYield - 0.5 * sigma * sigma) * t;
        const double diffusion = sigma * std::sqrt(t);
        const double discount = std::exp(-market.rate * t);

        // Store payoffs so we can compute both the average and sample variance.
        // Later this can be optimised to a streaming mean/variance accumulator.
        std::vector<double> payoffs;
        payoffs.reserve(paths_);
        for (std::size_t i = 0; i < paths_; ++i) {
            // Simulate one possible terminal spot and evaluate the option payoff
            // in that world.
            const double terminal = s * std::exp(drift + diffusion * normal(rng));
            payoffs.push_back(detail::monte_carlo_payoff(option.type, terminal, option.strike));
        }

        // The undiscounted option value estimate is the average simulated
        // payoff. Discounting converts it to today's value.
        const double mean = std::accumulate(payoffs.begin(), payoffs.end(), 0.0) / static_cast<double>(paths_);

        // Sample variance of payoffs. This measures path-to-path uncertainty.
        double variance = 0.0;
        for (double value : payoffs) {
            const double diff = value - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(paths_ > 1 ? paths_ - 1 : 1);

        PriceResult result;
        result.price = discount * mean;

        // Standard error of the discounted mean payoff. This shrinks roughly
        // like 1 / sqrt(paths), which is why Monte Carlo can be expensive when
        // high precision is required.
        result.standardError = discount * std::sqrt(variance / static_cast<double>(paths_));
        result.method = "Monte Carlo";
        return result;
    }

private:
    std::size_t paths_;
    std::uint32_t seed_;
};

} // namespace quantedge::pricing
