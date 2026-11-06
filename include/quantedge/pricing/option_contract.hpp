#pragma once

#include <string>

namespace quantedge::pricing {

// The two plain-vanilla option directions.
//
// A Call gives the holder exposure to upside in the underlying asset.
// A Put gives the holder exposure to downside in the underlying asset.
enum class OptionType {
    Call,
    Put
};

// Exercise style controls when the option can be exercised.
//
// European options can only be exercised at maturity.
// American options can be exercised before maturity.
//
// The current MVP prices European options directly in Black-Scholes and
// Monte Carlo. The enum is already here because the project roadmap includes
// binomial-tree American pricing.
enum class ExerciseStyle {
    European,
    American
};

// OptionContract describes the instrument we want to price.
//
// This is deliberately separate from MarketSnapshot. The contract is the fixed
// legal/financial object: underlying, strike, time to expiry, call/put, and
// exercise rules. The market snapshot is the changing market environment.
struct OptionContract {
    // Ticker or identifier of the underlying asset.
    std::string underlying{"AAPL"};

    // Strike price K. For a call, payoff at maturity is max(S - K, 0).
    // For a put, payoff at maturity is max(K - S, 0).
    double strike{100.0};

    // Time to maturity in years. For example, 0.5 means roughly six months.
    double maturity{0.5};

    // Whether this is a call or a put.
    OptionType type{OptionType::Call};

    // European is the current supported style for the MVP.
    ExerciseStyle exerciseStyle{ExerciseStyle::European};
};

} // namespace quantedge::pricing
