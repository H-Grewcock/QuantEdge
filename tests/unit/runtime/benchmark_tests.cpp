#include "quantedge/benchmarks/pricing_benchmark.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

bool file_contains(const std::string& path, const std::string& needle) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

} // namespace

int main() {
    using namespace quantedge;

    pricing::OptionContract option;
    option.strike = 100.0;
    option.maturity = 0.5;

    pricing::MarketSnapshot market;
    market.spot = 101.0;
    market.volatility = 0.22;
    market.rate = 0.048;

    benchmarks::PricingBenchmarkRunner runner;
    const auto results = runner.run(option, market, 2);
    if (results.size() != 3) {
        fail("Pricing benchmark runner did not return three methods");
    }
    for (const auto& row : results) {
        if (row.iterations != 2 || row.averageMs < 0.0 || row.lastPrice <= 0.0) {
            fail("Pricing benchmark row contained invalid values");
        }
    }

    const std::string path = "quantedge_benchmark_test.json";
    if (!runner.write_json(path, results)) {
        fail("Pricing benchmark runner failed to write JSON");
    }
    if (!file_contains(path, "pricingBenchmarks") || !file_contains(path, "Black-Scholes")) {
        fail("Pricing benchmark JSON did not contain expected fields");
    }
    std::remove(path.c_str());

    std::cout << "All QuantEdge benchmark tests passed.\n";
    return 0;
}
