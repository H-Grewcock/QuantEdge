#pragma once

#include "quantedge/pricing/pricer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace quantedge::pricing {

namespace detail {

// Standard normal probability density function.
//
// In Black-Scholes, the normal distribution appears because the model assumes
// log returns are normally distributed. The PDF is used in the Greeks,
// especially Gamma and Vega.
inline double normal_pdf(double x) {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * std::numbers::pi);
}

// Standard normal cumulative distribution function.
//
// This gives P(Z <= x) for a standard normal random variable Z. std::erfc is
// used because the C++ standard library does not provide a direct normal CDF.
inline double normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

// Option payoff at maturity.
//
// This helper is used by deterministic and stochastic pricing models:
// - Call payoff: max(S_T - K, 0)
// - Put payoff:  max(K - S_T, 0)
inline double payoff(OptionType type, double spot, double strike) {
    if (type == OptionType::Call) {
        return std::max(spot - strike, 0.0);
    }
    return std::max(strike - spot, 0.0);
}

} // namespace detail

// Analytic Black-Scholes pricer for European options.
//
// This is the fast baseline model. It is useful for two reasons:
// 1. It gives an exact closed-form answer under the Black-Scholes assumptions.
// 2. It gives analytic Greeks, which are essential for risk surfaces.
//
// The model assumptions are intentionally simple: lognormal spot dynamics,
// constant volatility, constant interest rate, continuous dividend yield, and
// European exercise.
class BlackScholesPricer final : public IPricer {
public:
    PriceResult price(const OptionContract& option, const MarketSnapshot& market) const override {
        // Clamp positive quantities away from zero. This avoids division by
        // zero and log(0) if a caller passes invalid or extreme demo values.
        const double s = std::max(market.spot, 1.0e-12);
        const double k = std::max(option.strike, 1.0e-12);
        const double t = std::max(option.maturity, 1.0e-12);
        const double sigma = std::max(market.volatility, 1.0e-12);
        const double r = market.rate;
        const double q = market.dividendYield;

        // d1 and d2 are the two central Black-Scholes terms.
        //
        // Informally:
        // - d1 appears in Delta, Gamma, and Vega.
        // - d2 appears in the discounted strike component of the price.
        //
        // They measure moneyness adjusted for time, volatility, rates, and
        // dividends.
        const double sqrt_t = std::sqrt(t);
        const double d1 = (std::log(s / k) + (r - q + 0.5 * sigma * sigma) * t) / (sigma * sqrt_t);
        const double d2 = d1 - sigma * sqrt_t;

        // Discount factors convert future values back to present value.
        // discount_r applies to the strike. discount_q applies to the stock
        // leg when the underlying pays a continuous dividend yield.
        const double discount_r = std::exp(-r * t);
        const double discount_q = std::exp(-q * t);

        PriceResult result;
        result.method = "Black-Scholes";

        if (option.type == OptionType::Call) {
            // European call:
            // S * exp(-qT) * N(d1) - K * exp(-rT) * N(d2)
            result.price = s * discount_q * detail::normal_cdf(d1) - k * discount_r * detail::normal_cdf(d2);

            // Delta is positive for calls because calls benefit from a higher
            // underlying price.
            result.delta = discount_q * detail::normal_cdf(d1);

            // Theta here is per year. It measures sensitivity to time passing.
            result.theta = (-s * discount_q * detail::normal_pdf(d1) * sigma / (2.0 * sqrt_t))
                - r * k * discount_r * detail::normal_cdf(d2)
                + q * s * discount_q * detail::normal_cdf(d1);

            // Rho is positive for calls in the basic model because higher rates
            // reduce the present value of the strike paid in the future.
            result.rho = k * t * discount_r * detail::normal_cdf(d2);
        } else {
            // European put:
            // K * exp(-rT) * N(-d2) - S * exp(-qT) * N(-d1)
            result.price = k * discount_r * detail::normal_cdf(-d2) - s * discount_q * detail::normal_cdf(-d1);

            // Put Delta is negative because puts usually lose value when spot
            // rises.
            result.delta = -discount_q * detail::normal_cdf(-d1);
            result.theta = (-s * discount_q * detail::normal_pdf(d1) * sigma / (2.0 * sqrt_t))
                + r * k * discount_r * detail::normal_cdf(-d2)
                - q * s * discount_q * detail::normal_cdf(-d1);
            result.rho = -k * t * discount_r * detail::normal_cdf(-d2);
        }

        // Gamma is the curvature of the option value with respect to spot.
        // It is shared by calls and puts in Black-Scholes.
        result.gamma = discount_q * detail::normal_pdf(d1) / (s * sigma * sqrt_t);

        // Vega is sensitivity to volatility. It is also shared by calls and
        // puts in Black-Scholes.
        result.vega = s * discount_q * detail::normal_pdf(d1) * sqrt_t;
        return result;
    }
};

} // namespace quantedge::pricing
