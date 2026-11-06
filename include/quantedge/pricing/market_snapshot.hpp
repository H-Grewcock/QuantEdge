#pragma once

namespace quantedge::pricing {

// MarketSnapshot captures the market inputs needed by the pricing models at
// one point in time. A stream of MarketTick objects is eventually reduced into
// this smaller pricing view.
struct MarketSnapshot {
    // Current underlying price S.
    double spot{100.0};

    // Annualised volatility sigma, expressed as a decimal.
    // Example: 0.2 means 20 percent annualised volatility.
    double volatility{0.2};

    // Continuously-compounded risk-free interest rate r.
    double rate{0.05};

    // Continuous dividend yield q. This is useful for equity options.
    double dividendYield{0.0};
};

} // namespace quantedge::pricing
