#pragma once

#include "quantedge/pricing/binomial_tree.hpp"
#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/monte_carlo.hpp"
#include "quantedge/pricing/option_contract.hpp"

#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <string>
#include <vector>

namespace quantedge::benchmarks {

// One benchmark row for one pricing method.
//
// The runner stores timings in milliseconds because this is readable in CLI and
// JSON output. More serious benchmark tooling can later add percentiles,
// CPU counters, and allocation tracking.
struct PricingBenchmarkResult {
    std::string method;
    std::size_t iterations{0};
    double totalMs{0.0};
    double averageMs{0.0};
    double lastPrice{0.0};
};

// Small benchmark runner for the current pricing models.
//
// This is not a replacement for Google Benchmark. It is a lightweight project
// artifact generator: enough to show relative pricing cost and create reports
// for the HTML preview without adding external dependencies.
class PricingBenchmarkRunner {
public:
    std::vector<PricingBenchmarkResult> run(
        const pricing::OptionContract& option,
        const pricing::MarketSnapshot& market,
        std::size_t iterations = 100
    ) const {
        iterations = iterations == 0 ? 1 : iterations;
        std::vector<PricingBenchmarkResult> results;
        results.push_back(run_one("Black-Scholes", iterations, [&] {
            pricing::BlackScholesPricer pricer;
            return pricer.price(option, market).price;
        }));
        results.push_back(run_one("Binomial Tree", iterations, [&] {
            pricing::BinomialTreePricer pricer(500);
            return pricer.price(option, market).price;
        }));
        results.push_back(run_one("Monte Carlo", iterations, [&] {
            pricing::MonteCarloPricer pricer(25000, 123);
            return pricer.price(option, market).price;
        }));
        return results;
    }

    bool write_json(const std::string& path, const std::vector<PricingBenchmarkResult>& results) const {
        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            return false;
        }

        file << std::fixed << std::setprecision(8);
        file << "{\n";
        file << "  \"pricingBenchmarks\": [\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& row = results[i];
            file << "    {"
                 << "\"method\": \"" << row.method << "\", "
                 << "\"iterations\": " << row.iterations << ", "
                 << "\"totalMs\": " << row.totalMs << ", "
                 << "\"averageMs\": " << row.averageMs << ", "
                 << "\"lastPrice\": " << row.lastPrice
                 << "}";
            if (i + 1 < results.size()) {
                file << ',';
            }
            file << '\n';
        }
        file << "  ]\n";
        file << "}\n";

        return static_cast<bool>(file);
    }

private:
    template <typename Fn>
    static PricingBenchmarkResult run_one(const std::string& method, std::size_t iterations, Fn&& fn) {
        const auto start = std::chrono::steady_clock::now();
        double last_price = 0.0;
        for (std::size_t i = 0; i < iterations; ++i) {
            last_price = fn();
        }
        const auto end = std::chrono::steady_clock::now();
        const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

        PricingBenchmarkResult result;
        result.method = method;
        result.iterations = iterations;
        result.totalMs = total_ms;
        result.averageMs = total_ms / static_cast<double>(iterations);
        result.lastPrice = last_price;
        return result;
    }
};

} // namespace quantedge::benchmarks
