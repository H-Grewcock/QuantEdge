#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <numeric>
#include <vector>

namespace quantedge::fractal {

// Bundle of diagnostics derived from recent price history.
//
// These values are descriptive diagnostics, not forecasts. They are designed
// to help the engine say "this path looks persistent/rough/unstable" and then
// compare that with pricing risk and runtime behaviour.
struct FractalDiagnostics {
    // H around 0.5 suggests random-walk-like behaviour.
    // H above 0.5 suggests persistence/trend reinforcement.
    // H below 0.5 suggests anti-persistence/mean reversion.
    double hurstExponent{0.5};

    // Measures how jagged the realised path is compared with a straight move
    // from first price to last price. Higher means more back-and-forth movement.
    double pathRoughness{0.0};

    // Measures how unstable absolute returns are from step to step.
    double volatilityRoughness{0.0};

    // Serial correlation of adjacent returns. Fractional Brownian motion uses
    // this distinction: positive values suggest persistent increments, negative
    // values suggest anti-persistence.
    double incrementAutocorrelation{0.0};

    // Coefficient of variation of local realised volatility. In the paper's
    // language this approximates whether real time is being stretched into
    // bursts of trading time.
    double tradingTimeIntensity{0.0};

    // Spread between quiet and active local volatility states. A larger spread
    // is a simple proxy for a wider multifractal spectrum.
    double multifractalSpectrumWidth{0.0};

    // Measures whether drawdown has recently worsened.
    double drawdownAcceleration{0.0};

    // Heuristic score combining trend, acceleration, persistence, and volatility
    // roughness. It flags bubble-like conditions; it does not predict bubbles.
    double bubbleLikeScore{0.0};

    // Simple confidence proxy based on sample size.
    double confidence{0.0};
};

// FractalAnalyzer turns a price history into interpretable roughness metrics.
//
// This MVP deliberately starts with simple, robust heuristics. More advanced
// multifractal or MF-DFA-style analysis can be added later without changing the
// public FractalDiagnostics output shape too much.
class FractalAnalyzer {
public:
    // Main analysis entrypoint.
    //
    // prices: recent spot prices
    // returns: usually log returns computed from those prices
    FractalDiagnostics analyze(const std::vector<double>& prices, const std::vector<double>& returns) const {
        FractalDiagnostics diagnostics;

        // Confidence ramps up as we get more observations. 128 is arbitrary but
        // useful for the MVP: a tiny sample should be treated cautiously.
        diagnostics.confidence = std::min(1.0, static_cast<double>(prices.size()) / 128.0);

        // Compute each diagnostic separately, then combine some of them into
        // the bubble-like score.
        diagnostics.hurstExponent = estimate_hurst(prices);
        diagnostics.pathRoughness = path_roughness(prices);
        diagnostics.volatilityRoughness = volatility_roughness(returns);
        diagnostics.incrementAutocorrelation = increment_autocorrelation(returns);
        diagnostics.tradingTimeIntensity = trading_time_intensity(returns);
        diagnostics.multifractalSpectrumWidth = multifractal_spectrum_width(returns);
        diagnostics.drawdownAcceleration = drawdown_acceleration(prices);
        diagnostics.bubbleLikeScore = bubble_score(prices, returns, diagnostics);
        return diagnostics;
    }

private:
    // Estimate Hurst exponent using a simple rescaled-range style statistic.
    //
    // A full Hurst estimator would compare multiple window sizes and fit a
    // scaling law. This compact version is enough for an MVP diagnostic and
    // keeps the first implementation easy to audit.
    static double estimate_hurst(const std::vector<double>& prices) {
        if (prices.size() < 8) {
            // Too few points to infer persistence, so return neutral.
            return 0.5;
        }

        // Centre the price series by subtracting its mean.
        const double mean = std::accumulate(prices.begin(), prices.end(), 0.0) / static_cast<double>(prices.size());
        std::vector<double> cumulative;
        cumulative.reserve(prices.size());
        double running = 0.0;
        for (double price : prices) {
            // Cumulative deviations reveal whether the series tends to keep
            // drifting in one direction or repeatedly mean-revert.
            running += price - mean;
            cumulative.push_back(running);
        }

        // Range of cumulative deviations.
        const auto [min_it, max_it] = std::minmax_element(cumulative.begin(), cumulative.end());
        const double range = *max_it - *min_it;

        // Standard deviation of the original prices.
        double variance = 0.0;
        for (double price : prices) {
            const double diff = price - mean;
            variance += diff * diff;
        }
        const double stdev = std::sqrt(variance / static_cast<double>(prices.size()));
        if (stdev <= 1.0e-12 || range <= 1.0e-12) {
            // Flat or nearly flat data carries no useful persistence signal.
            return 0.5;
        }

        // Rescaled range statistic. The log ratio gives a compact H estimate.
        const double rs = range / stdev;
        return std::clamp(std::log(rs) / std::log(static_cast<double>(prices.size())), 0.0, 1.0);
    }

    // Compare total travelled distance with direct distance.
    //
    // A smooth monotonic path has low roughness because total path length is
    // close to direct distance. A choppy path has high roughness because it
    // travels much farther than the start-to-end move.
    static double path_roughness(const std::vector<double>& prices) {
        if (prices.size() < 3) {
            return 0.0;
        }

        double absolute_path = 0.0;
        for (std::size_t i = 1; i < prices.size(); ++i) {
            // Add each local move, ignoring direction.
            absolute_path += std::abs(prices[i] - prices[i - 1]);
        }

        // Direct move from the first observation to the last.
        const double direct = std::abs(prices.back() - prices.front());
        return std::clamp((absolute_path - direct) / std::max(absolute_path, 1.0e-12), 0.0, 1.0);
    }

    // Measure how much realised volatility changes from tick to tick.
    //
    // This is intentionally simple: we look at absolute returns and ask whether
    // their size is stable or jumpy.
    static double volatility_roughness(const std::vector<double>& returns) {
        if (returns.size() < 4) {
            return 0.0;
        }

        std::vector<double> abs_returns;
        abs_returns.reserve(returns.size());
        for (double value : returns) {
            // Absolute returns proxy local realised volatility.
            abs_returns.push_back(std::abs(value));
        }

        // Sum changes in absolute return size.
        double total_change = 0.0;
        for (std::size_t i = 1; i < abs_returns.size(); ++i) {
            total_change += std::abs(abs_returns[i] - abs_returns[i - 1]);
        }

        // Normalise by average absolute return so the score is scale-aware.
        const double mean = std::accumulate(abs_returns.begin(), abs_returns.end(), 0.0)
            / static_cast<double>(abs_returns.size());
        return std::clamp(total_change / (mean * static_cast<double>(abs_returns.size()) + 1.0e-12), 0.0, 1.0);
    }

    static double increment_autocorrelation(const std::vector<double>& returns) {
        if (returns.size() < 3) {
            return 0.0;
        }

        const double mean = std::accumulate(returns.begin(), returns.end(), 0.0)
            / static_cast<double>(returns.size());
        double numerator = 0.0;
        double denominator = 0.0;
        for (std::size_t i = 1; i < returns.size(); ++i) {
            numerator += (returns[i - 1] - mean) * (returns[i] - mean);
        }
        for (double value : returns) {
            const double centered = value - mean;
            denominator += centered * centered;
        }
        if (denominator <= 1.0e-18) {
            return 0.0;
        }
        return std::clamp(numerator / denominator, -1.0, 1.0);
    }

    static std::vector<double> local_absolute_return_means(const std::vector<double>& returns) {
        std::vector<double> local;
        if (returns.size() < 8) {
            return local;
        }

        const std::size_t window = std::clamp<std::size_t>(returns.size() / 12, 4, 32);
        if (returns.size() < window) {
            return local;
        }
        local.reserve(returns.size() - window + 1);
        double rolling = 0.0;
        for (std::size_t i = 0; i < returns.size(); ++i) {
            rolling += std::abs(returns[i]);
            if (i >= window) {
                rolling -= std::abs(returns[i - window]);
            }
            if (i + 1 >= window) {
                local.push_back(rolling / static_cast<double>(window));
            }
        }
        return local;
    }

    static double trading_time_intensity(const std::vector<double>& returns) {
        const std::vector<double> local = local_absolute_return_means(returns);
        if (local.size() < 3) {
            return 0.0;
        }

        const double mean = std::accumulate(local.begin(), local.end(), 0.0) / static_cast<double>(local.size());
        if (mean <= 1.0e-12) {
            return 0.0;
        }
        double variance = 0.0;
        for (double value : local) {
            const double diff = value - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(local.size());
        return std::clamp(std::sqrt(variance) / mean, 0.0, 3.0) / 3.0;
    }

    static double multifractal_spectrum_width(const std::vector<double>& returns) {
        std::vector<double> local = local_absolute_return_means(returns);
        if (local.size() < 5) {
            return 0.0;
        }

        std::sort(local.begin(), local.end());
        const auto percentile = [&local](double p) {
            const std::size_t index = static_cast<std::size_t>(
                std::clamp(p, 0.0, 1.0) * static_cast<double>(local.size() - 1)
            );
            return local[index];
        };
        const double quiet = percentile(0.10);
        const double active = percentile(0.90);
        return std::clamp(std::log((active + 1.0e-12) / (quiet + 1.0e-12)) / 4.0, 0.0, 1.0);
    }

    // Estimate whether drawdown has recently accelerated.
    //
    // Drawdown is the percentage fall from the running peak. If the latest
    // drawdown is much worse than mid-sample drawdown, this score rises.
    static double drawdown_acceleration(const std::vector<double>& prices) {
        if (prices.size() < 6) {
            return 0.0;
        }

        double peak = prices.front();
        std::vector<double> drawdowns;
        drawdowns.reserve(prices.size());
        for (double price : prices) {
            // Update running maximum and measure loss from that maximum.
            peak = std::max(peak, price);
            drawdowns.push_back((peak - price) / std::max(peak, 1.0e-12));
        }

        // Compare recent drawdown with the middle of the sample.
        const double recent = drawdowns.back() - drawdowns[drawdowns.size() / 2];
        return std::clamp(recent * 10.0, 0.0, 1.0);
    }

    // Heuristic bubble-like diagnostic.
    //
    // This deliberately avoids saying "bubble prediction". It simply combines
    // features that make a path look persistent and unstable:
    // - price rose over the window
    // - returns accelerated
    // - Hurst exponent is above neutral
    // - volatility is rough
    static double bubble_score(
        const std::vector<double>& prices,
        const std::vector<double>& returns,
        const FractalDiagnostics& diagnostics
    ) {
        if (prices.size() < 8 || returns.size() < 8) {
            return 0.0;
        }

        // Total positive move over the sample, scaled into 0..1.
        const double trend = std::clamp((prices.back() / prices.front() - 1.0) * 2.0, 0.0, 1.0);

        // Split returns into early and late halves. If the late average is
        // higher, the path is accelerating.
        const std::size_t midpoint = returns.size() / 2;
        const double early = std::accumulate(returns.begin(), returns.begin() + static_cast<std::ptrdiff_t>(midpoint), 0.0)
            / static_cast<double>(midpoint);
        const double late = std::accumulate(returns.begin() + static_cast<std::ptrdiff_t>(midpoint), returns.end(), 0.0)
            / static_cast<double>(returns.size() - midpoint);
        const double acceleration = std::clamp((late - early) * 80.0, 0.0, 1.0);

        // Convert Hurst excess above 0.5 into a 0..1 persistence component.
        const double persistence = std::clamp((diagnostics.hurstExponent - 0.5) * 3.0, 0.0, 1.0);

        // Weighted combination. These weights are intentionally transparent so
        // they can be tuned later during experiments.
        const double trading_time = std::max(diagnostics.tradingTimeIntensity, diagnostics.multifractalSpectrumWidth);
        return std::clamp(0.30 * trend + 0.22 * acceleration + 0.22 * persistence
            + 0.14 * diagnostics.volatilityRoughness + 0.12 * trading_time, 0.0, 1.0);
    }
};

} // namespace quantedge::fractal
