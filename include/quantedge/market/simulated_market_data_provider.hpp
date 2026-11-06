#pragma once

#include "quantedge/market/market_data_provider.hpp"
#include "quantedge/market/market_regime.hpp"
#include "quantedge/market/market_simulator.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace quantedge::market {

// Adapter that exposes MarketSimulator through IMarketDataProvider.
//
// This looks slightly ceremonial now, but it means the CLI and future runtime
// engine can consume simulated and replayed data through the same shape.
class SimulatedMarketDataProvider final : public IMarketDataProvider {
public:
    SimulatedMarketDataProvider(
        std::string symbol,
        MarketRegime regime,
        double initialSpot = 100.0,
        std::uint32_t seed = 7
    )
        : symbol_(std::move(symbol)),
          regime_(regime),
          initialSpot_(initialSpot),
          seed_(seed),
          simulator_(symbol_, regime_, initialSpot_, seed_) {}

    void subscribe(const std::string& symbol) override {
        symbol_ = symbol;
        simulator_ = MarketSimulator(symbol_, regime_, initialSpot_, seed_);
    }

    std::optional<MarketTick> nextTick() override {
        return simulator_.next();
    }

private:
    std::string symbol_;
    MarketRegime regime_;
    double initialSpot_;
    std::uint32_t seed_;
    MarketSimulator simulator_;
};

} // namespace quantedge::market
