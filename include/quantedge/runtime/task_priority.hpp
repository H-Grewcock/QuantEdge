#pragma once

namespace quantedge::runtime {

// Priority levels used by the runtime scheduler.
//
// Lower numeric values are treated as more urgent in the priority queue. This
// mirrors the project roadmap:
// - Critical: market tick ingestion
// - High: fast repricing
// - Normal: Greeks and diagnostics
// - Low: risk surfaces and expensive reports
enum class TaskPriority {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3
};

inline int priority_rank(TaskPriority priority) {
    return static_cast<int>(priority);
}

} // namespace quantedge::runtime
