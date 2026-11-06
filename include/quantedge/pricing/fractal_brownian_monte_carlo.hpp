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

inline double fractal_monte_carlo_payoff(OptionType type, double spot, double strike) {
    if (type == OptionType::Call) {
        return std::max(spot - strike, 0.0);
    }
    return std::max(strike - spot, 0.0);
}

} // namespace detail

// Monte Carlo pricer using a lightweight fractional Brownian/trading-time model.
//
// The PDF motivates three related ideas:
// - Brownian motion is self-affine and gives independent increments.
// - Fractional Brownian motion keeps self-affinity but correlates increments.
// - Multifractal trading time stretches active periods and compresses quiet ones.
//
// This class keeps the implementation compact and auditable by approximating
// fractional increments with an AR(1) Gaussian shock and approximating trading
// time with a normalised lognormal activity clock. Setting hurst=0.5 and
// trading_time_clustering=0 recovers standard geometric Brownian dynamics.
class FractalBrownianMonteCarloPricer final : public IPricer {
public:
    FractalBrownianMonteCarloPricer(
        std::size_t paths = 50000,
        std::size_t steps = 64,
        double hurst = 0.5,
        double trading_time_clustering = 0.0,
        std::uint32_t seed = 91
    )
        : paths_(std::max<std::size_t>(paths, 1)),
          steps_(std::max<std::size_t>(steps, 1)),
          hurst_(std::clamp(hurst, 0.05, 0.95)),
          trading_time_clustering_(std::clamp(trading_time_clustering, 0.0, 1.0)),
          seed_(seed) {}

    PriceResult price(const OptionContract& option, const MarketSnapshot& market) const override {
        std::mt19937_64 rng(seed_);
        std::normal_distribution<double> normal(0.0, 1.0);

        const double s = std::max(market.spot, 1.0e-12);
        const double t = std::max(option.maturity, 1.0e-12);
        const double sigma = std::max(market.volatility, 1.0e-12);
        const double dt = t / static_cast<double>(steps_);
        const double discount = std::exp(-market.rate * t);

        // H > 0.5 gives persistent increments; H < 0.5 gives anti-persistence.
        const double phi = std::clamp(2.0 * hurst_ - 1.0, -0.85, 0.85);
        const double innovation_scale = std::sqrt(std::max(1.0 - phi * phi, 1.0e-12));

        std::vector<double> payoffs;
        payoffs.reserve(paths_);
        for (std::size_t path = 0; path < paths_; ++path) {
            double spot = s;
            double previous_shock = 0.0;

            for (std::size_t step = 0; step < steps_; ++step) {
                const double gaussian = normal(rng);
                const double shock = phi * previous_shock + innovation_scale * gaussian;
                previous_shock = shock;

                // Normalised lognormal activity increment. Mean is roughly one,
                // so total expected clock time still tracks calendar time.
                const double activity_noise = normal(rng);
                const double clustering = trading_time_clustering_;
                const double activity = std::exp(clustering * activity_noise - 0.5 * clustering * clustering);
                const double trading_dt = dt * std::clamp(activity, 0.15, 6.0);

                const double drift = (market.rate - market.dividendYield - 0.5 * sigma * sigma) * trading_dt;
                const double diffusion = sigma * std::pow(std::max(trading_dt, 1.0e-12), hurst_) * shock;
                spot = std::max(1.0e-12, spot * std::exp(drift + diffusion));
            }

            payoffs.push_back(detail::fractal_monte_carlo_payoff(option.type, spot, option.strike));
        }

        const double mean = std::accumulate(payoffs.begin(), payoffs.end(), 0.0) / static_cast<double>(paths_);
        double variance = 0.0;
        for (double value : payoffs) {
            const double diff = value - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(paths_ > 1 ? paths_ - 1 : 1);

        PriceResult result;
        result.price = discount * mean;
        result.standardError = discount * std::sqrt(variance / static_cast<double>(paths_));
        result.method = "Fractal Brownian Monte Carlo";
        return result;
    }

private:
    std::size_t paths_;
    std::size_t steps_;
    double hurst_;
    double trading_time_clustering_;
    std::uint32_t seed_;
};

} // namespace quantedge::pricing
