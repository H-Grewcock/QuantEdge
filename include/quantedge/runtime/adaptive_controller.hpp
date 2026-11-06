#pragma once

#include "quantedge/fractal/fractal_analyzer.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/runtime/runtime_metrics.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace quantedge::runtime {

// Decision produced by the adaptive controller.
//
// The controller does not execute these actions directly yet. It states what
// the runtime should do next: reduce Monte Carlo paths, pause expensive surface
// jobs, prioritise Greeks, or mark prices as less reliable.
struct AdaptiveDecision {
    bool enabled{false};
    std::size_t monteCarloPaths{100000};
    bool pauseRiskSurfaces{false};
    bool increaseGreeksPriority{false};
    bool markPricesUnreliable{false};
    std::vector<std::string> reasons;
    std::vector<std::string> actions;
};

struct AdaptiveThresholds {
    double p95LatencyMs{5.0};
    std::size_t queueDepth{500};
    double elevatedVolatility{0.35};
    double highRoughness{0.66};
    double persistentHurst{0.65};
};

// Rule-based adaptive controller.
//
// This is intentionally transparent rather than clever. The project can later
// make these rules configurable, but the first version should be easy to read
// and defend in a demo.
class AdaptiveController {
public:
    AdaptiveDecision evaluate(
        const RuntimeMetrics& runtime,
        const fractal::FractalDiagnostics& diagnostics,
        const pricing::MarketSnapshot& market,
        bool marketDataStale,
        std::size_t currentMonteCarloPaths = 100000,
        AdaptiveThresholds thresholds = {}
    ) const {
        AdaptiveDecision decision;
        decision.monteCarloPaths = currentMonteCarloPaths;

        if (runtime.p95LatencyMs > thresholds.p95LatencyMs) {
            decision.enabled = true;
            decision.reasons.push_back("P95 latency exceeded threshold");
            decision.monteCarloPaths = std::min(decision.monteCarloPaths, currentMonteCarloPaths / 4);
            decision.actions.push_back("Monte Carlo paths reduced");
        }

        if (runtime.queueDepth > thresholds.queueDepth) {
            decision.enabled = true;
            decision.reasons.push_back("queue depth exceeded threshold");
            decision.pauseRiskSurfaces = true;
            decision.actions.push_back("low-priority risk-surface jobs paused");
        }

        if (market.volatility > thresholds.elevatedVolatility) {
            decision.enabled = true;
            decision.reasons.push_back("market volatility is elevated");
            decision.increaseGreeksPriority = true;
            decision.actions.push_back("Greeks recalculation priority increased");
        }

        if (diagnostics.volatilityRoughness > thresholds.highRoughness || diagnostics.pathRoughness > thresholds.highRoughness) {
            decision.enabled = true;
            decision.reasons.push_back("fractal roughness is high");
            decision.actions.push_back("model-disagreement surface requested");
        }

        if (diagnostics.hurstExponent > thresholds.persistentHurst) {
            decision.enabled = true;
            decision.reasons.push_back("Hurst exponent indicates persistence");
            decision.actions.push_back("persistent regime flagged");
        }

        if (marketDataStale) {
            decision.enabled = true;
            decision.reasons.push_back("market data is stale or incomplete");
            decision.markPricesUnreliable = true;
            decision.actions.push_back("pricing results marked as data-quality sensitive");
        }

        decision.monteCarloPaths = std::max<std::size_t>(decision.monteCarloPaths, 1000);
        if (!decision.enabled) {
            decision.actions.push_back("no adaptive action required");
        }
        return decision;
    }
};

} // namespace quantedge::runtime
