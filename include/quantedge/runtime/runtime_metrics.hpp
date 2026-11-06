#pragma once

#include <cstddef>

namespace quantedge::runtime {

// Lightweight runtime metrics produced by the scheduler.
//
// Later this can be merged with Linux /proc metrics. For now it captures the
// pieces the engine itself knows: queue depth, latency percentiles, and task
// throughput.
struct RuntimeMetrics {
    std::size_t queueDepth{0};
    double p50LatencyMs{0.0};
    double p95LatencyMs{0.0};
    double p99LatencyMs{0.0};
    double tasksPerSecond{0.0};
};

} // namespace quantedge::runtime
