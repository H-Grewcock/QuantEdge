#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <vector>

namespace quantedge::runtime {

// Thread-safe latency recorder.
//
// Each completed task records its runtime in milliseconds. The tracker keeps a
// bounded rolling sample so percentile calculations remain cheap and memory use
// stays predictable.
class LatencyTracker {
public:
    explicit LatencyTracker(std::size_t maxSamples = 4096)
        : maxSamples_(std::max<std::size_t>(maxSamples, 1)) {}

    void record(std::chrono::steady_clock::duration duration) {
        const double millis = std::chrono::duration<double, std::milli>(duration).count();
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(millis);
        if (samples_.size() > maxSamples_) {
            samples_.erase(samples_.begin());
        }
    }

    double percentile(double percentileValue) const {
        std::vector<double> copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            copy = samples_;
        }

        if (copy.empty()) {
            return 0.0;
        }

        std::sort(copy.begin(), copy.end());
        const double clamped = std::clamp(percentileValue, 0.0, 100.0);
        const std::size_t index = static_cast<std::size_t>(
            (clamped / 100.0) * static_cast<double>(copy.size() - 1)
        );
        return copy[index];
    }

    std::size_t sampleCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }

private:
    std::size_t maxSamples_;
    mutable std::mutex mutex_;
    std::vector<double> samples_;
};

} // namespace quantedge::runtime
