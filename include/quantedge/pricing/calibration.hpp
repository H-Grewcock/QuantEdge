#pragma once

#include "quantedge/pricing/implied_volatility.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/option_contract.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace quantedge::pricing {

struct CalibrationQuote {
    OptionContract contract;
    double marketPrice{0.0};
    std::string label;
};

struct CalibrationRow {
    std::string label;
    double strike{0.0};
    double maturity{0.0};
    double marketPrice{0.0};
    double impliedVolatility{0.0};
    double modelPrice{0.0};
    double absoluteError{0.0};
};

struct CalibrationReport {
    std::vector<CalibrationRow> rows;
    double meanAbsoluteError{0.0};
    double rootMeanSquaredError{0.0};
    double averageImpliedVolatility{0.0};
};

// Fits implied volatility quote-by-quote and reports calibration error.
class CalibrationEngine {
public:
    CalibrationReport calibrate(const std::vector<CalibrationQuote>& quotes, const MarketSnapshot& market) const {
        CalibrationReport report;
        ImpliedVolatilitySolver solver;
        double abs_sum = 0.0;
        double squared_sum = 0.0;
        double iv_sum = 0.0;

        for (const auto& quote : quotes) {
            const ImpliedVolatilityResult implied = solver.solve(quote.contract, market, quote.marketPrice);
            report.rows.push_back({
                quote.label,
                quote.contract.strike,
                quote.contract.maturity,
                quote.marketPrice,
                implied.volatility,
                implied.modelPrice,
                implied.absoluteError
            });
            abs_sum += implied.absoluteError;
            squared_sum += implied.absoluteError * implied.absoluteError;
            iv_sum += implied.volatility;
        }

        if (!report.rows.empty()) {
            const double n = static_cast<double>(report.rows.size());
            report.meanAbsoluteError = abs_sum / n;
            report.rootMeanSquaredError = std::sqrt(squared_sum / n);
            report.averageImpliedVolatility = iv_sum / n;
        }
        return report;
    }
};

} // namespace quantedge::pricing
