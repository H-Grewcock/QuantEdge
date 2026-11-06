#include "quantedge/market/microstructure_simulator.hpp"
#include "quantedge/research/backtest_report.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}
}

int main() {
    using namespace quantedge;

    std::vector<market::MarketTick> ticks;
    for (std::size_t i = 1; i <= 80; ++i) {
        market::MarketTick tick;
        tick.sequence = i;
        tick.timestamp = std::chrono::milliseconds(i * 100);
        tick.spot = 100.0 + static_cast<double>(i) * 0.1;
        tick.volatility = 0.2 + static_cast<double>(i) * 0.0005;
        tick.rate = 0.04;
        tick.bid = tick.spot - 0.02;
        tick.ask = tick.spot + 0.02;
        tick.isStale = i % 31 == 0;
        ticks.push_back(tick);
    }

    market::MicrostructureAnalyzer micro;
    const auto micro_report = micro.analyze(ticks);
    if (micro_report.averageSpread <= 0.0 || micro_report.regime.empty()) {
        fail("Microstructure analyzer did not produce metrics");
    }

    research::BacktestReportBuilder builder;
    const auto backtest = builder.build(ticks, 16);
    if (backtest.points.empty() || backtest.maxLatencyMs <= 0.0) {
        fail("Backtest report did not produce a research trace");
    }

    std::cout << "All QuantEdge microstructure/backtest tests passed.\n";
    return 0;
}
