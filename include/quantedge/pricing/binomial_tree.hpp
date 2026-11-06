#pragma once

#include "quantedge/pricing/pricer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace quantedge::pricing {

namespace detail {

inline double binomial_payoff(OptionType type, double spot, double strike) {
    if (type == OptionType::Call) {
        return std::max(spot - strike, 0.0);
    }
    return std::max(strike - spot, 0.0);
}

} // namespace detail

// Cox-Ross-Rubinstein binomial tree pricer.
//
// The binomial model approximates the underlying price path as a recombining
// tree. At each time step the asset either moves up by factor u or down by
// factor d. The option value is first known at maturity, then worked backward
// through the tree by risk-neutral expectation.
//
// Why this matters for QuantEdge:
// - It gives a second deterministic model to compare with Black-Scholes.
// - It can handle American exercise by checking early exercise at each node.
// - It is intuitive and easy to test for convergence as the step count rises.
class BinomialTreePricer final : public IPricer {
public:
    explicit BinomialTreePricer(std::size_t steps = 300)
        : steps_(std::max<std::size_t>(steps, 1)) {}

    PriceResult price(const OptionContract& option, const MarketSnapshot& market) const override {
        // Clamp inputs that must be positive. This mirrors the other pricers
        // and keeps the tree numerically safe for rough CLI experiments.
        const double s = std::max(market.spot, 1.0e-12);
        const double k = std::max(option.strike, 1.0e-12);
        const double t = std::max(option.maturity, 1.0e-12);
        const double sigma = std::max(market.volatility, 1.0e-12);
        const double r = market.rate;
        const double q = market.dividendYield;

        // dt is the length of one tree time step in years.
        const double dt = t / static_cast<double>(steps_);

        // Cox-Ross-Rubinstein up/down factors. These make the tree volatility
        // match the requested annualised volatility.
        const double up = std::exp(sigma * std::sqrt(dt));
        const double down = 1.0 / up;

        // Per-step discounting and dividend-adjusted risk-neutral probability.
        //
        // The probability is not a real-world probability. It is the value that
        // makes discounted expected asset prices match no-arbitrage pricing.
        const double growth = std::exp((r - q) * dt);
        const double probability = std::clamp((growth - down) / (up - down), 0.0, 1.0);
        const double discount = std::exp(-r * dt);

        // Terminal option values at maturity.
        //
        // Node j at final step has j up moves and steps_-j down moves.
        std::vector<double> values(steps_ + 1);
        for (std::size_t j = 0; j <= steps_; ++j) {
            const double terminal_spot = s * std::pow(up, static_cast<double>(j))
                * std::pow(down, static_cast<double>(steps_ - j));
            values[j] = detail::binomial_payoff(option.type, terminal_spot, k);
        }

        // Work backward from maturity to today.
        //
        // For European options, each node is just discounted expected future
        // value. For American options, compare that continuation value with the
        // immediate exercise payoff and keep the larger value.
        for (std::size_t step = steps_; step > 0; --step) {
            for (std::size_t j = 0; j < step; ++j) {
                const double continuation = discount
                    * (probability * values[j + 1] + (1.0 - probability) * values[j]);

                if (option.exerciseStyle == ExerciseStyle::American) {
                    const double node_spot = s * std::pow(up, static_cast<double>(j))
                        * std::pow(down, static_cast<double>((step - 1) - j));
                    values[j] = std::max(continuation, detail::binomial_payoff(option.type, node_spot, k));
                } else {
                    values[j] = continuation;
                }
            }
        }

        PriceResult result;
        result.price = values.front();
        result.method = option.exerciseStyle == ExerciseStyle::American
            ? "Binomial Tree (American)"
            : "Binomial Tree (European)";

        // Estimate Greeks by finite differences. The tree itself prices the
        // option; these bumps reuse the same tree method at nearby inputs.
        //
        // This is slower than analytic Greeks but model-agnostic and clear.
        result.delta = finite_delta(option, market);
        result.gamma = finite_gamma(option, market);
        result.vega = finite_vega(option, market);
        return result;
    }

private:
    double price_only(const OptionContract& option, const MarketSnapshot& market) const {
        // Small internal helper used by finite-difference Greeks. It calls the
        // public price path and returns only the value. The public method does
        // not call this helper to avoid hiding the main algorithm from readers.
        const double s = std::max(market.spot, 1.0e-12);
        const double k = std::max(option.strike, 1.0e-12);
        const double t = std::max(option.maturity, 1.0e-12);
        const double sigma = std::max(market.volatility, 1.0e-12);
        const double r = market.rate;
        const double q = market.dividendYield;
        const double dt = t / static_cast<double>(steps_);
        const double up = std::exp(sigma * std::sqrt(dt));
        const double down = 1.0 / up;
        const double growth = std::exp((r - q) * dt);
        const double probability = std::clamp((growth - down) / (up - down), 0.0, 1.0);
        const double discount = std::exp(-r * dt);

        std::vector<double> values(steps_ + 1);
        for (std::size_t j = 0; j <= steps_; ++j) {
            const double terminal_spot = s * std::pow(up, static_cast<double>(j))
                * std::pow(down, static_cast<double>(steps_ - j));
            values[j] = detail::binomial_payoff(option.type, terminal_spot, k);
        }

        for (std::size_t step = steps_; step > 0; --step) {
            for (std::size_t j = 0; j < step; ++j) {
                const double continuation = discount
                    * (probability * values[j + 1] + (1.0 - probability) * values[j]);
                if (option.exerciseStyle == ExerciseStyle::American) {
                    const double node_spot = s * std::pow(up, static_cast<double>(j))
                        * std::pow(down, static_cast<double>((step - 1) - j));
                    values[j] = std::max(continuation, detail::binomial_payoff(option.type, node_spot, k));
                } else {
                    values[j] = continuation;
                }
            }
        }
        return values.front();
    }

    double finite_delta(const OptionContract& option, const MarketSnapshot& market) const {
        const double bump = std::max(market.spot * 0.001, 0.01);
        MarketSnapshot up = market;
        MarketSnapshot down = market;
        up.spot += bump;
        down.spot = std::max(1.0e-12, down.spot - bump);
        return (price_only(option, up) - price_only(option, down)) / (up.spot - down.spot);
    }

    double finite_gamma(const OptionContract& option, const MarketSnapshot& market) const {
        const double bump = std::max(market.spot * 0.001, 0.01);
        MarketSnapshot up = market;
        MarketSnapshot down = market;
        up.spot += bump;
        down.spot = std::max(1.0e-12, down.spot - bump);
        const double up_price = price_only(option, up);
        const double mid_price = price_only(option, market);
        const double down_price = price_only(option, down);
        return (up_price - 2.0 * mid_price + down_price) / (bump * bump);
    }

    double finite_vega(const OptionContract& option, const MarketSnapshot& market) const {
        const double bump = 0.0001;
        MarketSnapshot up = market;
        MarketSnapshot down = market;
        up.volatility += bump;
        down.volatility = std::max(1.0e-12, down.volatility - bump);
        return (price_only(option, up) - price_only(option, down)) / (up.volatility - down.volatility);
    }

    std::size_t steps_;
};

} // namespace quantedge::pricing
