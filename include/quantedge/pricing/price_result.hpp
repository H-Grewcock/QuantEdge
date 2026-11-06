#pragma once

#include <string>

namespace quantedge::pricing {

// PriceResult is the common output shape for every pricing model.
//
// Some models can fill every field directly. Others may only fill a subset.
// For example, the MVP Monte Carlo pricer returns price and standard error,
// while Black-Scholes also returns analytic Greeks.
struct PriceResult {
    // Option value produced by the model.
    double price{0.0};

    // First derivative of option price with respect to spot price.
    // Roughly: how much the option price changes when the underlying moves.
    double delta{0.0};

    // Second derivative with respect to spot.
    // High Gamma means Delta changes quickly, often near-the-money and close
    // to expiry. This matters for "Gamma cliff" risk-surface alerts.
    double gamma{0.0};

    // Sensitivity to volatility. A Vega of 18 means a volatility move of 1.00
    // in decimal units changes price by about 18. In practice people often
    // scale this to a 1 percentage point volatility move.
    double vega{0.0};

    // Sensitivity to time passing. Convention varies; here it is the analytic
    // Black-Scholes theta per year.
    double theta{0.0};

    // Sensitivity to the interest rate.
    double rho{0.0};

    // Monte Carlo uncertainty estimate. Deterministic models leave this at 0.
    double standardError{0.0};

    // Human-readable model name for reports and CLI output.
    std::string method;
};

} // namespace quantedge::pricing
