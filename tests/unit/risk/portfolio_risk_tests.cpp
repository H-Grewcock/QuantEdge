#include "quantedge/risk/portfolio_risk.hpp"

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

    pricing::MarketSnapshot market;
    market.spot = 100.0;
    market.volatility = 0.25;
    market.rate = 0.04;

    const std::vector<risk::OptionPosition> positions{
        {call, 5.0, "long call"},
        {put, -2.0, "short put"}
    };

    risk::PortfolioRiskAnalyzer analyzer;
    const auto scenarios = analyzer.default_scenarios();
    const risk::PortfolioRiskReport report = analyzer.analyze(positions, market, scenarios);

    if (report.scenarios.size() != scenarios.size()) {
        fail("Portfolio risk report did not include every scenario");
    }
    if (report.baseValue == 0.0) {
        fail("Portfolio base value should not be zero for this test book");
    }
    if (report.positions.size() != positions.size()) {
        fail("Portfolio risk report did not include position contributions");
    }
    if (report.totalVega == 0.0) {
        fail("Portfolio total Vega should not be zero for this test book");
    }
    if (report.minPnL > report.maxPnL) {
        fail("Portfolio risk min P&L was greater than max P&L");
    }
    if (report.valueAtRisk95 < 0.0 || report.expectedShortfall95 < 0.0) {
        fail("Portfolio risk loss metrics should be non-negative");
    }

    std::cout << "All QuantEdge portfolio risk tests passed.\n";
    return 0;
}
