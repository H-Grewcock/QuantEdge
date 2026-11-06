#pragma once

#include "quantedge/runtime/latency_tracker.hpp"
#include "quantedge/runtime/runtime_metrics.hpp"
#include "quantedge/runtime/task_priority.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace quantedge::runtime {

// Priority task scheduler backed by worker threads.
//
// This is the first real-time systems component in QuantEdge. It lets the
// engine submit work with different urgency levels and observe basic runtime
// pressure through queue depth and latency percentiles.
class TaskScheduler {
public:
    explicit TaskScheduler(std::size_t workerCount = std::thread::hardware_concurrency())
        : startedAt_(std::chrono::steady_clock::now()) {
        const std::size_t count = std::max<std::size_t>(workerCount, 1);
        workers_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    }

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    ~TaskScheduler() {
        shutdown();
    }

    template <typename Fn>
    auto submit(TaskPriority priority, Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
        using Result = std::invoke_result_t<Fn>;

        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Fn>(fn));
        std::future<Result> future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!acceptingWork_) {
                throw std::runtime_error("TaskScheduler is not accepting work");
            }
            tasks_.push(ScheduledTask{
                priority_rank(priority),
                sequence_++,
                [task] {
                    (*task)();
                }
            });
        }

        condition_.notify_one();
        return future;
    }

    RuntimeMetrics metrics() const {
        RuntimeMetrics result;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            result.queueDepth = tasks_.size();
        }

        result.p50LatencyMs = latency_.percentile(50.0);
        result.p95LatencyMs = latency_.percentile(95.0);
        result.p99LatencyMs = latency_.percentile(99.0);

        const auto elapsed = std::chrono::steady_clock::now() - startedAt_;
        const double seconds = std::chrono::duration<double>(elapsed).count();
        if (seconds > 0.0) {
            result.tasksPerSecond = static_cast<double>(completedTasks_.load()) / seconds;
        }
        return result;
    }

    void shutdown() {
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true)) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            acceptingWork_ = false;
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    struct ScheduledTask {
        int priority{priority_rank(TaskPriority::Normal)};
        std::size_t sequence{0};
        std::function<void()> run;
    };

    struct TaskOrder {
        bool operator()(const ScheduledTask& lhs, const ScheduledTask& rhs) const {
            if (lhs.priority != rhs.priority) {
                return lhs.priority > rhs.priority;
            }
            return lhs.sequence > rhs.sequence;
        }
    };

    void worker_loop() {
        while (true) {
            ScheduledTask task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] {
                    return !acceptingWork_ || !tasks_.empty();
                });

                if (!acceptingWork_ && tasks_.empty()) {
                    return;
                }

                task = tasks_.top();
                tasks_.pop();
            }

            const auto start = std::chrono::steady_clock::now();
            task.run();
            const auto end = std::chrono::steady_clock::now();
            latency_.record(end - start);
            ++completedTasks_;
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, TaskOrder> tasks_;
    std::vector<std::thread> workers_;
    LatencyTracker latency_;
    std::atomic<std::size_t> completedTasks_{0};
    std::atomic<bool> stopped_{false};
    std::chrono::steady_clock::time_point startedAt_;
    std::size_t sequence_{0};
    bool acceptingWork_{true};
};

} // namespace quantedge::runtime
