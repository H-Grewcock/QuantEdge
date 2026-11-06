#include "quantedge/runtime/adaptive_controller.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge;

    runtime::AdaptiveController controller;

    runtime::RuntimeMetrics calm_runtime;
    fractal::FractalDiagnostics calm_diagnostics;
    pricing::MarketSnapshot calm_market;
    calm_market.volatility = 0.2;

    const runtime::AdaptiveDecision calm =
        controller.evaluate(calm_runtime, calm_diagnostics, calm_market, false, 100000);
    if (calm.enabled) {
        fail("Adaptive controller enabled during calm conditions");
    }

    runtime::RuntimeMetrics stressed_runtime;
    stressed_runtime.p95LatencyMs = 8.0;
    stressed_runtime.queueDepth = 800;

    fractal::FractalDiagnostics stressed_diagnostics;
    stressed_diagnostics.hurstExponent = 0.7;
    stressed_diagnostics.volatilityRoughness = 0.8;

    pricing::MarketSnapshot stressed_market;
    stressed_market.volatility = 0.45;

    const runtime::AdaptiveDecision stressed =
        controller.evaluate(stressed_runtime, stressed_diagnostics, stressed_market, true, 100000);
    if (!stressed.enabled) {
        fail("Adaptive controller did not enable during stressed conditions");
    }
    if (stressed.monteCarloPaths != 25000) {
        fail("Adaptive controller did not reduce Monte Carlo paths correctly");
    }
    if (!stressed.pauseRiskSurfaces || !stressed.increaseGreeksPriority || !stressed.markPricesUnreliable) {
        fail("Adaptive controller did not set expected stressed actions");
    }

    std::cout << "All QuantEdge adaptive controller tests passed.\n";
    return 0;
}
