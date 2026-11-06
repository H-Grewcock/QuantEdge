#pragma once

#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/option_contract.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace quantedge::risk {

// One option holding in a small risk portfolio.
//
// quantity can be positive or negative:
// - positive means long the option
// - negative means short the option
struct OptionPosition {
    pricing::OptionContract contract;
    double quantity{1.0};
    std::string label;
};

// A deterministic market shock used for stress testing.
//
// spotShock is relative. Example: -0.05 means spot falls by 5 percent.
// volatilityShock is absolute. Example: +0.10 means volatility rises by 10
// percentage points, e.g. 20 percent to 30 percent.
struct MarketScenario {
    std::string name;
    double spotShock{0.0};
    double volatilityShock{0.0};
};

struct ScenarioPnL {
    std::string scenarioName;
    double pnl{0.0};
};

// Position-level risk contribution.
//
// This explains where the portfolio risk comes from. A portfolio-level Delta is
// useful, but a quant/risk user also wants to know which position contributes
// that exposure.
struct PositionRiskContribution {
    std::string label;
    double marketValue{0.0};
    double delta{0.0};
    double gamma{0.0};
    double vega{0.0};
};

// Portfolio-level risk summary.
//
// VaR and expected shortfall are reported as positive loss numbers. A 95 percent
// VaR of 120 means "losses exceeded 120 in the worst 5 percent of scenarios".
struct PortfolioRiskReport {
    double baseValue{0.0};
    double totalDelta{0.0};
    double totalGamma{0.0};
    double totalVega{0.0};
    double minPnL{0.0};
    double maxPnL{0.0};
    double meanPnL{0.0};
    double valueAtRisk95{0.0};
    double expectedShortfall95{0.0};
    std::vector<PositionRiskContribution> positions;
    std::vector<ScenarioPnL> scenarios;
};

// Scenario risk engine for option portfolios.
//
// This is a CV-friendly quant feature because it moves beyond pricing a single
// contract. It asks: if spot and volatility move together under stressed
// conditions, how does the whole option book behave?
class PortfolioRiskAnalyzer {
public:
    PortfolioRiskReport analyze(
        const std::vector<OptionPosition>& positions,
        const pricing::MarketSnapshot& market,
        const std::vector<MarketScenario>& scenarios
    ) const {
        PortfolioRiskReport report;
        report.baseValue = portfolio_value(positions, market);
        report.positions = position_contributions(positions, market);
        for (const auto& contribution : report.positions) {
            report.totalDelta += contribution.delta;
            report.totalGamma += contribution.gamma;
            report.totalVega += contribution.vega;
        }

        for (const auto& scenario : scenarios) {
            pricing::MarketSnapshot shocked = market;
            shocked.spot = std::max(1.0e-12, market.spot * (1.0 + scenario.spotShock));
            shocked.volatility = std::max(1.0e-12, market.volatility + scenario.volatilityShock);

            const double shocked_value = portfolio_value(positions, shocked);
            report.scenarios.push_back({scenario.name, shocked_value - report.baseValue});
        }

        if (report.scenarios.empty()) {
            return report;
        }

        std::vector<double> pnl;
        pnl.reserve(report.scenarios.size());
        for (const auto& scenario : report.scenarios) {
            pnl.push_back(scenario.pnl);
        }

        report.minPnL = *std::min_element(pnl.begin(), pnl.end());
        report.maxPnL = *std::max_element(pnl.begin(), pnl.end());
        report.meanPnL = std::accumulate(pnl.begin(), pnl.end(), 0.0) / static_cast<double>(pnl.size());

        std::sort(pnl.begin(), pnl.end());
        const std::size_t var_index = static_cast<std::size_t>(
            std::floor(0.05 * static_cast<double>(pnl.size() - 1))
        );
        const double var_pnl = pnl[var_index];
        report.valueAtRisk95 = std::max(0.0, -var_pnl);

        double tail_sum = 0.0;
        std::size_t tail_count = 0;
        for (std::size_t i = 0; i <= var_index; ++i) {
            tail_sum += pnl[i];
            ++tail_count;
        }
        report.expectedShortfall95 = tail_count == 0 ? 0.0 : std::max(0.0, -(tail_sum / static_cast<double>(tail_count)));
        return report;
    }

    std::vector<MarketScenario> default_scenarios() const {
        return {
            {"spot -10%, vol +10pp", -0.10, 0.10},
            {"spot -5%, vol +5pp", -0.05, 0.05},
            {"spot unchanged, vol +15pp", 0.0, 0.15},
            {"spot +5%, vol -3pp", 0.05, -0.03},
            {"spot +10%, vol -5pp", 0.10, -0.05},
            {"crash stress: spot -20%, vol +25pp", -0.20, 0.25},
            {"melt-up stress: spot +20%, vol +10pp", 0.20, 0.10}
        };
    }

private:
    double portfolio_value(const std::vector<OptionPosition>& positions, const pricing::MarketSnapshot& market) const {
        pricing::BlackScholesPricer pricer;
        double value = 0.0;
        for (const auto& position : positions) {
            value += position.quantity * pricer.price(position.contract, market).price;
        }
        return value;
    }

    std::vector<PositionRiskContribution> position_contributions(
        const std::vector<OptionPosition>& positions,
        const pricing::MarketSnapshot& market
    ) const {
        pricing::BlackScholesPricer pricer;
        std::vector<PositionRiskContribution> contributions;
        contributions.reserve(positions.size());
        for (const auto& position : positions) {
            const pricing::PriceResult result = pricer.price(position.contract, market);
            contributions.push_back({
                position.label.empty() ? position.contract.underlying : position.label,
                position.quantity * result.price,
                position.quantity * result.delta,
                position.quantity * result.gamma,
                position.quantity * result.vega
            });
        }
        return contributions;
    }
};

} // namespace quantedge::risk
