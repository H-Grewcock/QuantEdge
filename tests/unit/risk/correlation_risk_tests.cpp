#include "quantedge/risk/correlation_risk.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge::risk;

    const std::vector<AssetReturnSeries> assets{
        {"AAPL", 0.5, {0.01, 0.02, -0.01, 0.015, 0.005}},
        {"MSFT", 0.3, {0.008, 0.018, -0.008, 0.012, 0.004}},
        {"TLT", 0.2, {-0.002, -0.004, 0.003, -0.001, 0.002}}
    };

    CorrelationRiskAnalyzer analyzer;
    const CorrelationRiskReport report = analyzer.analyze(assets);

    if (report.symbols.size() != 3 || report.correlation.size() != 3 || report.covariance.size() != 3) {
        fail("Correlation report dimensions were incorrect");
    }
    if (report.correlation[0][0] != 1.0) {
        fail("Correlation diagonal should be one");
    }
    if (report.portfolioVariance < 0.0 || report.portfolioVolatility < 0.0) {
        fail("Portfolio variance/volatility should be non-negative");
    }
    if (report.strongestPairs.empty()) {
        fail("Correlation report should include strongest pairs");
    }

    std::cout << "All QuantEdge correlation risk tests passed.\n";
    return 0;
}
