#pragma once

#include "quantedge/market/market_tick.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace quantedge::market {

struct MicrostructureMetrics {
    double averageSpread{0.0};
    double maxSpread{0.0};
    double averageSlippage{0.0};
    double liquidityStressScore{0.0};
    std::size_t staleTicks{0};
    std::size_t missingPackets{0};
    std::string regime;
};

// Estimates bid/ask and liquidity quality from ticks.
//
// This is intentionally feed-like: spreads widen when volatility rises, stale
// data and missing packets add stress, and slippage is approximated from spread
// plus volatility. It makes QuantEdge more quant-dev oriented.
class MicrostructureAnalyzer {
public:
    MicrostructureMetrics analyze(const std::vector<MarketTick>& ticks) const {
        MicrostructureMetrics metrics;
        if (ticks.empty()) {
            return metrics;
        }

        double spread_sum = 0.0;
        double slippage_sum = 0.0;
        for (const auto& tick : ticks) {
            const double spread = std::max(0.0, tick.ask - tick.bid);
            spread_sum += spread;
            metrics.maxSpread = std::max(metrics.maxSpread, spread);
            slippage_sum += 0.5 * spread + tick.spot * tick.volatility * 0.0001;
            metrics.staleTicks += tick.isStale ? 1 : 0;
            metrics.missingPackets += tick.missingPacket ? 1 : 0;
        }

        const double n = static_cast<double>(ticks.size());
        metrics.averageSpread = spread_sum / n;
        metrics.averageSlippage = slippage_sum / n;
        metrics.liquidityStressScore = std::clamp(
            metrics.averageSpread * 10.0
                + metrics.averageSlippage
                + static_cast<double>(metrics.staleTicks + metrics.missingPackets) / n,
            0.0,
            1.0
        );

        if (metrics.liquidityStressScore > 0.7) {
            metrics.regime = "liquidity stressed";
        } else if (metrics.liquidityStressScore > 0.35) {
            metrics.regime = "watch";
        } else {
            metrics.regime = "normal";
        }
        return metrics;
    }
};

} // namespace quantedge::market
