#pragma once

#include "quantedge/market/market_tick.hpp"

#include <optional>
#include <string>

namespace quantedge::market {

// Common interface for anything that can provide market ticks.
//
// The rest of the engine should not care whether ticks come from:
// - the built-in simulator
// - a CSV replay file
// - a future live market-data API
//
// This interface is the first step toward that provider architecture.
class IMarketDataProvider {
public:
    // Start listening to or selecting a symbol.
    //
    // CSV replay may ignore this if the file already contains a symbol, but a
    // live provider would use it to subscribe to the correct market feed.
    virtual void subscribe(const std::string& symbol) = 0;

    // Return the next tick if one is available.
    //
    // std::nullopt means the provider has no tick ready, or the replay file has
    // reached the end.
    virtual std::optional<MarketTick> nextTick() = 0;

    virtual ~IMarketDataProvider() = default;
};

} // namespace quantedge::market
