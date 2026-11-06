#pragma once

#include "quantedge/pricing/black_scholes.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace quantedge::pricing {

// Result returned by the implied-volatility solver.
//
// Implied volatility is the volatility input that makes a model price match an
// observed market price. It is not directly observed; it is backed out by
// solving an inverse pricing problem.
struct ImpliedVolatilityResult {
    double volatility{0.0};
    double modelPrice{0.0};
    double absoluteError{0.0};
    std::size_t iterations{0};
    bool converged{false};
};

// Black-Scholes implied-volatility solver.
//
// This uses bisection instead of Newton-Raphson. Bisection is not the fastest
// method, but it is very robust and easy to explain: repeatedly cut the search
// interval in half until the model price is close to the target price.
class ImpliedVolatilitySolver {
public:
    ImpliedVolatilityResult solve(
        const OptionContract& option,
        const MarketSnapshot& market,
        double targetPrice,
        double tolerance = 1.0e-6,
        std::size_t maxIterations = 100
    ) const {
        BlackScholesPricer pricer;

        // Search over a broad volatility range: 0.01 percent to 500 percent.
        // The upper bound is intentionally generous for stressed simulations.
        double low = 0.0001;
        double high = 5.0;

        MarketSnapshot low_market = market;
        MarketSnapshot high_market = market;
        low_market.volatility = low;
        high_market.volatility = high;

        const double low_price = pricer.price(option, low_market).price;
        const double high_price = pricer.price(option, high_market).price;

        // If the target price is outside the prices produced by the search
        // interval, there is no sensible implied vol inside this bracket.
        if (targetPrice < low_price || targetPrice > high_price) {
            ImpliedVolatilityResult result;
            result.volatility = targetPrice < low_price ? low : high;
            result.modelPrice = targetPrice < low_price ? low_price : high_price;
            result.absoluteError = std::abs(result.modelPrice - targetPrice);
            result.converged = false;
            return result;
        }

        ImpliedVolatilityResult result;
        for (std::size_t iteration = 1; iteration <= maxIterations; ++iteration) {
            const double mid = 0.5 * (low + high);
            MarketSnapshot mid_market = market;
            mid_market.volatility = mid;
            const double mid_price = pricer.price(option, mid_market).price;
            const double error = mid_price - targetPrice;

            result.volatility = mid;
            result.modelPrice = mid_price;
            result.absoluteError = std::abs(error);
            result.iterations = iteration;

            if (result.absoluteError <= tolerance) {
                result.converged = true;
                return result;
            }

            // For vanilla options, Black-Scholes price is increasing in
            // volatility, so this tells us which half of the bracket to keep.
            if (mid_price < targetPrice) {
                low = mid;
            } else {
                high = mid;
            }
        }

        result.converged = false;
        return result;
    }
};

} // namespace quantedge::pricing
