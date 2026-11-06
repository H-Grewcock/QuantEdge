#pragma once

#include "quantedge/fractal/fractal_analyzer.hpp"
#include "quantedge/market/market_tick.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/option_contract.hpp"
#include "quantedge/risk/pnl_explain.hpp"
#include "quantedge/risk/portfolio_risk.hpp"
#include "quantedge/runtime/runtime_metrics.hpp"
#include "quantedge/utils/history.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace quantedge::research {

struct BacktestPoint {
    std::size_t tick{0};
    double spot{0.0};
    double hurst{0.5};
    double volatilityRoughness{0.0};
    double valueAtRisk95{0.0};
    double pnlExplainResidual{0.0};
    double latencyMs{0.0};
};

struct BacktestReport {
    std::vector<BacktestPoint> points;
    double maxVaR95{0.0};
    double maxResidual{0.0};
    double maxLatencyMs{0.0};
};

// Replay-style research report.
//
// It samples the tick history through time and records the quantities that make
// QuantEdge distinctive: fractal state, tail risk, explain residual, and
// runtime latency proxy.
class BacktestReportBuilder {
public:
    BacktestReport build(const std::vector<market::MarketTick>& ticks, std::size_t window = 64) const {
        BacktestReport report;
        if (ticks.size() < 4) {
            return report;
        }

        window = std::max<std::size_t>(window, 8);
        risk::PortfolioRiskAnalyzer portfolio_analyzer;
        risk::PnLExplainAnalyzer explain_analyzer;

        for (std::size_t end = std::min(window, ticks.size()); end <= ticks.size(); end += window / 2) {
            const std::size_t begin = end > window ? end - window : 0;
            std::vector<double> prices;
            prices.reserve(end - begin);
            for (std::size_t i = begin; i < end; ++i) {
                prices.push_back(ticks[i].spot);
            }
            const auto returns = utils::log_returns(prices);
            fractal::FractalAnalyzer fractal_analyzer;
            const auto diagnostics = fractal_analyzer.analyze(prices, returns);

            const auto& last = ticks[end - 1];
            pricing::MarketSnapshot market;
            market.spot = last.spot;
            market.volatility = last.volatility;
            market.rate = last.rate;
            market.dividendYield = last.dividendYield;

            pricing::OptionContract call;
            call.underlying = last.symbol;
            call.strike = last.spot;
            call.maturity = 0.5;
            call.type = pricing::OptionType::Call;
            pricing::OptionContract put = call;
            put.type = pricing::OptionType::Put;
            put.strike = last.spot * 0.95;
            const std::vector<risk::OptionPosition> positions{
                {call, 5.0, "rolling ATM call"},
                {put, -2.0, "rolling short put"}
            };
            const auto portfolio = portfolio_analyzer.analyze(positions, market, portfolio_analyzer.default_scenarios());
            const auto explain = explain_analyzer.explain(
                positions,
                market,
                {"rolling crash explain", -0.05, 0.05}
            );

            BacktestPoint point;
            point.tick = last.sequence;
            point.spot = last.spot;
            point.hurst = diagnostics.hurstExponent;
            point.volatilityRoughness = diagnostics.volatilityRoughness;
            point.valueAtRisk95 = portfolio.valueAtRisk95;
            point.pnlExplainResidual = explain.unexplainedPnL;
            point.latencyMs = 0.5 + diagnostics.volatilityRoughness * 4.0 + market.volatility;
            report.points.push_back(point);

            report.maxVaR95 = std::max(report.maxVaR95, point.valueAtRisk95);
            report.maxResidual = std::max(report.maxResidual, std::abs(point.pnlExplainResidual));
            report.maxLatencyMs = std::max(report.maxLatencyMs, point.latencyMs);
        }

        return report;
    }
};

} // namespace quantedge::research
