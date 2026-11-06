#pragma once

#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/risk/portfolio_risk.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace quantedge::risk {

struct PnLExplainRow {
    std::string label;
    double fullRevaluationPnL{0.0};
    double deltaApproxPnL{0.0};
    double deltaGammaVegaApproxPnL{0.0};
    double unexplainedPnL{0.0};
};

struct PnLExplainReport {
    double fullRevaluationPnL{0.0};
    double deltaApproxPnL{0.0};
    double deltaGammaVegaApproxPnL{0.0};
    double unexplainedPnL{0.0};
    std::vector<PnLExplainRow> rows;
};

// Explains scenario P&L using first/second-order Greeks.
//
// Full revaluation reprices each option under the shocked market. The
// approximation uses:
//
// dV ~= Delta*dS + 0.5*Gamma*dS^2 + Vega*dVol
//
// The unexplained residual is the nonlinear/model remainder. In real risk
// systems this kind of explain helps users understand whether losses came from
// directional spot moves, curvature, volatility, or model residual.
class PnLExplainAnalyzer {
public:
    PnLExplainReport explain(
        const std::vector<OptionPosition>& positions,
        const pricing::MarketSnapshot& baseMarket,
        const MarketScenario& scenario
    ) const {
        pricing::MarketSnapshot shocked = baseMarket;
        shocked.spot = std::max(1.0e-12, baseMarket.spot * (1.0 + scenario.spotShock));
        shocked.volatility = std::max(1.0e-12, baseMarket.volatility + scenario.volatilityShock);

        PnLExplainReport report;
        pricing::BlackScholesPricer pricer;
        const double d_spot = shocked.spot - baseMarket.spot;
        const double d_vol = shocked.volatility - baseMarket.volatility;

        for (const auto& position : positions) {
            const pricing::PriceResult base = pricer.price(position.contract, baseMarket);
            const pricing::PriceResult shocked_result = pricer.price(position.contract, shocked);

            const double full = position.quantity * (shocked_result.price - base.price);
            const double delta = position.quantity * base.delta * d_spot;
            const double delta_gamma_vega = position.quantity
                * (base.delta * d_spot + 0.5 * base.gamma * d_spot * d_spot + base.vega * d_vol);
            const double residual = full - delta_gamma_vega;

            report.rows.push_back({
                position.label.empty() ? position.contract.underlying : position.label,
                full,
                delta,
                delta_gamma_vega,
                residual
            });

            report.fullRevaluationPnL += full;
            report.deltaApproxPnL += delta;
            report.deltaGammaVegaApproxPnL += delta_gamma_vega;
            report.unexplainedPnL += residual;
        }

        return report;
    }
};

} // namespace quantedge::risk
