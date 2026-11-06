#pragma once

#include "quantedge/market/market_regime.hpp"
#include "quantedge/market/market_tick.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>

namespace quantedge::market {

// MarketSimulator creates deterministic synthetic market data.
//
// The aim is not to perfectly model markets. The aim is to create controlled
// regimes so the rest of QuantEdge can be exercised:
// - pricing responds to spot and volatility changes
// - fractal diagnostics see different path behaviours
// - risk surfaces change under stress
// - future runtime/adaptive components have repeatable workloads
class MarketSimulator {
public:
    // symbol: label placed on every tick
    // regime: scenario to simulate
    // initialSpot: starting price
    // seed: fixed random seed for reproducible demos and tests
    MarketSimulator(std::string symbol, MarketRegime regime, double initialSpot = 100.0, std::uint32_t seed = 7)
        : symbol_(std::move(symbol)), regime_(regime), spot_(initialSpot), rng_(seed) {}

    // Produce the next synthetic tick.
    //
    // Each call advances the internal sequence number, updates spot and
    // volatility according to the selected regime, then packages those values
    // into a MarketTick.
    MarketTick next() {
        // Standard normal shocks drive the random component of returns.
        std::normal_distribution<double> normal(0.0, 1.0);
        ++sequence_;

        // drift is the deterministic part of the log return.
        // shock_scale is the random part's standard deviation.
        //
        // We start with a small positive drift and annualised volatility scaled
        // down to one simulated step. 252 is the usual approximate number of
        // trading days in a year.
        double drift = 0.0001;
        double shock_scale = volatility_ / std::sqrt(252.0);
        double hurst = 0.5;
        double activity_clustering = 0.0;

        switch (regime_) {
        case MarketRegime::PersistentTrend:
            // Stronger positive drift creates a trending path.
            drift = 0.0008;
            hurst = 0.68;
            break;
        case MarketRegime::VolatilityShock:
            // After an initial calm period, volatility gradually rises.
            volatility_ = sequence_ > 50 ? std::min(0.65, volatility_ * 1.004) : volatility_;

            // A smaller denominator means larger per-step shocks.
            shock_scale = volatility_ / std::sqrt(180.0);
            activity_clustering = sequence_ > 50 ? 0.75 : 0.20;
            break;
        case MarketRegime::BubbleLike:
            // Drift accelerates over time and volatility creeps upward.
            // This gives the diagnostics a persistent, unstable-looking path.
            drift = 0.0005 + std::min(0.003, static_cast<double>(sequence_) * 0.000003);
            volatility_ = std::min(0.55, volatility_ + 0.00035);
            hurst = 0.72;
            activity_clustering = 0.45;
            break;
        case MarketRegime::Crash:
            // The crash phase starts after a short lead-in, then pushes returns
            // negative while volatility rises.
            drift = sequence_ > 40 ? -0.0025 : -0.0001;
            volatility_ = std::min(0.7, volatility_ + 0.0005);
            hurst = 0.62;
            activity_clustering = 0.85;
            break;
        case MarketRegime::MeanReverting:
            // Pulls the price gently back toward 100
            drift = (100.0 - spot_) * 0.00008;
            hurst = 0.34;
            break;
        case MarketRegime::RandomWalk:
            // Keep the default drift and volatility.
            break;
        }

        const double phi = std::clamp(2.0 * hurst - 1.0, -0.85, 0.85);
        const double innovation_scale = std::sqrt(std::max(1.0 - phi * phi, 1.0e-12));
        previousShock_ = phi * previousShock_ + innovation_scale * normal(rng_);

        const double activity_noise = normal(rng_);
        const double activity = std::exp(activity_clustering * activity_noise
            - 0.5 * activity_clustering * activity_clustering);
        const double trading_time_scale = std::clamp(activity, 0.20, 5.0);

        // Uses log-return dynamics so prices stay positive:
        // S_next = S_now * exp(return).
        const double return_value = drift * trading_time_scale
            + shock_scale * std::sqrt(trading_time_scale) * previousShock_;
        spot_ = std::max(1.0, spot_ * std::exp(return_value));

        // Build a tick. The simulator keeps fields simple, but the structure is
        // intentionally close to what a real feed adapter might produce.
        MarketTick tick;
        tick.timestamp = std::chrono::milliseconds(sequence_ * 100);
        tick.symbol = symbol_;
        tick.spot = spot_;
        tick.volatility = volatility_;
        tick.rate = 0.048;
        tick.dividendYield = 0.0;

        // A fixed one-cent spread keeps the demo readable.
        tick.bid = spot_ - 0.01;
        tick.ask = spot_ + 0.01;

        // Volume is noisy but always positive.
        tick.volume = 1000.0 + 100.0 * std::abs(normal(rng_));

        // Periodic data-quality flags allow future runtime logic to demonstrate
        // stale/missing data handling without needing a real broken feed.
        tick.isStale = sequence_ % 137 == 0;
        tick.missingPacket = sequence_ % 181 == 0;
        tick.sequence = sequence_;
        return tick;
    }

private:
    // Internal simulator state. Each call to next() mutates these values.
    std::string symbol_;
    MarketRegime regime_;
    double spot_;
    double volatility_{0.22};
    double previousShock_{0.0};
    std::mt19937_64 rng_;
    std::size_t sequence_{0};
};

} // namespace quantedge::market
