#pragma once

#include <string>

namespace quantedge::market {

// Synthetic regimes used by the simulator.
//
// These are not predictions about real markets. They are controlled scenarios
// that let us test pricing and diagnostics under different kinds of stress.
enum class MarketRegime {
    // No deliberate trend; price moves mostly through random shocks.
    RandomWalk,

    // Positive drift, useful for testing persistence diagnostics.
    PersistentTrend,

    // Volatility rises during the run, stressing Vega and Monte Carlo error.
    VolatilityShock,

    // Positive drift accelerates and volatility rises. This is only a
    // "bubble-like" diagnostic scenario, not a claim that bubbles are predicted.
    BubbleLike,

    // Negative drift plus rising volatility, useful for drawdown diagnostics.
    Crash,

    // Price is nudged back toward a long-run level.
    MeanReverting
};

// Convert enum values to CLI/report-friendly labels.
inline std::string to_string(MarketRegime regime) {
    switch (regime) {
    case MarketRegime::RandomWalk:
        return "random_walk";
    case MarketRegime::PersistentTrend:
        return "persistent_trend";
    case MarketRegime::VolatilityShock:
        return "volatility_shock";
    case MarketRegime::BubbleLike:
        return "bubble_like";
    case MarketRegime::Crash:
        return "crash";
    case MarketRegime::MeanReverting:
        return "mean_reverting";
    }
    return "unknown";
}

// Parse a CLI string into a regime. Unknown input falls back to RandomWalk so
// the demo remains forgiving rather than failing on a typo.
inline MarketRegime parse_regime(const std::string& value) {
    if (value == "persistent_trend") {
        return MarketRegime::PersistentTrend;
    }
    if (value == "volatility_shock") {
        return MarketRegime::VolatilityShock;
    }
    if (value == "bubble_like") {
        return MarketRegime::BubbleLike;
    }
    if (value == "crash") {
        return MarketRegime::Crash;
    }
    if (value == "mean_reverting") {
        return MarketRegime::MeanReverting;
    }
    return MarketRegime::RandomWalk;
}

} // namespace quantedge::market
