#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace quantedge::market {

// MarketTick represents one incoming market-data event.
//
// It is richer than MarketSnapshot because real feeds contain microstructure
// details and data-quality flags. The pricing layer does not always need all
// of this, but the runtime engine and diagnostics eventually will.
struct MarketTick {
    // Event time. Nanoseconds are used so the type can support low-latency
    // feeds later, even though the simulator currently advances in milliseconds.
    std::chrono::nanoseconds timestamp{};

    // Symbol being updated.
    std::string symbol{"AAPL"};

    // Latest spot/last price.
    double spot{100.0};

    // Current annualised volatility assumption.
    double volatility{0.2};

    // Risk-free rate and dividend yield carried with the tick for easy demos.
    double rate{0.05};
    double dividendYield{0.0};

    // Simple bid/ask fields. Later these can support spread diagnostics.
    double bid{99.99};
    double ask{100.01};

    // Simulated traded volume.
    double volume{0.0};

    // Data-quality flags. These are important because a real-time pricing
    // engine should know when it is pricing from stale or incomplete data.
    bool isStale{false};
    bool missingPacket{false};

    // Monotonic tick number from the simulator.
    std::size_t sequence{0};
};

} // namespace quantedge::market
