#include "quantedge/risk/pnl_explain.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge;

    pricing::OptionContract call;
    call.strike = 100.0;
    call.maturity = 0.5;
    call.type = pricing::OptionType::Call;

    pricing::OptionContract put = call;
    put.type = pricing::OptionType::Put;
    put.strike = 95.0;

    const std::vector<risk::OptionPosition> positions{
        {call, 3.0, "long call"},
        {put, -1.0, "short put"}
    };

    pricing::MarketSnapshot market;
    market.spot = 100.0;
    market.volatility = 0.25;
    market.rate = 0.04;

    risk::PnLExplainAnalyzer analyzer;
    const risk::MarketScenario scenario{"spot down, vol up", -0.05, 0.05};
    const risk::PnLExplainReport report = analyzer.explain(positions, market, scenario);

    if (report.rows.size() != positions.size()) {
        fail("P&L explain did not include every position");
    }
    double row_total = 0.0;
    for (const auto& row : report.rows) {
        row_total += row.fullRevaluationPnL;
    }
    if (std::abs(row_total - report.fullRevaluationPnL) > 1.0e-9) {
        fail("P&L explain totals did not match row sums");
    }

    std::cout << "All QuantEdge P&L explain tests passed.\n";
    return 0;
}
