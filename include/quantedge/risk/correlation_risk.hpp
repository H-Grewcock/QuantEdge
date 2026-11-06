#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace quantedge::risk {

// Return history for one asset.
//
// The analyzer expects returns, not prices, because covariance/correlation are
// defined over changes. In the CLI demo, one real simulated return stream is
// transformed into several synthetic symbols so the matrix machinery is visible.
struct AssetReturnSeries {
    std::string symbol;
    double portfolioWeight{0.0};
    std::vector<double> returns;
};

struct CorrelationPair {
    std::string first;
    std::string second;
    double correlation{0.0};
};

struct CorrelationRiskReport {
    std::vector<std::string> symbols;
    std::vector<std::vector<double>> covariance;
    std::vector<std::vector<double>> correlation;
    std::vector<CorrelationPair> strongestPairs;
    double portfolioVariance{0.0};
    double portfolioVolatility{0.0};
    double averageCorrelation{0.0};
    double concentrationScore{0.0};
    double correlationBreakLoss{0.0};
};

// Multi-asset covariance/correlation diagnostics.
//
// This is a quant-risk building block: when correlations rise or break down,
// diversification can disappear. QuantEdge uses this report to show cross-asset
// dependence and a simple "correlation shock" stress.
class CorrelationRiskAnalyzer {
public:
    CorrelationRiskReport analyze(const std::vector<AssetReturnSeries>& assets) const {
        CorrelationRiskReport report;
        if (assets.empty()) {
            return report;
        }

        const std::size_t n = assets.size();
        report.symbols.reserve(n);
        report.covariance.assign(n, std::vector<double>(n, 0.0));
        report.correlation.assign(n, std::vector<double>(n, 0.0));

        for (const auto& asset : assets) {
            report.symbols.push_back(asset.symbol);
        }

        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                report.covariance[i][j] = covariance(assets[i].returns, assets[j].returns);
                const double denom = std::sqrt(std::max(report.covariance[i][i], 0.0))
                    * std::sqrt(std::max(report.covariance[j][j], 0.0));
                report.correlation[i][j] = denom <= 1.0e-12 ? (i == j ? 1.0 : 0.0) : report.covariance[i][j] / denom;
            }
        }

        report.portfolioVariance = portfolio_variance(assets, report.covariance);
        report.portfolioVolatility = std::sqrt(std::max(report.portfolioVariance, 0.0));
        report.averageCorrelation = average_off_diagonal(report.correlation);
        report.concentrationScore = concentration_score(assets);
        report.strongestPairs = strongest_pairs(assets, report.correlation);
        report.correlationBreakLoss = correlation_break_loss(assets, report.covariance);
        return report;
    }

private:
    static double mean(const std::vector<double>& values) {
        if (values.empty()) {
            return 0.0;
        }
        return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    }

    static double covariance(const std::vector<double>& lhs, const std::vector<double>& rhs) {
        const std::size_t count = std::min(lhs.size(), rhs.size());
        if (count < 2) {
            return 0.0;
        }

        const double lhs_mean = std::accumulate(lhs.begin(), lhs.begin() + static_cast<std::ptrdiff_t>(count), 0.0)
            / static_cast<double>(count);
        const double rhs_mean = std::accumulate(rhs.begin(), rhs.begin() + static_cast<std::ptrdiff_t>(count), 0.0)
            / static_cast<double>(count);
        double total = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            total += (lhs[i] - lhs_mean) * (rhs[i] - rhs_mean);
        }
        return total / static_cast<double>(count - 1);
    }

    static double portfolio_variance(
        const std::vector<AssetReturnSeries>& assets,
        const std::vector<std::vector<double>>& covariance_matrix
    ) {
        double variance = 0.0;
        for (std::size_t i = 0; i < assets.size(); ++i) {
            for (std::size_t j = 0; j < assets.size(); ++j) {
                variance += assets[i].portfolioWeight * assets[j].portfolioWeight * covariance_matrix[i][j];
            }
        }
        return variance;
    }

    static double average_off_diagonal(const std::vector<std::vector<double>>& matrix) {
        double total = 0.0;
        std::size_t count = 0;
        for (std::size_t i = 0; i < matrix.size(); ++i) {
            for (std::size_t j = i + 1; j < matrix.size(); ++j) {
                total += matrix[i][j];
                ++count;
            }
        }
        return count == 0 ? 0.0 : total / static_cast<double>(count);
    }

    static double concentration_score(const std::vector<AssetReturnSeries>& assets) {
        double total = 0.0;
        for (const auto& asset : assets) {
            total += asset.portfolioWeight * asset.portfolioWeight;
        }
        return total;
    }

    static std::vector<CorrelationPair> strongest_pairs(
        const std::vector<AssetReturnSeries>& assets,
        const std::vector<std::vector<double>>& correlation_matrix
    ) {
        std::vector<CorrelationPair> pairs;
        for (std::size_t i = 0; i < assets.size(); ++i) {
            for (std::size_t j = i + 1; j < assets.size(); ++j) {
                pairs.push_back({assets[i].symbol, assets[j].symbol, correlation_matrix[i][j]});
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const auto& lhs, const auto& rhs) {
            return std::abs(lhs.correlation) > std::abs(rhs.correlation);
        });
        if (pairs.size() > 3) {
            pairs.resize(3);
        }
        return pairs;
    }

    static double correlation_break_loss(
        const std::vector<AssetReturnSeries>& assets,
        const std::vector<std::vector<double>>& covariance_matrix
    ) {
        if (assets.size() < 2) {
            return 0.0;
        }

        std::vector<std::vector<double>> shocked = covariance_matrix;
        for (std::size_t i = 0; i < assets.size(); ++i) {
            for (std::size_t j = i + 1; j < assets.size(); ++j) {
                const double vol_i = std::sqrt(std::max(covariance_matrix[i][i], 0.0));
                const double vol_j = std::sqrt(std::max(covariance_matrix[j][j], 0.0));
                shocked[i][j] = 0.95 * vol_i * vol_j;
                shocked[j][i] = shocked[i][j];
            }
        }

        const double base = portfolio_variance(assets, covariance_matrix);
        const double stressed = portfolio_variance(assets, shocked);
        return std::max(0.0, std::sqrt(std::max(stressed, 0.0)) - std::sqrt(std::max(base, 0.0)));
    }
};

} // namespace quantedge::risk
