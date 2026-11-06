#pragma once

#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/option_contract.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace quantedge::pricing {

struct SmilePoint {
    double strike{0.0};
    double maturity{0.0};
    double impliedVolatility{0.0};
    double localVolatilityProxy{0.0};
};

struct SmileDiagnostics {
    std::vector<SmilePoint> points;
    double atmVolatility{0.0};
    double skewSlope{0.0};
    double curvature{0.0};
    double smileWidth{0.0};
    std::string regime;
};

// Simple volatility smile diagnostics.
//
// This is a stylised model, not a full Dupire local-vol implementation. It
// gives the project a derivatives-calibration flavour by creating a smile,
// measuring skew/curvature, and producing a local-volatility proxy.
class VolatilitySmileAnalyzer {
public:
    SmileDiagnostics analyze(
        const MarketSnapshot& market,
        const std::vector<double>& strikes,
        double maturity
    ) const {
        SmileDiagnostics diagnostics;
        diagnostics.atmVolatility = market.volatility;
        diagnostics.points.reserve(strikes.size());

        for (double strike : strikes) {
            const double log_moneyness = std::log(std::max(strike, 1.0e-12) / std::max(market.spot, 1.0e-12));
            const double skew = -0.18 * log_moneyness;
            const double convexity = 0.32 * log_moneyness * log_moneyness;
            const double term = 0.03 * std::sqrt(std::max(maturity, 1.0e-12));
            const double implied = std::max(0.01, market.volatility + skew + convexity + term);
            const double local_proxy = std::max(0.01, implied * (1.0 + 0.5 * std::abs(log_moneyness)));
            diagnostics.points.push_back({strike, maturity, implied, local_proxy});
        }

        if (!diagnostics.points.empty()) {
            const auto [min_it, max_it] = std::minmax_element(
                diagnostics.points.begin(),
                diagnostics.points.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.impliedVolatility < rhs.impliedVolatility;
                }
            );
            diagnostics.smileWidth = max_it->impliedVolatility - min_it->impliedVolatility;
            diagnostics.skewSlope =
                (diagnostics.points.back().impliedVolatility - diagnostics.points.front().impliedVolatility)
                / std::max(diagnostics.points.back().strike - diagnostics.points.front().strike, 1.0e-12);
            const std::size_t mid = diagnostics.points.size() / 2;
            diagnostics.curvature = diagnostics.points.front().impliedVolatility
                - 2.0 * diagnostics.points[mid].impliedVolatility
                + diagnostics.points.back().impliedVolatility;
        }

        if (diagnostics.smileWidth > 0.12) {
            diagnostics.regime = "wide / stressed smile";
        } else if (diagnostics.skewSlope < -0.001) {
            diagnostics.regime = "downside skew";
        } else {
            diagnostics.regime = "stable smile";
        }
        return diagnostics;
    }
};

} // namespace quantedge::pricing
