#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/binomial_tree.hpp"
#include "quantedge/pricing/fractal_brownian_monte_carlo.hpp"
#include "quantedge/pricing/implied_volatility.hpp"
#include "quantedge/pricing/monte_carlo.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>

namespace {

// Minimal assertion helper so tests do not need GoogleTest/Catch2 yet.
//
// If actual is outside the expected tolerance, the test executable exits with
// a non-zero status, which CTest treats as failure.
void expect_near(double actual, double expected, double tolerance, const std::string& label) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << label << " failed: expected " << expected << ", got " << actual << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    using namespace quantedge::pricing;

    // Standard textbook-ish market setup for sanity checks.
    BlackScholesPricer pricer;
    MarketSnapshot market;
    market.spot = 100.0;
    market.volatility = 0.2;
    market.rate = 0.05;
    market.dividendYield = 0.0;

    OptionContract call;
    call.strike = 100.0;
    call.maturity = 1.0;
    call.type = OptionType::Call;

    // Same contract terms, opposite option direction.
    OptionContract put = call;
    put.type = OptionType::Put;

    const PriceResult call_result = pricer.price(call, market);
    const PriceResult put_result = pricer.price(put, market);

    expect_near(call_result.price, 10.4506, 0.001, "Black-Scholes call");
    expect_near(put_result.price, 5.5735, 0.001, "Black-Scholes put");

    // Put-call parity is one of the most important consistency checks for
    // European option pricing:
    //
    // C - P = S - K * exp(-rT)
    const double parity = call_result.price - put_result.price;
    const double expected_parity = market.spot - call.strike * std::exp(-market.rate * call.maturity);
    expect_near(parity, expected_parity, 0.001, "call-put parity");

    // Monte Carlo should converge toward Black-Scholes for a European option.
    // The tolerance is looser because Monte Carlo has sampling noise.
    MonteCarloPricer monte_carlo(200000, 42);
    const PriceResult mc = monte_carlo.price(call, market);
    expect_near(mc.price, call_result.price, 0.15, "Monte Carlo convergence");

    // With H=0.5 and no trading-time clustering, the fractal Brownian pricer
    // reduces to a Brownian-style model and should stay close to Black-Scholes.
    FractalBrownianMonteCarloPricer fractal_monte_carlo(120000, 48, 0.5, 0.0, 42);
    const PriceResult fractal_mc = fractal_monte_carlo.price(call, market);
    expect_near(fractal_mc.price, call_result.price, 0.35, "Fractal Brownian Monte Carlo Brownian limit");

    // Binomial trees converge toward Black-Scholes for European vanilla options
    // as the number of steps increases.
    BinomialTreePricer binomial_tree(1000);
    const PriceResult tree = binomial_tree.price(call, market);
    expect_near(tree.price, call_result.price, 0.03, "Binomial convergence");

    // American puts should be worth at least as much as equivalent European
    // puts because early exercise is allowed.
    OptionContract american_put = put;
    american_put.exerciseStyle = ExerciseStyle::American;
    const PriceResult american_put_result = binomial_tree.price(american_put, market);
    if (american_put_result.price < put_result.price) {
        std::cerr << "American put value failed: expected at least European put price\n";
        return 1;
    }

    // If we feed the Black-Scholes price into the implied-volatility solver, it
    // should recover the original volatility used to make that price.
    ImpliedVolatilitySolver implied_volatility_solver;
    const ImpliedVolatilityResult implied_vol =
        implied_volatility_solver.solve(call, market, call_result.price, 1.0e-7);
    if (!implied_vol.converged) {
        std::cerr << "Implied volatility solver failed to converge\n";
        return 1;
    }
    expect_near(implied_vol.volatility, market.volatility, 0.0001, "Implied volatility recovery");

    // Check analytic Delta against a finite-difference approximation.
    //
    // Finite difference:
    // Delta approx (V(S + h) - V(S - h)) / (2h)
    const double bump = 0.01;
    MarketSnapshot up = market;
    MarketSnapshot down = market;
    up.spot += bump;
    down.spot -= bump;
    const double finite_delta = (pricer.price(call, up).price - pricer.price(call, down).price) / (2.0 * bump);
    expect_near(call_result.delta, finite_delta, 0.001, "Delta finite difference");

    std::cout << "All QuantEdge pricing tests passed.\n";
    return 0;
}
