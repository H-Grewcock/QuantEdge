#pragma once

namespace quantedge::system {

// Metrics read from the operating system.
//
// Runtime scheduler metrics tell us about QuantEdge's internal queue and task
// latency. These system metrics tell us what the OS sees: CPU, memory, thread
// count, and context switches.
struct SystemMetrics {
    double processCpuSeconds{0.0};
    double memoryUsageMb{0.0};
    int threadCount{0};
    long voluntaryContextSwitches{0};
    long nonvoluntaryContextSwitches{0};
    bool available{false};
};

} // namespace quantedge::system
