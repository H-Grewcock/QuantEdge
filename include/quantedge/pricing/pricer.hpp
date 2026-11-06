#pragma once

#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/option_contract.hpp"
#include "quantedge/pricing/price_result.hpp"

namespace quantedge::pricing {

// Common interface for pricing models.
//
// This lets the rest of QuantEdge ask "price this contract in this market"
// without caring whether the implementation is Black-Scholes, Monte Carlo,
// a binomial tree, or a future GPU-accelerated engine.
class IPricer {
public:
    // The core pricing operation.
    //
    // option: the fixed option contract
    // market: the current market inputs
    // returns: price, Greeks if available, uncertainty if available, method name
    virtual PriceResult price(const OptionContract& option, const MarketSnapshot& market) const = 0;

    // Virtual destructor makes deletion through IPricer* safe.
    virtual ~IPricer() = default;
};

} // namespace quantedge::pricing
