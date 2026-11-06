#include "quantedge/pricing/calibration.hpp"
#include "quantedge/pricing/volatility_smile.hpp"

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
    using namespace quantedge::pricing;

    MarketSnapshot market;
    market.spot = 100.0;
    market.volatility = 0.25;
    market.rate = 0.04;

    VolatilitySmileAnalyzer smile;
    const auto diagnostics = smile.analyze(market, {80, 90, 100, 110, 120}, 0.5);
    if (diagnostics.points.size() != 5 || diagnostics.smileWidth <= 0.0) {
        fail("Smile diagnostics did not produce a useful smile");
    }

    BlackScholesPricer pricer;
    OptionContract option;
    option.strike = 100.0;
    option.maturity = 0.5;
    const double price = pricer.price(option, market).price;
    CalibrationEngine calibration;
    const auto report = calibration.calibrate({{option, price, "ATM call"}}, market);
    if (report.rows.size() != 1 || report.meanAbsoluteError > 1.0e-4) {
        fail("Calibration did not recover the generated quote");
    }

    std::cout << "All QuantEdge smile/calibration tests passed.\n";
    return 0;
}
