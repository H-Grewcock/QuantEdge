#pragma once

#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace quantedge::risk {

// One point on a two-dimensional risk grid.
//
// The grid axes are strike and maturity. The value can mean different things
// depending on which surface is being computed: Gamma, Vega, price, model
// disagreement, etc.
struct RiskGridPoint {
    double strike{0.0};
    double maturity{0.0};
    double value{0.0};
};

// Human-readable risk warning generated from a surface.
struct RiskAlert {
    std::string message;
    double strike{0.0};
    double maturity{0.0};
    double value{0.0};
};

// A risk surface is just a collection of grid points plus basic summary tools.
//
// For the MVP, we store points in a flat vector. Later, if we need image export
// or faster neighbourhood calculations, this can become a structured matrix.
class RiskSurface {
public:
    std::vector<RiskGridPoint> points;

    // Smallest value on the surface. Used for reports and colour scaling later.
    double minValue() const {
        if (points.empty()) {
            return 0.0;
        }
        return std::min_element(points.begin(), points.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.value < rhs.value;
        })->value;
    }

    // Largest value on the surface. Often this is where the first alert points.
    double maxValue() const {
        if (points.empty()) {
            return 0.0;
        }
        return std::max_element(points.begin(), points.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.value < rhs.value;
        })->value;
    }

    // Crude roughness metric: average jump between consecutive stored points.
    //
    // This is not a perfect geometric surface-gradient calculation yet, but it
    // is useful as a first "is this surface getting jagged?" diagnostic.
    double roughness() const {
        if (points.size() < 2) {
            return 0.0;
        }
        double total = 0.0;
        for (std::size_t i = 1; i < points.size(); ++i) {
            total += std::abs(points[i].value - points[i - 1].value);
        }
        return total / static_cast<double>(points.size() - 1);
    }
};

// Computes and inspects option risk surfaces.
//
// This is one of the project's distinctive pieces: instead of pricing one
// option in isolation, QuantEdge studies how risk changes across strike and
// maturity.
class RiskSurfaceAnalyzer {
public:
    // Build a surface whose value is Black-Scholes Gamma.
    //
    // High Gamma regions are important because they indicate rapidly changing
    // Delta. Near expiry, this can create sharp "cliffs" in risk exposure.
    RiskSurface computeGammaSurface(
        const std::vector<double>& strikes,
        const std::vector<double>& maturities,
        const pricing::MarketSnapshot& market
    ) const {
        pricing::BlackScholesPricer pricer;
        RiskSurface surface;
        for (double maturity : maturities) {
            for (double strike : strikes) {
                // Reuse the same market snapshot while varying contract terms.
                pricing::OptionContract option;
                option.strike = strike;
                option.maturity = maturity;
                surface.points.push_back({strike, maturity, pricer.price(option, market).gamma});
            }
        }
        return surface;
    }

    // Build a surface whose value is relative disagreement between two models.
    //
    // In the MVP this compares Black-Scholes and Monte Carlo. They should often
    // agree for European options, but disagreement can still appear from Monte
    // Carlo sampling error, high volatility, short maturity, or extreme strikes.
    RiskSurface computeModelDisagreementSurface(
        const std::vector<double>& strikes,
        const std::vector<double>& maturities,
        const pricing::MarketSnapshot& market
    ) const {
        pricing::BlackScholesPricer black_scholes;
        pricing::MonteCarloPricer monte_carlo(20000, 99);
        RiskSurface surface;
        for (double maturity : maturities) {
            for (double strike : strikes) {
                pricing::OptionContract option;
                option.strike = strike;
                option.maturity = maturity;
                const double bs = black_scholes.price(option, market).price;
                const double mc = monte_carlo.price(option, market).price;

                // Relative difference is easier to compare across options with
                // different absolute prices.
                surface.points.push_back({strike, maturity, std::abs(bs - mc) / std::max(bs, 1.0e-12)});
            }
        }
        return surface;
    }

    // Flag grid points close to the highest Gamma on the surface.
    //
    // This is a first-pass alert rule. A richer version would examine local
    // gradients, neighbouring cells, and persistence through time.
    std::vector<RiskAlert> detectRiskCliffs(const RiskSurface& surface) const {
        std::vector<RiskAlert> alerts;
        const double high = surface.maxValue();
        for (const auto& point : surface.points) {
            // Require both "near the surface maximum" and an absolute Gamma
            // threshold so tiny surfaces do not create meaningless alerts.
            if (point.value >= high * 0.9 && point.value > 0.03) {
                alerts.push_back({"short-maturity Gamma cliff detected", point.strike, point.maturity, point.value});
            }
        }
        return alerts;
    }
};

} // namespace quantedge::risk
